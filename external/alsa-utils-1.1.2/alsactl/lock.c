/*
 *  Advanced Linux Sound Architecture Control Program
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include "aconfig.h"
#include "version.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include "alsactl.h"

static int state_lock_(const char *file, int lock, int timeout, int _fd)
{
	int fd = -1, err = 0;
	struct flock lck;
	struct stat st;
	char lcktxt[12];
	char *nfile = lockfile;

	if (do_lock <= 0)
		return 0;

	lck.l_type = lock ? F_WRLCK : F_UNLCK;
	lck.l_whence = SEEK_SET;
	lck.l_start = 0;
	lck.l_len = 11;
	lck.l_pid = 0;
	if (lock) {
		snprintf(lcktxt, sizeof(lcktxt), "%10li\n", (long)getpid());
	} else {
		snprintf(lcktxt, sizeof(lcktxt), "%10s\n", "");
		fd = _fd;
	}
	while (fd < 0 && timeout-- > 0) {
		fd = open(nfile, O_RDWR);
		if (!lock && fd < 0) {
			err = -EIO;
			goto out;
		}
		if (fd < 0) {
			fd = open(nfile, O_RDWR|O_CREAT|O_EXCL, 0644);
			if (fd < 0) {
				if (errno == EBUSY || errno == EAGAIN) {
					sleep(1);
					timeout--;
				} else {
					err = -errno;
					goto out;
				}
			}
		}
	}
	if (fd < 0 && timeout <= 0) {
		err = -EBUSY;
		goto out;
	}
	if (fstat(fd, &st) < 0) {
		err = -errno;
		goto out;
	}
	if (st.st_size != 11 || !lock) {
		if (write(fd, lcktxt, 11) != 11) {
			err = -EIO;
			goto out;
		}
		if (lock && lseek(fd, 0, SEEK_SET)) {
			err = -errno;
			goto out;
		}
	}
	while (timeout > 0) {
		if (fcntl(fd, F_SETLK, &lck) < 0) {
			sleep(1);
			timeout--;
		} else {
			break;
		}
	}
	if (timeout <= 0) {
		err = -EBUSY;
		goto out;
	}
	if (lock) {
		if (write(fd, lcktxt, 11) != 11) {
			err = -EIO;
			goto out;
		}
		return fd;
	}
	err = 0;

out:
	if (fd >= 0)
		close(fd);
	return err;
}

int state_lock(const char *file, int timeout)
{
	int err;

	err = state_lock_(file, 1, timeout, -1);
	if (err < 0)
		error("file %s lock error: %s", file, strerror(-err));
	return err;
}

int state_unlock(int _fd, const char *file)
{
	int err;

	err = state_lock_(file, 0, 10, _fd);
	if (err < 0)
		error("file %s unlock error: %s", file, strerror(-err));
	return err;
}
