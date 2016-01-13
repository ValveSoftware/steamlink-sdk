UNIONFS_VERSION="2.6 (for 3.8.13)"

EXTRA_CFLAGS += -DUNIONFS_VERSION=\"$(UNIONFS_VERSION)\"

obj-$(CONFIG_UNION_FS) += unionfs.o

unionfs-y := subr.o dentry.o file.o inode.o main.o super.o \
	rdstate.o copyup.o dirhelper.o rename.o unlink.o \
	lookup.o commonfops.o dirfops.o sioq.o mmap.o whiteout.o

unionfs-$(CONFIG_UNION_FS_XATTR) += xattr.o

unionfs-$(CONFIG_UNION_FS_DEBUG) += debug.o

ifeq ($(CONFIG_UNION_FS_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif
