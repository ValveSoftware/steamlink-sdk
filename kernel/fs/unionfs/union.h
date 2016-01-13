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

#ifndef _UNION_H_
#define _UNION_H_

#include <linux/dcache.h>
#include <linux/file.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/aio.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/page-flags.h>
#include <linux/pagemap.h>
#include <linux/poll.h>
#include <linux/security.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/statfs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/writeback.h>
#include <linux/buffer_head.h>
#include <linux/xattr.h>
#include <linux/fs_stack.h>
#include <linux/magic.h>
#include <linux/log2.h>
#include <linux/poison.h>
#include <linux/mman.h>
#include <linux/backing-dev.h>
#include <linux/splice.h>
#include <linux/sched.h>

#include <linux/union_fs.h>

/* the file system name */
#define UNIONFS_NAME "unionfs"

/* unionfs root inode number */
#define UNIONFS_ROOT_INO     1

/* number of times we try to get a unique temporary file name */
#define GET_TMPNAM_MAX_RETRY	5

/* maximum number of branches we support, to avoid memory blowup */
#define UNIONFS_MAX_BRANCHES	128

/* minimum time (seconds) required for time-based cache-coherency */
#define UNIONFS_MIN_CC_TIME	3

/* Operations vectors defined in specific files. */
extern struct file_operations unionfs_main_fops;
extern struct file_operations unionfs_dir_fops;
extern struct inode_operations unionfs_main_iops;
extern struct inode_operations unionfs_dir_iops;
extern struct inode_operations unionfs_symlink_iops;
extern struct super_operations unionfs_sops;
extern struct dentry_operations unionfs_dops;
extern struct address_space_operations unionfs_aops, unionfs_dummy_aops;
extern struct vm_operations_struct unionfs_vm_ops;

/* How long should an entry be allowed to persist */
#define RDCACHE_JIFFIES	(5*HZ)

/* compatibility with Real-Time patches */
#ifdef CONFIG_PREEMPT_RT
# define unionfs_rw_semaphore	compat_rw_semaphore
#else /* not CONFIG_PREEMPT_RT */
# define unionfs_rw_semaphore	rw_semaphore
#endif /* not CONFIG_PREEMPT_RT */

/* file private data. */
struct unionfs_file_info {
	int bstart;
	int bend;
	atomic_t generation;

	struct unionfs_dir_state *rdstate;
	struct file **lower_files;
	int *saved_branch_ids; /* IDs of branches when file was opened */
	const struct vm_operations_struct *lower_vm_ops;
	bool wrote_to_file;	/* for delayed copyup */
};

/* unionfs inode data in memory */
struct unionfs_inode_info {
	int bstart;
	int bend;
	atomic_t generation;
	/* Stuff for readdir over NFS. */
	spinlock_t rdlock;
	struct list_head readdircache;
	int rdcount;
	int hashsize;
	int cookie;

	/* The lower inodes */
	struct inode **lower_inodes;

	struct inode vfs_inode;
};

/* unionfs dentry data in memory */
struct unionfs_dentry_info {
	/*
	 * The semaphore is used to lock the dentry as soon as we get into a
	 * unionfs function from the VFS.  Our lock ordering is that children
	 * go before their parents.
	 */
	struct mutex lock;
	int bstart;
	int bend;
	int bopaque;
	int bcount;
	atomic_t generation;
	struct path *lower_paths;
};

/* These are the pointers to our various objects. */
struct unionfs_data {
	struct super_block *sb;	/* lower super_block */
	atomic_t open_files;	/* number of open files on branch */
	int branchperms;
	int branch_id;		/* unique branch ID at re/mount time */
};

/* unionfs super-block data in memory */
struct unionfs_sb_info {
	int bend;

	atomic_t generation;

	/*
	 * This rwsem is used to make sure that a branch management
	 * operation...
	 *   1) will not begin before all currently in-flight operations
	 *      complete.
	 *   2) any new operations do not execute until the currently
	 *      running branch management operation completes.
	 *
	 * The write_lock_owner records the PID of the task which grabbed
	 * the rw_sem for writing.  If the same task also tries to grab the
	 * read lock, we allow it.  This prevents a self-deadlock when
	 * branch-management is used on a pivot_root'ed union, because we
	 * have to ->lookup paths which belong to the same union.
	 */
	struct unionfs_rw_semaphore rwsem;
	pid_t write_lock_owner;	/* PID of rw_sem owner (write lock) */
	int high_branch_id;	/* last unique branch ID given */
	char *dev_name;		/* to identify different unions in pr_debug */
	struct unionfs_data *data;
};

/*
 * structure for making the linked list of entries by readdir on left branch
 * to compare with entries on right branch
 */
struct filldir_node {
	struct list_head file_list;	/* list for directory entries */
	char *name;		/* name entry */
	int hash;		/* name hash */
	int namelen;		/* name len since name is not 0 terminated */

	/*
	 * we can check for duplicate whiteouts and files in the same branch
	 * in order to return -EIO.
	 */
	int bindex;

	/* is this a whiteout entry? */
	int whiteout;

	/* Inline name, so we don't need to separately kmalloc small ones */
	char iname[DNAME_INLINE_LEN];
};

/* Directory hash table. */
struct unionfs_dir_state {
	unsigned int cookie;	/* the cookie, based off of rdversion */
	unsigned int offset;	/* The entry we have returned. */
	int bindex;
	loff_t dirpos;		/* offset within the lower level directory */
	int size;		/* How big is the hash table? */
	int hashentries;	/* How many entries have been inserted? */
	unsigned long access;

	/* This cache list is used when the inode keeps us around. */
	struct list_head cache;
	struct list_head list[0];
};

/* externs needed for fanout.h or sioq.h */
extern int unionfs_get_nlinks(const struct inode *inode);
extern void unionfs_copy_attr_times(struct inode *upper);
extern void unionfs_copy_attr_all(struct inode *dest, const struct inode *src);

/* include miscellaneous macros */
#include "fanout.h"
#include "sioq.h"

/* externs for cache creation/deletion routines */
extern void unionfs_destroy_filldir_cache(void);
extern int unionfs_init_filldir_cache(void);
extern int unionfs_init_inode_cache(void);
extern void unionfs_destroy_inode_cache(void);
extern int unionfs_init_dentry_cache(void);
extern void unionfs_destroy_dentry_cache(void);

/* Initialize and free readdir-specific  state. */
extern int init_rdstate(struct file *file);
extern struct unionfs_dir_state *alloc_rdstate(struct inode *inode,
					       int bindex);
extern struct unionfs_dir_state *find_rdstate(struct inode *inode,
					      loff_t fpos);
extern void free_rdstate(struct unionfs_dir_state *state);
extern int add_filldir_node(struct unionfs_dir_state *rdstate,
			    const char *name, int namelen, int bindex,
			    int whiteout);
extern struct filldir_node *find_filldir_node(struct unionfs_dir_state *rdstate,
					      const char *name, int namelen,
					      int is_whiteout);

extern struct dentry **alloc_new_dentries(int objs);
extern struct unionfs_data *alloc_new_data(int objs);

/* We can only use 32-bits of offset for rdstate --- blech! */
#define DIREOF (0xfffff)
#define RDOFFBITS 20		/* This is the number of bits in DIREOF. */
#define MAXRDCOOKIE (0xfff)
/* Turn an rdstate into an offset. */
static inline off_t rdstate2offset(struct unionfs_dir_state *buf)
{
	off_t tmp;

	tmp = ((buf->cookie & MAXRDCOOKIE) << RDOFFBITS)
		| (buf->offset & DIREOF);
	return tmp;
}

/* Macros for locking a super_block. */
enum unionfs_super_lock_class {
	UNIONFS_SMUTEX_NORMAL,
	UNIONFS_SMUTEX_PARENT,	/* when locking on behalf of file */
	UNIONFS_SMUTEX_CHILD,	/* when locking on behalf of dentry */
};
static inline void unionfs_read_lock(struct super_block *sb, int subclass)
{
	if (UNIONFS_SB(sb)->write_lock_owner &&
	    UNIONFS_SB(sb)->write_lock_owner == current->pid)
		return;
	down_read_nested(&UNIONFS_SB(sb)->rwsem, subclass);
}
static inline void unionfs_read_unlock(struct super_block *sb)
{
	if (UNIONFS_SB(sb)->write_lock_owner &&
	    UNIONFS_SB(sb)->write_lock_owner == current->pid)
		return;
	up_read(&UNIONFS_SB(sb)->rwsem);
}
static inline void unionfs_write_lock(struct super_block *sb)
{
	down_write(&UNIONFS_SB(sb)->rwsem);
	UNIONFS_SB(sb)->write_lock_owner = current->pid;
}
static inline void unionfs_write_unlock(struct super_block *sb)
{
	up_write(&UNIONFS_SB(sb)->rwsem);
	UNIONFS_SB(sb)->write_lock_owner = 0;
}

static inline void unionfs_double_lock_dentry(struct dentry *d1,
					      struct dentry *d2)
{
	BUG_ON(d1 == d2);
	if (d1 < d2) {
		unionfs_lock_dentry(d1, UNIONFS_DMUTEX_PARENT);
		unionfs_lock_dentry(d2, UNIONFS_DMUTEX_CHILD);
	} else {
		unionfs_lock_dentry(d2, UNIONFS_DMUTEX_PARENT);
		unionfs_lock_dentry(d1, UNIONFS_DMUTEX_CHILD);
	}
}

static inline void unionfs_double_unlock_dentry(struct dentry *d1,
						struct dentry *d2)
{
	BUG_ON(d1 == d2);
	if (d1 < d2) { /* unlock in reverse order than double_lock_dentry */
		unionfs_unlock_dentry(d1);
		unionfs_unlock_dentry(d2);
	} else {
		unionfs_unlock_dentry(d2);
		unionfs_unlock_dentry(d1);
	}
}

static inline void unionfs_double_lock_parents(struct dentry *p1,
					       struct dentry *p2)
{
	if (p1 == p2) {
		unionfs_lock_dentry(p1, UNIONFS_DMUTEX_REVAL_PARENT);
		return;
	}
	if (p1 < p2) {
		unionfs_lock_dentry(p1, UNIONFS_DMUTEX_REVAL_PARENT);
		unionfs_lock_dentry(p2, UNIONFS_DMUTEX_REVAL_CHILD);
	} else {
		unionfs_lock_dentry(p2, UNIONFS_DMUTEX_REVAL_PARENT);
		unionfs_lock_dentry(p1, UNIONFS_DMUTEX_REVAL_CHILD);
	}
}

static inline void unionfs_double_unlock_parents(struct dentry *p1,
						 struct dentry *p2)
{
	if (p1 == p2) {
		unionfs_unlock_dentry(p1);
		return;
	}
	if (p1 < p2) { /* unlock in reverse order of double_lock_parents */
		unionfs_unlock_dentry(p1);
		unionfs_unlock_dentry(p2);
	} else {
		unionfs_unlock_dentry(p2);
		unionfs_unlock_dentry(p1);
	}
}

extern int new_dentry_private_data(struct dentry *dentry, int subclass);
extern int realloc_dentry_private_data(struct dentry *dentry);
extern void free_dentry_private_data(struct dentry *dentry);
extern void update_bstart(struct dentry *dentry);

/*
 * EXTERNALS:
 */

/* replicates the directory structure up to given dentry in given branch */
extern struct dentry *create_parents(struct inode *dir, struct dentry *dentry,
				     const char *name, int bindex);

/* partial lookup */
extern int unionfs_partial_lookup(struct dentry *dentry,
				  struct dentry *parent);
extern struct dentry *unionfs_lookup_full(struct dentry *dentry,
					  struct dentry *parent,
					  int lookupmode);

/* copies a file from dbstart to newbindex branch */
extern int copyup_file(struct inode *dir, struct file *file, int bstart,
		       int newbindex, loff_t size);
extern int copyup_named_file(struct inode *dir, struct file *file,
			     char *name, int bstart, int new_bindex,
			     loff_t len);
/* copies a dentry from dbstart to newbindex branch */
extern int copyup_dentry(struct inode *dir, struct dentry *dentry,
			 int bstart, int new_bindex, const char *name,
			 int namelen, struct file **copyup_file, loff_t len);
/* helper functions for post-copyup actions */
extern void unionfs_postcopyup_setmnt(struct dentry *dentry);
extern void unionfs_postcopyup_release(struct dentry *dentry);

/* Is this directory empty: 0 if it is empty, -ENOTEMPTY if not. */
extern int check_empty(struct dentry *dentry, struct dentry *parent,
		       struct unionfs_dir_state **namelist);
/* whiteout and opaque directory helpers */
extern char *alloc_whname(const char *name, int len);
extern bool is_whiteout_name(char **namep, int *namelenp);
extern bool is_validname(const char *name);
extern struct dentry *lookup_whiteout(const char *name,
				      struct dentry *lower_parent);
extern struct dentry *find_first_whiteout(struct dentry *dentry);
extern int unlink_whiteout(struct dentry *wh_dentry);
extern int check_unlink_whiteout(struct dentry *dentry,
				 struct dentry *lower_dentry, int bindex);
extern int create_whiteout(struct dentry *dentry, int start);
extern int delete_whiteouts(struct dentry *dentry, int bindex,
			    struct unionfs_dir_state *namelist);
extern int is_opaque_dir(struct dentry *dentry, int bindex);
extern int make_dir_opaque(struct dentry *dir, int bindex);
extern void unionfs_set_max_namelen(long *namelen);

extern void unionfs_reinterpose(struct dentry *this_dentry);
extern struct super_block *unionfs_duplicate_super(struct super_block *sb);

/* Locking functions. */
extern int unionfs_setlk(struct file *file, int cmd, struct file_lock *fl);
extern int unionfs_getlk(struct file *file, struct file_lock *fl);

/* Common file operations. */
extern int unionfs_file_revalidate(struct file *file, struct dentry *parent,
				   bool willwrite);
extern int unionfs_open(struct inode *inode, struct file *file);
extern int unionfs_file_release(struct inode *inode, struct file *file);
extern int unionfs_flush(struct file *file, fl_owner_t id);
extern long unionfs_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg);
extern int unionfs_fsync(struct file *file, loff_t start, loff_t end,
			 int datasync);
extern int unionfs_fasync(int fd, struct file *file, int flag);

/* Inode operations */
extern struct inode *unionfs_iget(struct super_block *sb, unsigned long ino);
extern int unionfs_rename(struct inode *old_dir, struct dentry *old_dentry,
			  struct inode *new_dir, struct dentry *new_dentry);
extern int unionfs_unlink(struct inode *dir, struct dentry *dentry);
extern int unionfs_rmdir(struct inode *dir, struct dentry *dentry);

extern bool __unionfs_d_revalidate(struct dentry *dentry,
				   struct dentry *parent, bool willwrite,
				   unsigned int flags);
extern bool is_negative_lower(const struct dentry *dentry);
extern bool is_newer_lower(const struct dentry *dentry);
extern void purge_sb_data(struct super_block *sb);

/* The values for unionfs_interpose's flag. */
#define INTERPOSE_DEFAULT	0
#define INTERPOSE_LOOKUP	1
#define INTERPOSE_REVAL		2
#define INTERPOSE_REVAL_NEG	3
#define INTERPOSE_PARTIAL	4

extern struct dentry *unionfs_interpose(struct dentry *this_dentry,
					struct super_block *sb, int flag);

#ifdef CONFIG_UNION_FS_XATTR
/* Extended attribute functions. */
extern void *unionfs_xattr_alloc(size_t size, size_t limit);
static inline void unionfs_xattr_kfree(const void *p)
{
	kfree(p);
}
extern ssize_t unionfs_getxattr(struct dentry *dentry, const char *name,
				void *value, size_t size);
extern int unionfs_removexattr(struct dentry *dentry, const char *name);
extern ssize_t unionfs_listxattr(struct dentry *dentry, char *list,
				 size_t size);
extern int unionfs_setxattr(struct dentry *dentry, const char *name,
			    const void *value, size_t size, int flags);
#endif /* CONFIG_UNION_FS_XATTR */

/* The root directory is unhashed, but isn't deleted. */
static inline int d_deleted(struct dentry *d)
{
	return d_unhashed(d) && (d != d->d_sb->s_root);
}

/* unionfs_permission, check if we should bypass error to facilitate copyup */
#define IS_COPYUP_ERR(err) ((err) == -EROFS)

/* unionfs_open, check if we need to copyup the file */
#define OPEN_WRITE_FLAGS (O_WRONLY | O_RDWR | O_APPEND)
#define IS_WRITE_FLAG(flag) ((flag) & OPEN_WRITE_FLAGS)

static inline int branchperms(const struct super_block *sb, int index)
{
	BUG_ON(index < 0);
	return UNIONFS_SB(sb)->data[index].branchperms;
}

static inline int set_branchperms(struct super_block *sb, int index, int perms)
{
	BUG_ON(index < 0);
	UNIONFS_SB(sb)->data[index].branchperms = perms;
	return perms;
}

/* check if readonly lower inode, but possibly unlinked (no inode->i_sb) */
static inline int __is_rdonly(const struct inode *inode)
{
	/* if unlinked, can't be readonly (?) */
	if (!inode->i_sb)
		return 0;
	return IS_RDONLY(inode);

}
/* Is this file on a read-only branch? */
static inline int is_robranch_super(const struct super_block *sb, int index)
{
	int ret;

	ret = (!(branchperms(sb, index) & MAY_WRITE)) ? -EROFS : 0;
	return ret;
}

/* Is this file on a read-only branch? */
static inline int is_robranch_idx(const struct dentry *dentry, int index)
{
	struct super_block *lower_sb;

	BUG_ON(index < 0);

	if (!(branchperms(dentry->d_sb, index) & MAY_WRITE))
		return -EROFS;

	lower_sb = unionfs_lower_super_idx(dentry->d_sb, index);
	BUG_ON(lower_sb == NULL);
	/*
	 * test sb flags directly, not IS_RDONLY(lower_inode) because the
	 * lower_dentry could be a negative.
	 */
	if (lower_sb->s_flags & MS_RDONLY)
		return -EROFS;

	return 0;
}

static inline int is_robranch(const struct dentry *dentry)
{
	int index;

	index = UNIONFS_D(dentry)->bstart;
	BUG_ON(index < 0);

	return is_robranch_idx(dentry, index);
}

/*
 * EXTERNALS:
 */
extern int check_branch(const struct path *path);
extern int parse_branch_mode(const char *name, int *perms);

/* locking helpers */
static inline struct dentry *lock_parent(struct dentry *dentry)
{
	struct dentry *dir = dget_parent(dentry);
	mutex_lock_nested(&dir->d_inode->i_mutex, I_MUTEX_PARENT);
	return dir;
}
static inline struct dentry *lock_parent_wh(struct dentry *dentry)
{
	struct dentry *dir = dget_parent(dentry);

	mutex_lock_nested(&dir->d_inode->i_mutex, UNIONFS_DMUTEX_WHITEOUT);
	return dir;
}

static inline void unlock_dir(struct dentry *dir)
{
	mutex_unlock(&dir->d_inode->i_mutex);
	dput(dir);
}

/* lock base inode mutex before calling lookup_one_len */
static inline struct dentry *lookup_lck_len(const char *name,
					    struct dentry *base, int len)
{
	struct dentry *d;

	mutex_lock(&base->d_inode->i_mutex);
	d = lookup_one_len(name, base, len); // XXX: pass flags?
	mutex_unlock(&base->d_inode->i_mutex);

	return d;
}

static inline struct vfsmount *unionfs_mntget(struct dentry *dentry,
					      int bindex)
{
	struct vfsmount *mnt;

	BUG_ON(!dentry || bindex < 0);

	mnt = mntget(unionfs_lower_mnt_idx(dentry, bindex));
#ifdef CONFIG_UNION_FS_DEBUG
	if (!mnt)
		pr_debug("unionfs: mntget: mnt=%p bindex=%d\n",
			 mnt, bindex);
#endif /* CONFIG_UNION_FS_DEBUG */

	return mnt;
}

static inline void unionfs_mntput(struct dentry *dentry, int bindex)
{
	struct vfsmount *mnt;

	if (!dentry && bindex < 0)
		return;
	BUG_ON(!dentry || bindex < 0);

	mnt = unionfs_lower_mnt_idx(dentry, bindex);
#ifdef CONFIG_UNION_FS_DEBUG
	/*
	 * Directories can have NULL lower objects in between start/end, but
	 * NOT if at the start/end range.  We cannot verify that this dentry
	 * is a type=DIR, because it may already be a negative dentry.  But
	 * if dbstart is greater than dbend, we know that this couldn't have
	 * been a regular file: it had to have been a directory.
	 */
	if (!mnt && !(bindex > dbstart(dentry) && bindex < dbend(dentry)))
		pr_debug("unionfs: mntput: mnt=%p bindex=%d\n", mnt, bindex);
#endif /* CONFIG_UNION_FS_DEBUG */
	mntput(mnt);
}

#ifdef CONFIG_UNION_FS_DEBUG

/* useful for tracking code reachability */
#define UDBG pr_debug("DBG:%s:%s:%d\n", __FILE__, __func__, __LINE__)

#define unionfs_check_inode(i)	__unionfs_check_inode((i),	\
	__FILE__, __func__, __LINE__)
#define unionfs_check_dentry(d)	__unionfs_check_dentry((d),	\
	__FILE__, __func__, __LINE__)
#define unionfs_check_file(f)	__unionfs_check_file((f),	\
	__FILE__, __func__, __LINE__)
#define unionfs_check_nd(n)	__unionfs_check_nd((n),		\
	__FILE__, __func__, __LINE__)
#define show_branch_counts(sb)	__show_branch_counts((sb),	\
	__FILE__, __func__, __LINE__)
#define show_inode_times(i)	__show_inode_times((i),		\
	__FILE__, __func__, __LINE__)
#define show_dinode_times(d)	__show_dinode_times((d),	\
	__FILE__, __func__, __LINE__)
#define show_inode_counts(i)	__show_inode_counts((i),	\
	__FILE__, __func__, __LINE__)

extern void __unionfs_check_inode(const struct inode *inode, const char *fname,
				  const char *fxn, int line);
extern void __unionfs_check_dentry(const struct dentry *dentry,
				   const char *fname, const char *fxn,
				   int line);
extern void __unionfs_check_file(const struct file *file,
				 const char *fname, const char *fxn, int line);
extern void __show_branch_counts(const struct super_block *sb,
				 const char *file, const char *fxn, int line);
extern void __show_inode_times(const struct inode *inode,
			       const char *file, const char *fxn, int line);
extern void __show_dinode_times(const struct dentry *dentry,
				const char *file, const char *fxn, int line);
extern void __show_inode_counts(const struct inode *inode,
				const char *file, const char *fxn, int line);

#else /* not CONFIG_UNION_FS_DEBUG */

/* we leave useful hooks for these check functions throughout the code */
#define unionfs_check_inode(i)		do { } while (0)
#define unionfs_check_dentry(d)		do { } while (0)
#define unionfs_check_file(f)		do { } while (0)
#define unionfs_check_nd(n)		do { } while (0)
#define show_branch_counts(sb)		do { } while (0)
#define show_inode_times(i)		do { } while (0)
#define show_dinode_times(d)		do { } while (0)
#define show_inode_counts(i)		do { } while (0)
#define UDBG				do { } while (0)

#endif /* not CONFIG_UNION_FS_DEBUG */

#endif	/* not _UNION_H_ */
