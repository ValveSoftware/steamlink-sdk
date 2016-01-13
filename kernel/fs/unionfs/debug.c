/*
 * Copyright (c) 2003-2014 Erez Zadok
 * Copyright (c) 2005-2007 Josef 'Jeff' Sipek
 * Copyright (c) 2003-2014 Stony Brook University
 * Copyright (c) 2003-2014 The Research Foundation of SUNY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "union.h"
#include "../mount.h"

/*
 * Helper debugging functions for maintainers (and for users to report back
 * useful information back to maintainers)
 */

/* it's always useful to know what part of the code called us */
#define PRINT_CALLER(fname, fxn, line)					\
	do {								\
		if (!printed_caller) {					\
			pr_debug("PC:%s:%s:%d\n", (fname), (fxn), (line)); \
			printed_caller = 1;				\
		}							\
	} while (0)

/*
 * __unionfs_check_{inode,dentry,file} perform exhaustive sanity checking on
 * the fan-out of various Unionfs objects.  We check that no lower objects
 * exist  outside the start/end branch range; that all objects within are
 * non-NULL (with some allowed exceptions); that for every lower file
 * there's a lower dentry+inode; that the start/end ranges match for all
 * corresponding lower objects; that open files/symlinks have only one lower
 * objects, but directories can have several; and more.
 */
void __unionfs_check_inode(const struct inode *inode,
			   const char *fname, const char *fxn, int line)
{
	int bindex;
	int istart, iend;
	struct inode *lower_inode;
	struct super_block *sb;
	int printed_caller = 0;
	void *poison_ptr;

	/* for inodes now */
	BUG_ON(!inode);
	sb = inode->i_sb;
	istart = ibstart(inode);
	iend = ibend(inode);
	/* don't check inode if no lower branches */
	if (istart < 0 && iend < 0)
		return;
	if (unlikely(istart > iend)) {
		PRINT_CALLER(fname, fxn, line);
		pr_debug(" Ci0: inode=%p istart/end=%d:%d\n",
			 inode, istart, iend);
	}
	if (unlikely((istart == -1 && iend != -1) ||
		     (istart != -1 && iend == -1))) {
		PRINT_CALLER(fname, fxn, line);
		pr_debug(" Ci1: inode=%p istart/end=%d:%d\n",
			 inode, istart, iend);
	}
	if (!S_ISDIR(inode->i_mode)) {
		if (unlikely(iend != istart)) {
			PRINT_CALLER(fname, fxn, line);
			pr_debug(" Ci2: inode=%p istart=%d iend=%d\n",
				 inode, istart, iend);
		}
	}

	for (bindex = sbstart(sb); bindex < sbmax(sb); bindex++) {
		if (unlikely(!UNIONFS_I(inode))) {
			PRINT_CALLER(fname, fxn, line);
			pr_debug(" Ci3: no inode_info %p\n", inode);
			return;
		}
		if (unlikely(!UNIONFS_I(inode)->lower_inodes)) {
			PRINT_CALLER(fname, fxn, line);
			pr_debug(" Ci4: no lower_inodes %p\n", inode);
			return;
		}
		lower_inode = unionfs_lower_inode_idx(inode, bindex);
		if (lower_inode) {
			memset(&poison_ptr, POISON_INUSE, sizeof(void *));
			if (unlikely(bindex < istart || bindex > iend)) {
				PRINT_CALLER(fname, fxn, line);
				pr_debug(" Ci5: inode/linode=%p:%p bindex=%d "
					 "istart/end=%d:%d\n", inode,
					 lower_inode, bindex, istart, iend);
			} else if (unlikely(lower_inode == poison_ptr)) {
				/* freed inode! */
				PRINT_CALLER(fname, fxn, line);
				pr_debug(" Ci6: inode/linode=%p:%p bindex=%d "
					 "istart/end=%d:%d\n", inode,
					 lower_inode, bindex, istart, iend);
			}
			continue;
		}
		/* if we get here, then lower_inode == NULL */
		if (bindex < istart || bindex > iend)
			continue;
		/*
		 * directories can have NULL lower inodes in b/t start/end,
		 * but NOT if at the start/end range.
		 */
		if (unlikely(S_ISDIR(inode->i_mode) &&
			     bindex > istart && bindex < iend))
			continue;
		PRINT_CALLER(fname, fxn, line);
		pr_debug(" Ci7: inode/linode=%p:%p "
			 "bindex=%d istart/end=%d:%d\n",
			 inode, lower_inode, bindex, istart, iend);
	}
}

void __unionfs_check_dentry(const struct dentry *dentry,
			    const char *fname, const char *fxn, int line)
{
	int bindex;
	int dstart, dend, istart, iend;
	struct dentry *lower_dentry;
	struct inode *inode, *lower_inode;
	struct super_block *sb;
	struct vfsmount *lower_mnt;
	int printed_caller = 0;
	void *poison_ptr;

	BUG_ON(!dentry);
	sb = dentry->d_sb;
	inode = dentry->d_inode;
	dstart = dbstart(dentry);
	dend = dbend(dentry);
	/* don't check dentry/mnt if no lower branches */
	if (dstart < 0 && dend < 0)
		goto check_inode;
	BUG_ON(dstart > dend);

	if (unlikely((dstart == -1 && dend != -1) ||
		     (dstart != -1 && dend == -1))) {
		PRINT_CALLER(fname, fxn, line);
		pr_debug(" CD0: dentry=%p dstart/end=%d:%d\n",
			 dentry, dstart, dend);
	}
	/*
	 * check for NULL dentries inside the start/end range, or
	 * non-NULL dentries outside the start/end range.
	 */
	for (bindex = sbstart(sb); bindex < sbmax(sb); bindex++) {
		lower_dentry = unionfs_lower_dentry_idx(dentry, bindex);
		if (lower_dentry) {
			if (unlikely(bindex < dstart || bindex > dend)) {
				PRINT_CALLER(fname, fxn, line);
				pr_debug(" CD1: dentry/lower=%p:%p(%p) "
					 "bindex=%d dstart/end=%d:%d\n",
					 dentry, lower_dentry,
					 (lower_dentry ? lower_dentry->d_inode :
					  (void *) -1L),
					 bindex, dstart, dend);
			}
		} else {	/* lower_dentry == NULL */
			if (bindex < dstart || bindex > dend)
				continue;
			/*
			 * Directories can have NULL lower inodes in b/t
			 * start/end, but NOT if at the start/end range.
			 * Ignore this rule, however, if this is a NULL
			 * dentry or a deleted dentry.
			 */
			if (unlikely(!d_deleted((struct dentry *) dentry) &&
				     inode &&
				     !(inode && S_ISDIR(inode->i_mode) &&
				       bindex > dstart && bindex < dend))) {
				PRINT_CALLER(fname, fxn, line);
				pr_debug(" CD2: dentry/lower=%p:%p(%p) "
					 "bindex=%d dstart/end=%d:%d\n",
					 dentry, lower_dentry,
					 (lower_dentry ?
					  lower_dentry->d_inode :
					  (void *) -1L),
					 bindex, dstart, dend);
			}
		}
	}

	/* check for vfsmounts same as for dentries */
	for (bindex = sbstart(sb); bindex < sbmax(sb); bindex++) {
		lower_mnt = unionfs_lower_mnt_idx(dentry, bindex);
		if (lower_mnt) {
			if (unlikely(bindex < dstart || bindex > dend)) {
				PRINT_CALLER(fname, fxn, line);
				pr_debug(" CM0: dentry/lmnt=%p:%p bindex=%d "
					 "dstart/end=%d:%d\n", dentry,
					 lower_mnt, bindex, dstart, dend);
			}
		} else {	/* lower_mnt == NULL */
			if (bindex < dstart || bindex > dend)
				continue;
			/*
			 * Directories can have NULL lower inodes in b/t
			 * start/end, but NOT if at the start/end range.
			 * Ignore this rule, however, if this is a NULL
			 * dentry.
			 */
			if (unlikely(inode &&
				     !(inode && S_ISDIR(inode->i_mode) &&
				       bindex > dstart && bindex < dend))) {
				PRINT_CALLER(fname, fxn, line);
				pr_debug(" CM1: dentry/lmnt=%p:%p "
					 "bindex=%d dstart/end=%d:%d\n",
					 dentry, lower_mnt, bindex,
					 dstart, dend);
			}
		}
	}

check_inode:
	/* for inodes now */
	if (!inode)
		return;
	istart = ibstart(inode);
	iend = ibend(inode);
	/* don't check inode if no lower branches */
	if (istart < 0 && iend < 0)
		return;
	BUG_ON(istart > iend);
	if (unlikely((istart == -1 && iend != -1) ||
		     (istart != -1 && iend == -1))) {
		PRINT_CALLER(fname, fxn, line);
		pr_debug(" CI0: dentry/inode=%p:%p istart/end=%d:%d\n",
			 dentry, inode, istart, iend);
	}
	if (unlikely(istart != dstart)) {
		PRINT_CALLER(fname, fxn, line);
		pr_debug(" CI1: dentry/inode=%p:%p istart=%d dstart=%d\n",
			 dentry, inode, istart, dstart);
	}
	if (unlikely(iend != dend)) {
		PRINT_CALLER(fname, fxn, line);
		pr_debug(" CI2: dentry/inode=%p:%p iend=%d dend=%d\n",
			 dentry, inode, iend, dend);
	}

	if (!S_ISDIR(inode->i_mode)) {
		if (unlikely(dend != dstart)) {
			PRINT_CALLER(fname, fxn, line);
			pr_debug(" CI3: dentry/inode=%p:%p dstart=%d dend=%d\n",
				 dentry, inode, dstart, dend);
		}
		if (unlikely(iend != istart)) {
			PRINT_CALLER(fname, fxn, line);
			pr_debug(" CI4: dentry/inode=%p:%p istart=%d iend=%d\n",
				 dentry, inode, istart, iend);
		}
	}

	for (bindex = sbstart(sb); bindex < sbmax(sb); bindex++) {
		lower_inode = unionfs_lower_inode_idx(inode, bindex);
		if (lower_inode) {
			memset(&poison_ptr, POISON_INUSE, sizeof(void *));
			if (unlikely(bindex < istart || bindex > iend)) {
				PRINT_CALLER(fname, fxn, line);
				pr_debug(" CI5: dentry/linode=%p:%p bindex=%d "
					 "istart/end=%d:%d\n", dentry,
					 lower_inode, bindex, istart, iend);
			} else if (unlikely(lower_inode == poison_ptr)) {
				/* freed inode! */
				PRINT_CALLER(fname, fxn, line);
				pr_debug(" CI6: dentry/linode=%p:%p bindex=%d "
					 "istart/end=%d:%d\n", dentry,
					 lower_inode, bindex, istart, iend);
			}
			continue;
		}
		/* if we get here, then lower_inode == NULL */
		if (bindex < istart || bindex > iend)
			continue;
		/*
		 * directories can have NULL lower inodes in b/t start/end,
		 * but NOT if at the start/end range.
		 */
		if (unlikely(S_ISDIR(inode->i_mode) &&
			     bindex > istart && bindex < iend))
			continue;
		PRINT_CALLER(fname, fxn, line);
		pr_debug(" CI7: dentry/linode=%p:%p "
			 "bindex=%d istart/end=%d:%d\n",
			 dentry, lower_inode, bindex, istart, iend);
	}

	/*
	 * If it's a directory, then intermediate objects b/t start/end can
	 * be NULL.  But, check that all three are NULL: lower dentry, mnt,
	 * and inode.
	 */
	if (dstart >= 0 && dend >= 0 && S_ISDIR(inode->i_mode))
		for (bindex = dstart+1; bindex < dend; bindex++) {
			lower_inode = unionfs_lower_inode_idx(inode, bindex);
			lower_dentry = unionfs_lower_dentry_idx(dentry,
								bindex);
			lower_mnt = unionfs_lower_mnt_idx(dentry, bindex);
			if (unlikely(!((lower_inode && lower_dentry &&
					lower_mnt) ||
				       (!lower_inode &&
					!lower_dentry && !lower_mnt)))) {
				PRINT_CALLER(fname, fxn, line);
				pr_debug(" Cx: lmnt/ldentry/linode=%p:%p:%p "
					 "bindex=%d dstart/end=%d:%d\n",
					 lower_mnt, lower_dentry, lower_inode,
					 bindex, dstart, dend);
			}
		}
	/* check if lower inode is newer than upper one (it shouldn't) */
	if (unlikely(is_newer_lower(dentry) && !is_negative_lower(dentry))) {
		PRINT_CALLER(fname, fxn, line);
		for (bindex = ibstart(inode); bindex <= ibend(inode);
		     bindex++) {
			lower_inode = unionfs_lower_inode_idx(inode, bindex);
			if (unlikely(!lower_inode))
				continue;
			pr_debug(" CI8: bindex=%d mtime/lmtime=%lu.%lu/%lu.%lu "
				 "ctime/lctime=%lu.%lu/%lu.%lu\n",
				 bindex,
				 inode->i_mtime.tv_sec,
				 inode->i_mtime.tv_nsec,
				 lower_inode->i_mtime.tv_sec,
				 lower_inode->i_mtime.tv_nsec,
				 inode->i_ctime.tv_sec,
				 inode->i_ctime.tv_nsec,
				 lower_inode->i_ctime.tv_sec,
				 lower_inode->i_ctime.tv_nsec);
		}
	}
}

void __unionfs_check_file(const struct file *file,
			  const char *fname, const char *fxn, int line)
{
	int bindex;
	int dstart, dend, fstart, fend;
	struct dentry *dentry;
	struct file *lower_file;
	struct inode *inode;
	struct super_block *sb;
	int printed_caller = 0;

	BUG_ON(!file);
	dentry = file->f_path.dentry;
	sb = dentry->d_sb;
	dstart = dbstart(dentry);
	dend = dbend(dentry);
	BUG_ON(dstart > dend);
	fstart = fbstart(file);
	fend = fbend(file);
	BUG_ON(fstart > fend);

	if (unlikely((fstart == -1 && fend != -1) ||
		     (fstart != -1 && fend == -1))) {
		PRINT_CALLER(fname, fxn, line);
		pr_debug(" CF0: file/dentry=%p:%p fstart/end=%d:%d\n",
			 file, dentry, fstart, fend);
	}
	/* d_deleted dentries can be ignored for this test */
	if (unlikely(fstart != dstart) && !d_deleted(dentry)) {
		PRINT_CALLER(fname, fxn, line);
		pr_debug(" CF1: file/dentry=%p:%p fstart=%d dstart=%d\n",
			 file, dentry, fstart, dstart);
	}
	if (unlikely(fend != dend) && !d_deleted(dentry)) {
		PRINT_CALLER(fname, fxn, line);
		pr_debug(" CF2: file/dentry=%p:%p fend=%d dend=%d\n",
			 file, dentry, fend, dend);
	}
	inode = dentry->d_inode;
	if (!S_ISDIR(inode->i_mode)) {
		if (unlikely(fend != fstart)) {
			PRINT_CALLER(fname, fxn, line);
			pr_debug(" CF3: file/inode=%p:%p fstart=%d fend=%d\n",
				 file, inode, fstart, fend);
		}
		if (unlikely(dend != dstart)) {
			PRINT_CALLER(fname, fxn, line);
			pr_debug(" CF4: file/dentry=%p:%p dstart=%d dend=%d\n",
				 file, dentry, dstart, dend);
		}
	}

	/*
	 * check for NULL dentries inside the start/end range, or
	 * non-NULL dentries outside the start/end range.
	 */
	for (bindex = sbstart(sb); bindex < sbmax(sb); bindex++) {
		lower_file = unionfs_lower_file_idx(file, bindex);
		if (lower_file) {
			if (unlikely(bindex < fstart || bindex > fend)) {
				PRINT_CALLER(fname, fxn, line);
				pr_debug(" CF5: file/lower=%p:%p bindex=%d "
					 "fstart/end=%d:%d\n", file,
					 lower_file, bindex, fstart, fend);
			}
		} else {	/* lower_file == NULL */
			if (bindex >= fstart && bindex <= fend) {
				/*
				 * directories can have NULL lower inodes in
				 * b/t start/end, but NOT if at the
				 * start/end range.
				 */
				if (unlikely(!(S_ISDIR(inode->i_mode) &&
					       bindex > fstart &&
					       bindex < fend))) {
					PRINT_CALLER(fname, fxn, line);
					pr_debug(" CF6: file/lower=%p:%p "
						 "bindex=%d fstart/end=%d:%d\n",
						 file, lower_file, bindex,
						 fstart, fend);
				}
			}
		}
	}

	__unionfs_check_dentry(dentry, fname, fxn, line);
}

static unsigned int __mnt_get_count(struct vfsmount *mnt)
{
	struct mount *m = real_mount(mnt);
#ifdef CONFIG_SMP
	unsigned int count = 0;
	int cpu;

	for_each_possible_cpu(cpu) {
		count += per_cpu_ptr(m->mnt_pcp, cpu)->mnt_count;
	}

	return count;
#else
	return m->mnt_count;
#endif
}

/* useful to track vfsmount leaks that could cause EBUSY on unmount */
void __show_branch_counts(const struct super_block *sb,
			  const char *file, const char *fxn, int line)
{
	int i;
	struct vfsmount *mnt;

	pr_debug("BC:");
	for (i = 0; i < sbmax(sb); i++) {
		if (likely(sb->s_root))
			mnt = UNIONFS_D(sb->s_root)->lower_paths[i].mnt;
		else
			mnt = NULL;
		printk(KERN_CONT "%d:",
		       (mnt ? __mnt_get_count(mnt) : -99));
	}
	printk(KERN_CONT "%s:%s:%d\n", file, fxn, line);
}

void __show_inode_times(const struct inode *inode,
			const char *file, const char *fxn, int line)
{
	struct inode *lower_inode;
	int bindex;

	for (bindex = ibstart(inode); bindex <= ibend(inode); bindex++) {
		lower_inode = unionfs_lower_inode_idx(inode, bindex);
		if (unlikely(!lower_inode))
			continue;
		pr_debug("IT(%lu:%d): %s:%s:%d "
			 "um=%lu/%lu lm=%lu/%lu uc=%lu/%lu lc=%lu/%lu\n",
			 inode->i_ino, bindex,
			 file, fxn, line,
			 inode->i_mtime.tv_sec, inode->i_mtime.tv_nsec,
			 lower_inode->i_mtime.tv_sec,
			 lower_inode->i_mtime.tv_nsec,
			 inode->i_ctime.tv_sec, inode->i_ctime.tv_nsec,
			 lower_inode->i_ctime.tv_sec,
			 lower_inode->i_ctime.tv_nsec);
	}
}

void __show_dinode_times(const struct dentry *dentry,
			const char *file, const char *fxn, int line)
{
	struct inode *inode = dentry->d_inode;
	struct inode *lower_inode;
	int bindex;

	for (bindex = ibstart(inode); bindex <= ibend(inode); bindex++) {
		lower_inode = unionfs_lower_inode_idx(inode, bindex);
		if (!lower_inode)
			continue;
		pr_debug("DT(%s:%lu:%d): %s:%s:%d "
			 "um=%lu/%lu lm=%lu/%lu uc=%lu/%lu lc=%lu/%lu\n",
			 dentry->d_name.name, inode->i_ino, bindex,
			 file, fxn, line,
			 inode->i_mtime.tv_sec, inode->i_mtime.tv_nsec,
			 lower_inode->i_mtime.tv_sec,
			 lower_inode->i_mtime.tv_nsec,
			 inode->i_ctime.tv_sec, inode->i_ctime.tv_nsec,
			 lower_inode->i_ctime.tv_sec,
			 lower_inode->i_ctime.tv_nsec);
	}
}

void __show_inode_counts(const struct inode *inode,
			const char *file, const char *fxn, int line)
{
	struct inode *lower_inode;
	int bindex;

	if (unlikely(!inode)) {
		pr_debug("SiC: Null inode\n");
		return;
	}
	for (bindex = sbstart(inode->i_sb); bindex <= sbend(inode->i_sb);
	     bindex++) {
		lower_inode = unionfs_lower_inode_idx(inode, bindex);
		if (unlikely(!lower_inode))
			continue;
		pr_debug("SIC(%lu:%d:%d): lc=%d %s:%s:%d\n",
			 inode->i_ino, bindex,
			 atomic_read(&(inode)->i_count),
			 atomic_read(&(lower_inode)->i_count),
			 file, fxn, line);
	}
}
