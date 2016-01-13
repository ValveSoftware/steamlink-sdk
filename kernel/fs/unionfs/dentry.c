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

bool is_negative_lower(const struct dentry *dentry)
{
	int bindex;
	struct dentry *lower_dentry;

	BUG_ON(!dentry);
	/* cache coherency: check if file was deleted on lower branch */
	if (dbstart(dentry) < 0)
		return true;
	for (bindex = dbstart(dentry); bindex <= dbend(dentry); bindex++) {
		lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);
		/* unhashed (i.e., unlinked) lower dentries don't count */
		if (lower_dentry && lower_dentry->d_inode &&
		    !d_deleted(lower_dentry) &&
		    !(lower_dentry->d_flags & DCACHE_NFSFS_RENAMED))
			return false;
	}
	return true;
}

static inline void __dput_lowers(struct dentry *dentry, int start, int end)
{
	struct dentry *lower_dentry;
	int bindex;

	if (start < 0)
		return;
	for (bindex = start; bindex <= end; bindex++) {
		lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);
		if (!lower_dentry)
			continue;
		unionfs_set_lower_dentry_idx(dentry, bindex, NULL);
		dput(lower_dentry);
	}
}

/*
 * Purge and invalidate as many data pages of a unionfs inode.  This is
 * called when the lower inode has changed, and we want to force processes
 * to re-get the new data.
 */
static inline void purge_inode_data(struct inode *inode)
{
	/* remove all non-private mappings */
	unmap_mapping_range(inode->i_mapping, 0, 0, 0);
	/* invalidate as many pages as possible */
	invalidate_mapping_pages(inode->i_mapping, 0, -1);
	/*
	 * Don't try to truncate_inode_pages here, because this could lead
	 * to a deadlock between some of address_space ops and dentry
	 * revalidation: the address space op is invoked with a lock on our
	 * own page, and truncate_inode_pages will block on locked pages.
	 */
}

/*
 * Revalidate a single file/symlink/special dentry.  Assume that info nodes
 * of the @dentry and its @parent are locked.  Assume parent is valid,
 * otherwise return false (and let's hope the VFS will try to re-lookup this
 * dentry).  Returns true if valid, false otherwise.
 */
bool __unionfs_d_revalidate(struct dentry *dentry, struct dentry *parent,
			    bool willwrite, unsigned int flags)
{
	bool valid = true;	/* default is valid */
	struct dentry *lower_dentry;
	struct dentry *result;
	int bindex, bstart, bend;
	int sbgen, dgen, pdgen;
	int positive = 0;
	int interpose_flag;

	verify_locked(dentry);
	verify_locked(parent);

	/* if the dentry is unhashed, do NOT revalidate */
	if (d_deleted(dentry))
		goto out;

	dgen = atomic_read(&UNIONFS_D(dentry)->generation);

	if (is_newer_lower(dentry)) {
		/* root dentry is always valid */
		if (IS_ROOT(dentry)) {
			unionfs_copy_attr_times(dentry->d_inode);
		} else {
			/*
			 * reset generation number to zero, guaranteed to be
			 * "old"
			 */
			dgen = 0;
			atomic_set(&UNIONFS_D(dentry)->generation, dgen);
		}
		if (!willwrite)
			purge_inode_data(dentry->d_inode);
	}

	sbgen = atomic_read(&UNIONFS_SB(dentry->d_sb)->generation);

	BUG_ON(dbstart(dentry) == -1);
	if (dentry->d_inode)
		positive = 1;

	/* if our dentry is valid, then validate all lower ones */
	if (sbgen == dgen)
		goto validate_lowers;

	/* The root entry should always be valid */
	BUG_ON(IS_ROOT(dentry));

	/* We can't work correctly if our parent isn't valid. */
	pdgen = atomic_read(&UNIONFS_D(parent)->generation);

	/* Free the pointers for our inodes and this dentry. */
	path_put_lowers_all(dentry, false);

	interpose_flag = INTERPOSE_REVAL_NEG;
	if (positive) {
		interpose_flag = INTERPOSE_REVAL;
		iput_lowers_all(dentry->d_inode, true);
	}

	if (realloc_dentry_private_data(dentry) != 0) {
		valid = false;
		goto out;
	}

	result = unionfs_lookup_full(dentry, parent, interpose_flag);
	if (result) {
		if (IS_ERR(result)) {
			valid = false;
			goto out;
		}
		/*
		 * current unionfs_lookup_backend() doesn't return
		 * a valid dentry
		 */
		dput(dentry);
		dentry = result;
	}

	if (unlikely(positive && is_negative_lower(dentry))) {
		/* call make_bad_inode here ? */
		d_drop(dentry);
		valid = false;
		goto out;
	}

	/*
	 * if we got here then we have revalidated our dentry and all lower
	 * ones, so we can return safely.
	 */
	if (!valid)		/* lower dentry revalidation failed */
		goto out;

	/*
	 * If the parent's gen no.  matches the superblock's gen no., then
	 * we can update our denty's gen no.  If they didn't match, then it
	 * was OK to revalidate this dentry with a stale parent, but we'll
	 * purposely not update our dentry's gen no. (so it can be redone);
	 * and, we'll mark our parent dentry as invalid so it'll force it
	 * (and our dentry) to be revalidated.
	 */
	if (pdgen == sbgen)
		atomic_set(&UNIONFS_D(dentry)->generation, sbgen);
	goto out;

validate_lowers:

	/* The revalidation must occur across all branches */
	bstart = dbstart(dentry);
	bend = dbend(dentry);
	BUG_ON(bstart == -1);
	for (bindex = bstart; bindex <= bend; bindex++) {
		lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);
		if (!lower_dentry || !lower_dentry->d_op
		    || !lower_dentry->d_op->d_revalidate)
			continue;
		if (!lower_dentry->d_op->d_revalidate(lower_dentry, flags))
			valid = false;
	}

	if (!dentry->d_inode ||
	    ibstart(dentry->d_inode) < 0 ||
	    ibend(dentry->d_inode) < 0) {
		valid = false;
		goto out;
	}

	if (valid) {
		/*
		 * If we get here, and we copy the meta-data from the lower
		 * inode to our inode, then it is vital that we have already
		 * purged all unionfs-level file data.  We do that in the
		 * caller (__unionfs_d_revalidate) by calling
		 * purge_inode_data.
		 */
		unionfs_copy_attr_all(dentry->d_inode,
				      unionfs_lower_inode(dentry->d_inode));
		fsstack_copy_inode_size(dentry->d_inode,
					unionfs_lower_inode(dentry->d_inode));
	}

out:
	return valid;
}

/*
 * Determine if the lower inode objects have changed from below the unionfs
 * inode.  Return true if changed, false otherwise.
 *
 * We check if the mtime or ctime have changed.  However, the inode times
 * can be changed by anyone without much protection, including
 * asynchronously.  This can sometimes cause unionfs to find that the lower
 * file system doesn't change its inode times quick enough, resulting in a
 * false positive indication (which is harmless, it just makes unionfs do
 * extra work in re-validating the objects).  To minimize the chances of
 * these situations, we still consider such small time changes valid, but we
 * don't print debugging messages unless the time changes are greater than
 * UNIONFS_MIN_CC_TIME (which defaults to 3 seconds, as with NFS's acregmin)
 * because significant changes are more likely due to users manually
 * touching lower files.
 */
bool is_newer_lower(const struct dentry *dentry)
{
	int bindex;
	struct inode *inode;
	struct inode *lower_inode;

	/* ignore if we're called on semi-initialized dentries/inodes */
	if (!dentry || !UNIONFS_D(dentry))
		return false;
	inode = dentry->d_inode;
	if (!inode || !UNIONFS_I(inode)->lower_inodes ||
	    ibstart(inode) < 0 || ibend(inode) < 0)
		return false;

	for (bindex = ibstart(inode); bindex <= ibend(inode); bindex++) {
		lower_inode = unionfs_lower_inode_idx(inode, bindex);
		if (!lower_inode)
			continue;

		/* check if mtime/ctime have changed */
		if (unlikely(timespec_compare(&inode->i_mtime,
					      &lower_inode->i_mtime) < 0)) {
			if ((lower_inode->i_mtime.tv_sec -
			     inode->i_mtime.tv_sec) > UNIONFS_MIN_CC_TIME) {
				pr_info("unionfs: new lower inode mtime "
					"(bindex=%d, name=%s)\n", bindex,
					dentry->d_name.name);
				show_dinode_times(dentry);
			}
			return true;
		}
		if (unlikely(timespec_compare(&inode->i_ctime,
					      &lower_inode->i_ctime) < 0)) {
			if ((lower_inode->i_ctime.tv_sec -
			     inode->i_ctime.tv_sec) > UNIONFS_MIN_CC_TIME) {
				pr_info("unionfs: new lower inode ctime "
					"(bindex=%d, name=%s)\n", bindex,
					dentry->d_name.name);
				show_dinode_times(dentry);
			}
			return true;
		}
	}

	/*
	 * Last check: if this is a positive dentry, but somehow all lower
	 * dentries are negative or unhashed, then this dentry needs to be
	 * revalidated, because someone probably deleted the objects from
	 * the lower branches directly.
	 */
	if (is_negative_lower(dentry))
		return true;

	return false;		/* default: lower is not newer */
}

static int unionfs_d_revalidate(struct dentry *dentry, unsigned int flags)
{
	bool valid = true;
	int err = 1;		/* 1 means valid for the VFS */
	struct dentry *parent;

	if (flags & LOOKUP_RCU)
		return -ECHILD;

	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	parent = unionfs_lock_parent(dentry, UNIONFS_DMUTEX_PARENT);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	if (!unionfs_lower_dentry(dentry)) {
		err = 0;
		goto out;
	}

	valid = __unionfs_d_revalidate(dentry, parent, false, flags);
	if (valid) {
		unionfs_postcopyup_setmnt(dentry);
		unionfs_check_dentry(dentry);
	} else {
		d_drop(dentry);
		err = valid;
	}
out:
	unionfs_unlock_dentry(dentry);
	unionfs_unlock_parent(dentry, parent);
	unionfs_read_unlock(dentry->d_sb);

	return err;
}

static void unionfs_d_release(struct dentry *dentry)
{
	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	if (unlikely(!UNIONFS_D(dentry)))
		goto out;	/* skip if no lower branches */
	/* must lock our branch configuration here */
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	unionfs_check_dentry(dentry);
	/* this could be a negative dentry, so check first */
	if (dbstart(dentry) < 0) {
		unionfs_unlock_dentry(dentry);
		goto out;	/* due to a (normal) failed lookup */
	}

	/* Release all the lower dentries */
	path_put_lowers_all(dentry, true);

	unionfs_unlock_dentry(dentry);

out:
	free_dentry_private_data(dentry);
	unionfs_read_unlock(dentry->d_sb);
	return;
}

/*
 * Called when we're removing the last reference to our dentry.  So we
 * should drop all lower references too.
 */
static void unionfs_d_iput(struct dentry *dentry, struct inode *inode)
{
	int rc;

	BUG_ON(!dentry);
	unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
	unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);

	if (!UNIONFS_D(dentry) || dbstart(dentry) < 0)
		goto drop_lower_inodes;
	path_put_lowers_all(dentry, false);

drop_lower_inodes:
	rc = atomic_read(&inode->i_count);
	if (rc == 1 && inode->i_nlink == 1 && ibstart(inode) >= 0) {
		/* see Documentation/filesystems/unionfs/issues.txt */
		lockdep_off();
		iput(unionfs_lower_inode(inode));
		lockdep_on();
		unionfs_set_lower_inode(inode, NULL);
		/* XXX: may need to set start/end to -1? */
	}

	iput(inode);

	unionfs_unlock_dentry(dentry);
	unionfs_read_unlock(dentry->d_sb);
}

struct dentry_operations unionfs_dops = {
	.d_revalidate	= unionfs_d_revalidate,
	.d_release	= unionfs_d_release,
	.d_iput		= unionfs_d_iput,
};
