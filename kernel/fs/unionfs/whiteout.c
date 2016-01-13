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
 * whiteout and opaque directory helpers
 */

/* What do we use for whiteouts. */
#define UNIONFS_WHPFX ".wh."
#define UNIONFS_WHLEN 4
/*
 * If a directory contains this file, then it is opaque.  We start with the
 * .wh. flag so that it is blocked by lookup.
 */
#define UNIONFS_DIR_OPAQUE_NAME "__dir_opaque"
#define UNIONFS_DIR_OPAQUE UNIONFS_WHPFX UNIONFS_DIR_OPAQUE_NAME

/* construct whiteout filename */
char *alloc_whname(const char *name, int len)
{
	char *buf;

	buf = kmalloc(len + UNIONFS_WHLEN + 1, GFP_KERNEL);
	if (unlikely(!buf))
		return ERR_PTR(-ENOMEM);

	strcpy(buf, UNIONFS_WHPFX);
	strlcat(buf, name, len + UNIONFS_WHLEN + 1);

	return buf;
}

/*
 * XXX: this can be inline or CPP macro, but is here to keep all whiteout
 * code in one place.
 */
void unionfs_set_max_namelen(long *namelen)
{
	*namelen -= UNIONFS_WHLEN;
}

/* check if @namep is a whiteout, update @namep and @namelenp accordingly */
bool is_whiteout_name(char **namep, int *namelenp)
{
	if (*namelenp > UNIONFS_WHLEN &&
	    !strncmp(*namep, UNIONFS_WHPFX, UNIONFS_WHLEN)) {
		*namep += UNIONFS_WHLEN;
		*namelenp -= UNIONFS_WHLEN;
		return true;
	}
	return false;
}

/* is the filename valid == !(whiteout for a file or opaque dir marker) */
bool is_validname(const char *name)
{
	if (!strncmp(name, UNIONFS_WHPFX, UNIONFS_WHLEN))
		return false;
	if (!strncmp(name, UNIONFS_DIR_OPAQUE_NAME,
		     sizeof(UNIONFS_DIR_OPAQUE_NAME) - 1))
		return false;
	return true;
}

/*
 * Look for a whiteout @name in @lower_parent directory.  If error, return
 * ERR_PTR.  Caller must dput() the returned dentry if not an error.
 *
 * XXX: some callers can reuse the whname allocated buffer to avoid repeated
 * free then re-malloc calls.  Need to provide a different API for those
 * callers.
 */
struct dentry *lookup_whiteout(const char *name, struct dentry *lower_parent)
{
	char *whname = NULL;
	int err = 0, namelen;
	struct dentry *wh_dentry = NULL;

	namelen = strlen(name);
	whname = alloc_whname(name, namelen);
	if (unlikely(IS_ERR(whname))) {
		err = PTR_ERR(whname);
		goto out;
	}

	/* check if whiteout exists in this branch: lookup .wh.foo */
	wh_dentry = lookup_lck_len(whname, lower_parent, strlen(whname));
	if (IS_ERR(wh_dentry)) {
		err = PTR_ERR(wh_dentry);
		goto out;
	}

	/* check if negative dentry (ENOENT) */
	if (!wh_dentry->d_inode)
		goto out;

	/* whiteout found: check if valid type */
	if (!S_ISREG(wh_dentry->d_inode->i_mode)) {
		printk(KERN_ERR "unionfs: invalid whiteout %s entry type %d\n",
		       whname, wh_dentry->d_inode->i_mode);
		dput(wh_dentry);
		err = -EIO;
		goto out;
	}

out:
	kfree(whname);
	if (err)
		wh_dentry = ERR_PTR(err);
	return wh_dentry;
}

/* find and return first whiteout in parent directory, else ENOENT */
struct dentry *find_first_whiteout(struct dentry *dentry)
{
	int bindex, bstart, bend;
	struct dentry *parent, *lower_parent, *wh_dentry;

	parent = dget_parent(dentry);

	bstart = dbstart(parent);
	bend = dbend(parent);
	wh_dentry = ERR_PTR(-ENOENT);

	for (bindex = bstart; bindex <= bend; bindex++) {
		lower_parent = unionfs_lower_dentry_idx(parent, bindex);
		if (!lower_parent)
			continue;
		wh_dentry = lookup_whiteout(dentry->d_name.name, lower_parent);
		if (IS_ERR(wh_dentry))
			continue;
		if (wh_dentry->d_inode)
			break;
		dput(wh_dentry);
		wh_dentry = ERR_PTR(-ENOENT);
	}

	dput(parent);

	return wh_dentry;
}

/*
 * Unlink a whiteout dentry.  Returns 0 or -errno.  Caller must hold and
 * release dentry reference.
 */
int unlink_whiteout(struct dentry *wh_dentry)
{
	int err;
	struct dentry *lower_dir_dentry;

	/* dget and lock parent dentry */
	lower_dir_dentry = lock_parent_wh(wh_dentry);

	/* see Documentation/filesystems/unionfs/issues.txt */
	lockdep_off();
	err = vfs_unlink(lower_dir_dentry->d_inode, wh_dentry);
	lockdep_on();
	unlock_dir(lower_dir_dentry);

	/*
	 * Whiteouts are special files and should be deleted no matter what
	 * (as if they never existed), in order to allow this create
	 * operation to succeed.  This is especially important in sticky
	 * directories: a whiteout may have been created by one user, but
	 * the newly created file may be created by another user.
	 * Therefore, in order to maintain Unix semantics, if the vfs_unlink
	 * above failed, then we have to try to directly unlink the
	 * whiteout.  Note: in the ODF version of unionfs, whiteout are
	 * handled much more cleanly.
	 */
	if (err == -EPERM) {
		struct inode *inode = lower_dir_dentry->d_inode;
		err = inode->i_op->unlink(inode, wh_dentry);
	}
	if (err)
		printk(KERN_ERR "unionfs: could not unlink whiteout %s, "
		       "err = %d\n", wh_dentry->d_name.name, err);

	return err;

}

/*
 * Helper function when creating new objects (create, symlink, mknod, etc.).
 * Checks to see if there's a whiteout in @lower_dentry's parent directory,
 * whose name is taken from @dentry.  Then tries to remove that whiteout, if
 * found.  If <dentry,bindex> is a branch marked readonly, return -EROFS.
 * If it finds both a regular file and a whiteout, delete whiteout (this
 * should never happen).
 *
 * Return 0 if no whiteout was found.  Return 1 if one was found and
 * successfully removed.  Therefore a value >= 0 tells the caller that
 * @lower_dentry belongs to a good branch to create the new object in).
 * Return -ERRNO if an error occurred during whiteout lookup or in trying to
 * unlink the whiteout.
 */
int check_unlink_whiteout(struct dentry *dentry, struct dentry *lower_dentry,
			  int bindex)
{
	int err;
	struct dentry *wh_dentry = NULL;
	struct dentry *lower_dir_dentry = NULL;

	/* look for whiteout dentry first */
	lower_dir_dentry = dget_parent(lower_dentry);
	wh_dentry = lookup_whiteout(dentry->d_name.name, lower_dir_dentry);
	dput(lower_dir_dentry);
	if (IS_ERR(wh_dentry)) {
		err = PTR_ERR(wh_dentry);
		goto out;
	}

	if (!wh_dentry->d_inode) { /* no whiteout exists*/
		err = 0;
		goto out_dput;
	}

	/* check if regular file and whiteout were both found */
	if (unlikely(lower_dentry->d_inode))
		printk(KERN_WARNING "unionfs: removing whiteout; regular "
		       "file exists in directory %s (branch %d)\n",
		       lower_dir_dentry->d_name.name, bindex);

	/* check if branch is writeable */
	err = is_robranch_super(dentry->d_sb, bindex);
	if (err)
		goto out_dput;

	/* .wh.foo has been found, so let's unlink it */
	err = unlink_whiteout(wh_dentry);
	if (!err)
		err = 1; /* a whiteout was found and successfully removed */
out_dput:
	dput(wh_dentry);
out:
	return err;
}

/*
 * Pass an unionfs dentry and an index.  It will try to create a whiteout
 * for the filename in dentry, and will try in branch 'index'.  On error,
 * it will proceed to a branch to the left.
 */
int create_whiteout(struct dentry *dentry, int start)
{
	int bstart, bend, bindex;
	struct dentry *lower_dir_dentry;
	struct dentry *lower_dentry;
	struct dentry *lower_wh_dentry;
	char *name = NULL;
	int err = -EINVAL;

	verify_locked(dentry);

	bstart = dbstart(dentry);
	bend = dbend(dentry);

	/* create dentry's whiteout equivalent */
	name = alloc_whname(dentry->d_name.name, dentry->d_name.len);
	if (unlikely(IS_ERR(name))) {
		err = PTR_ERR(name);
		goto out;
	}

	for (bindex = start; bindex >= 0; bindex--) {
		lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);

		if (!lower_dentry) {
			/*
			 * if lower dentry is not present, create the
			 * entire lower dentry directory structure and go
			 * ahead.  Since we want to just create whiteout, we
			 * only want the parent dentry, and hence get rid of
			 * this dentry.
			 */
			lower_dentry = create_parents(dentry->d_inode,
						      dentry,
						      dentry->d_name.name,
						      bindex);
			if (!lower_dentry || IS_ERR(lower_dentry)) {
				int ret = PTR_ERR(lower_dentry);
				if (!IS_COPYUP_ERR(ret))
					printk(KERN_ERR
					       "unionfs: create_parents for "
					       "whiteout failed: bindex=%d "
					       "err=%d\n", bindex, ret);
				continue;
			}
		}

		lower_wh_dentry =
			lookup_lck_len(name, lower_dentry->d_parent,
				       dentry->d_name.len + UNIONFS_WHLEN);
		if (IS_ERR(lower_wh_dentry))
			continue;

		/*
		 * The whiteout already exists. This used to be impossible,
		 * but now is possible because of opaqueness.
		 */
		if (lower_wh_dentry->d_inode) {
			dput(lower_wh_dentry);
			err = 0;
			goto out;
		}

		lower_dir_dentry = lock_parent_wh(lower_wh_dentry);
		err = is_robranch_super(dentry->d_sb, bindex);
		if (!err)
			err = vfs_create(lower_dir_dentry->d_inode,
					 lower_wh_dentry,
					 current_umask() & S_IRUGO,
					 0); // XXX: pass want_excl?
		unlock_dir(lower_dir_dentry);
		dput(lower_wh_dentry);

		if (!err || !IS_COPYUP_ERR(err))
			break;
	}

	/* set dbopaque so that lookup will not proceed after this branch */
	if (!err)
		dbopaque(dentry) = bindex;

out:
	kfree(name);
	return err;
}

/*
 * Delete all of the whiteouts in a given directory for rmdir.
 *
 * lower directory inode should be locked
 */
static int do_delete_whiteouts(struct dentry *dentry, int bindex,
			       struct unionfs_dir_state *namelist)
{
	int err = 0;
	struct dentry *lower_dir_dentry = NULL;
	struct dentry *lower_dentry;
	char *name = NULL, *p;
	struct inode *lower_dir;
	int i;
	struct list_head *pos;
	struct filldir_node *cursor;

	/* Find out lower parent dentry */
	lower_dir_dentry = unionfs_lower_dentry_idx(dentry, bindex);
	BUG_ON(!S_ISDIR(lower_dir_dentry->d_inode->i_mode));
	lower_dir = lower_dir_dentry->d_inode;
	BUG_ON(!S_ISDIR(lower_dir->i_mode));

	err = -ENOMEM;
	name = __getname();
	if (unlikely(!name))
		goto out;
	strcpy(name, UNIONFS_WHPFX);
	p = name + UNIONFS_WHLEN;

	err = 0;
	for (i = 0; !err && i < namelist->size; i++) {
		list_for_each(pos, &namelist->list[i]) {
			cursor =
				list_entry(pos, struct filldir_node,
					   file_list);
			/* Only operate on whiteouts in this branch. */
			if (cursor->bindex != bindex)
				continue;
			if (!cursor->whiteout)
				continue;

			strlcpy(p, cursor->name, PATH_MAX - UNIONFS_WHLEN);
			lower_dentry =
				lookup_lck_len(name, lower_dir_dentry,
					       cursor->namelen +
					       UNIONFS_WHLEN);
			if (IS_ERR(lower_dentry)) {
				err = PTR_ERR(lower_dentry);
				break;
			}
			if (lower_dentry->d_inode) {
				mutex_lock(&(lower_dir->i_mutex));
				err = vfs_unlink(lower_dir, lower_dentry);
				mutex_unlock(&(lower_dir->i_mutex));
			}
			dput(lower_dentry);
			if (err)
				break;
		}
	}

	__putname(name);

	/* After all of the removals, we should copy the attributes once. */
	fsstack_copy_attr_times(dentry->d_inode, lower_dir_dentry->d_inode);

out:
	return err;
}


void __delete_whiteouts(struct work_struct *work)
{
	struct sioq_args *args = container_of(work, struct sioq_args, work);
	struct deletewh_args *d = &args->deletewh;

	args->err = do_delete_whiteouts(d->dentry, d->bindex, d->namelist);
	complete(&args->comp);
}

/* delete whiteouts in a dir (for rmdir operation) using sioq if necessary */
int delete_whiteouts(struct dentry *dentry, int bindex,
		     struct unionfs_dir_state *namelist)
{
	int err;
	struct super_block *sb;
	struct dentry *lower_dir_dentry;
	struct inode *lower_dir;
	struct sioq_args args;

	sb = dentry->d_sb;

	BUG_ON(!S_ISDIR(dentry->d_inode->i_mode));
	BUG_ON(bindex < dbstart(dentry));
	BUG_ON(bindex > dbend(dentry));
	err = is_robranch_super(sb, bindex);
	if (err)
		goto out;

	lower_dir_dentry = unionfs_lower_dentry_idx(dentry, bindex);
	BUG_ON(!S_ISDIR(lower_dir_dentry->d_inode->i_mode));
	lower_dir = lower_dir_dentry->d_inode;
	BUG_ON(!S_ISDIR(lower_dir->i_mode));

	if (!inode_permission(lower_dir, MAY_WRITE | MAY_EXEC)) {
		err = do_delete_whiteouts(dentry, bindex, namelist);
	} else {
		args.deletewh.namelist = namelist;
		args.deletewh.dentry = dentry;
		args.deletewh.bindex = bindex;
		run_sioq(__delete_whiteouts, &args);
		err = args.err;
	}

out:
	return err;
}

/****************************************************************************
 * Opaque directory helpers                                                 *
 ****************************************************************************/

/*
 * is_opaque_dir: returns 0 if it is NOT an opaque dir, 1 if it is, and
 * -errno if an error occurred trying to figure this out.
 */
int is_opaque_dir(struct dentry *dentry, int bindex)
{
	int err = 0;
	struct dentry *lower_dentry;
	struct dentry *wh_lower_dentry;
	struct inode *lower_inode;
	struct sioq_args args;

	lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);
	lower_inode = lower_dentry->d_inode;

	BUG_ON(!S_ISDIR(lower_inode->i_mode));

	mutex_lock(&lower_inode->i_mutex);

	if (!inode_permission(lower_inode, MAY_EXEC)) {
		wh_lower_dentry =
			lookup_one_len(UNIONFS_DIR_OPAQUE, lower_dentry,
				       sizeof(UNIONFS_DIR_OPAQUE) - 1); // XXX: pass flags?
	} else {
		args.is_opaque.dentry = lower_dentry;
		run_sioq(__is_opaque_dir, &args);
		wh_lower_dentry = args.ret;
	}

	mutex_unlock(&lower_inode->i_mutex);

	if (IS_ERR(wh_lower_dentry)) {
		err = PTR_ERR(wh_lower_dentry);
		goto out;
	}

	/* This is an opaque dir iff wh_lower_dentry is positive */
	err = !!wh_lower_dentry->d_inode;

	dput(wh_lower_dentry);
out:
	return err;
}

void __is_opaque_dir(struct work_struct *work)
{
	struct sioq_args *args = container_of(work, struct sioq_args, work);

	args->ret = lookup_one_len(UNIONFS_DIR_OPAQUE,
				   args->is_opaque.dentry,
				   sizeof(UNIONFS_DIR_OPAQUE) - 1); // XXX: pass flags?
	complete(&args->comp);
}

int make_dir_opaque(struct dentry *dentry, int bindex)
{
	int err = 0;
	struct dentry *lower_dentry, *diropq;
	struct inode *lower_dir;
	const struct cred *old_creds;
	struct cred *new_creds;

	/*
	 * Opaque directory whiteout markers are special files (like regular
	 * whiteouts), and should appear to the users as if they don't
	 * exist.  They should be created/deleted regardless of directory
	 * search/create permissions, but only for the duration of this
	 * creation of the .wh.__dir_opaque: file.  Note, this does not
	 * circumvent normal ->permission).
	 */
	new_creds = prepare_creds();
	if (unlikely(!new_creds)) {
		err = -ENOMEM;
		goto out_err;
	}
	cap_raise(new_creds->cap_effective, CAP_DAC_READ_SEARCH);
	cap_raise(new_creds->cap_effective, CAP_DAC_OVERRIDE);
	old_creds = override_creds(new_creds);

	lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);
	lower_dir = lower_dentry->d_inode;
	BUG_ON(!S_ISDIR(dentry->d_inode->i_mode) ||
	       !S_ISDIR(lower_dir->i_mode));

	mutex_lock(&lower_dir->i_mutex);
	diropq = lookup_one_len(UNIONFS_DIR_OPAQUE, lower_dentry,
				sizeof(UNIONFS_DIR_OPAQUE) - 1); // XXX: pass flags?
	if (IS_ERR(diropq)) {
		err = PTR_ERR(diropq);
		goto out;
	}

	if (!diropq->d_inode)
		err = vfs_create(lower_dir, diropq, S_IRUGO, 0); // XXX: pass want_excl?
	if (!err)
		dbopaque(dentry) = bindex;

	dput(diropq);

out:
	mutex_unlock(&lower_dir->i_mutex);
	revert_creds(old_creds);
out_err:
	return err;
}
