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

/* This is lifted from fs/xattr.c */
void *unionfs_xattr_alloc(size_t size, size_t limit)
{
	void *ptr;

	if (size > limit)
		return ERR_PTR(-E2BIG);

	if (!size)		/* size request, no buffer is needed */
		return NULL;

	ptr = kmalloc(size, GFP_KERNEL);
	if (unlikely(!ptr))
		return ERR_PTR(-ENOMEM);
	return ptr;
}

/*
 * BKL held by caller.
 * dentry->d_inode->i_mutex locked
 */
ssize_t unionfs_getxattr(struct dentry *dentry, const char *name, void *value,
			 size_t size)
{
	struct dentry *lower_dentry = NULL;
	struct dentry *parent;
	int err = -EOPNOTSUPP;
	bool valid;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	valid = __unionfs_d_revalidate(dentry, parent, false, 0);
	if (unlikely(!valid)) {
		err = -ESTALE;
		goto out;
	}

	lower_dentry = unionfs_lower_dentry(dentry);

	err = vfs_getxattr(lower_dentry, (char *) name, value, size);

out:
	unionfs_check_dentry(dentry);
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	return err;
}

/*
 * BKL held by caller.
 * dentry->d_inode->i_mutex locked
 */
int unionfs_setxattr(struct dentry *dentry, const char *name,
		     const void *value, size_t size, int flags)
{
	struct dentry *lower_dentry = NULL;
	struct dentry *parent;
	int err = -EOPNOTSUPP;
	bool valid;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	valid = __unionfs_d_revalidate(dentry, parent, false, 0);
	if (unlikely(!valid)) {
		err = -ESTALE;
		goto out;
	}

	lower_dentry = unionfs_lower_dentry(dentry);

	err = vfs_setxattr(lower_dentry, (char *) name, (void *) value,
			   size, flags);

out:
	unionfs_check_dentry(dentry);
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	return err;
}

/*
 * BKL held by caller.
 * dentry->d_inode->i_mutex locked
 */
int unionfs_removexattr(struct dentry *dentry, const char *name)
{
	struct dentry *lower_dentry = NULL;
	struct dentry *parent;
	int err = -EOPNOTSUPP;
	bool valid;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	valid = __unionfs_d_revalidate(dentry, parent, false, 0);
	if (unlikely(!valid)) {
		err = -ESTALE;
		goto out;
	}

	lower_dentry = unionfs_lower_dentry(dentry);

	err = vfs_removexattr(lower_dentry, (char *) name);

out:
	unionfs_check_dentry(dentry);
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	return err;
}

/*
 * BKL held by caller.
 * dentry->d_inode->i_mutex locked
 */
ssize_t unionfs_listxattr(struct dentry *dentry, char *list, size_t size)
{
	struct dentry *lower_dentry = NULL;
	struct dentry *parent;
	int err = -EOPNOTSUPP;
	char *encoded_list = NULL;
	bool valid;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	valid = __unionfs_d_revalidate(dentry, parent, false, 0);
	if (unlikely(!valid)) {
		err = -ESTALE;
		goto out;
	}

	lower_dentry = unionfs_lower_dentry(dentry);

	encoded_list = list;
	err = vfs_listxattr(lower_dentry, encoded_list, size);

out:
	unionfs_check_dentry(dentry);
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);
	return err;
}
