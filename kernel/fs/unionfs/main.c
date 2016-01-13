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
#include <linux/module.h>
#include <linux/moduleparam.h>

static void unionfs_fill_inode(struct dentry *dentry,
			       struct inode *inode)
{
	struct inode *lower_inode;
	struct dentry *lower_dentry;
	int bindex, bstart, bend;

	bstart = dbstart(dentry);
	bend = dbend(dentry);

	for (bindex = bstart; bindex <= bend; bindex++) {
		lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);
		if (!lower_dentry) {
			unionfs_set_lower_inode_idx(inode, bindex, NULL);
			continue;
		}

		/* Initialize the lower inode to the new lower inode. */
		if (!lower_dentry->d_inode)
			continue;

		unionfs_set_lower_inode_idx(inode, bindex,
					    igrab(lower_dentry->d_inode));
	}

	ibstart(inode) = dbstart(dentry);
	ibend(inode) = dbend(dentry);

	/* Use attributes from the first branch. */
	lower_inode = unionfs_lower_inode(inode);

	/* Use different set of inode ops for symlinks & directories */
	if (S_ISLNK(lower_inode->i_mode))
		inode->i_op = &unionfs_symlink_iops;
	else if (S_ISDIR(lower_inode->i_mode))
		inode->i_op = &unionfs_dir_iops;

	/* Use different set of file ops for directories */
	if (S_ISDIR(lower_inode->i_mode))
		inode->i_fop = &unionfs_dir_fops;

	/* properly initialize special inodes */
	if (S_ISBLK(lower_inode->i_mode) || S_ISCHR(lower_inode->i_mode) ||
	    S_ISFIFO(lower_inode->i_mode) || S_ISSOCK(lower_inode->i_mode))
		init_special_inode(inode, lower_inode->i_mode,
				   lower_inode->i_rdev);

	/* all well, copy inode attributes */
	unionfs_copy_attr_all(inode, lower_inode);
	fsstack_copy_inode_size(inode, lower_inode);
}

/*
 * Connect a unionfs inode dentry/inode with several lower ones.  This is
 * the classic stackable file system "vnode interposition" action.
 *
 * @sb: unionfs's super_block
 */
struct dentry *unionfs_interpose(struct dentry *dentry, struct super_block *sb,
				 int flag)
{
	int err = 0;
	struct inode *inode;
	int need_fill_inode = 1;
	struct dentry *spliced = NULL;

	verify_locked(dentry);

	/*
	 * We allocate our new inode below by calling unionfs_iget,
	 * which will initialize some of the new inode's fields
	 */

	/*
	 * On revalidate we've already got our own inode and just need
	 * to fix it up.
	 */
	if (flag == INTERPOSE_REVAL) {
		inode = dentry->d_inode;
		UNIONFS_I(inode)->bstart = -1;
		UNIONFS_I(inode)->bend = -1;
		atomic_set(&UNIONFS_I(inode)->generation,
			   atomic_read(&UNIONFS_SB(sb)->generation));

		UNIONFS_I(inode)->lower_inodes =
			kcalloc(sbmax(sb), sizeof(struct inode *), GFP_KERNEL);
		if (unlikely(!UNIONFS_I(inode)->lower_inodes)) {
			err = -ENOMEM;
			goto out;
		}
	} else {
		/* get unique inode number for unionfs */
		inode = unionfs_iget(sb, iunique(sb, UNIONFS_ROOT_INO));
		if (IS_ERR(inode)) {
			err = PTR_ERR(inode);
			goto out;
		}
		if (atomic_read(&inode->i_count) > 1)
			goto skip;
	}

	need_fill_inode = 0;
	unionfs_fill_inode(dentry, inode);

skip:
	/* only (our) lookup wants to do a d_add */
	switch (flag) {
	case INTERPOSE_DEFAULT:
		/* for operations which create new inodes */
		d_add(dentry, inode);
		break;
	case INTERPOSE_REVAL_NEG:
		d_instantiate(dentry, inode);
		break;
	case INTERPOSE_LOOKUP:
		spliced = d_splice_alias(inode, dentry);
		if (spliced && spliced != dentry) {
			/*
			 * d_splice can return a dentry if it was
			 * disconnected and had to be moved.  We must ensure
			 * that the private data of the new dentry is
			 * correct and that the inode info was filled
			 * properly.  Finally we must return this new
			 * dentry.
			 */
			d_set_d_op(spliced, &unionfs_dops);
			spliced->d_fsdata = dentry->d_fsdata;
			dentry->d_fsdata = NULL;
			dentry = spliced;
			if (need_fill_inode) {
				need_fill_inode = 0;
				unionfs_fill_inode(dentry, inode);
			}
			goto out_spliced;
		} else if (!spliced) {
			if (need_fill_inode) {
				need_fill_inode = 0;
				unionfs_fill_inode(dentry, inode);
				goto out_spliced;
			}
		}
		break;
	case INTERPOSE_REVAL:
		/* Do nothing. */
		break;
	default:
		printk(KERN_CRIT "unionfs: invalid interpose flag passed!\n");
		BUG();
	}
	goto out;

out_spliced:
	if (!err)
		return spliced;
out:
	return ERR_PTR(err);
}

/* like interpose above, but for an already existing dentry */
void unionfs_reinterpose(struct dentry *dentry)
{
	struct dentry *lower_dentry;
	struct inode *inode;
	int bindex, bstart, bend;

	verify_locked(dentry);

	/* This is pre-allocated inode */
	inode = dentry->d_inode;

	bstart = dbstart(dentry);
	bend = dbend(dentry);
	for (bindex = bstart; bindex <= bend; bindex++) {
		lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);
		if (!lower_dentry)
			continue;

		if (!lower_dentry->d_inode)
			continue;
		if (unionfs_lower_inode_idx(inode, bindex))
			continue;
		unionfs_set_lower_inode_idx(inode, bindex,
					    igrab(lower_dentry->d_inode));
	}
	ibstart(inode) = dbstart(dentry);
	ibend(inode) = dbend(dentry);
}

/*
 * make sure the branch we just looked up (nd) makes sense:
 *
 * 1) we're not trying to stack unionfs on top of unionfs
 * 2) it exists
 * 3) is a directory
 */
int check_branch(const struct path *path)
{
	/* XXX: remove in ODF code -- stacking unions allowed there */
	if (!strcmp(path->dentry->d_sb->s_type->name, UNIONFS_NAME))
		return -EINVAL;
	if (!path->dentry->d_inode)
		return -ENOENT;
	if (!S_ISDIR(path->dentry->d_inode->i_mode))
		return -ENOTDIR;
	return 0;
}

/* checks if two lower_dentries have overlapping branches */
static int is_branch_overlap(struct dentry *dent1, struct dentry *dent2)
{
	struct dentry *dent = NULL;

	dent = dent1;
	while ((dent != dent2) && (dent->d_parent != dent))
		dent = dent->d_parent;

	if (dent == dent2)
		return 1;

	dent = dent2;
	while ((dent != dent1) && (dent->d_parent != dent))
		dent = dent->d_parent;

	return (dent == dent1);
}

/*
 * Parse "ro" or "rw" options, but default to "rw" if no mode options was
 * specified.  Fill the mode bits in @perms.  If encounter an unknown
 * string, return -EINVAL.  Otherwise return 0.
 */
int parse_branch_mode(const char *name, int *perms)
{
	if (!name || !strcmp(name, "rw")) {
		*perms = MAY_READ | MAY_WRITE;
		return 0;
	}
	if (!strcmp(name, "ro")) {
		*perms = MAY_READ;
		return 0;
	}
	return -EINVAL;
}

/*
 * parse the dirs= mount argument
 *
 * We don't need to lock the superblock private data's rwsem, as we get
 * called only by unionfs_read_super - it is still a long time before anyone
 * can even get a reference to us.
 */
static int parse_dirs_option(struct super_block *sb, struct unionfs_dentry_info
			     *lower_root_info, char *options)
{
	struct path path;
	char *name;
	int err = 0;
	int branches = 1;
	int bindex = 0;
	int i = 0;
	int j = 0;
	struct dentry *dent1;
	struct dentry *dent2;

	if (options[0] == '\0') {
		printk(KERN_ERR "unionfs: no branches specified\n");
		err = -EINVAL;
		goto out_return;
	}

	/*
	 * Each colon means we have a separator, this is really just a rough
	 * guess, since strsep will handle empty fields for us.
	 */
	for (i = 0; options[i]; i++)
		if (options[i] == ':')
			branches++;

	/* allocate space for underlying pointers to lower dentry */
	UNIONFS_SB(sb)->data =
		kcalloc(branches, sizeof(struct unionfs_data), GFP_KERNEL);
	if (unlikely(!UNIONFS_SB(sb)->data)) {
		err = -ENOMEM;
		goto out_return;
	}

	lower_root_info->lower_paths =
		kcalloc(branches, sizeof(struct path), GFP_KERNEL);
	if (unlikely(!lower_root_info->lower_paths)) {
		err = -ENOMEM;
		/* free the underlying pointer array */
		kfree(UNIONFS_SB(sb)->data);
		UNIONFS_SB(sb)->data = NULL;
		goto out_return;
	}

	/* now parsing a string such as "b1:b2=rw:b3=ro:b4" */
	branches = 0;
	while ((name = strsep(&options, ":")) != NULL) {
		int perms;
		char *mode = strchr(name, '=');

		if (!name)
			continue;
		if (!*name) {	/* bad use of ':' (extra colons) */
			err = -EINVAL;
			goto out;
		}

		branches++;

		/* strip off '=' if any */
		if (mode)
			*mode++ = '\0';

		err = parse_branch_mode(mode, &perms);
		if (err) {
			printk(KERN_ERR "unionfs: invalid mode \"%s\" for "
			       "branch %d\n", mode, bindex);
			goto out;
		}
		/* ensure that leftmost branch is writeable */
		if (!bindex && !(perms & MAY_WRITE)) {
			printk(KERN_ERR "unionfs: leftmost branch cannot be "
			       "read-only (use \"-o ro\" to create a "
			       "read-only union)\n");
			err = -EINVAL;
			goto out;
		}

		err = kern_path(name, LOOKUP_FOLLOW, &path);
		if (err) {
			printk(KERN_ERR "unionfs: error accessing "
			       "lower directory '%s' (error %d)\n",
			       name, err);
			goto out;
		}

		err = check_branch(&path);
		if (err) {
			printk(KERN_ERR "unionfs: lower directory "
			       "'%s' is not a valid branch\n", name);
			path_put(&path);
			goto out;
		}

		lower_root_info->lower_paths[bindex].dentry = path.dentry;
		lower_root_info->lower_paths[bindex].mnt = path.mnt;

		set_branchperms(sb, bindex, perms);
		set_branch_count(sb, bindex, 0);
		new_branch_id(sb, bindex);

		if (lower_root_info->bstart < 0)
			lower_root_info->bstart = bindex;
		lower_root_info->bend = bindex;
		bindex++;
	}

	if (branches == 0) {
		printk(KERN_ERR "unionfs: no branches specified\n");
		err = -EINVAL;
		goto out;
	}

	BUG_ON(branches != (lower_root_info->bend + 1));

	/*
	 * Ensure that no overlaps exist in the branches.
	 *
	 * This test is required because the Linux kernel has no support
	 * currently for ensuring coherency between stackable layers and
	 * branches.  If we were to allow overlapping branches, it would be
	 * possible, for example, to delete a file via one branch, which
	 * would not be reflected in another branch.  Such incoherency could
	 * lead to inconsistencies and even kernel oopses.  Rather than
	 * implement hacks to work around some of these cache-coherency
	 * problems, we prevent branch overlapping, for now.  A complete
	 * solution will involve proper kernel/VFS support for cache
	 * coherency, at which time we could safely remove this
	 * branch-overlapping test.
	 */
	for (i = 0; i < branches; i++) {
		dent1 = lower_root_info->lower_paths[i].dentry;
		for (j = i + 1; j < branches; j++) {
			dent2 = lower_root_info->lower_paths[j].dentry;
			if (is_branch_overlap(dent1, dent2)) {
				printk(KERN_ERR "unionfs: branches %d and "
				       "%d overlap\n", i, j);
				err = -EINVAL;
				goto out;
			}
		}
	}

out:
	if (err) {
		for (i = 0; i < branches; i++)
			path_put(&lower_root_info->lower_paths[i]);

		kfree(lower_root_info->lower_paths);
		kfree(UNIONFS_SB(sb)->data);

		/*
		 * MUST clear the pointers to prevent potential double free if
		 * the caller dies later on
		 */
		lower_root_info->lower_paths = NULL;
		UNIONFS_SB(sb)->data = NULL;
	}
out_return:
	return err;
}

/*
 * Parse mount options.  See the manual page for usage instructions.
 *
 * Returns the dentry object of the lower-level (lower) directory;
 * We want to mount our stackable file system on top of that lower directory.
 */
static struct unionfs_dentry_info *unionfs_parse_options(
					 struct super_block *sb,
					 char *options)
{
	struct unionfs_dentry_info *lower_root_info;
	char *optname;
	int err = 0;
	int bindex;
	int dirsfound = 0;

	/* allocate private data area */
	err = -ENOMEM;
	lower_root_info =
		kzalloc(sizeof(struct unionfs_dentry_info), GFP_KERNEL);
	if (unlikely(!lower_root_info))
		goto out_error;
	lower_root_info->bstart = -1;
	lower_root_info->bend = -1;
	lower_root_info->bopaque = -1;

	while ((optname = strsep(&options, ",")) != NULL) {
		char *optarg;

		if (!optname || !*optname)
			continue;

		optarg = strchr(optname, '=');
		if (optarg)
			*optarg++ = '\0';

		/*
		 * All of our options take an argument now. Insert ones that
		 * don't, above this check.
		 */
		if (!optarg) {
			printk(KERN_ERR "unionfs: %s requires an argument\n",
			       optname);
			err = -EINVAL;
			goto out_error;
		}

		if (!strcmp("dirs", optname)) {
			if (++dirsfound > 1) {
				printk(KERN_ERR
				       "unionfs: multiple dirs specified\n");
				err = -EINVAL;
				goto out_error;
			}
			err = parse_dirs_option(sb, lower_root_info, optarg);
			if (err)
				goto out_error;
			continue;
		}

		err = -EINVAL;
		printk(KERN_ERR
		       "unionfs: unrecognized option '%s'\n", optname);
		goto out_error;
	}
	if (dirsfound != 1) {
		printk(KERN_ERR "unionfs: dirs option required\n");
		err = -EINVAL;
		goto out_error;
	}
	goto out;

out_error:
	if (lower_root_info && lower_root_info->lower_paths) {
		for (bindex = lower_root_info->bstart;
		     bindex >= 0 && bindex <= lower_root_info->bend;
		     bindex++)
			path_put(&lower_root_info->lower_paths[bindex]);
	}

	kfree(lower_root_info->lower_paths);
	kfree(lower_root_info);

	kfree(UNIONFS_SB(sb)->data);
	UNIONFS_SB(sb)->data = NULL;

	lower_root_info = ERR_PTR(err);
out:
	return lower_root_info;
}

/*
 * There is no need to lock the unionfs_super_info's rwsem as there is no
 * way anyone can have a reference to the superblock at this point in time.
 */
static int unionfs_read_super(struct super_block *sb, void *raw_data,
			      int silent)
{
	int err = 0;
	struct unionfs_dentry_info *lower_root_info = NULL;
	int bindex, bstart, bend;
	struct inode *inode = NULL;

	if (!raw_data) {
		printk(KERN_ERR
		       "unionfs: read_super: missing data argument\n");
		err = -EINVAL;
		goto out;
	}

	/* Allocate superblock private data */
	sb->s_fs_info = kzalloc(sizeof(struct unionfs_sb_info), GFP_KERNEL);
	if (unlikely(!UNIONFS_SB(sb))) {
		printk(KERN_CRIT "unionfs: read_super: out of memory\n");
		err = -ENOMEM;
		goto out;
	}

	UNIONFS_SB(sb)->bend = -1;
	atomic_set(&UNIONFS_SB(sb)->generation, 1);
	init_rwsem(&UNIONFS_SB(sb)->rwsem);
	UNIONFS_SB(sb)->high_branch_id = -1; /* -1 == invalid branch ID */

	lower_root_info = unionfs_parse_options(sb, raw_data);
	if (IS_ERR(lower_root_info)) {
		printk(KERN_ERR
		       "unionfs: read_super: error while parsing options "
		       "(err = %ld)\n", PTR_ERR(lower_root_info));
		err = PTR_ERR(lower_root_info);
		lower_root_info = NULL;
		goto out_free;
	}
	if (lower_root_info->bstart == -1) {
		err = -ENOENT;
		goto out_free;
	}

	/* set the lower superblock field of upper superblock */
	bstart = lower_root_info->bstart;
	BUG_ON(bstart != 0);
	sbend(sb) = bend = lower_root_info->bend;
	for (bindex = bstart; bindex <= bend; bindex++) {
		struct dentry *d = lower_root_info->lower_paths[bindex].dentry;
		atomic_inc(&d->d_sb->s_active);
		unionfs_set_lower_super_idx(sb, bindex, d->d_sb);
	}

	/* max Bytes is the maximum bytes from highest priority branch */
	sb->s_maxbytes = unionfs_lower_super_idx(sb, 0)->s_maxbytes;

	/*
	 * Our c/m/atime granularity is 1 ns because we may stack on file
	 * systems whose granularity is as good.  This is important for our
	 * time-based cache coherency.
	 */
	sb->s_time_gran = 1;

	sb->s_op = &unionfs_sops;

	/* get a new inode and allocate our root dentry */
	inode = unionfs_iget(sb, iunique(sb, UNIONFS_ROOT_INO));
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_dput;
	}
	sb->s_root = d_make_root(inode);
	if (unlikely(!sb->s_root)) {
		err = -ENOMEM;
		goto out_iput;
	}
	d_set_d_op(sb->s_root, &unionfs_dops);

	/* link the upper and lower dentries */
	sb->s_root->d_fsdata = NULL;
	err = new_dentry_private_data(sb->s_root, UNIONFS_DMUTEX_ROOT);
	if (unlikely(err))
		goto out_freedpd;

	/* if get here: cannot have error */

	/* Set the lower dentries for s_root */
	for (bindex = bstart; bindex <= bend; bindex++) {
		struct dentry *d;
		struct vfsmount *m;

		d = lower_root_info->lower_paths[bindex].dentry;
		m = lower_root_info->lower_paths[bindex].mnt;

		unionfs_set_lower_dentry_idx(sb->s_root, bindex, d);
		unionfs_set_lower_mnt_idx(sb->s_root, bindex, m);
	}
	dbstart(sb->s_root) = bstart;
	dbend(sb->s_root) = bend;

	/* Set the generation number to one, since this is for the mount. */
	atomic_set(&UNIONFS_D(sb->s_root)->generation, 1);

	if (atomic_read(&inode->i_count) <= 1)
		unionfs_fill_inode(sb->s_root, inode);

	/*
	 * No need to call interpose because we already have a positive
	 * dentry, which was instantiated by d_alloc_root.  Just need to
	 * d_rehash it.
	 */
	d_rehash(sb->s_root);

	unionfs_unlock_dentry(sb->s_root);
	goto out; /* all is well */

out_freedpd:
	if (UNIONFS_D(sb->s_root)) {
		kfree(UNIONFS_D(sb->s_root)->lower_paths);
		free_dentry_private_data(sb->s_root);
	}
	dput(sb->s_root);

out_iput:
	iput(inode);

out_dput:
	if (lower_root_info && !IS_ERR(lower_root_info)) {
		for (bindex = lower_root_info->bstart;
		     bindex <= lower_root_info->bend; bindex++) {
			struct dentry *d;
			d = lower_root_info->lower_paths[bindex].dentry;
			/* drop refs we took earlier */
			atomic_dec(&d->d_sb->s_active);
			path_put(&lower_root_info->lower_paths[bindex]);
		}
		kfree(lower_root_info->lower_paths);
		kfree(lower_root_info);
		lower_root_info = NULL;
	}

out_free:
	kfree(UNIONFS_SB(sb)->data);
	kfree(UNIONFS_SB(sb));
	sb->s_fs_info = NULL;

out:
	if (lower_root_info && !IS_ERR(lower_root_info)) {
		kfree(lower_root_info->lower_paths);
		kfree(lower_root_info);
	}
	return err;
}

static struct dentry *unionfs_mount(struct file_system_type *fs_type,
				    int flags, const char *dev_name,
				    void *raw_data)
{
	struct dentry *dentry;

	dentry = mount_nodev(fs_type, flags, raw_data, unionfs_read_super);
	if (!IS_ERR(dentry))
		UNIONFS_SB(dentry->d_sb)->dev_name =
			kstrdup(dev_name, GFP_KERNEL);
	return dentry;
}

static struct file_system_type unionfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= UNIONFS_NAME,
	.mount		= unionfs_mount,
	.kill_sb	= generic_shutdown_super,
	.fs_flags	= FS_REVAL_DOT,
};

static int __init init_unionfs_fs(void)
{
	int err;

	pr_info("Registering unionfs " UNIONFS_VERSION "\n");

	err = unionfs_init_filldir_cache();
	if (unlikely(err))
		goto out;
	err = unionfs_init_inode_cache();
	if (unlikely(err))
		goto out;
	err = unionfs_init_dentry_cache();
	if (unlikely(err))
		goto out;
	err = init_sioq();
	if (unlikely(err))
		goto out;
	err = register_filesystem(&unionfs_fs_type);
out:
	if (unlikely(err)) {
		stop_sioq();
		unionfs_destroy_filldir_cache();
		unionfs_destroy_inode_cache();
		unionfs_destroy_dentry_cache();
	}
	return err;
}

static void __exit exit_unionfs_fs(void)
{
	stop_sioq();
	unionfs_destroy_filldir_cache();
	unionfs_destroy_inode_cache();
	unionfs_destroy_dentry_cache();
	unregister_filesystem(&unionfs_fs_type);
	pr_info("Completed unionfs module unload\n");
}

MODULE_AUTHOR("Erez Zadok, Filesystems and Storage Lab, Stony Brook University"
	      " (http://www.fsl.cs.sunysb.edu)");
MODULE_DESCRIPTION("Unionfs " UNIONFS_VERSION
		   " (http://unionfs.filesystems.org)");
MODULE_LICENSE("GPL");

module_init(init_unionfs_fs);
module_exit(exit_unionfs_fs);
