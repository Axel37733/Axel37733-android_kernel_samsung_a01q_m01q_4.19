// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 Google, Inc.
 */

#include <linux/kernel.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>

#include "ion.h"
#include "ion_system_secure_heap.h"

#ifdef CONFIG_ION_LEGACY
#include "ion_legacy.h"
#endif

union ion_ioctl_arg {
	struct ion_allocation_data allocation;
	struct ion_heap_query query;
	struct ion_prefetch_data prefetch_data;
#ifdef CONFIG_ION_LEGACY
	struct ion_fd_data fd;
	struct ion_old_allocation_data old_allocation;
	struct ion_handle_data handle;
	struct ion_flush_data flush;
	struct ion_custom_data custom;
#endif
};

static int validate_ioctl_arg(unsigned int cmd, union ion_ioctl_arg *arg)
{
	switch (cmd) {
	case ION_IOC_HEAP_QUERY:
		if (arg->query.reserved0 ||
		    arg->query.reserved1 ||
		    arg->query.reserved2)
			return -EINVAL;
		break;
	default:
		break;
	}

	return 0;
}

/* fix up the cases where the ioctl direction bits are incorrect */
static unsigned int ion_ioctl_dir(unsigned int cmd)
{
	switch (cmd) {
#ifdef CONFIG_ION_LEGACY
	case ION_IOC_CLEAN_CACHES:
	case ION_IOC_INV_CACHES:
	case ION_IOC_CLEAN_INV_CACHES:
	case ION_IOC_FREE:
		return _IOC_WRITE;
#endif
	default:
		return _IOC_DIR(cmd);
	}
}

#ifdef CONFIG_ION_LEGACY
/* a01q: the legacy handle is the dma-buf fd itself (see ION_OLD_IOC_ALLOC) */
static int a01q_ion_cache_op(unsigned int cmd, int fd)
{
	struct dma_buf *dmabuf;
	int ret = 0, ret2;

	pr_info_once("a01q ion: legacy cache ioctls in use\n");
	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);

	switch (cmd) {
	case ION_IOC_CLEAN_CACHES:
		ret = dma_buf_end_cpu_access(dmabuf, DMA_TO_DEVICE);
		break;
	case ION_IOC_INV_CACHES:
		ret = dma_buf_begin_cpu_access(dmabuf, DMA_FROM_DEVICE);
		break;
	case ION_IOC_CLEAN_INV_CACHES:
		ret = dma_buf_end_cpu_access(dmabuf, DMA_TO_DEVICE);
		ret2 = dma_buf_begin_cpu_access(dmabuf, DMA_FROM_DEVICE);
		if (!ret)
			ret = ret2;
		break;
	}

	dma_buf_put(dmabuf);
	return ret;
}
#endif

#ifdef CONFIG_ION_LEGACY
/* a01q: ION_IOC_CUSTOM carries the legacy msm cache ioctls; layout depends on the caller ABI */
static int a01q_ion_custom(unsigned int ucmd, void __user *uarg)
{
	int op, handle, fd;

	if (_IOC_TYPE(ucmd) != ION_IOC_MSM_MAGIC)
		return -ENOTTY;

	switch (_IOC_NR(ucmd)) {
	case 0:
		op = ION_IOC_CLEAN_CACHES;
		break;
	case 1:
		op = ION_IOC_INV_CACHES;
		break;
	case 2:
		op = ION_IOC_CLEAN_INV_CACHES;
		break;
	default:
		return -ENOTTY;
	}

	if (_IOC_SIZE(ucmd) == 20) {
		struct {
			s32 handle;
			s32 fd;
			u32 vaddr;
			u32 offset;
			u32 length;
		} d;

		if (copy_from_user(&d, uarg, sizeof(d)))
			return -EFAULT;
		handle = d.handle;
		fd = d.fd;
	} else if (_IOC_SIZE(ucmd) == sizeof(struct ion_flush_data)) {
		struct ion_flush_data d;

		if (copy_from_user(&d, uarg, sizeof(d)))
			return -EFAULT;
		handle = d.handle;
		fd = d.fd;
	} else {
		return -EINVAL;
	}

	return a01q_ion_cache_op(op, handle > 0 ? handle : fd);
}
#endif

static long __a01_ion_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned int dir;
	union ion_ioctl_arg data;

	dir = ion_ioctl_dir(cmd);

	if (_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;

	/*
	 * The copy_from_user is unconditional here for both read and write
	 * to do the validate. If there is no write for the ioctl, the
	 * buffer is cleared
	 */
	if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	ret = validate_ioctl_arg(cmd, &data);
	if (ret) {
		pr_warn_once("%s: ioctl validate failed\n", __func__);
		return ret;
	}

	if (!(dir & _IOC_WRITE))
		memset(&data, 0, sizeof(data));

	switch (cmd) {
	case ION_IOC_ALLOC:
	{
		int fd;

		fd = ion_alloc_fd(data.allocation.len,
				  data.allocation.heap_id_mask,
				  data.allocation.flags);
		if (fd < 0)
			return fd;

		data.allocation.fd = fd;

		break;
	}
	case ION_IOC_HEAP_QUERY:
		ret = ion_query_heaps(&data.query);
		break;
	case ION_IOC_PREFETCH:
	{
		int ret;

		ret = ion_walk_heaps(data.prefetch_data.heap_id,
				     (enum ion_heap_type)
				     ION_HEAP_TYPE_SYSTEM_SECURE,
				     (void *)&data.prefetch_data,
				     ion_system_secure_heap_prefetch);
		if (ret)
			return ret;
		break;
	}
	case ION_IOC_DRAIN:
	{
		int ret;

		ret = ion_walk_heaps(data.prefetch_data.heap_id,
				     (enum ion_heap_type)
				     ION_HEAP_TYPE_SYSTEM_SECURE,
				     (void *)&data.prefetch_data,
				     ion_system_secure_heap_drain);

		if (ret)
			return ret;
		break;
	}
#ifdef CONFIG_ION_LEGACY
	case ION_OLD_IOC_ALLOC:
	{
		int fd;

		fd = ion_alloc_fd(data.old_allocation.len,
				  data.old_allocation.heap_id_mask,
				  data.old_allocation.flags);
		if (fd < 0)
			return fd;

		data.old_allocation.handle = fd;

		break;
	}
	case ION_IOC_FREE:
		/*
		 * libion passes 0 as the handle to check for this ioctl's
		 * existence and expects -ENOTTY on kernel 4.12+ as an indicator
		 * of having a new ION ABI. We want to use new ION as much as
		 * possible, so pretend that this ioctl doesn't exist when
		 * libion checks for it.
		 */
		if (!data.handle.handle)
			ret = -ENOTTY;

		break;
	case ION_IOC_SHARE:
	case ION_IOC_MAP:
		data.fd.fd = data.fd.handle;
		break;
	case ION_IOC_IMPORT:
		data.fd.handle = data.fd.fd;
		break;
	case ION_IOC_CUSTOM:
		ret = a01q_ion_custom(data.custom.cmd,
				(void __user *)data.custom.arg);
		if (ret)
			return ret;
		break;
	case ION_IOC_CLEAN_CACHES:
	case ION_IOC_INV_CACHES:
	case ION_IOC_CLEAN_INV_CACHES:
		ret = a01q_ion_cache_op(cmd, data.flush.handle > 0 ?
					data.flush.handle : data.flush.fd);
		if (ret)
			return ret;
		break;
#endif
	default:
		return -ENOTTY;
	}

	if (dir & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
	}
	return ret;
}

long ion_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long r = __a01_ion_ioctl(filp, cmd, arg);
	if (r < 0 && (strstr(current->comm, "qsee") || strstr(current->comm, "hardwar") || strstr(current->comm, "keymaster")))
		pr_err("a01q ion: ion_ioctl %s cmd=0x%x ret=%ld\n", current->comm, cmd, r);
	if (r == -ENOTTY || r == -ENOIOCTLCMD)
		pr_err_ratelimited("a01q ion: unhandled ioctl %s cmd=0x%x type=0x%02x nr=%u size=%u ret=%ld\n",
				   current->comm, cmd, _IOC_TYPE(cmd), _IOC_NR(cmd), _IOC_SIZE(cmd), r);
	return r;
}
