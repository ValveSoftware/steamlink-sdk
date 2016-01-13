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

#define __NFDBITS	(8 * sizeof(unsigned long))
#define __FDSET_LONGS	(__FD_SETSIZE/__NFDBITS)
static inline void __FD_SET(unsigned long __fd, __kernel_fd_set *__fdsetp)
{
	unsigned long __tmp = __fd / __NFDBITS;
	unsigned long __rem = __fd % __NFDBITS;
	__fdsetp->fds_bits[__tmp] |= (1UL<<__rem);
}
static inline void __FD_ZERO(__kernel_fd_set *__p)
{
	unsigned long *__tmp = __p->fds_bits;
	int __i;

	if (__builtin_constant_p(__FDSET_LONGS)) {
		switch (__FDSET_LONGS) {
		case 16:
			__tmp[ 0] = 0; __tmp[ 1] = 0;
			__tmp[ 2] = 0; __tmp[ 3] = 0;
			__tmp[ 4] = 0; __tmp[ 5] = 0;
			__tmp[ 6] = 0; __tmp[ 7] = 0;
			__tmp[ 8] = 0; __tmp[ 9] = 0;
			__tmp[10] = 0; __tmp[11] = 0;
			__tmp[12] = 0; __tmp[13] = 0;
			__tmp[14] = 0; __tmp[15] = 0;
			return;

		case 8:
			__tmp[ 0] = 0; __tmp[ 1] = 0;
			__tmp[ 2] = 0; __tmp[ 3] = 0;
			__tmp[ 4] = 0; __tmp[ 5] = 0;
			__tmp[ 6] = 0; __tmp[ 7] = 0;
			return;

		case 4:
			__tmp[ 0] = 0; __tmp[ 1] = 0;
			__tmp[ 2] = 0; __tmp[ 3] = 0;
			return;
		}
	}
	__i = __FDSET_LONGS;
	while (__i) {
		__i--;
		*__tmp = 0;
		__tmp++;
	}
}


/*
 * 1) Copyup the file
 * 2) Rename the file to '.unionfs<original inode#><counter>' - obviously
 * stolen from NFS's silly rename
 */
static int copyup_deleted_file(struct file *file, struct dentry *dentry,
			       struct dentry *parent, int bstart, int bindex)
{
	static unsigned int counter;
	const int i_inosize = sizeof(dentry->d_inode->i_ino) * 2;
	const int countersize = sizeof(counter) * 2;
	const int nlen = sizeof(".unionfs") + i_inosize + countersize - 1;
	char name[nlen + 1];
	int err;
	struct dentry *tmp_dentry = NULL;
	struct dentry *lower_dentry;
	struct dentry *lower_dir_dentry = NULL;

	lower_dentry = unionfs_lower_dentry_idx(dentry, bstart);

	sprintf(name, ".unionfs%*.*lx",
		i_inosize, i_inosize, lower_dentry->d_inode->i_ino);

	/*
	 * Loop, looking for an unused temp name to copyup to.
	 *
	 * It's somewhat silly that we look for a free temp tmp name in the
	 * source branch (bstart) instead of the dest branch (bindex), where
	 * the final name will be created.  We _will_ catch it if somehow
	 * the name exists in the dest branch, but it'd be nice to catch it
	 * sooner than later.
	 */
retry:
	tmp_dentry = NULL;
	do {
		char *suffix = name + nlen - countersize;

		dput(tmp_dentry);
		counter++;
		sprintf(suffix, "%*.*x", countersize, countersize, counter);

		pr_debug("unionfs: trying to rename %s to %s\n",
			 dentry->d_name.name, name);

		tmp_dentry = lookup_lck_len(name, lower_dentry->d_parent,
					    nlen);
		if (IS_ERR(tmp_dentry)) {
			err = PTR_ERR(tmp_dentry);
			goto out;
		}
	} while (tmp_dentry->d_inode != NULL);	/* need negative dentry */
	dput(tmp_dentry);

	err = copyup_named_file(parent->d_inode, file, name, bstart, bindex,
				i_size_read(file->f_path.dentry->d_inode));
	if (err) {
		if (unlikely(err == -EEXIST))
			goto retry;
		goto out;
	}

	/* bring it to the same state as an unlinked file */
	lower_dentry = unionfs_lower_dentry_idx(dentry, dbstart(dentry));
	if (!unionfs_lower_inode_idx(dentry->d_inode, bindex)) {
		atomic_inc(&lower_dentry->d_inode->i_count);
		unionfs_set_lower_inode_idx(dentry->d_inode, bindex,
					    lower_dentry->d_inode);
	}
	lower_dir_dentry = lock_parent(lower_dentry);
	err = vfs_unlink(lower_dir_dentry->d_inode, lower_dentry);
	unlock_dir(lower_dir_dentry);

out:
	if (!err)
		unionfs_check_dentry(dentry);
	return err;
}

/*
 * put all references held by upper struct file and free lower file pointer
 * array
 */
static void cleanup_file(struct file *file)
{
	int bindex, bstart, bend;
	struct file **lower_files;
	struct file *lower_file;
	struct super_block *sb = file->f_path.dentry->d_sb;

	lower_files = UNIONFS_F(file)->lower_files;
	bstart = fbstart(file);
	bend = fbend(file);

	for (bindex = bstart; bindex <= bend; bindex++) {
		int i;	/* holds (possibly) updated branch index */
		int old_bid;

		lower_file = unionfs_lower_file_idx(file, bindex);
		if (!lower_file)
			continue;

		/*
		 * Find new index of matching branch with an open
		 * file, since branches could have been added or
		 * deleted causing the one with open files to shift.
		 */
		old_bid = UNIONFS_F(file)->saved_branch_ids[bindex];
		i = branch_id_to_idx(sb, old_bid);
		if (unlikely(i < 0)) {
			printk(KERN_ERR "unionfs: no superblock for "
			       "file %p\n", file);
			continue;
		}

		/* decrement count of open files */
		branchput(sb, i);
		/*
		 * fput will perform an mntput for us on the correct branch.
		 * Although we're using the file's old branch configuration,
		 * bindex, which is the old index, correctly points to the
		 * right branch in the file's branch list.  In other words,
		 * we're going to mntput the correct branch even if branches
		 * have been added/removed.
		 */
		fput(lower_file);
		UNIONFS_F(file)->lower_files[bindex] = NULL;
		UNIONFS_F(file)->saved_branch_ids[bindex] = -1;
	}

	UNIONFS_F(file)->lower_files = NULL;
	kfree(lower_files);
	kfree(UNIONFS_F(file)->saved_branch_ids);
	/* set to NULL because caller needs to know if to kfree on error */
	UNIONFS_F(file)->saved_branch_ids = NULL;
}

/* open all lower files for a given file */
static int open_all_files(struct file *file)
{
	int bindex, bstart, bend, err = 0;
	struct file *lower_file;
	struct dentry *lower_dentry;
	struct dentry *dentry = file->f_path.dentry;
	struct super_block *sb = dentry->d_sb;
	struct path path;

	bstart = dbstart(dentry);
	bend = dbend(dentry);

	for (bindex = bstart; bindex <= bend; bindex++) {
		lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);
		if (!lower_dentry)
			continue;

		dget(lower_dentry);
		unionfs_mntget(dentry, bindex);
		branchget(sb, bindex);

		path.dentry = lower_dentry;
		path.mnt = unionfs_lower_mnt_idx(dentry, bindex);
		lower_file = dentry_open(&path, file->f_flags, current_cred());
		path_put(&path);
		if (IS_ERR(lower_file)) {
			branchput(sb, bindex);
			err = PTR_ERR(lower_file);
			goto out;
		} else {
			unionfs_set_lower_file_idx(file, bindex, lower_file);
		}
	}
out:
	return err;
}

/* open the highest priority file for a given upper file */
static int open_highest_file(struct file *file, bool willwrite)
{
	int bindex, bstart, bend, err = 0;
	struct file *lower_file;
	struct dentry *lower_dentry;
	struct dentry *dentry = file->f_path.dentry;
	struct dentry *parent = dget_parent(dentry);
	struct inode *parent_inode = parent->d_inode;
	struct super_block *sb = dentry->d_sb;
	struct path path;

	bstart = dbstart(dentry);
	bend = dbend(dentry);

	lower_dentry = unionfs_lower_dentry(dentry);
	if (willwrite && IS_WRITE_FLAG(file->f_flags) && is_robranch(dentry)) {
		for (bindex = bstart - 1; bindex >= 0; bindex--) {
			err = copyup_file(parent_inode, file, bstart, bindex,
					  i_size_read(dentry->d_inode));
			if (!err)
				break;
		}
		atomic_set(&UNIONFS_F(file)->generation,
			   atomic_read(&UNIONFS_I(dentry->d_inode)->
				       generation));
		goto out;
	}

	dget(lower_dentry);
	unionfs_mntget(dentry, bstart);
	path.dentry = lower_dentry;
	path.mnt = unionfs_lower_mnt_idx(dentry, bstart);
	lower_file = dentry_open(&path, file->f_flags, current_cred());
	path_put(&path);
	if (IS_ERR(lower_file)) {
		err = PTR_ERR(lower_file);
		goto out;
	}
	branchget(sb, bstart);
	unionfs_set_lower_file(file, lower_file);
	/* Fix up the position. */
	lower_file->f_pos = file->f_pos;

	memcpy(&lower_file->f_ra, &file->f_ra, sizeof(struct file_ra_state));
out:
	dput(parent);
	return err;
}

/* perform a delayed copyup of a read-write file on a read-only branch */
static int do_delayed_copyup(struct file *file, struct dentry *parent)
{
	int bindex, bstart, bend, err = 0;
	struct dentry *dentry = file->f_path.dentry;
	struct inode *parent_inode = parent->d_inode;

	bstart = fbstart(file);
	bend = fbend(file);

	BUG_ON(!S_ISREG(dentry->d_inode->i_mode));

	unionfs_check_file(file);
	for (bindex = bstart - 1; bindex >= 0; bindex--) {
		if (!d_deleted(dentry))
			err = copyup_file(parent_inode, file, bstart,
					  bindex,
					  i_size_read(dentry->d_inode));
		else
			err = copyup_deleted_file(file, dentry, parent,
						  bstart, bindex);
		/* if succeeded, set lower open-file flags and break */
		if (!err) {
			struct file *lower_file;
			lower_file = unionfs_lower_file_idx(file, bindex);
			lower_file->f_flags = file->f_flags;
			break;
		}
	}
	if (err || (bstart <= fbstart(file)))
		goto out;
	bend = fbend(file);
	for (bindex = bstart; bindex <= bend; bindex++) {
		if (unionfs_lower_file_idx(file, bindex)) {
			branchput(dentry->d_sb, bindex);
			fput(unionfs_lower_file_idx(file, bindex));
			unionfs_set_lower_file_idx(file, bindex, NULL);
		}
	}
	path_put_lowers(dentry, bstart, bend, false);
	iput_lowers(dentry->d_inode, bstart, bend, false);
	/* for reg file, we only open it "once" */
	fbend(file) = fbstart(file);
	dbend(dentry) = dbstart(dentry);
	ibend(dentry->d_inode) = ibstart(dentry->d_inode);

out:
	unionfs_check_file(file);
	return err;
}

/*
 * Helper function for unionfs_file_revalidate/locked.
 * Expects dentry/parent to be locked already, and revalidated.
 */
static int __unionfs_file_revalidate(struct file *file, struct dentry *dentry,
				     struct dentry *parent,
				     struct super_block *sb, int sbgen,
				     int dgen, bool willwrite)
{
	int fgen;
	int bstart, bend, orig_brid;
	int size;
	int err = 0;

	fgen = atomic_read(&UNIONFS_F(file)->generation);

	/*
	 * There are two cases we are interested in.  The first is if the
	 * generation is lower than the super-block.  The second is if
	 * someone has copied up this file from underneath us, we also need
	 * to refresh things.
	 */
	if (d_deleted(dentry) ||
	    (sbgen <= fgen &&
	     dbstart(dentry) == fbstart(file) &&
	     unionfs_lower_file(file)))
		goto out_may_copyup;

	/* save orig branch ID */
	orig_brid = UNIONFS_F(file)->saved_branch_ids[fbstart(file)];

	/* First we throw out the existing files. */
	cleanup_file(file);

	/* Now we reopen the file(s) as in unionfs_open. */
	bstart = fbstart(file) = dbstart(dentry);
	bend = fbend(file) = dbend(dentry);

	size = sizeof(struct file *) * sbmax(sb);
	UNIONFS_F(file)->lower_files = kzalloc(size, GFP_KERNEL);
	if (unlikely(!UNIONFS_F(file)->lower_files)) {
		err = -ENOMEM;
		goto out;
	}
	size = sizeof(int) * sbmax(sb);
	UNIONFS_F(file)->saved_branch_ids = kzalloc(size, GFP_KERNEL);
	if (unlikely(!UNIONFS_F(file)->saved_branch_ids)) {
		err = -ENOMEM;
		goto out;
	}

	if (S_ISDIR(dentry->d_inode->i_mode)) {
		/* We need to open all the files. */
		err = open_all_files(file);
		if (err)
			goto out;
	} else {
		int new_brid;
		/* We only open the highest priority branch. */
		err = open_highest_file(file, willwrite);
		if (err)
			goto out;
		new_brid = UNIONFS_F(file)->saved_branch_ids[fbstart(file)];
		if (unlikely(new_brid != orig_brid && sbgen > fgen)) {
			/*
			 * If we re-opened the file on a different branch
			 * than the original one, and this was due to a new
			 * branch inserted, then update the mnt counts of
			 * the old and new branches accordingly.
			 */
			unionfs_mntget(dentry, bstart);
			unionfs_mntput(sb->s_root,
				       branch_id_to_idx(sb, orig_brid));
		}
		/* regular files have only one open lower file */
		fbend(file) = fbstart(file);
	}
	atomic_set(&UNIONFS_F(file)->generation,
		   atomic_read(&UNIONFS_I(dentry->d_inode)->generation));

out_may_copyup:
	/* Copyup on the first write to a file on a readonly branch. */
	if (willwrite && IS_WRITE_FLAG(file->f_flags) &&
	    !IS_WRITE_FLAG(unionfs_lower_file(file)->f_flags) &&
	    is_robranch(dentry)) {
		pr_debug("unionfs: do delay copyup of \"%s\"\n",
			 dentry->d_name.name);
		err = do_delayed_copyup(file, parent);
		/* regular files have only one open lower file */
		if (!err && !S_ISDIR(dentry->d_inode->i_mode))
			fbend(file) = fbstart(file);
	}

out:
	if (err) {
		kfree(UNIONFS_F(file)->lower_files);
		kfree(UNIONFS_F(file)->saved_branch_ids);
	}
	return err;
}

/*
 * Revalidate the struct file
 * @file: file to revalidate
 * @parent: parent dentry (locked by caller)
 * @willwrite: true if caller may cause changes to the file; false otherwise.
 * Caller must lock/unlock dentry's branch configuration.
 */
int unionfs_file_revalidate(struct file *file, struct dentry *parent,
			    bool willwrite)
{
	struct super_block *sb;
	struct dentry *dentry;
	int sbgen, dgen;
	int err = 0;

	dentry = file->f_path.dentry;
	sb = dentry->d_sb;
	verify_locked(dentry);
	verify_locked(parent);

	/*
	 * First revalidate the dentry inside struct file,
	 * but not unhashed dentries.
	 */
	if (!d_deleted(dentry) &&
	    !__unionfs_d_revalidate(dentry, parent, willwrite, 0)) {
		err = -ESTALE;
		goto out;
	}

	sbgen = atomic_read(&UNIONFS_SB(sb)->generation);
	dgen = atomic_read(&UNIONFS_D(dentry)->generation);

	if (unlikely(sbgen > dgen)) { /* XXX: should never happen */
		pr_debug("unionfs: failed to revalidate dentry (%s)\n",
			 dentry->d_name.name);
		err = -ESTALE;
		goto out;
	}

	err = __unionfs_file_revalidate(file, dentry, parent, sb,
					sbgen, dgen, willwrite);
out:
	return err;
}

/* unionfs_open helper function: open a directory */
static int __open_dir(struct inode *inode, struct file *file,
		      struct dentry *parent)
{
	struct dentry *lower_dentry;
	struct file *lower_file;
	int bindex, bstart, bend;
	struct vfsmount *lower_mnt;
	struct dentry *dentry = file->f_path.dentry;
	struct path path;

	bstart = fbstart(file) = dbstart(dentry);
	bend = fbend(file) = dbend(dentry);

	for (bindex = bstart; bindex <= bend; bindex++) {
		lower_dentry =
			unionfs_lower_dentry_idx(dentry, bindex);
		if (!lower_dentry)
			continue;

		dget(lower_dentry);
		lower_mnt = unionfs_mntget(dentry, bindex);
		if (!lower_mnt)
			lower_mnt = unionfs_mntget(parent, bindex);
		path.dentry = lower_dentry;
		path.mnt = lower_mnt;
		lower_file = dentry_open(&path, file->f_flags, current_cred());
		path_put(&path);
		if (IS_ERR(lower_file))
			return PTR_ERR(lower_file);

		unionfs_set_lower_file_idx(file, bindex, lower_file);
		if (!unionfs_lower_mnt_idx(dentry, bindex))
			unionfs_set_lower_mnt_idx(dentry, bindex, lower_mnt);

		/*
		 * The branchget goes after the open, because otherwise
		 * we would miss the reference on release.
		 */
		branchget(inode->i_sb, bindex);
	}

	return 0;
}

/* unionfs_open helper function: open a file */
static int __open_file(struct inode *inode, struct file *file,
		       struct dentry *parent)
{
	struct dentry *lower_dentry;
	struct file *lower_file;
	int lower_flags;
	int bindex, bstart, bend;
	struct dentry *dentry = file->f_path.dentry;
	struct vfsmount *lower_mnt;
	struct path path;

	lower_dentry = unionfs_lower_dentry(dentry);
	lower_flags = file->f_flags;

	bstart = fbstart(file) = dbstart(dentry);
	bend = fbend(file) = dbend(dentry);

	/*
	 * check for the permission for lower file.  If the error is
	 * COPYUP_ERR, copyup the file.
	 */
	if (lower_dentry->d_inode && is_robranch(dentry)) {
		/*
		 * if the open will change the file, copy it up otherwise
		 * defer it.
		 */
		if (lower_flags & O_TRUNC) {
			int size = 0;
			int err = -EROFS;

			/* copyup the file */
			for (bindex = bstart - 1; bindex >= 0; bindex--) {
				err = copyup_file(parent->d_inode, file,
						  bstart, bindex, size);
				if (!err) {
					/* only one regular file open */
					fbend(file) = fbstart(file);
					break;
				}
			}
			return err;
		} else {
			/*
			 * turn off writeable flags, to force delayed copyup
			 * by caller.
			 */
			lower_flags &= ~(OPEN_WRITE_FLAGS);
		}
	}

	dget(lower_dentry);

	/*
	 * dentry_open used to decrement mnt refcnt if err.
	 * otherwise fput() will do an mntput() for us upon file close.
	 */
	lower_mnt = unionfs_mntget(dentry, bstart);
	path.dentry = lower_dentry;
	path.mnt = lower_mnt;
	lower_file = dentry_open(&path, lower_flags, current_cred());
	path_put(&path);
	if (IS_ERR(lower_file))
		return PTR_ERR(lower_file);

	unionfs_set_lower_file(file, lower_file);
	branchget(inode->i_sb, bstart);

	return 0;
}

int unionfs_open(struct inode *inode, struct file *file)
{
	int err = 0;
	struct file *lower_file = NULL;
	struct dentry *dentry = file->f_path.dentry;
	struct dentry *parent;
	int bindex = 0, bstart = 0, bend = 0;
	int size;
	int valid = 0;

	unionfs_read_lock(inode->i_sb, UNIONFS_SMUTEX_PARENT);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	/* don't open unhashed/deleted files */
	if (d_deleted(dentry)) {
		err = -ENOENT;
		goto out_nofree;
	}

	/* XXX: should I change 'false' below to the 'willwrite' flag? */
	valid = __unionfs_d_revalidate(dentry, parent, false, 0);
	if (unlikely(!valid)) {
		err = -ESTALE;
		goto out_nofree;
	}

	file->private_data =
		kzalloc(sizeof(struct unionfs_file_info), GFP_KERNEL);
	if (unlikely(!UNIONFS_F(file))) {
		err = -ENOMEM;
		goto out_nofree;
	}
	fbstart(file) = -1;
	fbend(file) = -1;
	atomic_set(&UNIONFS_F(file)->generation,
		   atomic_read(&UNIONFS_I(inode)->generation));

	size = sizeof(struct file *) * sbmax(inode->i_sb);
	UNIONFS_F(file)->lower_files = kzalloc(size, GFP_KERNEL);
	if (unlikely(!UNIONFS_F(file)->lower_files)) {
		err = -ENOMEM;
		goto out;
	}
	size = sizeof(int) * sbmax(inode->i_sb);
	UNIONFS_F(file)->saved_branch_ids = kzalloc(size, GFP_KERNEL);
	if (unlikely(!UNIONFS_F(file)->saved_branch_ids)) {
		err = -ENOMEM;
		goto out;
	}

	bstart = fbstart(file) = dbstart(dentry);
	bend = fbend(file) = dbend(dentry);

	/*
	 * open all directories and make the unionfs file struct point to
	 * these lower file structs
	 */
	if (S_ISDIR(inode->i_mode))
		err = __open_dir(inode, file, parent); /* open a dir */
	else
		err = __open_file(inode, file, parent);	/* open a file */

	/* freeing the allocated resources, and fput the opened files */
	if (err) {
		for (bindex = bstart; bindex <= bend; bindex++) {
			lower_file = unionfs_lower_file_idx(file, bindex);
			if (!lower_file)
				continue;

			branchput(dentry->d_sb, bindex);
			/* fput calls dput for lower_dentry */
			fput(lower_file);
		}
	}

out:
	if (err) {
		kfree(UNIONFS_F(file)->lower_files);
		kfree(UNIONFS_F(file)->saved_branch_ids);
		kfree(UNIONFS_F(file));
	}
out_nofree:
	if (!err) {
		unionfs_postcopyup_setmnt(dentry);
		unionfs_copy_attr_times(inode);
		unionfs_check_file(file);
		unionfs_check_inode(inode);
	}
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(inode->i_sb);
	return err;
}

/*
 * release all lower object references & free the file info structure
 *
 * No need to grab sb info's rwsem.
 */
int unionfs_file_release(struct inode *inode, struct file *file)
{
	struct file *lower_file = NULL;
	struct unionfs_file_info *fileinfo;
	struct unionfs_inode_info *inodeinfo;
	struct super_block *sb = inode->i_sb;
	struct dentry *dentry = file->f_path.dentry;
	struct dentry *parent;
	int bindex, bstart, bend;
	int err = 0;

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
	unionfs_read_lock(sb, UNIONFS_SMUTEX_PARENT);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	/*
	 * We try to revalidate, but the VFS ignores return return values
	 * from file->release, so we must always try to succeed here,
	 * including to do the kfree and dput below.  So if revalidation
	 * failed, all we can do is print some message and keep going.
	 */
	err = unionfs_file_revalidate(file, parent,
				      UNIONFS_F(file)->wrote_to_file);
	if (!err)
		unionfs_check_file(file);
	fileinfo = UNIONFS_F(file);
	BUG_ON(file->f_path.dentry->d_inode != inode);
	inodeinfo = UNIONFS_I(inode);

	/* fput all the lower files */
	bstart = fbstart(file);
	bend = fbend(file);

	for (bindex = bstart; bindex <= bend; bindex++) {
		lower_file = unionfs_lower_file_idx(file, bindex);

		if (lower_file) {
			unionfs_set_lower_file_idx(file, bindex, NULL);
			fput(lower_file);
			branchput(sb, bindex);
		}

		/* if there are no more refs to the dentry, dput it */
		if (d_deleted(dentry)) {
			dput(unionfs_lower_dentry_idx(dentry, bindex));
			unionfs_set_lower_dentry_idx(dentry, bindex, NULL);
		}
	}

	kfree(fileinfo->lower_files);
	kfree(fileinfo->saved_branch_ids);

	if (fileinfo->rdstate) {
		fileinfo->rdstate->access = jiffies;
		spin_lock(&inodeinfo->rdlock);
		inodeinfo->rdcount++;
		list_add_tail(&fileinfo->rdstate->cache,
			      &inodeinfo->readdircache);
		mark_inode_dirty(inode);
		spin_unlock(&inodeinfo->rdlock);
		fileinfo->rdstate = NULL;
	}
	kfree(fileinfo);

	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(sb);
	lockdep_on();
	return err;
}

/* pass the ioctl to the lower fs */
static long do_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct file *lower_file;
	int err;

	lower_file = unionfs_lower_file(file);

	err = -ENOTTY;
	if (!lower_file || !lower_file->f_op)
		goto out;
	if (lower_file->f_op->unlocked_ioctl) {
		err = lower_file->f_op->unlocked_ioctl(lower_file, cmd, arg);
#ifdef CONFIG_COMPAT
	} else if (lower_file->f_op->compat_ioctl) {
		err = lower_file->f_op->compat_ioctl(lower_file, cmd, arg);
#endif
	}

out:
	return err;
}

/*
 * return to user-space the branch indices containing the file in question
 *
 * We use fd_set and therefore we are limited to the number of the branches
 * to FD_SETSIZE, which is currently 1024 - plenty for most people
 */
static int unionfs_ioctl_queryfile(struct file *file, struct dentry *parent,
				   unsigned int cmd, unsigned long arg)
{
	int err = 0;
	fd_set branchlist;
	int bstart = 0, bend = 0, bindex = 0;
	int orig_bstart, orig_bend;
	struct dentry *dentry, *lower_dentry;
	struct vfsmount *mnt;

	dentry = file->f_path.dentry;
	orig_bstart = dbstart(dentry);
	orig_bend = dbend(dentry);
	err = unionfs_partial_lookup(dentry, parent);
	if (err)
		goto out;
	bstart = dbstart(dentry);
	bend = dbend(dentry);

	__FD_ZERO(&branchlist);

	for (bindex = bstart; bindex <= bend; bindex++) {
		lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);
		if (!lower_dentry)
			continue;
		if (likely(lower_dentry->d_inode))
			__FD_SET(bindex, &branchlist);
		/* purge any lower objects after partial_lookup */
		if (bindex < orig_bstart || bindex > orig_bend) {
			dput(lower_dentry);
			unionfs_set_lower_dentry_idx(dentry, bindex, NULL);
			iput(unionfs_lower_inode_idx(dentry->d_inode, bindex));
			unionfs_set_lower_inode_idx(dentry->d_inode, bindex,
						    NULL);
			mnt = unionfs_lower_mnt_idx(dentry, bindex);
			if (!mnt)
				continue;
			unionfs_mntput(dentry, bindex);
			unionfs_set_lower_mnt_idx(dentry, bindex, NULL);
		}
	}
	/* restore original dentry's offsets */
	dbstart(dentry) = orig_bstart;
	dbend(dentry) = orig_bend;
	ibstart(dentry->d_inode) = orig_bstart;
	ibend(dentry->d_inode) = orig_bend;

	err = copy_to_user((void __user *)arg, &branchlist, sizeof(fd_set));
	if (unlikely(err))
		err = -EFAULT;

out:
	return err < 0 ? err : bend;
}

long unionfs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long err;
	struct dentry *dentry = file->f_path.dentry;
	struct dentry *parent;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_PARENT);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	err = unionfs_file_revalidate(file, parent, true);
	if (unlikely(err))
		goto out;

	/* check if asked for local commands */
	switch (cmd) {
	case UNIONFS_IOCTL_INCGEN:
		/* Increment the superblock generation count */
		pr_info("unionfs: incgen ioctl deprecated; "
			"use \"-o remount,incgen\"\n");
		err = -ENOSYS;
		break;

	case UNIONFS_IOCTL_QUERYFILE:
		/* Return list of branches containing the given file */
		err = unionfs_ioctl_queryfile(file, parent, cmd, arg);
		break;

	default:
		/* pass the ioctl down */
		err = do_ioctl(file, cmd, arg);
		break;
	}

out:
	unionfs_check_file(file);
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	return err;
}

int unionfs_flush(struct file *file, fl_owner_t id)
{
	int err = 0;
	struct file *lower_file = NULL;
	struct dentry *dentry = file->f_path.dentry;
	struct dentry *parent;
	int bindex, bstart, bend;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_PARENT);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	err = unionfs_file_revalidate(file, parent,
				      UNIONFS_F(file)->wrote_to_file);
	if (unlikely(err))
		goto out;
	unionfs_check_file(file);

	bstart = fbstart(file);
	bend = fbend(file);
	for (bindex = bstart; bindex <= bend; bindex++) {
		lower_file = unionfs_lower_file_idx(file, bindex);

		if (lower_file && lower_file->f_op &&
		    lower_file->f_op->flush) {
			err = lower_file->f_op->flush(lower_file, id);
			if (err)
				goto out;
		}

	}

out:
	if (!err)
		unionfs_check_file(file);
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	return err;
}
