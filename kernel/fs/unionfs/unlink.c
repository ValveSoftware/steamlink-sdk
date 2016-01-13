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
 * Helper function for Unionfs's unlink operation.
 *
 * The main goal of this function is to optimize the unlinking of non-dir
 * objects in unionfs by deleting all possible lower inode objects from the
 * underlying branches having same dentry name as the non-dir dentry on
 * which this unlink operation is called.  This way we delete as many lower
 * inodes as possible, and save space.  Whiteouts need to be created in
 * branch0 only if unlinking fails on any of the lower branch other than
 * branch0, or if a lower branch is marked read-only.
 *
 * Also, while unlinking a file, if we encounter any dir type entry in any
 * intermediate branch, then we remove the directory by calling vfs_rmdir.
 * The following special cases are also handled:

 * (1) If an error occurs in branch0 during vfs_unlink, then we return
 *     appropriate error.
 *
 * (2) If we get an error during unlink in any of other lower branch other
 *     than branch0, then we create a whiteout in branch0.
 *
 * (3) If a whiteout already exists in any intermediate branch, we delete
 *     all possible inodes only up to that branch (this is an "opaqueness"
 *     as as per Documentation/filesystems/unionfs/concepts.txt).
 *
 */
static int unionfs_unlink_whiteout(struct inode *dir, struct dentry *dentry,
				   struct dentry *parent)
{
	struct dentry *lower_dentry;
	struct dentry *lower_dir_dentry;
	int bindex;
	int err = 0;

	err = unionfs_partial_lookup(dentry, parent);
	if (err)
		goto out;

	/* trying to unlink all possible valid instances */
	for (bindex = dbstart(dentry); bindex <= dbend(dentry); bindex++) {
		lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);
		if (!lower_dentry || !lower_dentry->d_inode)
			continue;

		lower_dir_dentry = lock_parent(lower_dentry);

		/* avoid destroying the lower inode if the object is in use */
		dget(lower_dentry);
		err = is_robranch_super(dentry->d_sb, bindex);
		if (!err) {
			/* see Documentation/filesystems/unionfs/issues.txt */
			lockdep_off();
			if (!S_ISDIR(lower_dentry->d_inode->i_mode))
				err = vfs_unlink(lower_dir_dentry->d_inode,
								lower_dentry);
			else
				err = vfs_rmdir(lower_dir_dentry->d_inode,
								lower_dentry);
			lockdep_on();
		}

		/* if lower object deletion succeeds, update inode's times */
		if (!err)
			unionfs_copy_attr_times(dentry->d_inode);
		dput(lower_dentry);
		fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
		unlock_dir(lower_dir_dentry);

		if (err)
			break;
	}

	/*
	 * Create the whiteout in branch 0 (highest priority) only if (a)
	 * there was an error in any intermediate branch other than branch 0
	 * due to failure of vfs_unlink/vfs_rmdir or (b) a branch marked or
	 * mounted read-only.
	 */
	if (err) {
		if ((bindex == 0) ||
		    ((bindex == dbstart(dentry)) &&
		     (!IS_COPYUP_ERR(err))))
			goto out;
		else {
			if (!IS_COPYUP_ERR(err))
				pr_debug("unionfs: lower object deletion "
					     "failed in branch:%d\n", bindex);
			err = create_whiteout(dentry, sbstart(dentry->d_sb));
		}
	}

out:
	if (!err)
		inode_dec_link_count(dentry->d_inode);

	/* We don't want to leave negative leftover dentries for revalidate. */
	if (!err && (dbopaque(dentry) != -1))
		update_bstart(dentry);

	return err;
}

int unionfs_unlink(struct inode *dir, struct dentry *dentry)
{
	int err = 0;
	struct inode *inode = dentry->d_inode;
	struct dentry *parent;
	int valid;

	BUG_ON(S_ISDIR(inode->i_mode));
	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	valid = __unionfs_d_revalidate(dentry, parent, false, 0);
	if (unlikely(!valid)) {
		err = -ESTALE;
		goto out;
	}
	unionfs_check_dentry(dentry);

	err = unionfs_unlink_whiteout(dir, dentry, parent);
	/* call d_drop so the system "forgets" about us */
	if (!err) {
		unionfs_postcopyup_release(dentry);
		unionfs_postcopyup_setmnt(parent);
		if (inode->i_nlink == 0) /* drop lower inodes */
			iput_lowers_all(inode, false);
		d_drop(dentry);
		/*
		 * if unlink/whiteout succeeded, parent dir mtime has
		 * changed
		 */
		unionfs_copy_attr_times(dir);
	}

out:
	if (!err) {
		unionfs_check_dentry(dentry);
		unionfs_check_inode(dir);
	}
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	return err;
}

static int unionfs_rmdir_first(struct inode *dir, struct dentry *dentry,
			       struct unionfs_dir_state *namelist)
{
	int err;
	struct dentry *lower_dentry;
	struct dentry *lower_dir_dentry = NULL;

	/* Here we need to remove whiteout entries. */
	err = delete_whiteouts(dentry, dbstart(dentry), namelist);
	if (err)
		goto out;

	lower_dentry = unionfs_lower_dentry(dentry);

	lower_dir_dentry = lock_parent(lower_dentry);

	/* avoid destroying the lower inode if the file is in use */
	dget(lower_dentry);
	err = is_robranch(dentry);
	if (!err)
		err = vfs_rmdir(lower_dir_dentry->d_inode, lower_dentry);
	dput(lower_dentry);

	fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
	/* propagate number of hard-links */
	set_nlink(dentry->d_inode, unionfs_get_nlinks(dentry->d_inode));

out:
	if (lower_dir_dentry)
		unlock_dir(lower_dir_dentry);
	return err;
}

int unionfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	int err = 0;
	struct unionfs_dir_state *namelist = NULL;
	struct dentry *parent;
	int dstart, dend;
	bool valid;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	valid = __unionfs_d_revalidate(dentry, parent, false, 0);
	if (unlikely(!valid)) {
		err = -ESTALE;
		goto out;
	}
	unionfs_check_dentry(dentry);

	/* check if this unionfs directory is empty or not */
	err = check_empty(dentry, parent, &namelist);
	if (err)
		goto out;

	err = unionfs_rmdir_first(dir, dentry, namelist);
	dstart = dbstart(dentry);
	dend = dbend(dentry);
	/*
	 * We create a whiteout for the directory if there was an error to
	 * rmdir the first directory entry in the union.  Otherwise, we
	 * create a whiteout only if there is no chance that a lower
	 * priority branch might also have the same named directory.  IOW,
	 * if there is not another same-named directory at a lower priority
	 * branch, then we don't need to create a whiteout for it.
	 */
	if (!err) {
		if (dstart < dend)
			err = create_whiteout(dentry, dstart);
	} else {
		int new_err;

		if (dstart == 0)
			goto out;

		/* exit if the error returned was NOT -EROFS */
		if (!IS_COPYUP_ERR(err))
			goto out;

		new_err = create_whiteout(dentry, dstart - 1);
		if (new_err != -EEXIST)
			err = new_err;
	}

out:
	/*
	 * Drop references to lower dentry/inode so storage space for them
	 * can be reclaimed.  Then, call d_drop so the system "forgets"
	 * about us.
	 */
	if (!err) {
		iput_lowers_all(dentry->d_inode, false);
		dput(unionfs_lower_dentry_idx(dentry, dstart));
		unionfs_set_lower_dentry_idx(dentry, dstart, NULL);
		d_drop(dentry);
		/* update our lower vfsmnts, in case a copyup took place */
		unionfs_postcopyup_setmnt(dentry);
		unionfs_check_dentry(dentry);
		unionfs_check_inode(dir);
	}

	if (namelist)
		free_rdstate(namelist);

	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	return err;
}
