/*
 * Copyright (c) 2003-2013 Erez Zadok
 * Copyright (c) 2005-2007 Josef 'Jeff' Sipek
 * Copyright (c) 2003-2013 Stony Brook University
 * Copyright (c) 2003-2013 The Research Foundation of SUNY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_UNION_FS_H
#define _LINUX_UNION_FS_H

/*
 * DEFINITIONS FOR USER AND KERNEL CODE:
 */
# define UNIONFS_IOCTL_INCGEN		_IOR(0x15, 11, int)
# define UNIONFS_IOCTL_QUERYFILE	_IOR(0x15, 15, int)

#endif /* _LINUX_UNIONFS_H */

