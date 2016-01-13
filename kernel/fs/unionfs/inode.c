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

/*
 * Find a writeable branch to create new object in.  Checks all writeble
 * branches of the parent inode, from istart to iend order; if none are
 * suitable, also tries branch 0 (which may require a copyup).
 *
 * Return a lower_dentry we can use to create object in, or ERR_PTR.
 */
static struct dentry *find_writeable_branch(struct inode *parent,
					    struct dentry *dentry)
{
	int err = -EINVAL;
	int bindex, istart, iend;
	struct dentry *lower_dentry = NULL;

	istart = ibstart(parent);
	iend = ibend(parent);
	if (istart < 0)
		goto out;

begin:
	for (bindex = istart; bindex <= iend; bindex++) {
		/* skip non-writeable branches */
		err = is_robranch_super(dentry->d_sb, bindex);
		if (err) {
			err = -EROFS;
			continue;
		}
		lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);
		if (!lower_dentry)
			continue;
		/*
		 * check for whiteouts in writeable branch, and remove them
		 * if necessary.
		 */
		err = check_unlink_whiteout(dentry, lower_dentry, bindex);
		if (err > 0)	/* ignore if whiteout found and removed */
			err = 0;
		if (err)
			continue;
		/* if get here, we can write to the branch */
		break;
	}
	/*
	 * If istart wasn't already branch 0, and we got any error, then try
	 * branch 0 (which may require copyup)
	 */
	if (err && istart > 0) {
		istart = iend = 0;
		goto begin;
	}

	/*
	 * If we tried even branch 0, and still got an error, abort.  But if
	 * the error was an EROFS, then we should try to copyup.
	 */
	if (err && err != -EROFS)
		goto out;

	/*
	 * If we get here, then check if copyup needed.  If lower_dentry is
	 * NULL, create the entire dentry directory structure in branch 0.
	 */
	if (!lower_dentry) {
		bindex = 0;
		lower_dentry = create_parents(parent, dentry,
					      dentry->d_name.name, bindex);
		if (IS_ERR(lower_dentry)) {
			err = PTR_ERR(lower_dentry);
			goto out;
		}
	}
	err = 0;		/* all's well */
out:
	if (err)
		return ERR_PTR(err);
	return lower_dentry;
}

static int unionfs_create(struct inode *dir, struct dentry *dentry,
			  umode_t mode, bool want_excl)
{
	int err = 0;
	struct dentry *lower_dentry = NULL;
	struct dentry *lower_parent_dentry = NULL;
	struct dentry *parent;
	int valid = 0;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	valid = __unionfs_d_revalidate(dentry, parent, false, 0);
	if (unlikely(!valid)) {
		err = -ESTALE;	/* same as what real_lookup does */
		goto out;
	}

	lower_dentry = find_writeable_branch(dir, dentry);
	if (IS_ERR(lower_dentry)) {
		err = PTR_ERR(lower_dentry);
		goto out;
	}

	lower_parent_dentry = lock_parent(lower_dentry);
	if (IS_ERR(lower_parent_dentry)) {
		err = PTR_ERR(lower_parent_dentry);
		goto out_unlock;
	}

	err = vfs_create(lower_parent_dentry->d_inode, lower_dentry, mode,
			 want_excl);
	if (!err) {
		err = PTR_ERR(unionfs_interpose(dentry, dir->i_sb, 0));
		if (!err) {
			unionfs_copy_attr_times(dir);
			fsstack_copy_inode_size(dir,
						lower_parent_dentry->d_inode);
			/* update no. of links on parent directory */
			set_nlink(dir, unionfs_get_nlinks(dir));
		}
	}

out_unlock:
	unlock_dir(lower_parent_dentry);
out:
	if (!err) {
		unionfs_postcopyup_setmnt(dentry);
		unionfs_check_inode(dir);
		unionfs_check_dentry(dentry);
	}
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	return err;
}

/*
 * unionfs_lookup is the only special function which takes a dentry, yet we
 * do NOT want to call __unionfs_d_revalidate_chain because by definition,
 * we don't have a valid dentry here yet.
 */
static struct dentry *unionfs_lookup(struct inode *dir,
				     struct dentry *dentry,
				     /* XXX: pass flags to lower? */
				     unsigned int flags_unused)
{
	struct dentry *ret, *parent;
	int err = 0;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);

	/*
	 * As long as we lock/dget the parent, then can skip validating the
	 * parent now; we may have to rebuild this dentry on the next
	 * ->d_revalidate, however.
	 */

	/* allocate dentry private data.  We free it in ->d_release */
	err = new_dentry_private_data(dentry, UNIONFS_DMUTEX_CHILD);
	if (unlikely(err)) {
		ret = ERR_PTR(err);
		goto out;
	}

	ret = unionfs_lookup_full(dentry, parent, INTERPOSE_LOOKUP);

	if (!IS_ERR(ret)) {
		if (ret)
			dentry = ret;
		/* lookup_full can return multiple positive dentries */
		if (dentry->d_inode && !S_ISDIR(dentry->d_inode->i_mode)) {
			BUG_ON(dbstart(dentry) < 0);
			unionfs_postcopyup_release(dentry);
		}
		unionfs_copy_attr_times(dentry->d_inode);
	}

	unionfs_check_inode(dir);
	if (!IS_ERR(ret))
		unionfs_check_dentry(dentry);
	unionfs_check_dentry(parent);
	unionfs_unlock_dentry(dentry); /* locked in new_dentry_private data */

out:
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);

	return ret;
}

static int unionfs_link(struct dentry *old_dentry, struct inode *dir,
			struct dentry *new_dentry)
{
	int err = 0;
	struct dentry *lower_old_dentry = NULL;
	struct dentry *lower_new_dentry = NULL;
	struct dentry *lower_dir_dentry = NULL;
	struct dentry *old_parent, *new_parent;
	char *name = NULL;
	bool valid;

	unionfs_read_lock(old_dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	old_parent = dget_parent(old_dentry);
	new_parent = dget_parent(new_dentry);
	unionfs_double_lock_parents(old_parent, new_parent);
	unionfs_double_lock_dentry(old_dentry, new_dentry);

	valid = __unionfs_d_revalidate(old_dentry, old_parent, false, 0);
	if (unlikely(!valid)) {
		err = -ESTALE;
		goto out;
	}
	if (new_dentry->d_inode) {
		valid = __unionfs_d_revalidate(new_dentry, new_parent, false, 0);
		if (unlikely(!valid)) {
			err = -ESTALE;
			goto out;
		}
	}

	lower_new_dentry = unionfs_lower_dentry(new_dentry);

	/* check for a whiteout in new dentry branch, and delete it */
	err = check_unlink_whiteout(new_dentry, lower_new_dentry,
				    dbstart(new_dentry));
	if (err > 0) {	       /* whiteout found and removed successfully */
		lower_dir_dentry = dget_parent(lower_new_dentry);
		fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
		dput(lower_dir_dentry);
		set_nlink(dir, unionfs_get_nlinks(dir));
		err = 0;
	}
	if (err)
		goto out;

	/* check if parent hierachy is needed, then link in same branch */
	if (dbstart(old_dentry) != dbstart(new_dentry)) {
		lower_new_dentry = create_parents(dir, new_dentry,
						  new_dentry->d_name.name,
						  dbstart(old_dentry));
		err = PTR_ERR(lower_new_dentry);
		if (IS_COPYUP_ERR(err))
			goto docopyup;
		if (!lower_new_dentry || IS_ERR(lower_new_dentry))
			goto out;
	}
	lower_new_dentry = unionfs_lower_dentry(new_dentry);
	lower_old_dentry = unionfs_lower_dentry(old_dentry);

	BUG_ON(dbstart(old_dentry) != dbstart(new_dentry));
	lower_dir_dentry = lock_parent(lower_new_dentry);
	err = is_robranch(old_dentry);
	if (!err) {
		/* see Documentation/filesystems/unionfs/issues.txt */
		lockdep_off();
		err = vfs_link(lower_old_dentry, lower_dir_dentry->d_inode,
			       lower_new_dentry);
		lockdep_on();
	}
	unlock_dir(lower_dir_dentry);

docopyup:
	if (IS_COPYUP_ERR(err)) {
		int old_bstart = dbstart(old_dentry);
		int bindex;

		for (bindex = old_bstart - 1; bindex >= 0; bindex--) {
			err = copyup_dentry(old_parent->d_inode,
					    old_dentry, old_bstart,
					    bindex, old_dentry->d_name.name,
					    old_dentry->d_name.len, NULL,
					    i_size_read(old_dentry->d_inode));
			if (err)
				continue;
			lower_new_dentry =
				create_parents(dir, new_dentry,
					       new_dentry->d_name.name,
					       bindex);
			lower_old_dentry = unionfs_lower_dentry(old_dentry);
			lower_dir_dentry = lock_parent(lower_new_dentry);
			/* see Documentation/filesystems/unionfs/issues.txt */
			lockdep_off();
			/* do vfs_link */
			err = vfs_link(lower_old_dentry,
				       lower_dir_dentry->d_inode,
				       lower_new_dentry);
			lockdep_on();
			unlock_dir(lower_dir_dentry);
			goto check_link;
		}
		goto out;
	}

check_link:
	if (err || !lower_new_dentry->d_inode)
		goto out;

	/* Its a hard link, so use the same inode */
	new_dentry->d_inode = igrab(old_dentry->d_inode);
	d_add(new_dentry, new_dentry->d_inode);
	unionfs_copy_attr_all(dir, lower_new_dentry->d_parent->d_inode);
	fsstack_copy_inode_size(dir, lower_new_dentry->d_parent->d_inode);

	/* propagate number of hard-links */
	set_nlink(old_dentry->d_inode,
		  unionfs_get_nlinks(old_dentry->d_inode));
	/* new dentry's ctime may have changed due to hard-link counts */
	unionfs_copy_attr_times(new_dentry->d_inode);

out:
	if (!new_dentry->d_inode)
		d_drop(new_dentry);

	kfree(name);
	if (!err)
		unionfs_postcopyup_setmnt(new_dentry);

	unionfs_check_inode(dir);
	unionfs_check_dentry(new_dentry);
	unionfs_check_dentry(old_dentry);

	unionfs_double_unlock_dentry(old_dentry, new_dentry);
	unionfs_double_unlock_parents(old_parent, new_parent);
	dput(new_parent);
	dput(old_parent);
	unionfs_read_unlock(old_dentry->d_sb);

	return err;
}

static int unionfs_symlink(struct inode *dir, struct dentry *dentry,
			   const char *symname)
{
	int err = 0;
	struct dentry *lower_dentry = NULL;
	struct dentry *wh_dentry = NULL;
	struct dentry *lower_parent_dentry = NULL;
	struct dentry *parent;
	char *name = NULL;
	int valid = 0;
	umode_t mode;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	valid = __unionfs_d_revalidate(dentry, parent, false, 0);
	if (unlikely(!valid)) {
		err = -ESTALE;
		goto out;
	}

	/*
	 * It's only a bug if this dentry was not negative and couldn't be
	 * revalidated (shouldn't happen).
	 */
	BUG_ON(!valid && dentry->d_inode);

	lower_dentry = find_writeable_branch(dir, dentry);
	if (IS_ERR(lower_dentry)) {
		err = PTR_ERR(lower_dentry);
		goto out;
	}

	lower_parent_dentry = lock_parent(lower_dentry);
	if (IS_ERR(lower_parent_dentry)) {
		err = PTR_ERR(lower_parent_dentry);
		goto out_unlock;
	}

	mode = S_IALLUGO;
	err = vfs_symlink(lower_parent_dentry->d_inode, lower_dentry, symname);
	if (!err) {
		err = PTR_ERR(unionfs_interpose(dentry, dir->i_sb, 0));
		if (!err) {
			unionfs_copy_attr_times(dir);
			fsstack_copy_inode_size(dir,
						lower_parent_dentry->d_inode);
			/* update no. of links on parent directory */
			set_nlink(dir, unionfs_get_nlinks(dir));
		}
	}

out_unlock:
	unlock_dir(lower_parent_dentry);
out:
	dput(wh_dentry);
	kfree(name);

	if (!err) {
		unionfs_postcopyup_setmnt(dentry);
		unionfs_check_inode(dir);
		unionfs_check_dentry(dentry);
	}
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	return err;
}

static int unionfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	int err = 0;
	struct dentry *lower_dentry = NULL;
	struct dentry *lower_parent_dentry = NULL;
	struct dentry *parent;
	int bindex = 0, bstart;
	char *name = NULL;
	int valid;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	valid = __unionfs_d_revalidate(dentry, parent, false, 0);
	if (unlikely(!valid)) {
		err = -ESTALE;	/* same as what real_lookup does */
		goto out;
	}

	bstart = dbstart(dentry);

	lower_dentry = unionfs_lower_dentry(dentry);

	/* check for a whiteout in new dentry branch, and delete it */
	err = check_unlink_whiteout(dentry, lower_dentry, bstart);
	if (err > 0)	       /* whiteout found and removed successfully */
		err = 0;
	if (err) {
		/* exit if the error returned was NOT -EROFS */
		if (!IS_COPYUP_ERR(err))
			goto out;
		bstart--;
	}

	/* check if copyup's needed, and mkdir */
	for (bindex = bstart; bindex >= 0; bindex--) {
		int i;
		int bend = dbend(dentry);

		if (is_robranch_super(dentry->d_sb, bindex))
			continue;

		lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);
		if (!lower_dentry) {
			lower_dentry = create_parents(dir, dentry,
						      dentry->d_name.name,
						      bindex);
			if (!lower_dentry || IS_ERR(lower_dentry)) {
				printk(KERN_ERR "unionfs: lower dentry "
				       " NULL for bindex = %d\n", bindex);
				continue;
			}
		}

		lower_parent_dentry = lock_parent(lower_dentry);

		if (IS_ERR(lower_parent_dentry)) {
			err = PTR_ERR(lower_parent_dentry);
			goto out;
		}

		err = vfs_mkdir(lower_parent_dentry->d_inode, lower_dentry,
				mode);

		unlock_dir(lower_parent_dentry);

		/* did the mkdir succeed? */
		if (err)
			break;

		for (i = bindex + 1; i <= bend; i++) {
			/* XXX: use path_put_lowers? */
			if (unionfs_lower_dentry_idx(dentry, i)) {
				dput(unionfs_lower_dentry_idx(dentry, i));
				unionfs_set_lower_dentry_idx(dentry, i, NULL);
			}
		}
		dbend(dentry) = bindex;

		/*
		 * Only INTERPOSE_LOOKUP can return a value other than 0 on
		 * err.
		 */
		err = PTR_ERR(unionfs_interpose(dentry, dir->i_sb, 0));
		if (!err) {
			unionfs_copy_attr_times(dir);
			fsstack_copy_inode_size(dir,
						lower_parent_dentry->d_inode);

			/* update number of links on parent directory */
			set_nlink(dir, unionfs_get_nlinks(dir));
		}

		err = make_dir_opaque(dentry, dbstart(dentry));
		if (err) {
			printk(KERN_ERR "unionfs: mkdir: error creating "
			       ".wh.__dir_opaque: %d\n", err);
			goto out;
		}

		/* we are done! */
		break;
	}

out:
	if (!dentry->d_inode)
		d_drop(dentry);

	kfree(name);

	if (!err) {
		unionfs_copy_attr_times(dentry->d_inode);
		unionfs_postcopyup_setmnt(dentry);
	}
	unionfs_check_inode(dir);
	unionfs_check_dentry(dentry);
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);

	return err;
}

static int unionfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode,
			 dev_t dev)
{
	int err = 0;
	struct dentry *lower_dentry = NULL;
	struct dentry *wh_dentry = NULL;
	struct dentry *lower_parent_dentry = NULL;
	struct dentry *parent;
	char *name = NULL;
	int valid = 0;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	valid = __unionfs_d_revalidate(dentry, parent, false, 0);
	if (unlikely(!valid)) {
		err = -ESTALE;
		goto out;
	}

	/*
	 * It's only a bug if this dentry was not negative and couldn't be
	 * revalidated (shouldn't happen).
	 */
	BUG_ON(!valid && dentry->d_inode);

	lower_dentry = find_writeable_branch(dir, dentry);
	if (IS_ERR(lower_dentry)) {
		err = PTR_ERR(lower_dentry);
		goto out;
	}

	lower_parent_dentry = lock_parent(lower_dentry);
	if (IS_ERR(lower_parent_dentry)) {
		err = PTR_ERR(lower_parent_dentry);
		goto out_unlock;
	}

	err = vfs_mknod(lower_parent_dentry->d_inode, lower_dentry, mode, dev);
	if (!err) {
		err = PTR_ERR(unionfs_interpose(dentry, dir->i_sb, 0));
		if (!err) {
			unionfs_copy_attr_times(dir);
			fsstack_copy_inode_size(dir,
						lower_parent_dentry->d_inode);
			/* update no. of links on parent directory */
			set_nlink(dir, unionfs_get_nlinks(dir));
		}
	}

out_unlock:
	unlock_dir(lower_parent_dentry);
out:
	dput(wh_dentry);
	kfree(name);

	if (!err) {
		unionfs_postcopyup_setmnt(dentry);
		unionfs_check_inode(dir);
		unionfs_check_dentry(dentry);
	}
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	return err;
}

/* requires sb, dentry, and parent to already be locked */
static int __unionfs_readlink(struct dentry *dentry, char __user *buf,
			      int bufsiz)
{
	int err;
	struct dentry *lower_dentry;

	lower_dentry = unionfs_lower_dentry(dentry);

	if (!lower_dentry->d_inode->i_op ||
	    !lower_dentry->d_inode->i_op->readlink) {
		err = -EINVAL;
		goto out;
	}

	err = lower_dentry->d_inode->i_op->readlink(lower_dentry,
						    buf, bufsiz);
	if (err >= 0)
		fsstack_copy_attr_atime(dentry->d_inode,
					lower_dentry->d_inode);

out:
	return err;
}

static int unionfs_readlink(struct dentry *dentry, char __user *buf,
			    int bufsiz)
{
	int err;
	struct dentry *parent;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	if (unlikely(!__unionfs_d_revalidate(dentry, parent, false, 0))) {
		err = -ESTALE;
		goto out;
	}

	err = __unionfs_readlink(dentry, buf, bufsiz);

out:
	unionfs_check_dentry(dentry);
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);

	return err;
}

static void *unionfs_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	char *buf;
	int len = PAGE_SIZE, err;
	mm_segment_t old_fs;
	struct dentry *parent;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	/* This is freed by the put_link method assuming a successful call. */
	buf = kmalloc(len, GFP_KERNEL);
	if (unlikely(!buf)) {
		err = -ENOMEM;
		goto out;
	}

	/* read the symlink, and then we will follow it */
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	err = __unionfs_readlink(dentry, buf, len);
	set_fs(old_fs);
	if (err < 0) {
		kfree(buf);
		buf = NULL;
		goto out;
	}
	buf[err] = 0;
	nd_set_link(nd, buf);
	err = 0;

out:
	if (err >= 0)
		unionfs_check_dentry(dentry);

	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);

	return ERR_PTR(err);
}

/* this @nd *IS* still used */
static void unionfs_put_link(struct dentry *dentry, struct nameidata *nd,
			     void *cookie)
{
	struct dentry *parent;
	char *buf;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	if (unlikely(!__unionfs_d_revalidate(dentry, parent, false, 0)))
		printk(KERN_ERR
		       "unionfs: put_link failed to revalidate dentry\n");

	unionfs_check_dentry(dentry);
	buf = nd_get_link(nd);
	if (!IS_ERR(buf))
		kfree(buf);
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
}

/*
 * This is a variant of fs/namei.c:permission() or inode_permission() which
 * skips over EROFS tests (because we perform copyup on EROFS).
 */
static int __inode_permission(struct inode *inode, int mask)
{
	int retval;

	/* nobody gets write access to an immutable file */
	if ((mask & MAY_WRITE) && IS_IMMUTABLE(inode))
		return -EACCES;

	/* Ordinary permission routines do not understand MAY_APPEND. */
	if (inode->i_op && inode->i_op->permission) {
		retval = inode->i_op->permission(inode, mask);
		if (!retval) {
			/*
			 * Exec permission on a regular file is denied if none
			 * of the execute bits are set.
			 *
			 * This check should be done by the ->permission()
			 * method.
			 */
			if ((mask & MAY_EXEC) && S_ISREG(inode->i_mode) &&
			    !(inode->i_mode & S_IXUGO))
				return -EACCES;
		}
	} else {
		retval = generic_permission(inode, mask);
	}
	if (retval)
		return retval;

	return security_inode_permission(inode,
			mask & (MAY_READ|MAY_WRITE|MAY_EXEC|MAY_APPEND));
}

/*
 * Don't grab the superblock read-lock in unionfs_permission, which prevents
 * a deadlock with the branch-management "add branch" code (which grabbed
 * the write lock).  It is safe to not grab the read lock here, because even
 * with branch management taking place, there is no chance that
 * unionfs_permission, or anything it calls, will use stale branch
 * information.
 */
static int unionfs_permission(struct inode *inode, int mask)
{
	struct inode *lower_inode = NULL;
	int err = 0;
	int bindex, bstart, bend;
	int is_file;
	const int write_mask = (mask & MAY_WRITE) && !(mask & MAY_READ);
	struct inode *inode_grabbed;

	inode_grabbed = igrab(inode);
	is_file = !S_ISDIR(inode->i_mode);

	if (!UNIONFS_I(inode)->lower_inodes) {
		if (is_file)	/* dirs can be unlinked but chdir'ed to */
			err = -ESTALE;	/* force revalidate */
		goto out;
	}
	bstart = ibstart(inode);
	bend = ibend(inode);
	if (unlikely(bstart < 0 || bend < 0)) {
		/*
		 * With branch-management, we can get a stale inode here.
		 * If so, we return ESTALE back to link_path_walk, which
		 * would discard the dcache entry and re-lookup the
		 * dentry+inode.  This should be equivalent to issuing
		 * __unionfs_d_revalidate_chain on nd.dentry here.
		 */
		if (is_file)	/* dirs can be unlinked but chdir'ed to */
			err = -ESTALE;	/* force revalidate */
		goto out;
	}

	for (bindex = bstart; bindex <= bend; bindex++) {
		lower_inode = unionfs_lower_inode_idx(inode, bindex);
		if (!lower_inode)
			continue;

		/*
		 * check the condition for D-F-D underlying files/directories,
		 * we don't have to check for files, if we are checking for
		 * directories.
		 */
		if (!is_file && !S_ISDIR(lower_inode->i_mode))
			continue;

		/*
		 * We check basic permissions, but we ignore any conditions
		 * such as readonly file systems or branches marked as
		 * readonly, because those conditions should lead to a
		 * copyup taking place later on.  However, if user never had
		 * access to the file, then no copyup could ever take place.
		 */
		err = __inode_permission(lower_inode, mask);
		if (err && err != -EACCES && err != EPERM && bindex > 0) {
			umode_t mode = lower_inode->i_mode;
			if ((is_robranch_super(inode->i_sb, bindex) ||
			     __is_rdonly(lower_inode)) &&
			    (S_ISREG(mode) || S_ISDIR(mode) || S_ISLNK(mode)))
				err = 0;
			if (IS_COPYUP_ERR(err))
				err = 0;
		}

		/*
		 * NFS HACK: NFSv2/3 return EACCES on readonly-exported,
		 * locally readonly-mounted file systems, instead of EROFS
		 * like other file systems do.  So we have no choice here
		 * but to intercept this and ignore it for NFS branches
		 * marked readonly.  Specifically, we avoid using NFS's own
		 * "broken" ->permission method, and rely on
		 * generic_permission() to do basic checking for us.
		 */
		if (err && err == -EACCES &&
		    is_robranch_super(inode->i_sb, bindex) &&
		    lower_inode->i_sb->s_magic == NFS_SUPER_MAGIC)
			err = generic_permission(lower_inode, mask);

		/*
		 * The permissions are an intersection of the overall directory
		 * permissions, so we fail if one fails.
		 */
		if (err)
			goto out;

		/* only the leftmost file matters. */
		if (is_file || write_mask) {
			if (is_file && write_mask) {
				err = get_write_access(lower_inode);
				if (!err)
					put_write_access(lower_inode);
			}
			break;
		}
	}
	/* sync times which may have changed (asynchronously) below */
	unionfs_copy_attr_times(inode);

out:
	unionfs_check_inode(inode);
	iput(inode_grabbed);
	return err;
}

static int unionfs_setattr(struct dentry *dentry, struct iattr *ia)
{
	int err = 0;
	struct dentry *lower_dentry;
	struct dentry *parent;
	struct inode *inode;
	struct inode *lower_inode;
	int bstart, bend, bindex;
	loff_t size;
	struct iattr lower_ia;

	/* check if user has permission to change inode */
	err = inode_change_ok(dentry->d_inode, ia);
	if (err)
		goto out_err;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	if (unlikely(!__unionfs_d_revalidate(dentry, parent, false, 0))) {
		err = -ESTALE;
		goto out;
	}

	bstart = dbstart(dentry);
	bend = dbend(dentry);
	inode = dentry->d_inode;

	/*
	 * mode change is for clearing setuid/setgid. Allow lower filesystem
	 * to reinterpret it in its own way.
	 */
	if (ia->ia_valid & (ATTR_KILL_SUID | ATTR_KILL_SGID))
		ia->ia_valid &= ~ATTR_MODE;

	lower_dentry = unionfs_lower_dentry(dentry);
	if (!lower_dentry) { /* should never happen after above revalidate */
		err = -EINVAL;
		goto out;
	}

	/*
	 * Get the lower inode directly from lower dentry, in case ibstart
	 * is -1 (which happens when the file is open but unlinked.
	 */
	lower_inode = lower_dentry->d_inode;

	/* check if user has permission to change lower inode */
	err = inode_change_ok(lower_inode, ia);
	if (err)
		goto out;

	/* copyup if the file is on a read only branch */
	if (is_robranch_super(dentry->d_sb, bstart)
	    || __is_rdonly(lower_inode)) {
		/* check if we have a branch to copy up to */
		if (bstart <= 0) {
			err = -EACCES;
			goto out;
		}

		if (ia->ia_valid & ATTR_SIZE)
			size = ia->ia_size;
		else
			size = i_size_read(inode);
		/* copyup to next available branch */
		for (bindex = bstart - 1; bindex >= 0; bindex--) {
			err = copyup_dentry(parent->d_inode,
					    dentry, bstart, bindex,
					    dentry->d_name.name,
					    dentry->d_name.len,
					    NULL, size);
			if (!err)
				break;
		}
		if (err)
			goto out;
		/* get updated lower_dentry/inode after copyup */
		lower_dentry = unionfs_lower_dentry(dentry);
		lower_inode = unionfs_lower_inode(inode);
		/*
		 * check for whiteouts in writeable branch, and remove them
		 * if necessary.
		 */
		if (lower_dentry) {
			err = check_unlink_whiteout(dentry, lower_dentry,
						    bindex);
			if (err > 0) /* ignore if whiteout found and removed */
				err = 0;
		}
	}

	/*
	 * If shrinking, first truncate upper level to cancel writing dirty
	 * pages beyond the new eof; and also if its' maxbytes is more
	 * limiting (fail with -EFBIG before making any change to the lower
	 * level).  There is no need to vmtruncate the upper level
	 * afterwards in the other cases: we fsstack_copy_inode_size from
	 * the lower level.
	 */
	if (ia->ia_valid & ATTR_SIZE) {
		err = inode_newsize_ok(inode, ia->ia_size);
		if (err)
			goto out;
		truncate_setsize(inode, ia->ia_size);
	}

	/* notify the (possibly copied-up) lower inode */
	/*
	 * Note: we use lower_dentry->d_inode, because lower_inode may be
	 * unlinked (no inode->i_sb and i_ino==0.  This happens if someone
	 * tries to open(), unlink(), then ftruncate() a file.
	 */
	/* prepare our own lower struct iattr (with our own lower file) */
	memcpy(&lower_ia, ia, sizeof(lower_ia));
	if (ia->ia_valid & ATTR_FILE) {
		lower_ia.ia_file = unionfs_lower_file(ia->ia_file);
		BUG_ON(!lower_ia.ia_file); // XXX?
	}

	mutex_lock(&lower_dentry->d_inode->i_mutex);
	err = notify_change(lower_dentry, &lower_ia);
	mutex_unlock(&lower_dentry->d_inode->i_mutex);
	if (err)
		goto out;

	/* get attributes from the first lower inode */
	if (ibstart(inode) >= 0)
		unionfs_copy_attr_all(inode, lower_inode);
	/*
	 * unionfs_copy_attr_all will copy the lower times to our inode if
	 * the lower ones are newer (useful for cache coherency).  However,
	 * ->setattr is the only place in which we may have to copy the
	 * lower inode times absolutely, to support utimes(2).
	 */
	if (ia->ia_valid & ATTR_MTIME_SET)
		inode->i_mtime = lower_inode->i_mtime;
	if (ia->ia_valid & ATTR_CTIME)
		inode->i_ctime = lower_inode->i_ctime;
	if (ia->ia_valid & ATTR_ATIME_SET)
		inode->i_atime = lower_inode->i_atime;
	fsstack_copy_inode_size(inode, lower_inode);

out:
	if (!err)
		unionfs_check_dentry(dentry);
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
out_err:
	return err;
}

struct inode_operations unionfs_symlink_iops = {
	.readlink	= unionfs_readlink,
	.permission	= unionfs_permission,
	.follow_link	= unionfs_follow_link,
	.setattr	= unionfs_setattr,
	.put_link	= unionfs_put_link,
};

struct inode_operations unionfs_dir_iops = {
	.create		= unionfs_create,
	.lookup		= unionfs_lookup,
	.link		= unionfs_link,
	.unlink		= unionfs_unlink,
	.symlink	= unionfs_symlink,
	.mkdir		= unionfs_mkdir,
	.rmdir		= unionfs_rmdir,
	.mknod		= unionfs_mknod,
	.rename		= unionfs_rename,
	.permission	= unionfs_permission,
	.setattr	= unionfs_setattr,
#ifdef CONFIG_UNION_FS_XATTR
	.setxattr	= unionfs_setxattr,
	.getxattr	= unionfs_getxattr,
	.removexattr	= unionfs_removexattr,
	.listxattr	= unionfs_listxattr,
#endif /* CONFIG_UNION_FS_XATTR */
};

struct inode_operations unionfs_main_iops = {
	.permission	= unionfs_permission,
	.setattr	= unionfs_setattr,
#ifdef CONFIG_UNION_FS_XATTR
	.setxattr	= unionfs_setxattr,
	.getxattr	= unionfs_getxattr,
	.removexattr	= unionfs_removexattr,
	.listxattr	= unionfs_listxattr,
#endif /* CONFIG_UNION_FS_XATTR */
};
