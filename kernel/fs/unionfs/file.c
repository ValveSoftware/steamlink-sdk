/*
 * Copyright (c) 2003-2014 Erez Zadok
 * Copyright (c) 2003-2006 Charles P. Wright
 * Copyright (c) 2005-2007 Josef 'Jeff' Sipek
 * Copyright (c) 2005-2006 Junjiro Okajima
 * Copyright (c) 2005      Arun M. Krishnakumar
 * Copyright (c) 2004-2006 David P. Quigley
 * Copyright (c) 2003-2004 Mohammad Nayyer Zubair
 * Copyright (c) 2003      Puja Gupta
 * Copyright (c) 2003      Harikesavan Krishnan
 * Copyright (c) 2003-2014 Stony Brook University
 * Copyright (c) 2003-2014 The Research Foundation of SUNY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "union.h"

static ssize_t unionfs_read(struct file *file, char __user *buf,
			    size_t count, loff_t *ppos)
{
	int err;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	struct dentry *parent;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_PARENT);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	err = unionfs_file_revalidate(file, parent, false);
	if (unlikely(err))
		goto out;

	lower_file = unionfs_lower_file(file);
	err = vfs_read(lower_file, buf, count, ppos);
	/* update our inode atime upon a successful lower read */
	if (err >= 0) {
		fsstack_copy_attr_atime(dentry->d_inode,
					lower_file->f_path.dentry->d_inode);
		unionfs_check_file(file);
	}

out:
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	return err;
}

static ssize_t unionfs_write(struct file *file, const char __user *buf,
			     size_t count, loff_t *ppos)
{
	int err = 0;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	struct dentry *parent;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_PARENT);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	err = unionfs_file_revalidate(file, parent, true);
	if (unlikely(err))
		goto out;

	lower_file = unionfs_lower_file(file);
	err = vfs_write(lower_file, buf, count, ppos);
	/* update our inode times+sizes upon a successful lower write */
	if (err >= 0) {
		fsstack_copy_inode_size(dentry->d_inode,
					lower_file->f_path.dentry->d_inode);
		fsstack_copy_attr_times(dentry->d_inode,
					lower_file->f_path.dentry->d_inode);
		UNIONFS_F(file)->wrote_to_file = true; /* for delayed copyup */
		unionfs_check_file(file);
	}

out:
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	return err;
}

static ssize_t unionfs_aio_read(struct kiocb *iocb, const struct iovec
				*iov, unsigned long nr_segs, loff_t pos)
{
	int err = -EINVAL;
	struct file *file = iocb->ki_filp, *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	struct dentry *parent;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_PARENT);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	err = unionfs_file_revalidate(file, parent, true);
	if (unlikely(err))
		goto out;

	lower_file = unionfs_lower_file(file);
	if (!lower_file->f_op->aio_read)
		goto out;

	get_file(lower_file);
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->aio_read(iocb, iov, nr_segs, pos);
	iocb->ki_filp = file;
	fput(lower_file);

	/* update our inode atime upon a successful lower read */
	/* XXX: need to update upper inode atime when AIO completes */
	if (err >= 0) {
		fsstack_copy_attr_atime(dentry->d_inode,
					lower_file->f_path.dentry->d_inode);
		unionfs_check_file(file);
	}
out:
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	return err;
}

static ssize_t unionfs_aio_write(struct kiocb *iocb, const struct iovec
		*iov, unsigned long nr_segs, loff_t pos)
{
	int err = -EINVAL;
	struct file *file = iocb->ki_filp, *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	struct dentry *parent;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_PARENT);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	err = unionfs_file_revalidate(file, parent, true);
	if (unlikely(err))
		goto out;

	lower_file = unionfs_lower_file(file);
	if (!lower_file->f_op->aio_write)
		goto out;

	get_file(lower_file);
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->aio_write(iocb, iov, nr_segs, pos);
	iocb->ki_filp = file;
	fput(lower_file);

	/* update our inode times+sizes upon a successful lower write */
	/* XXX: need to update upper inode times/sizes when AIO completes */
	if (err >= 0) {
		fsstack_copy_inode_size(dentry->d_inode,
					lower_file->f_path.dentry->d_inode);
		fsstack_copy_attr_times(dentry->d_inode,
					lower_file->f_path.dentry->d_inode);
		UNIONFS_F(file)->wrote_to_file = true; /* for delayed copyup */
		unionfs_check_file(file);
	}
out:
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	return err;
}

static int unionfs_file_readdir(struct file *file, void *dirent,
				filldir_t filldir)
{
	return -ENOTDIR;
}

static int unionfs_mmap(struct file *file, struct vm_area_struct *vma)
{
	int err = 0;
	bool willwrite;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	struct dentry *parent;
	const struct vm_operations_struct *saved_vm_ops = NULL;

	/*
	 * Since mm/memory.c:might_fault() (under PROVE_LOCKING) was
	 * modified in 2.6.29-rc1 to call might_lock_read on mmap_sem, this
	 * has been causing false positives in file system stacking layers.
	 * In particular, our ->mmap is called after sys_mmap2 already holds
	 * mmap_sem, then we lock our own mutexes; but earlier, it's
	 * possible for lockdep to have locked our mutexes first, and then
	 * we call a lower ->readdir which could call might_fault.  The
	 * different ordering of the locks is what lockdep complains about
	 * -- unnecessarily.  Therefore, we have no choice but to tell
	 * lockdep to temporarily turn off lockdep here.  Note: the comments
	 * inside might_sleep also suggest that it would have been
	 * nicer to only annotate paths that needs that might_lock_read.
	 */
	lockdep_off();
	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_PARENT);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	/* This might be deferred to mmap's writepage */
	willwrite = ((vma->vm_flags | VM_SHARED | VM_WRITE) == vma->vm_flags);
	err = unionfs_file_revalidate(file, parent, willwrite);
	if (unlikely(err))
		goto out;
	unionfs_check_file(file);

	/*
	 * File systems which do not implement ->writepage may use
	 * generic_file_readonly_mmap as their ->mmap op.  If you call
	 * generic_file_readonly_mmap with VM_WRITE, you'd get an -EINVAL.
	 * But we cannot call the lower ->mmap op, so we can't tell that
	 * writeable mappings won't work.  Therefore, our only choice is to
	 * check if the lower file system supports the ->writepage, and if
	 * not, return EINVAL (the same error that
	 * generic_file_readonly_mmap returns in that case).
	 */
	lower_file = unionfs_lower_file(file);
	if (willwrite && !lower_file->f_mapping->a_ops->writepage) {
		err = -EINVAL;
		printk(KERN_ERR "unionfs: branch %d file system does not "
		       "support writeable mmap\n", fbstart(file));
		goto out;
	}

	/*
	 * find and save lower vm_ops.
	 *
	 * XXX: the VFS should have a cleaner way of finding the lower vm_ops
	 */
	if (!UNIONFS_F(file)->lower_vm_ops) {
		err = lower_file->f_op->mmap(lower_file, vma);
		if (err) {
			printk(KERN_ERR "unionfs: lower mmap failed %d\n", err);
			goto out;
		}
		saved_vm_ops = vma->vm_ops;
		up_write(&current->mm->mmap_sem); /* VFS already holds sema... */
		err = vm_munmap(vma->vm_start, vma->vm_end - vma->vm_start);
		down_write(&current->mm->mmap_sem);
		if (err) {
			printk(KERN_ERR "unionfs: do_munmap failed %d\n", err);
			goto out;
		}
	}

	file->f_mapping->a_ops = &unionfs_dummy_aops;
	err = generic_file_mmap(file, vma);
	file->f_mapping->a_ops = &unionfs_aops;
	if (err) {
		printk(KERN_ERR "unionfs: generic_file_mmap failed %d\n", err);
		goto out;
	}
	vma->vm_ops = &unionfs_vm_ops;
	if (!UNIONFS_F(file)->lower_vm_ops)
		UNIONFS_F(file)->lower_vm_ops = saved_vm_ops;

out:
	if (!err) {
		/* copyup could cause parent dir times to change */
		unionfs_copy_attr_times(parent->d_inode);
		unionfs_check_file(file);
	}
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	lockdep_on();
	return err;
}

int unionfs_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
	int bindex, bstart, bend;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	struct dentry *lower_dentry;
	struct dentry *parent;
	struct inode *lower_inode, *inode;
	int err = -EINVAL;

	lockdep_off();
	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_PARENT);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	err = unionfs_file_revalidate(file, parent, true);
	if (unlikely(err))
		goto out;
	unionfs_check_file(file);

	err = generic_file_fsync(file, start, end, datasync);
	if (err)
		goto out;

	bstart = fbstart(file);
	bend = fbend(file);
	if (bstart < 0 || bend < 0)
		goto out;

	inode = dentry->d_inode;
	if (unlikely(!inode)) {
		printk(KERN_ERR
		       "unionfs: null lower inode in unionfs_fsync\n");
		goto out;
	}
	for (bindex = bstart; bindex <= bend; bindex++) {
		lower_inode = unionfs_lower_inode_idx(inode, bindex);
		if (!lower_inode || !lower_inode->i_fop->fsync)
			continue;
		lower_file = unionfs_lower_file_idx(file, bindex);
		lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);
		err = vfs_fsync_range(lower_file, start, end, datasync);
		if (!err && bindex == bstart)
			fsstack_copy_attr_times(inode, lower_inode);
		if (err)
			goto out;
	}

out:
	if (!err)
		unionfs_check_file(file);
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	lockdep_on();
	return err;
}

int unionfs_fasync(int fd, struct file *file, int flag)
{
	int bindex, bstart, bend;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	struct dentry *parent;
	struct inode *lower_inode, *inode;
	int err = 0;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_PARENT);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	err = unionfs_file_revalidate(file, parent, true);
	if (unlikely(err))
		goto out;
	unionfs_check_file(file);

	bstart = fbstart(file);
	bend = fbend(file);
	if (bstart < 0 || bend < 0)
		goto out;

	inode = dentry->d_inode;
	if (unlikely(!inode)) {
		printk(KERN_ERR
		       "unionfs: null lower inode in unionfs_fasync\n");
		goto out;
	}
	for (bindex = bstart; bindex <= bend; bindex++) {
		lower_inode = unionfs_lower_inode_idx(inode, bindex);
		if (!lower_inode || !lower_inode->i_fop->fasync)
			continue;
		lower_file = unionfs_lower_file_idx(file, bindex);
		mutex_lock(&lower_inode->i_mutex);
		err = lower_inode->i_fop->fasync(fd, lower_file, flag);
		if (!err && bindex == bstart)
			fsstack_copy_attr_times(inode, lower_inode);
		mutex_unlock(&lower_inode->i_mutex);
		if (err)
			goto out;
	}

out:
	if (!err)
		unionfs_check_file(file);
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	return err;
}

static ssize_t unionfs_splice_read(struct file *file, loff_t *ppos,
				   struct pipe_inode_info *pipe, size_t len,
				   unsigned int flags)
{
	ssize_t err;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	struct dentry *parent;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_PARENT);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	err = unionfs_file_revalidate(file, parent, false);
	if (unlikely(err))
		goto out;

	lower_file = unionfs_lower_file(file);
	err = vfs_splice_to(lower_file, ppos, pipe, len, flags);
	/* update our inode atime upon a successful lower splice-read */
	if (err >= 0) {
		fsstack_copy_attr_atime(dentry->d_inode,
					lower_file->f_path.dentry->d_inode);
		unionfs_check_file(file);
	}

out:
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	return err;
}

static ssize_t unionfs_splice_write(struct pipe_inode_info *pipe,
				    struct file *file, loff_t *ppos,
				    size_t len, unsigned int flags)
{
	ssize_t err = 0;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	struct dentry *parent;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_PARENT);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	err = unionfs_file_revalidate(file, parent, true);
	if (unlikely(err))
		goto out;

	lower_file = unionfs_lower_file(file);
	err = vfs_splice_from(pipe, lower_file, ppos, len, flags);
	/* update our inode times+sizes upon a successful lower write */
	if (err >= 0) {
		fsstack_copy_inode_size(dentry->d_inode,
					lower_file->f_path.dentry->d_inode);
		fsstack_copy_attr_times(dentry->d_inode,
					lower_file->f_path.dentry->d_inode);
		unionfs_check_file(file);
	}

out:
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	return err;
}

struct file_operations unionfs_main_fops = {
	.llseek		= generic_file_llseek,
	.read		= unionfs_read,
	.write		= unionfs_write,
	.aio_read	= unionfs_aio_read,
	.aio_write	= unionfs_aio_write,
	.readdir	= unionfs_file_readdir,
	.unlocked_ioctl	= unionfs_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= unionfs_ioctl,
#endif
	.mmap		= unionfs_mmap,
	.open		= unionfs_open,
	.flush		= unionfs_flush,
	.release	= unionfs_file_release,
	.fsync		= unionfs_fsync,
	.fasync		= unionfs_fasync,
	.splice_read	= unionfs_splice_read,
	.splice_write	= unionfs_splice_write,
};
