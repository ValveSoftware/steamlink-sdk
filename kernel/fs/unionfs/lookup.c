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
 * Lookup one path component @name relative to a <base,mnt> path pair.
 * Behaves nearly the same as lookup_one_len (i.e., return negative dentry
 * on ENOENT), but uses the @mnt passed, so it can cross bind mounts and
 * other lower mounts properly.  If @new_mnt is non-null, will fill in the
 * new mnt there.  Caller is responsible to dput/mntput/path_put returned
 * @dentry and @new_mnt.
 */
struct dentry *__lookup_one(struct dentry *base, struct vfsmount *mnt,
			    const char *name, struct vfsmount **new_mnt)
{
	struct dentry *dentry = NULL;
	struct path lower_path = {NULL, NULL};
	int err;

	/* we use flags=0 to get basic lookup */
	err = vfs_path_lookup(base, mnt, name, 0, &lower_path);

	switch (err) {
	case 0: /* no error */
		dentry = lower_path.dentry;
		if (new_mnt)
			*new_mnt = lower_path.mnt; /* rc already inc'ed */
		break;
	case -ENOENT:
		 /*
		  * We don't consider ENOENT an error, and we want to return
		  * a negative dentry (ala lookup_one_len).  As we know
		  * there was no inode for this name before (-ENOENT), then
		  * it's safe to call lookup_one_len (which doesn't take a
		  * vfsmount).
		  */
		dentry = lookup_lck_len(name, base, strlen(name));
		if (new_mnt)
			*new_mnt = mntget(lower_path.mnt);
		break;
	default: /* all other real errors */
		dentry = ERR_PTR(err);
		break;
	}

	return dentry;
}

/*
 * This is a utility function that fills in a unionfs dentry.
 * Caller must lock this dentry with unionfs_lock_dentry.
 *
 * Returns: 0 (ok), or -ERRNO if an error occurred.
 * XXX: get rid of _partial_lookup and make callers call _lookup_full directly
 */
int unionfs_partial_lookup(struct dentry *dentry, struct dentry *parent)
{
	struct dentry *tmp;
	int err = -ENOSYS;

	tmp = unionfs_lookup_full(dentry, parent, INTERPOSE_PARTIAL);

	if (!tmp) {
		err = 0;
		goto out;
	}
	if (IS_ERR(tmp)) {
		err = PTR_ERR(tmp);
		goto out;
	}
	/* XXX: need to change the interface */
	BUG_ON(tmp != dentry);
out:
	return err;
}

/* The dentry cache is just so we have properly sized dentries. */
static struct kmem_cache *unionfs_dentry_cachep;
int unionfs_init_dentry_cache(void)
{
	unionfs_dentry_cachep =
		kmem_cache_create("unionfs_dentry",
				  sizeof(struct unionfs_dentry_info),
				  0, SLAB_RECLAIM_ACCOUNT, NULL);

	return (unionfs_dentry_cachep ? 0 : -ENOMEM);
}

void unionfs_destroy_dentry_cache(void)
{
	if (unionfs_dentry_cachep)
		kmem_cache_destroy(unionfs_dentry_cachep);
}

void free_dentry_private_data(struct dentry *dentry)
{
	if (!dentry || !dentry->d_fsdata)
		return;
	kfree(UNIONFS_D(dentry)->lower_paths);
	UNIONFS_D(dentry)->lower_paths = NULL;
	kmem_cache_free(unionfs_dentry_cachep, dentry->d_fsdata);
	dentry->d_fsdata = NULL;
}

static inline int __realloc_dentry_private_data(struct dentry *dentry)
{
	struct unionfs_dentry_info *info = UNIONFS_D(dentry);
	void *p;
	int size;

	BUG_ON(!info);

	size = sizeof(struct path) * sbmax(dentry->d_sb);
	p = krealloc(info->lower_paths, size, GFP_ATOMIC);
	if (unlikely(!p))
		return -ENOMEM;

	info->lower_paths = p;

	info->bstart = -1;
	info->bend = -1;
	info->bopaque = -1;
	info->bcount = sbmax(dentry->d_sb);
	atomic_set(&info->generation,
			atomic_read(&UNIONFS_SB(dentry->d_sb)->generation));

	memset(info->lower_paths, 0, size);

	return 0;
}

/* UNIONFS_D(dentry)->lock must be locked */
int realloc_dentry_private_data(struct dentry *dentry)
{
	if (!__realloc_dentry_private_data(dentry))
		return 0;

	kfree(UNIONFS_D(dentry)->lower_paths);
	free_dentry_private_data(dentry);
	return -ENOMEM;
}

/* allocate new dentry private data */
int new_dentry_private_data(struct dentry *dentry, int subclass)
{
	struct unionfs_dentry_info *info = UNIONFS_D(dentry);

	BUG_ON(info);

	info = kmem_cache_alloc(unionfs_dentry_cachep, GFP_ATOMIC);
	if (unlikely(!info))
		return -ENOMEM;

	mutex_init(&info->lock);
	mutex_lock_nested(&info->lock, subclass);

	info->lower_paths = NULL;

	dentry->d_fsdata = info;

	if (!__realloc_dentry_private_data(dentry))
		return 0;

	mutex_unlock(&info->lock);
	free_dentry_private_data(dentry);
	return -ENOMEM;
}

/*
 * scan through the lower dentry objects, and set bstart to reflect the
 * starting branch
 */
void update_bstart(struct dentry *dentry)
{
	int bindex;
	int bstart = dbstart(dentry);
	int bend = dbend(dentry);
	struct dentry *lower_dentry;

	for (bindex = bstart; bindex <= bend; bindex++) {
		lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);
		if (!lower_dentry)
			continue;
		if (lower_dentry->d_inode) {
			dbstart(dentry) = bindex;
			break;
		}
		dput(lower_dentry);
		unionfs_set_lower_dentry_idx(dentry, bindex, NULL);
	}
}

/*
 * Main (and complex) driver function for Unionfs's lookup
 *
 * Returns: NULL (ok), ERR_PTR if an error occurred, or a non-null non-error
 * PTR if d_splice returned a different dentry.
 *
 * If lookupmode is INTERPOSE_PARTIAL/REVAL/REVAL_NEG, the passed dentry's
 * inode info must be locked.  If lookupmode is INTERPOSE_LOOKUP (i.e., a
 * newly looked-up dentry), then unionfs_lookup_backend will return a locked
 * dentry's info, which the caller must unlock.
 */
struct dentry *unionfs_lookup_full(struct dentry *dentry,
				   struct dentry *parent, int lookupmode)
{
	int err = 0;
	struct dentry *lower_dentry = NULL;
	struct vfsmount *lower_mnt;
	struct vfsmount *lower_dir_mnt;
	struct dentry *wh_lower_dentry = NULL;
	struct dentry *lower_dir_dentry = NULL;
	struct dentry *d_interposed = NULL;
	int bindex, bstart, bend, bopaque;
	int opaque, num_positive = 0;
	const char *name;
	int namelen;
	int pos_start, pos_end;

	/*
	 * We should already have a lock on this dentry in the case of a
	 * partial lookup, or a revalidation.  Otherwise it is returned from
	 * new_dentry_private_data already locked.
	 */
	verify_locked(dentry);
	verify_locked(parent);

	/* must initialize dentry operations */
	if (lookupmode == INTERPOSE_LOOKUP)
		d_set_d_op(dentry, &unionfs_dops);

	/* We never partial lookup the root directory. */
	if (IS_ROOT(dentry))
		goto out;

	name = dentry->d_name.name;
	namelen = dentry->d_name.len;

	/* No dentries should get created for possible whiteout names. */
	if (!is_validname(name)) {
		err = -EPERM;
		goto out_free;
	}

	/* Now start the actual lookup procedure. */
	bstart = dbstart(parent);
	bend = dbend(parent);
	bopaque = dbopaque(parent);
	BUG_ON(bstart < 0);

	/* adjust bend to bopaque if needed */
	if ((bopaque >= 0) && (bopaque < bend))
		bend = bopaque;

	/* lookup all possible dentries */
	for (bindex = bstart; bindex <= bend; bindex++) {

		lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);
		lower_mnt = unionfs_lower_mnt_idx(dentry, bindex);

		/* skip if we already have a positive lower dentry */
		if (lower_dentry) {
			if (dbstart(dentry) < 0)
				dbstart(dentry) = bindex;
			if (bindex > dbend(dentry))
				dbend(dentry) = bindex;
			if (lower_dentry->d_inode)
				num_positive++;
			continue;
		}

		lower_dir_dentry =
			unionfs_lower_dentry_idx(parent, bindex);
		/* if the lower dentry's parent does not exist, skip this */
		if (!lower_dir_dentry || !lower_dir_dentry->d_inode)
			continue;

		/* also skip it if the parent isn't a directory. */
		if (!S_ISDIR(lower_dir_dentry->d_inode->i_mode))
			continue; /* XXX: should be BUG_ON */

		/* check for whiteouts: stop lookup if found */
		wh_lower_dentry = lookup_whiteout(name, lower_dir_dentry);
		if (IS_ERR(wh_lower_dentry)) {
			err = PTR_ERR(wh_lower_dentry);
			goto out_free;
		}
		if (wh_lower_dentry->d_inode) {
			dbend(dentry) = dbopaque(dentry) = bindex;
			if (dbstart(dentry) < 0)
				dbstart(dentry) = bindex;
			dput(wh_lower_dentry);
			break;
		}
		dput(wh_lower_dentry);

		/* Now do regular lookup; lookup @name */
		lower_dir_mnt = unionfs_lower_mnt_idx(parent, bindex);
		lower_mnt = NULL; /* XXX: needed? */

		lower_dentry = __lookup_one(lower_dir_dentry, lower_dir_mnt,
					    name, &lower_mnt);

		if (IS_ERR(lower_dentry)) {
			err = PTR_ERR(lower_dentry);
			goto out_free;
		}
		unionfs_set_lower_dentry_idx(dentry, bindex, lower_dentry);
		if (!lower_mnt)
			lower_mnt = unionfs_mntget(dentry->d_sb->s_root,
						   bindex);
		unionfs_set_lower_mnt_idx(dentry, bindex, lower_mnt);

		/* adjust dbstart/end */
		if (dbstart(dentry) < 0)
			dbstart(dentry) = bindex;
		if (bindex > dbend(dentry))
			dbend(dentry) = bindex;
		/*
		 * We always store the lower dentries above, and update
		 * dbstart/dbend, even if the whole unionfs dentry is
		 * negative (i.e., no lower inodes).
		 */
		if (!lower_dentry->d_inode)
			continue;
		num_positive++;

		/*
		 * check if we just found an opaque directory, if so, stop
		 * lookups here.
		 */
		if (!S_ISDIR(lower_dentry->d_inode->i_mode))
			continue;
		opaque = is_opaque_dir(dentry, bindex);
		if (opaque < 0) {
			err = opaque;
			goto out_free;
		} else if (opaque) {
			dbend(dentry) = dbopaque(dentry) = bindex;
			break;
		}
		dbend(dentry) = bindex;

		/* update parent directory's atime with the bindex */
		fsstack_copy_attr_atime(parent->d_inode,
					lower_dir_dentry->d_inode);
	}

	/* sanity checks, then decide if to process a negative dentry */
	BUG_ON(dbstart(dentry) < 0 && dbend(dentry) >= 0);
	BUG_ON(dbstart(dentry) >= 0 && dbend(dentry) < 0);

	if (num_positive > 0)
		goto out_positive;

	/*** handle NEGATIVE dentries ***/

	/*
	 * If negative, keep only first lower negative dentry, to save on
	 * memory.
	 */
	if (dbstart(dentry) < dbend(dentry)) {
		path_put_lowers(dentry, dbstart(dentry) + 1,
				dbend(dentry), false);
		dbend(dentry) = dbstart(dentry);
	}
	if (lookupmode == INTERPOSE_PARTIAL)
		goto out;
	if (lookupmode == INTERPOSE_LOOKUP) {
		/*
		 * If all we found was a whiteout in the first available
		 * branch, then create a negative dentry for a possibly new
		 * file to be created.
		 */
		if (dbopaque(dentry) < 0)
			goto out;
		/* XXX: need to get mnt here */
		bindex = dbstart(dentry);
		if (unionfs_lower_dentry_idx(dentry, bindex))
			goto out;
		lower_dir_dentry =
			unionfs_lower_dentry_idx(parent, bindex);
		if (!lower_dir_dentry || !lower_dir_dentry->d_inode)
			goto out;
		if (!S_ISDIR(lower_dir_dentry->d_inode->i_mode))
			goto out; /* XXX: should be BUG_ON */
		/* XXX: do we need to cross bind mounts here? */
		lower_dentry = lookup_lck_len(name, lower_dir_dentry, namelen);
		if (IS_ERR(lower_dentry)) {
			err = PTR_ERR(lower_dentry);
			goto out;
		}
		/* XXX: need to mntget/mntput as needed too! */
		unionfs_set_lower_dentry_idx(dentry, bindex, lower_dentry);
		/* XXX: wrong mnt for crossing bind mounts! */
		lower_mnt = unionfs_mntget(dentry->d_sb->s_root, bindex);
		unionfs_set_lower_mnt_idx(dentry, bindex, lower_mnt);

		goto out;
	}

	/* if we're revalidating a positive dentry, don't make it negative */
	if (lookupmode != INTERPOSE_REVAL)
		d_add(dentry, NULL);

	goto out;

out_positive:
	/*** handle POSITIVE dentries ***/

	/*
	 * This unionfs dentry is positive (at least one lower inode
	 * exists), so scan entire dentry from beginning to end, and remove
	 * any negative lower dentries, if any.  Then, update dbstart/dbend
	 * to reflect the start/end of positive dentries.
	 */
	pos_start = pos_end = -1;
	for (bindex = bstart; bindex <= bend; bindex++) {
		lower_dentry = unionfs_lower_dentry_idx(dentry,
							bindex);
		if (lower_dentry && lower_dentry->d_inode) {
			if (pos_start < 0)
				pos_start = bindex;
			if (bindex > pos_end)
				pos_end = bindex;
			continue;
		}
		path_put_lowers(dentry, bindex, bindex, false);
	}
	if (pos_start >= 0)
		dbstart(dentry) = pos_start;
	if (pos_end >= 0)
		dbend(dentry) = pos_end;

	/* Partial lookups need to re-interpose, or throw away older negs. */
	if (lookupmode == INTERPOSE_PARTIAL) {
		if (dentry->d_inode) {
			unionfs_reinterpose(dentry);
			goto out;
		}

		/*
		 * This dentry was positive, so it is as if we had a
		 * negative revalidation.
		 */
		lookupmode = INTERPOSE_REVAL_NEG;
		update_bstart(dentry);
	}

	/*
	 * Interpose can return a dentry if d_splice returned a different
	 * dentry.
	 */
	d_interposed = unionfs_interpose(dentry, dentry->d_sb, lookupmode);
	if (IS_ERR(d_interposed))
		err = PTR_ERR(d_interposed);
	else if (d_interposed)
		dentry = d_interposed;

	if (!err)
		goto out;
	d_drop(dentry);

out_free:
	/* should dput/mntput all the underlying dentries on error condition */
	if (dbstart(dentry) >= 0)
		path_put_lowers_all(dentry, false);
	/* free lower_paths unconditionally */
	kfree(UNIONFS_D(dentry)->lower_paths);
	UNIONFS_D(dentry)->lower_paths = NULL;

out:
	if (dentry && UNIONFS_D(dentry)) {
		BUG_ON(dbstart(dentry) < 0 && dbend(dentry) >= 0);
		BUG_ON(dbstart(dentry) >= 0 && dbend(dentry) < 0);
	}
	if (d_interposed && UNIONFS_D(d_interposed)) {
		BUG_ON(dbstart(d_interposed) < 0 && dbend(d_interposed) >= 0);
		BUG_ON(dbstart(d_interposed) >= 0 && dbend(d_interposed) < 0);
	}

	if (!err && d_interposed)
		return d_interposed;
	return ERR_PTR(err);
}
