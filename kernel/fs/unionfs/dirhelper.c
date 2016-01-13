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

#define RD_NONE 0
#define RD_CHECK_EMPTY 1
/* The callback structure for check_empty. */
struct unionfs_rdutil_callback {
	int err;
	int filldir_called;
	struct unionfs_dir_state *rdstate;
	int mode;
};

/* This filldir function makes sure only whiteouts exist within a directory. */
static int readdir_util_callback(void *dirent, const char *oname, int namelen,
				 loff_t offset, u64 ino, unsigned int d_type)
{
	int err = 0;
	struct unionfs_rdutil_callback *buf = dirent;
	int is_whiteout;
	struct filldir_node *found;
	char *name = (char *) oname;

	buf->filldir_called = 1;

	if (name[0] == '.' && (namelen == 1 ||
			       (name[1] == '.' && namelen == 2)))
		goto out;

	is_whiteout = is_whiteout_name(&name, &namelen);

	found = find_filldir_node(buf->rdstate, name, namelen, is_whiteout);
	/* If it was found in the table there was a previous whiteout. */
	if (found)
		goto out;

	/*
	 * if it wasn't found and isn't a whiteout, the directory isn't
	 * empty.
	 */
	err = -ENOTEMPTY;
	if ((buf->mode == RD_CHECK_EMPTY) && !is_whiteout)
		goto out;

	err = add_filldir_node(buf->rdstate, name, namelen,
			       buf->rdstate->bindex, is_whiteout);

out:
	buf->err = err;
	return err;
}

/* Is a directory logically empty? */
int check_empty(struct dentry *dentry, struct dentry *parent,
		struct unionfs_dir_state **namelist)
{
	int err = 0;
	struct dentry *lower_dentry = NULL;
	struct vfsmount *mnt;
	struct super_block *sb;
	struct file *lower_file;
	struct unionfs_rdutil_callback *buf = NULL;
	int bindex, bstart, bend, bopaque;
	struct path path;

	sb = dentry->d_sb;


	BUG_ON(!S_ISDIR(dentry->d_inode->i_mode));

	err = unionfs_partial_lookup(dentry, parent);
	if (err)
		goto out;

	bstart = dbstart(dentry);
	bend = dbend(dentry);
	bopaque = dbopaque(dentry);
	if (0 <= bopaque && bopaque < bend)
		bend = bopaque;

	buf = kmalloc(sizeof(struct unionfs_rdutil_callback), GFP_KERNEL);
	if (unlikely(!buf)) {
		err = -ENOMEM;
		goto out;
	}
	buf->err = 0;
	buf->mode = RD_CHECK_EMPTY;
	buf->rdstate = alloc_rdstate(dentry->d_inode, bstart);
	if (unlikely(!buf->rdstate)) {
		err = -ENOMEM;
		goto out;
	}

	/* Process the lower directories with rdutil_callback as a filldir. */
	for (bindex = bstart; bindex <= bend; bindex++) {
		lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);
		if (!lower_dentry)
			continue;
		if (!lower_dentry->d_inode)
			continue;
		if (!S_ISDIR(lower_dentry->d_inode->i_mode))
			continue;

		dget(lower_dentry);
		mnt = unionfs_mntget(dentry, bindex);
		branchget(sb, bindex);
		path.dentry = lower_dentry;
		path.mnt = mnt;
		lower_file = dentry_open(&path, O_RDONLY, current_cred());
		path_put(&path);
		if (IS_ERR(lower_file)) {
			err = PTR_ERR(lower_file);
			branchput(sb, bindex);
			goto out;
		}

		do {
			buf->filldir_called = 0;
			buf->rdstate->bindex = bindex;
			err = vfs_readdir(lower_file,
					  readdir_util_callback, buf);
			if (buf->err)
				err = buf->err;
		} while ((err >= 0) && buf->filldir_called);

		/* fput calls dput for lower_dentry */
		fput(lower_file);
		branchput(sb, bindex);

		if (err < 0)
			goto out;
	}

out:
	if (buf) {
		if (namelist && !err)
			*namelist = buf->rdstate;
		else if (buf->rdstate)
			free_rdstate(buf->rdstate);
		kfree(buf);
	}


	return err;
}
