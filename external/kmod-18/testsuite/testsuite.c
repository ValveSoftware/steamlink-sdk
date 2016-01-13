/*
 * Copyright (C) 2012-2013  ProFUSION embedded systems
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <dirent.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "libkmod-util.h"
#include "testsuite.h"

static const char *ANSI_HIGHLIGHT_GREEN_ON = "\x1B[1;32m";
static const char *ANSI_HIGHLIGHT_RED_ON =  "\x1B[1;31m";
static const char *ANSI_HIGHLIGHT_OFF = "\x1B[0m";

static const char *progname;
static int oneshot = 0;
static const char options_short[] = "lhn";
static const struct option options[] = {
	{ "list", no_argument, 0, 'l' },
	{ "help", no_argument, 0, 'h' },
	{ NULL, 0, 0, 0 }
};

#define OVERRIDE_LIBDIR ABS_TOP_BUILDDIR "/testsuite/.libs/"

struct _env_config {
	const char *key;
	const char *ldpreload;
} env_config[_TC_LAST] = {
	[TC_UNAME_R] = { S_TC_UNAME_R, OVERRIDE_LIBDIR  "uname.so" },
	[TC_ROOTFS] = { S_TC_ROOTFS, OVERRIDE_LIBDIR "path.so" },
	[TC_INIT_MODULE_RETCODES] = { S_TC_INIT_MODULE_RETCODES, OVERRIDE_LIBDIR "init_module.so" },
	[TC_DELETE_MODULE_RETCODES] = { S_TC_DELETE_MODULE_RETCODES, OVERRIDE_LIBDIR "delete_module.so" },
};

#define USEC_PER_SEC  1000000ULL
#define USEC_PER_MSEC  1000ULL
#define TEST_TIMEOUT_USEC 2 * USEC_PER_SEC
static unsigned long long now_usec(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return 0;

	return ts_usec(&ts);
}

static void help(void)
{
	const struct option *itr;
	const char *itr_short;

	printf("Usage:\n"
	       "\t%s [options] <test>\n"
	       "Options:\n", basename(progname));

	for (itr = options, itr_short = options_short;
				itr->name != NULL; itr++, itr_short++)
		printf("\t-%c, --%s\n", *itr_short, itr->name);
}

static void test_list(const struct test *tests[])
{
	size_t i;

	printf("Available tests:\n");
	for (i = 0; tests[i] != NULL; i++)
		printf("\t%s, %s\n", tests[i]->name, tests[i]->description);
}

int test_init(int argc, char *const argv[], const struct test *tests[])
{
	progname = argv[0];

	for (;;) {
		int c, idx = 0;
		c = getopt_long(argc, argv, options_short, options, &idx);
		if (c == -1)
			break;
		switch (c) {
		case 'l':
			test_list(tests);
			return 0;
		case 'h':
			help();
			return 0;
		case 'n':
			oneshot = 1;
			break;
		case '?':
			return -1;
		default:
			ERR("unexpected getopt_long() value %c\n", c);
			return -1;
		}
	}

	if (isatty(STDOUT_FILENO) == 0) {
		ANSI_HIGHLIGHT_OFF = "";
		ANSI_HIGHLIGHT_RED_ON = "";
		ANSI_HIGHLIGHT_GREEN_ON = "";
	}

	return optind;
}

const struct test *test_find(const struct test *tests[], const char *name)
{
	size_t i;

	for (i = 0; tests[i] != NULL; i++) {
		if (strcmp(tests[i]->name, name) == 0)
			return tests[i];
	}

	return NULL;
}

static int test_spawn_test(const struct test *t)
{
	const char *const args[] = { progname, "-n", t->name, NULL };

	execv(progname, (char *const *) args);

	ERR("failed to spawn %s for %s: %m\n", progname, t->name);
	return EXIT_FAILURE;
}

static int test_run_spawned(const struct test *t)
{
	int err = t->func(t);
	exit(err);

	return EXIT_FAILURE;
}

int test_spawn_prog(const char *prog, const char *const args[])
{
	execv(prog, (char *const *) args);

	ERR("failed to spawn %s\n", prog);
	ERR("did you forget to build tools?\n");
	return EXIT_FAILURE;
}

static void test_export_environ(const struct test *t)
{
	char *preload = NULL;
	size_t preloadlen = 0;
	size_t i;
	const struct keyval *env;

	unsetenv("LD_PRELOAD");

	for (i = 0; i < _TC_LAST; i++) {
		const char *ldpreload;
		size_t ldpreloadlen;
		char *tmp;

		if (t->config[i] == NULL)
			continue;

		setenv(env_config[i].key, t->config[i], 1);

		ldpreload = env_config[i].ldpreload;
		ldpreloadlen = strlen(ldpreload);
		tmp = realloc(preload, preloadlen + 2 + ldpreloadlen);
		if (tmp == NULL) {
			ERR("oom: test_export_environ()\n");
			return;
		}
		preload = tmp;

		if (preloadlen > 0)
			preload[preloadlen++] = ' ';
		memcpy(preload + preloadlen, ldpreload, ldpreloadlen);
		preloadlen += ldpreloadlen;
		preload[preloadlen] = '\0';
	}

	if (preload != NULL)
		setenv("LD_PRELOAD", preload, 1);

	free(preload);

	for (env = t->env_vars; env && env->key; env++)
		setenv(env->key, env->val, 1);
}

static inline int test_run_child(const struct test *t, int fdout[2],
						int fderr[2], int fdmonitor[2])
{
	/* kill child if parent dies */
	prctl(PR_SET_PDEATHSIG, SIGTERM);

	test_export_environ(t);

	/* Close read-fds and redirect std{out,err} to the write-fds */
	if (t->output.out != NULL) {
		close(fdout[0]);
		if (dup2(fdout[1], STDOUT_FILENO) < 0) {
			ERR("could not redirect stdout to pipe: %m\n");
			exit(EXIT_FAILURE);
		}
	}

	if (t->output.err != NULL) {
		close(fderr[0]);
		if (dup2(fderr[1], STDERR_FILENO) < 0) {
			ERR("could not redirect stderr to pipe: %m\n");
			exit(EXIT_FAILURE);
		}
	}

	close(fdmonitor[0]);

	if (t->config[TC_ROOTFS] != NULL) {
		const char *stamp = TESTSUITE_ROOTFS "../stamp-rootfs";
		const char *rootfs = t->config[TC_ROOTFS];
		struct stat rootfsst, stampst;

		if (stat(stamp, &stampst) != 0) {
			ERR("could not stat %s\n - %m", stamp);
			exit(EXIT_FAILURE);
		}

		if (stat(rootfs, &rootfsst) != 0) {
			ERR("could not stat %s\n - %m", rootfs);
			exit(EXIT_FAILURE);
		}

		if (stat_mstamp(&rootfsst) > stat_mstamp(&stampst)) {
			ERR("rootfs %s is dirty, please run 'make rootfs' before runnning this test\n",
								rootfs);
			exit(EXIT_FAILURE);
		}
	}

	if (t->need_spawn)
		return test_spawn_test(t);
	else
		return test_run_spawned(t);
}

static inline bool test_run_parent_check_outputs(const struct test *t,
			int fdout, int fderr, int fdmonitor, pid_t child)
{
	struct epoll_event ep_outpipe, ep_errpipe, ep_monitor;
	int err, fd_ep, fd_matchout = -1, fd_matcherr = -1;
	unsigned long long end_usec, start_usec;

	fd_ep = epoll_create1(EPOLL_CLOEXEC);
	if (fd_ep < 0) {
		ERR("could not create epoll fd: %m\n");
		return false;
	}

	if (t->output.out != NULL) {
		fd_matchout = open(t->output.out, O_RDONLY);
		if (fd_matchout < 0) {
			err = -errno;
			ERR("could not open %s for read: %m\n",
							t->output.out);
			goto out;
		}
		memset(&ep_outpipe, 0, sizeof(struct epoll_event));
		ep_outpipe.events = EPOLLIN;
		ep_outpipe.data.ptr = &fdout;
		if (epoll_ctl(fd_ep, EPOLL_CTL_ADD, fdout, &ep_outpipe) < 0) {
			err = -errno;
			ERR("could not add fd to epoll: %m\n");
			goto out;
		}
	} else
		fdout = -1;

	if (t->output.err != NULL) {
		fd_matcherr = open(t->output.err, O_RDONLY);
		if (fd_matcherr < 0) {
			err = -errno;
			ERR("could not open %s for read: %m\n",
					t->output.err);
			goto out;

		}
		memset(&ep_errpipe, 0, sizeof(struct epoll_event));
		ep_errpipe.events = EPOLLIN;
		ep_errpipe.data.ptr = &fderr;
		if (epoll_ctl(fd_ep, EPOLL_CTL_ADD, fderr, &ep_errpipe) < 0) {
			err = -errno;
			ERR("could not add fd to epoll: %m\n");
			goto out;
		}
	} else
		fderr = -1;

	memset(&ep_monitor, 0, sizeof(struct epoll_event));
	ep_monitor.events = EPOLLHUP;
	ep_monitor.data.ptr = &fdmonitor;
	if (epoll_ctl(fd_ep, EPOLL_CTL_ADD, fdmonitor, &ep_monitor) < 0) {
		err = -errno;
		ERR("could not add monitor fd to epoll: %m\n");
		goto out;
	}

	start_usec = now_usec();
	end_usec = start_usec + TEST_TIMEOUT_USEC;

	for (err = 0; fdmonitor >= 0 || fdout >= 0 || fderr >= 0;) {
		int fdcount, i, timeout;
		struct epoll_event ev[4];
		unsigned long long curr_usec = now_usec();

		if (curr_usec > end_usec)
			break;

		timeout = (end_usec - curr_usec) / USEC_PER_MSEC;
		fdcount = epoll_wait(fd_ep, ev, 4, timeout);
		if (fdcount < 0) {
			if (errno == EINTR)
				continue;
			err = -errno;
			ERR("could not poll: %m\n");
			goto out;
		}

		for (i = 0;  i < fdcount; i++) {
			int *fd = ev[i].data.ptr;

			if (ev[i].events & EPOLLIN) {
				ssize_t r, done = 0;
				char buf[4096];
				char bufmatch[4096];
				int fd_match;

				/*
				 * compare the output from child with the one
				 * saved as correct
				 */

				r = read(*fd, buf, sizeof(buf) - 1);
				if (r <= 0)
					continue;

				if (*fd == fdout)
					fd_match = fd_matchout;
				else if (*fd == fderr)
					fd_match = fd_matcherr;
				else {
					ERR("Unexpected activity on monitor pipe\n");
					err = -EINVAL;
					goto out;
				}

				for (;;) {
					int rmatch = read(fd_match,
						bufmatch + done, r - done);
					if (rmatch == 0)
						break;

					if (rmatch < 0) {
						if (errno == EINTR)
							continue;
						err = -errno;
						ERR("could not read match fd %d\n",
								fd_match);
						goto out;
					}

					done += rmatch;
				}

				buf[r] = '\0';
				bufmatch[r] = '\0';
				if (strcmp(buf, bufmatch) != 0) {
					ERR("Outputs do not match on %s:\n",
						fd_match == fd_matchout ? "stdout" : "stderr");
					ERR("correct:\n%s\n", bufmatch);
					ERR("wrong:\n%s\n", buf);
					err = -1;
					goto out;
				}
			} else if (ev[i].events & EPOLLHUP) {
				if (epoll_ctl(fd_ep, EPOLL_CTL_DEL,
							*fd, NULL) < 0) {
					ERR("could not remove fd %d from epoll: %m\n",
									*fd);
				}
				*fd = -1;
			}
		}
	}

	if (err == 0 && fdmonitor >= 0) {
		err = -EINVAL;
		ERR("Test '%s' timed out, killing %d\n", t->name, child);
		kill(child, SIGKILL);
	}

out:
	if (fd_matchout >= 0)
		close(fd_matchout);
	if (fd_matcherr >= 0)
		close(fd_matcherr);
	if (fd_ep >= 0)
		close(fd_ep);
	return err == 0;
}

static inline int safe_read(int fd, void *buf, size_t count)
{
	int r;

	while (1) {
		r = read(fd, buf, count);
		if (r == -1 && errno == EINTR)
			continue;
		break;
	}

	return r;
}

static bool check_generated_files(const struct test *t)
{
	const struct keyval *k;

	/* This is not meant to be a diff replacement, just stupidly check if
	 * the files match. Bear in mind they can be binary files */
	for (k = t->output.files; k && k->key; k++) {
		struct stat sta, stb;
		int fda = -1, fdb = -1;
		char bufa[4096];
		char bufb[4096];

		fda = open(k->key, O_RDONLY);
		if (fda < 0) {
			ERR("could not open %s\n - %m\n", k->key);
			goto fail;
		}

		fdb = open(k->val, O_RDONLY);
		if (fdb < 0) {
			ERR("could not open %s\n - %m\n", k->val);
			goto fail;
		}

		if (fstat(fda, &sta) != 0) {
			ERR("could not fstat %d %s\n - %m\n", fda, k->key);
			goto fail;
		}

		if (fstat(fdb, &stb) != 0) {
			ERR("could not fstat %d %s\n - %m\n", fdb, k->key);
			goto fail;
		}

		if (sta.st_size != stb.st_size) {
			ERR("sizes do not match %s %s\n", k->key, k->val);
			goto fail;
		}

		for (;;) {
			int r, done;

			r = safe_read(fda, bufa, sizeof(bufa));
			if (r < 0)
				goto fail;

			if (r == 0)
				/* size is already checked, go to next file */
				goto next;

			for (done = 0; done < r;) {
				int r2 = safe_read(fdb, bufb + done, r - done);

				if (r2 <= 0)
					goto fail;

				done += r2;
			}

			if (memcmp(bufa, bufb, r) != 0)
				goto fail;
		}

next:
		close(fda);
		close(fdb);
		continue;

fail:
		if (fda >= 0)
			close(fda);
		if (fdb >= 0)
			close(fdb);

		return false;
	}

	return true;
}

static int cmp_modnames(const void *m1, const void *m2)
{
	const char *s1 = *(char *const *)m1;
	const char *s2 = *(char *const *)m2;
	int i;

	for (i = 0; s1[i] || s2[i]; i++) {
		char c1 = s1[i], c2 = s2[i];
		if (c1 == '-')
			c1 = '_';
		if (c2 == '-')
			c2 = '_';
		if (c1 != c2)
			return c1 - c2;
	}
	return 0;
}

/*
 * Store the expected module names in buf and return a list of pointers to
 * them.
 */
static const char **read_expected_modules(const struct test *t,
		char **buf, int *count)
{
	const char **res;
	int len;
	int i;
	char *p;

	if (t->modules_loaded[0] == '\0') {
		*count = 0;
		*buf = NULL;
		return NULL;
	}
	*buf = strdup(t->modules_loaded);
	if (!*buf) {
		*count = -1;
		return NULL;
	}
	len = 1;
	for (p = *buf; *p; p++)
		if (*p == ',')
			len++;
	res = malloc(sizeof(char *) * len);
	if (!res) {
		perror("malloc");
		*count = -1;
		free(*buf);
		*buf = NULL;
		return NULL;
	}
	i = 0;
	res[i++] = *buf;
	for (p = *buf; i < len; p++)
		if (*p == ',') {
			*p = '\0';
			res[i++] = p + 1;
		}
	*count = len;
	return res;
}

static char **read_loaded_modules(const struct test *t, char **buf, int *count)
{
	char dirname[PATH_MAX];
	DIR *dir;
	struct dirent *dirent;
	int i;
	int len = 0, bufsz;
	char **res = NULL;
	char *p;
	const char *rootfs = t->config[TC_ROOTFS] ? t->config[TC_ROOTFS] : "";

	/* Store the entries in /sys/module to res */
	if (snprintf(dirname, sizeof(dirname), "%s/sys/module", rootfs)
			>= (int)sizeof(dirname)) {
		ERR("rootfs path too long: %s\n", rootfs);
		*buf = NULL;
		len = -1;
		goto out;
	}
	dir = opendir(dirname);
	/* not an error, simply return empty list */
	if (!dir) {
		*buf = NULL;
		goto out;
	}
	bufsz = 0;
	while ((dirent = readdir(dir))) {
		if (dirent->d_name[0] == '.')
			continue;
		len++;
		bufsz += strlen(dirent->d_name) + 1;
	}
	res = malloc(sizeof(char *) * len);
	if (!res) {
		perror("malloc");
		len = -1;
		goto out_dir;
	}
	*buf = malloc(bufsz);
	if (!*buf) {
		perror("malloc");
		free(res);
		res = NULL;
		len = -1;
		goto out_dir;
	}
	rewinddir(dir);
	i = 0;
	p = *buf;
	while ((dirent = readdir(dir))) {
		int size;

		if (dirent->d_name[0] == '.')
			continue;
		size = strlen(dirent->d_name) + 1;
		memcpy(p, dirent->d_name, size);
		res[i++] = p;
		p += size;
	}
out_dir:
	closedir(dir);
out:
	*count = len;
	return res;
}

static int check_loaded_modules(const struct test *t)
{
	int l1, l2, i1, i2;
	const char **a1;
	char **a2;
	char *buf1, *buf2;
	int err = false;

	a1 = read_expected_modules(t, &buf1, &l1);
	if (l1 < 0)
		return err;
	a2 = read_loaded_modules(t, &buf2, &l2);
	if (l2 < 0)
		goto out_a1;
	qsort(a1, l1, sizeof(char *), cmp_modnames);
	qsort(a2, l2, sizeof(char *), cmp_modnames);
	i1 = i2 = 0;
	err = true;
	while (i1 < l1 || i2 < l2) {
		int cmp;

		if (i1 >= l1)
			cmp = 1;
		else if (i2 >= l2)
			cmp = -1;
		else
			cmp = cmp_modnames(&a1[i1], &a2[i2]);
		if (cmp == 0) {
			i1++;
			i2++;
		} else if (cmp < 0) {
			err = false;
			ERR("module %s not loaded\n", a1[i1]);
			i1++;
		} else  {
			err = false;
			ERR("module %s is loaded but should not be \n", a2[i2]);
			i2++;
		}
	}
	free(a2);
	free(buf2);
out_a1:
	free(a1);
	free(buf1);
	return err;
}

static inline int test_run_parent(const struct test *t, int fdout[2],
				int fderr[2], int fdmonitor[2], pid_t child)
{
	pid_t pid;
	int err;
	bool matchout, match_modules;

	/* Close write-fds */
	if (t->output.out != NULL)
		close(fdout[1]);
	if (t->output.err != NULL)
		close(fderr[1]);
	close(fdmonitor[1]);

	matchout = test_run_parent_check_outputs(t, fdout[0], fderr[0],
							fdmonitor[0], child);

	/*
	 * break pipe on the other end: either child already closed or we want
	 * to stop it
	 */
	if (t->output.out != NULL)
		close(fdout[0]);
	if (t->output.err != NULL)
		close(fderr[0]);
	close(fdmonitor[0]);

	do {
		pid = wait(&err);
		if (pid == -1) {
			ERR("error waitpid(): %m\n");
			err = EXIT_FAILURE;
			goto exit;
		}
	} while (!WIFEXITED(err) && !WIFSIGNALED(err));

	if (WIFEXITED(err)) {
		if (WEXITSTATUS(err) != 0)
			ERR("'%s' [%u] exited with return code %d\n",
					t->name, pid, WEXITSTATUS(err));
		else
			LOG("'%s' [%u] exited with return code %d\n",
					t->name, pid, WEXITSTATUS(err));
	} else if (WIFSIGNALED(err)) {
		ERR("'%s' [%u] terminated by signal %d (%s)\n", t->name, pid,
				WTERMSIG(err), strsignal(WTERMSIG(err)));
		err = t->expected_fail ? EXIT_SUCCESS : EXIT_FAILURE;
		goto exit;
	}

	if (matchout)
		matchout = check_generated_files(t);
	if (t->modules_loaded)
		match_modules = check_loaded_modules(t);
	else
		match_modules = true;

	if (t->expected_fail == false) {
		if (err == 0) {
			if (matchout && match_modules)
				LOG("%sPASSED%s: %s\n",
					ANSI_HIGHLIGHT_GREEN_ON, ANSI_HIGHLIGHT_OFF,
					t->name);
			else {
				ERR("%sFAILED%s: exit ok but %s do not match: %s\n",
					ANSI_HIGHLIGHT_RED_ON, ANSI_HIGHLIGHT_OFF,
					matchout ? "loaded modules" : "outputs",
					t->name);
				err = EXIT_FAILURE;
			}
		} else {
			ERR("%sFAILED%s: %s\n",
					ANSI_HIGHLIGHT_RED_ON, ANSI_HIGHLIGHT_OFF,
					t->name);
		}
	} else {
		if (err == 0) {
			if (matchout) {
				ERR("%sUNEXPECTED PASS%s: exit with 0: %s\n",
					ANSI_HIGHLIGHT_RED_ON, ANSI_HIGHLIGHT_OFF,
					t->name);
				err = EXIT_FAILURE;
			} else {
				ERR("%sUNEXPECTED PASS%s: exit with 0 and outputs do not match: %s\n",
					ANSI_HIGHLIGHT_RED_ON, ANSI_HIGHLIGHT_OFF,
					t->name);
				err = EXIT_FAILURE;
			}
		} else {
			if (matchout) {
				LOG("%sEXPECTED FAIL%s: %s\n",
					ANSI_HIGHLIGHT_GREEN_ON, ANSI_HIGHLIGHT_OFF,
					t->name);
				err = EXIT_SUCCESS;
			} else {
				LOG("%sEXPECTED FAIL%s: exit with %d but outputs do not match: %s\n",
					ANSI_HIGHLIGHT_GREEN_ON, ANSI_HIGHLIGHT_OFF,
					WEXITSTATUS(err), t->name);
				err = EXIT_FAILURE;
			}
		}
	}

exit:
	LOG("------\n");
	return err;
}

static int prepend_path(const char *extra)
{
	char *oldpath, *newpath;
	int r;

	if (extra == NULL)
		return 0;

	oldpath = getenv("PATH");
	if (oldpath == NULL)
		return setenv("PATH", extra, 1);

	if (asprintf(&newpath, "%s:%s", extra, oldpath) < 0) {
		ERR("failed to allocate memory to new PATH\n");
		return -1;
	}

	r = setenv("PATH", newpath, 1);
	free(newpath);

	return r;
}

int test_run(const struct test *t)
{
	pid_t pid;
	int fdout[2];
	int fderr[2];
	int fdmonitor[2];

	if (t->need_spawn && oneshot)
		test_run_spawned(t);

	if (t->output.out != NULL) {
		if (pipe(fdout) != 0) {
			ERR("could not create out pipe for %s\n", t->name);
			return EXIT_FAILURE;
		}
	}

	if (t->output.err != NULL) {
		if (pipe(fderr) != 0) {
			ERR("could not create err pipe for %s\n", t->name);
			return EXIT_FAILURE;
		}
	}

	if (pipe(fdmonitor) != 0) {
		ERR("could not create monitor pipe for %s\n", t->name);
		return EXIT_FAILURE;
	}

	if (prepend_path(t->path) < 0) {
		ERR("failed to prepend '%s' to PATH\n", t->path);
		return EXIT_FAILURE;
	}

	LOG("running %s, in forked context\n", t->name);

	pid = fork();
	if (pid < 0) {
		ERR("could not fork(): %m\n");
		LOG("FAILED: %s\n", t->name);
		return EXIT_FAILURE;
	}

	if (pid > 0)
		return test_run_parent(t, fdout, fderr, fdmonitor, pid);

	return test_run_child(t, fdout, fderr, fdmonitor);
}
