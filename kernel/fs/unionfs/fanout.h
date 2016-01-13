/*
 * Copyright (c) 2003-2014 Erez Zadok
 * Copyright (c) 2003-2006 Charles P. Wright
 * Copyright (c) 2005-2007 Josef 'Jeff' Sipek
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

#ifndef _FANOUT_H_
#define _FANOUT_H_

/*
 * Inode to private data
 *
 * Since we use containers and the struct inode is _inside_ the
 * unionfs_inode_info structure, UNIONFS_I will always (given a non-NULL
 * inode pointer), return a valid non-NULL pointer.
 */
static inline struct unionfs_inode_info *UNIONFS_I(const struct inode *inode)
{
	return container_of(inode, struct unionfs_inode_info, vfs_inode);
}

#define ibstart(ino) (UNIONFS_I(ino)->bstart)
#define ibend(ino) (UNIONFS_I(ino)->bend)

/* Dentry to private data */
#define UNIONFS_D(dent) ((struct unionfs_dentry_info *)(dent)->d_fsdata)
#define dbstart(dent) (UNIONFS_D(dent)->bstart)
#define dbend(dent) (UNIONFS_D(dent)->bend)
#define dbopaque(dent) (UNIONFS_D(dent)->bopaque)

/* Superblock to private data */
#define UNIONFS_SB(super) ((struct unionfs_sb_info *)(super)->s_fs_info)
#define sbstart(sb) 0
#define sbend(sb) (UNIONFS_SB(sb)->bend)
#define sbmax(sb) (UNIONFS_SB(sb)->bend + 1)
#define sbhbid(sb) (UNIONFS_SB(sb)->high_branch_id)

/* File to private Data */
#define UNIONFS_F(file) ((struct unionfs_file_info *)((file)->private_data))
#define fbstart(file) (UNIONFS_F(file)->bstart)
#define fbend(file) (UNIONFS_F(file)->bend)

/* macros to manipulate branch IDs in stored in our superblock */
static inline int branch_id(struct super_block *sb, int index)
{
	BUG_ON(!sb || index < 0);
	return UNIONFS_SB(sb)->data[index].branch_id;
}

static inline void set_branch_id(struct super_block *sb, int index, int val)
{
	BUG_ON(!sb || index < 0);
	UNIONFS_SB(sb)->data[index].branch_id = val;
}

static inline void new_branch_id(struct super_block *sb, int index)
{
	BUG_ON(!sb || index < 0);
	set_branch_id(sb, index, ++UNIONFS_SB(sb)->high_branch_id);
}

/*
 * Find new index of matching branch with an existing superblock of a known
 * (possibly old) id.  This is needed because branches could have been
 * added/deleted causing the branches of any open files to shift.
 *
 * @sb: the new superblock which may have new/different branch IDs
 * @id: the old/existing id we're looking for
 * Returns index of newly found branch (0 or greater), -1 otherwise.
 */
static inline int branch_id_to_idx(struct super_block *sb, int id)
{
	int i;
	for (i = 0; i < sbmax(sb); i++) {
		if (branch_id(sb, i) == id)
			return i;
	}
	/* in the non-ODF code, this should really never happen */
	printk(KERN_WARNING "unionfs: cannot find branch with id %d\n", id);
	return -1;
}

/* File to lower file. */
static inline struct file *unionfs_lower_file(const struct file *f)
{
	BUG_ON(!f);
	return UNIONFS_F(f)->lower_files[fbstart(f)];
}

static inline struct file *unionfs_lower_file_idx(const struct file *f,
						  int index)
{
	BUG_ON(!f || index < 0);
	return UNIONFS_F(f)->lower_files[index];
}

static inline void unionfs_set_lower_file_idx(struct file *f, int index,
					      struct file *val)
{
	BUG_ON(!f || index < 0);
	UNIONFS_F(f)->lower_files[index] = val;
	/* save branch ID (may be redundant?) */
	UNIONFS_F(f)->saved_branch_ids[index] =
		branch_id((f)->f_path.dentry->d_sb, index);
}

static inline void unionfs_set_lower_file(struct file *f, struct file *val)
{
	BUG_ON(!f);
	unionfs_set_lower_file_idx((f), fbstart(f), (val));
}

/* Inode to lower inode. */
static inline struct inode *unionfs_lower_inode(const struct inode *i)
{
	BUG_ON(!i);
	return UNIONFS_I(i)->lower_inodes[ibstart(i)];
}

static inline struct inode *unionfs_lower_inode_idx(const struct inode *i,
						    int index)
{
	BUG_ON(!i || index < 0);
	return UNIONFS_I(i)->lower_inodes[index];
}

static inline void unionfs_set_lower_inode_idx(struct inode *i, int index,
					       struct inode *val)
{
	BUG_ON(!i || index < 0);
	UNIONFS_I(i)->lower_inodes[index] = val;
}

static inline void unionfs_set_lower_inode(struct inode *i, struct inode *val)
{
	BUG_ON(!i);
	UNIONFS_I(i)->lower_inodes[ibstart(i)] = val;
}

/* Superblock to lower superblock. */
static inline struct super_block *unionfs_lower_super(
					const struct super_block *sb)
{
	BUG_ON(!sb);
	return UNIONFS_SB(sb)->data[sbstart(sb)].sb;
}

static inline struct super_block *unionfs_lower_super_idx(
					const struct super_block *sb,
					int index)
{
	BUG_ON(!sb || index < 0);
	return UNIONFS_SB(sb)->data[index].sb;
}

static inline void unionfs_set_lower_super_idx(struct super_block *sb,
					       int index,
					       struct super_block *val)
{
	BUG_ON(!sb || index < 0);
	UNIONFS_SB(sb)->data[index].sb = val;
}

static inline void unionfs_set_lower_super(struct super_block *sb,
					   struct super_block *val)
{
	BUG_ON(!sb);
	UNIONFS_SB(sb)->data[sbstart(sb)].sb = val;
}

/* Branch count macros. */
static inline int branch_count(const struct super_block *sb, int index)
{
	BUG_ON(!sb || index < 0);
	return atomic_read(&UNIONFS_SB(sb)->data[index].open_files);
}

static inline void set_branch_count(struct super_block *sb, int index, int val)
{
	BUG_ON(!sb || index < 0);
	atomic_set(&UNIONFS_SB(sb)->data[index].open_files, val);
}

static inline void branchget(struct super_block *sb, int index)
{
	BUG_ON(!sb || index < 0);
	atomic_inc(&UNIONFS_SB(sb)->data[index].open_files);
}

static inline void branchput(struct super_block *sb, int index)
{
	BUG_ON(!sb || index < 0);
	atomic_dec(&UNIONFS_SB(sb)->data[index].open_files);
}

/* Dentry macros */
static inline void unionfs_set_lower_dentry_idx(struct dentry *dent, int index,
						struct dentry *val)
{
	BUG_ON(!dent || index < 0);
	UNIONFS_D(dent)->lower_paths[index].dentry = val;
}

static inline struct dentry *unionfs_lower_dentry_idx(
				const struct dentry *dent,
				int index)
{
	BUG_ON(!dent || index < 0);
	return UNIONFS_D(dent)->lower_paths[index].dentry;
}

static inline struct dentry *unionfs_lower_dentry(const struct dentry *dent)
{
	BUG_ON(!dent);
	return unionfs_lower_dentry_idx(dent, dbstart(dent));
}

static inline void unionfs_set_lower_mnt_idx(struct dentry *dent, int index,
					     struct vfsmount *mnt)
{
	BUG_ON(!dent || index < 0);
	UNIONFS_D(dent)->lower_paths[index].mnt = mnt;
}

static inline struct vfsmount *unionfs_lower_mnt_idx(
					const struct dentry *dent,
					int index)
{
	BUG_ON(!dent || index < 0);
	return UNIONFS_D(dent)->lower_paths[index].mnt;
}

static inline struct vfsmount *unionfs_lower_mnt(const struct dentry *dent)
{
	BUG_ON(!dent);
	return unionfs_lower_mnt_idx(dent, dbstart(dent));
}

/* Macros for locking a dentry. */
enum unionfs_dentry_lock_class {
	UNIONFS_DMUTEX_NORMAL,
	UNIONFS_DMUTEX_ROOT,
	UNIONFS_DMUTEX_PARENT,
	UNIONFS_DMUTEX_CHILD,
	UNIONFS_DMUTEX_WHITEOUT,
	UNIONFS_DMUTEX_REVAL_PARENT, /* for file/dentry revalidate */
	UNIONFS_DMUTEX_REVAL_CHILD,   /* for file/dentry revalidate */
};

static inline void unionfs_lock_dentry(struct dentry *d,
				       unsigned int subclass)
{
	BUG_ON(!d);
	mutex_lock_nested(&UNIONFS_D(d)->lock, subclass);
}

static inline void unionfs_unlock_dentry(struct dentry *d)
{
	BUG_ON(!d);
	mutex_unlock(&UNIONFS_D(d)->lock);
}

static inline struct dentry *unionfs_lock_parent(struct dentry *d,
						 unsigned int subclass)
{
	struct dentry *p;

	BUG_ON(!d);
	p = dget_parent(d);
	if (p != d)
		mutex_lock_nested(&UNIONFS_D(p)->lock, subclass);
	return p;
}

static inline void unionfs_unlock_parent(struct dentry *d, struct dentry *p)
{
	BUG_ON(!d);
	BUG_ON(!p);
	if (p != d) {
		BUG_ON(!mutex_is_locked(&UNIONFS_D(p)->lock));
		mutex_unlock(&UNIONFS_D(p)->lock);
	}
	dput(p);
}

static inline void verify_locked(struct dentry *d)
{
	BUG_ON(!d);
	BUG_ON(!mutex_is_locked(&UNIONFS_D(d)->lock));
}

/* macros to put lower objects */

/*
 * iput lower inodes of an unionfs dentry, from bstart to bend.  If
 * @free_lower is true, then also kfree the memory used to hold the lower
 * object pointers.
 */
static inline void iput_lowers(struct inode *inode,
			       int bstart, int bend, bool free_lower)
{
	struct inode *lower_inode;
	int bindex;

	BUG_ON(!inode);
	BUG_ON(!UNIONFS_I(inode));
	BUG_ON(bstart < 0);

	for (bindex = bstart; bindex <= bend; bindex++) {
		lower_inode = unionfs_lower_inode_idx(inode, bindex);
		if (lower_inode) {
			unionfs_set_lower_inode_idx(inode, bindex, NULL);
			/* see Documentation/filesystems/unionfs/issues.txt */
			lockdep_off();
			iput(lower_inode);
			lockdep_on();
		}
	}

	if (free_lower) {
		kfree(UNIONFS_I(inode)->lower_inodes);
		UNIONFS_I(inode)->lower_inodes = NULL;
	}
}

/* iput all lower inodes, and reset start/end branch indices to -1 */
static inline void iput_lowers_all(struct inode *inode, bool free_lower)
{
	int bstart, bend;

	BUG_ON(!inode);
	BUG_ON(!UNIONFS_I(inode));
	bstart = ibstart(inode);
	bend = ibend(inode);
	BUG_ON(bstart < 0);

	iput_lowers(inode, bstart, bend, free_lower);
	ibstart(inode) = ibend(inode) = -1;
}

/*
 * dput/mntput all lower dentries and vfsmounts of an unionfs dentry, from
 * bstart to bend.  If @free_lower is true, then also kfree the memory used
 * to hold the lower object pointers.
 *
 * XXX: implement using path_put VFS macros
 */
static inline void path_put_lowers(struct dentry *dentry,
				   int bstart, int bend, bool free_lower)
{
	struct dentry *lower_dentry;
	struct vfsmount *lower_mnt;
	int bindex;

	BUG_ON(!dentry);
	BUG_ON(!UNIONFS_D(dentry));
	BUG_ON(bstart < 0);

	for (bindex = bstart; bindex <= bend; bindex++) {
		lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);
		if (lower_dentry) {
			unionfs_set_lower_dentry_idx(dentry, bindex, NULL);
			dput(lower_dentry);
		}
		lower_mnt = unionfs_lower_mnt_idx(dentry, bindex);
		if (lower_mnt) {
			unionfs_set_lower_mnt_idx(dentry, bindex, NULL);
			mntput(lower_mnt);
		}
	}

	if (free_lower) {
		kfree(UNIONFS_D(dentry)->lower_paths);
		UNIONFS_D(dentry)->lower_paths = NULL;
	}
}

/*
 * dput/mntput all lower dentries and vfsmounts, and reset start/end branch
 * indices to -1.
 */
static inline void path_put_lowers_all(struct dentry *dentry, bool free_lower)
{
	int bstart, bend;

	BUG_ON(!dentry);
	BUG_ON(!UNIONFS_D(dentry));
	bstart = dbstart(dentry);
	bend = dbend(dentry);
	BUG_ON(bstart < 0);

	path_put_lowers(dentry, bstart, bend, free_lower);
	dbstart(dentry) = dbend(dentry) = -1;
}

#endif	/* not _FANOUT_H */
