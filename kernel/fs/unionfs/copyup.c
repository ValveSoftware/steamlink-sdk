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
 * For detailed explanation of copyup see:
 * Documentation/filesystems/unionfs/concepts.txt
 */

#ifdef CONFIG_UNION_FS_XATTR
/* copyup all extended attrs for a given dentry */
static int copyup_xattrs(struct dentry *old_lower_dentry,
			 struct dentry *new_lower_dentry)
{
	int err = 0;
	ssize_t list_size = -1;
	char *name_list = NULL;
	char *attr_value = NULL;
	char *name_list_buf = NULL;

	/* query the actual size of the xattr list */
	list_size = vfs_listxattr(old_lower_dentry, NULL, 0);
	if (list_size <= 0) {
		err = list_size;
		goto out;
	}

	/* allocate space for the actual list */
	name_list = unionfs_xattr_alloc(list_size + 1, XATTR_LIST_MAX);
	if (unlikely(!name_list || IS_ERR(name_list))) {
		err = PTR_ERR(name_list);
		goto out;
	}

	name_list_buf = name_list; /* save for kfree at end */

	/* now get the actual xattr list of the source file */
	list_size = vfs_listxattr(old_lower_dentry, name_list, list_size);
	if (list_size <= 0) {
		err = list_size;
		goto out;
	}

	/* allocate space to hold each xattr's value */
	attr_value = unionfs_xattr_alloc(XATTR_SIZE_MAX, XATTR_SIZE_MAX);
	if (unlikely(!attr_value || IS_ERR(attr_value))) {
		err = PTR_ERR(name_list);
		goto out;
	}

	/* in a loop, get and set each xattr from src to dst file */
	while (*name_list) {
		ssize_t size;

		/* Lock here since vfs_getxattr doesn't lock for us */
		mutex_lock(&old_lower_dentry->d_inode->i_mutex);
		size = vfs_getxattr(old_lower_dentry, name_list,
				    attr_value, XATTR_SIZE_MAX);
		mutex_unlock(&old_lower_dentry->d_inode->i_mutex);
		if (size < 0) {
			err = size;
			goto out;
		}
		if (size > XATTR_SIZE_MAX) {
			err = -E2BIG;
			goto out;
		}
		/* Don't lock here since vfs_setxattr does it for us. */
		err = vfs_setxattr(new_lower_dentry, name_list, attr_value,
				   size, 0);
		/*
		 * Selinux depends on "security.*" xattrs, so to maintain
		 * the security of copied-up files, if Selinux is active,
		 * then we must copy these xattrs as well.  So we need to
		 * temporarily get FOWNER privileges.
		 * XXX: move entire copyup code to SIOQ.
		 */
		if (err == -EPERM && !capable(CAP_FOWNER)) {
			const struct cred *old_creds;
			struct cred *new_creds;

			new_creds = prepare_creds();
			if (unlikely(!new_creds)) {
				err = -ENOMEM;
				goto out;
			}
			cap_raise(new_creds->cap_effective, CAP_FOWNER);
			old_creds = override_creds(new_creds);
			err = vfs_setxattr(new_lower_dentry, name_list,
					   attr_value, size, 0);
			revert_creds(old_creds);
		}
		if (err < 0)
			goto out;
		name_list += strlen(name_list) + 1;
	}
out:
	unionfs_xattr_kfree(name_list_buf);
	unionfs_xattr_kfree(attr_value);
	/* Ignore if xattr isn't supported */
	if (err == -ENOTSUPP || err == -EOPNOTSUPP)
		err = 0;
	return err;
}
#endif /* CONFIG_UNION_FS_XATTR */

/*
 * Determine the mode based on the copyup flags, and the existing dentry.
 *
 * Handle file systems which may not support certain options.  For example
 * jffs2 doesn't allow one to chmod a symlink.  So we ignore such harmless
 * errors, rather than propagating them up, which results in copyup errors
 * and errors returned back to users.
 */
static int copyup_permissions(struct super_block *sb,
			      struct dentry *old_lower_dentry,
			      struct dentry *new_lower_dentry)
{
	struct inode *i = old_lower_dentry->d_inode;
	struct iattr newattrs;
	int err;

	newattrs.ia_atime = i->i_atime;
	newattrs.ia_mtime = i->i_mtime;
	newattrs.ia_ctime = i->i_ctime;
	newattrs.ia_gid = i->i_gid;
	newattrs.ia_uid = i->i_uid;
	newattrs.ia_valid = ATTR_CTIME | ATTR_ATIME | ATTR_MTIME |
		ATTR_ATIME_SET | ATTR_MTIME_SET | ATTR_FORCE |
		ATTR_GID | ATTR_UID;
	mutex_lock(&new_lower_dentry->d_inode->i_mutex);
	err = notify_change(new_lower_dentry, &newattrs);
	if (err)
		goto out;

	/* now try to change the mode and ignore EOPNOTSUPP on symlinks */
	newattrs.ia_mode = i->i_mode;
	newattrs.ia_valid = ATTR_MODE | ATTR_FORCE;
	err = notify_change(new_lower_dentry, &newattrs);
	if (err == -EOPNOTSUPP &&
	    S_ISLNK(new_lower_dentry->d_inode->i_mode)) {
		printk(KERN_WARNING
		       "unionfs: changing \"%s\" symlink mode unsupported\n",
		       new_lower_dentry->d_name.name);
		err = 0;
	}

out:
	mutex_unlock(&new_lower_dentry->d_inode->i_mutex);
	return err;
}

/*
 * create the new device/file/directory - use copyup_permission to copyup
 * times, and mode
 *
 * if the object being copied up is a regular file, the file is only created,
 * the contents have to be copied up separately
 */
static int __copyup_ndentry(struct dentry *old_lower_dentry,
			    struct dentry *new_lower_dentry,
			    struct dentry *new_lower_parent_dentry,
			    char *symbuf)
{
	int err = 0;
	umode_t old_mode = old_lower_dentry->d_inode->i_mode;
	struct sioq_args args;

	if (S_ISDIR(old_mode)) {
		args.mkdir.parent = new_lower_parent_dentry->d_inode;
		args.mkdir.dentry = new_lower_dentry;
		args.mkdir.mode = old_mode;

		run_sioq(__unionfs_mkdir, &args);
		err = args.err;
	} else if (S_ISLNK(old_mode)) {
		args.symlink.parent = new_lower_parent_dentry->d_inode;
		args.symlink.dentry = new_lower_dentry;
		args.symlink.symbuf = symbuf;

		run_sioq(__unionfs_symlink, &args);
		err = args.err;
	} else if (S_ISBLK(old_mode) || S_ISCHR(old_mode) ||
		   S_ISFIFO(old_mode) || S_ISSOCK(old_mode)) {
		args.mknod.parent = new_lower_parent_dentry->d_inode;
		args.mknod.dentry = new_lower_dentry;
		args.mknod.mode = old_mode;
		args.mknod.dev = old_lower_dentry->d_inode->i_rdev;

		run_sioq(__unionfs_mknod, &args);
		err = args.err;
	} else if (S_ISREG(old_mode)) {
		args.create.parent = new_lower_parent_dentry->d_inode;
		args.create.dentry = new_lower_dentry;
		args.create.mode = old_mode;
		args.create.want_excl = false; /* XXX: pass to this fxn */

		run_sioq(__unionfs_create, &args);
		err = args.err;
	} else {
		printk(KERN_CRIT "unionfs: unknown inode type %d\n",
		       old_mode);
		BUG();
	}

	return err;
}

static int __copyup_reg_data(struct dentry *dentry,
			     struct dentry *new_lower_dentry, int new_bindex,
			     struct dentry *old_lower_dentry, int old_bindex,
			     struct file **copyup_file, loff_t len)
{
	struct super_block *sb = dentry->d_sb;
	struct file *input_file;
	struct file *output_file;
	struct vfsmount *output_mnt;
	mm_segment_t old_fs;
	char *buf = NULL;
	ssize_t read_bytes, write_bytes;
	loff_t size;
	int err = 0;
	struct path input_path, output_path;

	/* open old file */
	unionfs_mntget(dentry, old_bindex);
	branchget(sb, old_bindex);
	/* dentry_open used to call dput and mntput if it returns an error */
	input_path.dentry = old_lower_dentry;
	input_path.mnt = unionfs_lower_mnt_idx(dentry, old_bindex);
	input_file = dentry_open(&input_path,
				 O_RDONLY | O_LARGEFILE, current_cred());
	path_put(&input_path);
	if (IS_ERR(input_file)) {
		dput(old_lower_dentry);
		err = PTR_ERR(input_file);
		goto out;
	}
	if (unlikely(!input_file->f_op || !input_file->f_op->read)) {
		err = -EINVAL;
		goto out_close_in;
	}

	/* open new file */
	dget(new_lower_dentry);
	output_mnt = unionfs_mntget(sb->s_root, new_bindex);
	branchget(sb, new_bindex);
	output_path.dentry = new_lower_dentry;
	output_path.mnt = output_mnt;
	output_file = dentry_open(&output_path,
				  O_RDWR | O_LARGEFILE, current_cred());
	path_put(&output_path);
	if (IS_ERR(output_file)) {
		err = PTR_ERR(output_file);
		goto out_close_in2;
	}
	if (unlikely(!output_file->f_op || !output_file->f_op->write)) {
		err = -EINVAL;
		goto out_close_out;
	}

	/* allocating a buffer */
	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (unlikely(!buf)) {
		err = -ENOMEM;
		goto out_close_out;
	}

	input_file->f_pos = 0;
	output_file->f_pos = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	size = len;
	err = 0;
	do {
		if (len >= PAGE_SIZE)
			size = PAGE_SIZE;
		else if ((len < PAGE_SIZE) && (len > 0))
			size = len;

		len -= PAGE_SIZE;

		read_bytes =
			input_file->f_op->read(input_file,
					       (char __user *)buf, size,
					       &input_file->f_pos);
		if (read_bytes <= 0) {
			err = read_bytes;
			break;
		}

		/* see Documentation/filesystems/unionfs/issues.txt */
		lockdep_off();
		write_bytes =
			output_file->f_op->write(output_file,
						 (char __user *)buf,
						 read_bytes,
						 &output_file->f_pos);
		lockdep_on();
		if ((write_bytes < 0) || (write_bytes < read_bytes)) {
			err = write_bytes;
			break;
		}
	} while ((read_bytes > 0) && (len > 0));

	set_fs(old_fs);

	kfree(buf);

#if 0
	/* XXX: code no longer needed? */
	if (!err)
		err = output_file->f_op->fsync(output_file, 0);
#endif

	if (err)
		goto out_close_out;

	if (copyup_file) {
		*copyup_file = output_file;
		goto out_close_in;
	}

out_close_out:
	fput(output_file);

out_close_in2:
	branchput(sb, new_bindex);

out_close_in:
	fput(input_file);

out:
	branchput(sb, old_bindex);

	return err;
}

/*
 * dput the lower references for old and new dentry & clear a lower dentry
 * pointer
 */
static void __clear(struct dentry *dentry, struct dentry *old_lower_dentry,
		    int old_bstart, int old_bend,
		    struct dentry *new_lower_dentry, int new_bindex)
{
	/* get rid of the lower dentry and all its traces */
	unionfs_set_lower_dentry_idx(dentry, new_bindex, NULL);
	dbstart(dentry) = old_bstart;
	dbend(dentry) = old_bend;

	dput(new_lower_dentry);
	dput(old_lower_dentry);
}

/*
 * Copy up a dentry to a file of specified name.
 *
 * @dir: used to pull the ->i_sb to access other branches
 * @dentry: the non-negative dentry whose lower_inode we should copy
 * @bstart: the branch of the lower_inode to copy from
 * @new_bindex: the branch to create the new file in
 * @name: the name of the file to create
 * @namelen: length of @name
 * @copyup_file: the "struct file" to return (optional)
 * @len: how many bytes to copy-up?
 */
int copyup_dentry(struct inode *dir, struct dentry *dentry, int bstart,
		  int new_bindex, const char *name, int namelen,
		  struct file **copyup_file, loff_t len)
{
	struct dentry *new_lower_dentry;
	struct dentry *old_lower_dentry = NULL;
	struct super_block *sb;
	int err = 0;
	int old_bindex;
	int old_bstart;
	int old_bend;
	struct dentry *new_lower_parent_dentry = NULL;
	mm_segment_t oldfs;
	char *symbuf = NULL;

	verify_locked(dentry);

	old_bindex = bstart;
	old_bstart = dbstart(dentry);
	old_bend = dbend(dentry);

	BUG_ON(new_bindex < 0);
	BUG_ON(new_bindex >= old_bindex);

	sb = dir->i_sb;

	err = is_robranch_super(sb, new_bindex);
	if (err)
		goto out;

	/* Create the directory structure above this dentry. */
	new_lower_dentry = create_parents(dir, dentry, name, new_bindex);
	if (IS_ERR(new_lower_dentry)) {
		err = PTR_ERR(new_lower_dentry);
		goto out;
	}

	old_lower_dentry = unionfs_lower_dentry_idx(dentry, old_bindex);
	/* we conditionally dput this old_lower_dentry at end of function */
	dget(old_lower_dentry);

	/* For symlinks, we must read the link before we lock the directory. */
	if (S_ISLNK(old_lower_dentry->d_inode->i_mode)) {

		symbuf = kmalloc(PATH_MAX, GFP_KERNEL);
		if (unlikely(!symbuf)) {
			__clear(dentry, old_lower_dentry,
				old_bstart, old_bend,
				new_lower_dentry, new_bindex);
			err = -ENOMEM;
			goto out_free;
		}

		oldfs = get_fs();
		set_fs(KERNEL_DS);
		err = old_lower_dentry->d_inode->i_op->readlink(
			old_lower_dentry,
			(char __user *)symbuf,
			PATH_MAX);
		set_fs(oldfs);
		if (err < 0) {
			__clear(dentry, old_lower_dentry,
				old_bstart, old_bend,
				new_lower_dentry, new_bindex);
			goto out_free;
		}
		symbuf[err] = '\0';
	}

	/* Now we lock the parent, and create the object in the new branch. */
	new_lower_parent_dentry = lock_parent(new_lower_dentry);

	/* create the new inode */
	err = __copyup_ndentry(old_lower_dentry, new_lower_dentry,
			       new_lower_parent_dentry, symbuf);

	if (err) {
		__clear(dentry, old_lower_dentry,
			old_bstart, old_bend,
			new_lower_dentry, new_bindex);
		goto out_unlock;
	}

	/* We actually copyup the file here. */
	if (S_ISREG(old_lower_dentry->d_inode->i_mode))
		err = __copyup_reg_data(dentry, new_lower_dentry, new_bindex,
					old_lower_dentry, old_bindex,
					copyup_file, len);
	if (err)
		goto out_unlink;

	/* Set permissions. */
	err = copyup_permissions(sb, old_lower_dentry, new_lower_dentry);
	if (err)
		goto out_unlink;

#ifdef CONFIG_UNION_FS_XATTR
	/* Selinux uses extended attributes for permissions. */
	err = copyup_xattrs(old_lower_dentry, new_lower_dentry);
	if (err)
		goto out_unlink;
#endif /* CONFIG_UNION_FS_XATTR */

	/* do not allow files getting deleted to be re-interposed */
	if (!d_deleted(dentry))
		unionfs_reinterpose(dentry);

	goto out_unlock;

out_unlink:
	/*
	 * copyup failed, because we possibly ran out of space or
	 * quota, or something else happened so let's unlink; we don't
	 * really care about the return value of vfs_unlink
	 */
	vfs_unlink(new_lower_parent_dentry->d_inode, new_lower_dentry);

	if (copyup_file) {
		/* need to close the file */

		fput(*copyup_file);
		branchput(sb, new_bindex);
	}

	/*
	 * TODO: should we reset the error to something like -EIO?
	 *
	 * If we don't reset, the user may get some nonsensical errors, but
	 * on the other hand, if we reset to EIO, we guarantee that the user
	 * will get a "confusing" error message.
	 */

out_unlock:
	unlock_dir(new_lower_parent_dentry);

out_free:
	/*
	 * If old_lower_dentry was not a file, then we need to dput it.  If
	 * it was a file, then it was already dput indirectly by other
	 * functions we call above which operate on regular files.
	 */
	if (old_lower_dentry && old_lower_dentry->d_inode &&
	    !S_ISREG(old_lower_dentry->d_inode->i_mode))
		dput(old_lower_dentry);
	kfree(symbuf);

	if (err) {
		/*
		 * if directory creation succeeded, but inode copyup failed,
		 * then purge new dentries.
		 */
		if (dbstart(dentry) < old_bstart &&
		    ibstart(dentry->d_inode) > dbstart(dentry))
			__clear(dentry, NULL, old_bstart, old_bend,
				unionfs_lower_dentry(dentry), dbstart(dentry));
		goto out;
	}
	if (!S_ISDIR(dentry->d_inode->i_mode)) {
		unionfs_postcopyup_release(dentry);
		if (!unionfs_lower_inode(dentry->d_inode)) {
			/*
			 * If we got here, then we copied up to an
			 * unlinked-open file, whose name is .unionfsXXXXX.
			 */
			struct inode *inode = new_lower_dentry->d_inode;
			atomic_inc(&inode->i_count);
			unionfs_set_lower_inode_idx(dentry->d_inode,
						    ibstart(dentry->d_inode),
						    inode);
		}
	}
	unionfs_postcopyup_setmnt(dentry);
	/* sync inode times from copied-up inode to our inode */
	unionfs_copy_attr_times(dentry->d_inode);
	unionfs_check_inode(dir);
	unionfs_check_dentry(dentry);
out:
	return err;
}

/*
 * This function creates a copy of a file represented by 'file' which
 * currently resides in branch 'bstart' to branch 'new_bindex.'  The copy
 * will be named "name".
 */
int copyup_named_file(struct inode *dir, struct file *file, char *name,
		      int bstart, int new_bindex, loff_t len)
{
	int err = 0;
	struct file *output_file = NULL;

	err = copyup_dentry(dir, file->f_path.dentry, bstart, new_bindex,
			    name, strlen(name), &output_file, len);
	if (!err) {
		fbstart(file) = new_bindex;
		unionfs_set_lower_file_idx(file, new_bindex, output_file);
	}

	return err;
}

/*
 * This function creates a copy of a file represented by 'file' which
 * currently resides in branch 'bstart' to branch 'new_bindex'.
 */
int copyup_file(struct inode *dir, struct file *file, int bstart,
		int new_bindex, loff_t len)
{
	int err = 0;
	struct file *output_file = NULL;
	struct dentry *dentry = file->f_path.dentry;

	err = copyup_dentry(dir, dentry, bstart, new_bindex,
			    dentry->d_name.name, dentry->d_name.len,
			    &output_file, len);
	if (!err) {
		fbstart(file) = new_bindex;
		unionfs_set_lower_file_idx(file, new_bindex, output_file);
	}

	return err;
}

/* purge a dentry's lower-branch states (dput/mntput, etc.) */
static void __cleanup_dentry(struct dentry *dentry, int bindex,
			     int old_bstart, int old_bend)
{
	int loop_start;
	int loop_end;
	int new_bstart = -1;
	int new_bend = -1;
	int i;

	loop_start = min(old_bstart, bindex);
	loop_end = max(old_bend, bindex);

	/*
	 * This loop sets the bstart and bend for the new dentry by
	 * traversing from left to right.  It also dputs all negative
	 * dentries except bindex
	 */
	for (i = loop_start; i <= loop_end; i++) {
		if (!unionfs_lower_dentry_idx(dentry, i))
			continue;

		if (i == bindex) {
			new_bend = i;
			if (new_bstart < 0)
				new_bstart = i;
			continue;
		}

		if (!unionfs_lower_dentry_idx(dentry, i)->d_inode) {
			dput(unionfs_lower_dentry_idx(dentry, i));
			unionfs_set_lower_dentry_idx(dentry, i, NULL);

			unionfs_mntput(dentry, i);
			unionfs_set_lower_mnt_idx(dentry, i, NULL);
		} else {
			if (new_bstart < 0)
				new_bstart = i;
			new_bend = i;
		}
	}

	if (new_bstart < 0)
		new_bstart = bindex;
	if (new_bend < 0)
		new_bend = bindex;
	dbstart(dentry) = new_bstart;
	dbend(dentry) = new_bend;

}

/* set lower inode ptr and update bstart & bend if necessary */
static void __set_inode(struct dentry *upper, struct dentry *lower,
			int bindex)
{
	unionfs_set_lower_inode_idx(upper->d_inode, bindex,
				    igrab(lower->d_inode));
	if (likely(ibstart(upper->d_inode) > bindex))
		ibstart(upper->d_inode) = bindex;
	if (likely(ibend(upper->d_inode) < bindex))
		ibend(upper->d_inode) = bindex;

}

/* set lower dentry ptr and update bstart & bend if necessary */
static void __set_dentry(struct dentry *upper, struct dentry *lower,
			 int bindex)
{
	unionfs_set_lower_dentry_idx(upper, bindex, lower);
	if (likely(dbstart(upper) > bindex))
		dbstart(upper) = bindex;
	if (likely(dbend(upper) < bindex))
		dbend(upper) = bindex;
}

/*
 * This function replicates the directory structure up-to given dentry
 * in the bindex branch.
 */
struct dentry *create_parents(struct inode *dir, struct dentry *dentry,
			      const char *name, int bindex)
{
	int err;
	struct dentry *child_dentry;
	struct dentry *parent_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct dentry *lower_dentry = NULL;
	const char *childname;
	unsigned int childnamelen;
	int nr_dentry;
	int count = 0;
	int old_bstart;
	int old_bend;
	struct dentry **path = NULL;
	struct super_block *sb;

	verify_locked(dentry);

	err = is_robranch_super(dir->i_sb, bindex);
	if (err) {
		lower_dentry = ERR_PTR(err);
		goto out;
	}

	old_bstart = dbstart(dentry);
	old_bend = dbend(dentry);

	lower_dentry = ERR_PTR(-ENOMEM);

	/* There is no sense allocating any less than the minimum. */
	nr_dentry = 1;
	path = kmalloc(nr_dentry * sizeof(struct dentry *), GFP_KERNEL);
	if (unlikely(!path))
		goto out;

	/* assume the negative dentry of unionfs as the parent dentry */
	parent_dentry = dentry;

	/*
	 * This loop finds the first parent that exists in the given branch.
	 * We start building the directory structure from there.  At the end
	 * of the loop, the following should hold:
	 *  - child_dentry is the first nonexistent child
	 *  - parent_dentry is the first existent parent
	 *  - path[0] is the = deepest child
	 *  - path[count] is the first child to create
	 */
	do {
		child_dentry = parent_dentry;

		/* find the parent directory dentry in unionfs */
		parent_dentry = dget_parent(child_dentry);

		/* find out the lower_parent_dentry in the given branch */
		lower_parent_dentry =
			unionfs_lower_dentry_idx(parent_dentry, bindex);

		/* grow path table */
		if (count == nr_dentry) {
			void *p;

			nr_dentry *= 2;
			p = krealloc(path, nr_dentry * sizeof(struct dentry *),
				     GFP_KERNEL);
			if (unlikely(!p)) {
				lower_dentry = ERR_PTR(-ENOMEM);
				goto out;
			}
			path = p;
		}

		/* store the child dentry */
		path[count++] = child_dentry;
	} while (!lower_parent_dentry);
	count--;

	sb = dentry->d_sb;

	/*
	 * This code goes between the begin/end labels and basically
	 * emulates a while(child_dentry != dentry), only cleaner and
	 * shorter than what would be a much longer while loop.
	 */
begin:
	/* get lower parent dir in the current branch */
	lower_parent_dentry = unionfs_lower_dentry_idx(parent_dentry, bindex);
	dput(parent_dentry);

	/* init the values to lookup */
	childname = child_dentry->d_name.name;
	childnamelen = child_dentry->d_name.len;

	if (child_dentry != dentry) {
		/* lookup child in the underlying file system */
		lower_dentry = lookup_lck_len(childname, lower_parent_dentry,
					      childnamelen);
		if (IS_ERR(lower_dentry))
			goto out;
	} else {
		/*
		 * Is the name a whiteout of the child name ?  lookup the
		 * whiteout child in the underlying file system
		 */
		lower_dentry = lookup_lck_len(name, lower_parent_dentry,
					      strlen(name));
		if (IS_ERR(lower_dentry))
			goto out;

		/* Replace the current dentry (if any) with the new one */
		dput(unionfs_lower_dentry_idx(dentry, bindex));
		unionfs_set_lower_dentry_idx(dentry, bindex,
					     lower_dentry);

		__cleanup_dentry(dentry, bindex, old_bstart, old_bend);
		goto out;
	}

	if (lower_dentry->d_inode) {
		/*
		 * since this already exists we dput to avoid
		 * multiple references on the same dentry
		 */
		dput(lower_dentry);
	} else {
		struct sioq_args args;

		/* it's a negative dentry, create a new dir */
		lower_parent_dentry = lock_parent(lower_dentry);

		args.mkdir.parent = lower_parent_dentry->d_inode;
		args.mkdir.dentry = lower_dentry;
		args.mkdir.mode = child_dentry->d_inode->i_mode;

		run_sioq(__unionfs_mkdir, &args);
		err = args.err;

		if (!err)
			err = copyup_permissions(dir->i_sb, child_dentry,
						 lower_dentry);
		unlock_dir(lower_parent_dentry);
		if (err) {
			dput(lower_dentry);
			lower_dentry = ERR_PTR(err);
			goto out;
		}

	}

	__set_inode(child_dentry, lower_dentry, bindex);
	__set_dentry(child_dentry, lower_dentry, bindex);
	/*
	 * update times of this dentry, but also the parent, because if
	 * we changed, the parent may have changed too.
	 */
	fsstack_copy_attr_times(parent_dentry->d_inode,
				lower_parent_dentry->d_inode);
	unionfs_copy_attr_times(child_dentry->d_inode);

	parent_dentry = child_dentry;
	child_dentry = path[--count];
	goto begin;
out:
	/* cleanup any leftover locks from the do/while loop above */
	if (IS_ERR(lower_dentry))
		while (count)
			dput(path[count--]);
	kfree(path);
	return lower_dentry;
}

/*
 * Post-copyup helper to ensure we have valid mnts: set lower mnt of
 * dentry+parents to the first parent node that has an mnt.
 */
void unionfs_postcopyup_setmnt(struct dentry *dentry)
{
	struct dentry *parent, *hasone;
	int bindex = dbstart(dentry);

	if (unionfs_lower_mnt_idx(dentry, bindex))
		return;
	hasone = dentry->d_parent;
	/* this loop should stop at root dentry */
	while (!unionfs_lower_mnt_idx(hasone, bindex))
		hasone = hasone->d_parent;
	parent = dentry;
	while (!unionfs_lower_mnt_idx(parent, bindex)) {
		unionfs_set_lower_mnt_idx(parent, bindex,
					  unionfs_mntget(hasone, bindex));
		parent = parent->d_parent;
	}
}

/*
 * Post-copyup helper to release all non-directory source objects of a
 * copied-up file.  Regular files should have only one lower object.
 */
void unionfs_postcopyup_release(struct dentry *dentry)
{
	int bstart, bend;

	BUG_ON(S_ISDIR(dentry->d_inode->i_mode));
	bstart = dbstart(dentry);
	bend = dbend(dentry);

	path_put_lowers(dentry, bstart + 1, bend, false);
	iput_lowers(dentry->d_inode, bstart + 1, bend, false);

	dbend(dentry) = bstart;
	ibend(dentry->d_inode) = ibstart(dentry->d_inode) = bstart;
}
