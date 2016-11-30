/*
 *  Advanced Linux Sound Architecture Control Program
 *  Copyright (c) by Abramo Bagnara <abramo@alsa-project.org>
 *                   Jaroslav Kysela <perex@perex.cz>
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
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <syslog.h>
#include <sched.h>
#include <alsa/asoundlib.h>
#include "alsactl.h"

#ifndef SYS_ASOUNDRC
#define SYS_ASOUNDRC "/var/lib/alsa/asound.state"
#endif
#ifndef SYS_PIDFILE
#define SYS_PIDFILE "/var/run/alsactl.pid"
#endif
#ifndef SYS_LOCKPATH
#define SYS_LOCKPATH "/var/lock"
#endif

int debugflag = 0;
int force_restore = 1;
int ignore_nocards = 0;
int do_lock = 0;
int use_syslog = 0;
char *command;
char *statefile = NULL;
char *lockfile = SYS_LOCKFILE;

#define TITLE	0x0100
#define HEADER	0x0200
#define FILEARG 0x0400
#define ENVARG	0x0800
#define INTARG  0x1000
#define EMPCMD	0x2000
#define CARDCMD 0x4000
#define KILLCMD 0x8000

struct arg {
	int sarg;
	char *larg;
	char *comment;
};

static struct arg args[] = {
{ TITLE, NULL, "Usage: alsactl <options> command" },
{ HEADER, NULL, "global options:" },
{ 'h', "help", "this help" },
{ 'd', "debug", "debug mode" },
{ 'v', "version", "print version of this program" },
{ HEADER, NULL, "Available state options:" },
{ FILEARG | 'f', "file", "configuration file (default " SYS_ASOUNDRC ")" },
{ 'l', "lock", "use file locking to serialize concurrent access" },
{ 'L', "no-lock", "do not use file locking to serialize concurrent access" },
{ FILEARG | 'O', "lock-state-file", "state lock file path (default " SYS_LOCKFILE ")" },
{ 'F', "force", "try to restore the matching controls as much as possible" },
{ 0, NULL, "  (default mode)" },
{ 'g', "ignore", "ignore 'No soundcards found' error" },
{ 'P', "pedantic", "do not restore mismatching controls (old default)" },
{ 'I', "no-init-fallback", "" },
{ 0, NULL, "don't initialize even if restore fails" },
{ FILEARG | 'r', "runstate", "save restore and init state to this file (only errors)" },
{ 0, NULL, "  default settings is 'no file set'" },
{ 'R', "remove", "remove runstate file at first, otherwise append errors" },
{ INTARG | 'p', "period", "store period in seconds for the daemon command" },
{ FILEARG | 'e', "pid-file", "pathname for the process id (daemon mode)" },
{ HEADER, NULL, "Available init options:" },
{ ENVARG | 'E', "env", "set environment variable for init phase (NAME=VALUE)" },
{ FILEARG | 'i', "initfile", "main configuation file for init phase" },
{ 0, NULL, "  (default " DATADIR "/init/00main)" },
{ 'b', "background", "run daemon in background" },
{ 's', "syslog", "use syslog for messages" },
{ INTARG | 'n', "nice", "set the process priority (see 'man nice')" },
{ 'c', "sched-idle", "set the process scheduling policy to idle (SCHED_IDLE)" },
{ HEADER, NULL, "Available commands:" },
{ CARDCMD, "store", "save current driver setup for one or each soundcards" },
{ EMPCMD, NULL, "  to configuration file" },
{ CARDCMD, "restore", "load current driver setup for one or each soundcards" },
{ EMPCMD, NULL, "  from configuration file" },
{ CARDCMD, "nrestore", "like restore, but notify the daemon to rescan soundcards" },
{ CARDCMD, "init", "initialize driver to a default state" },
{ CARDCMD, "daemon", "store state periodically for one or each soundcards" },
{ CARDCMD, "rdaemon", "like daemon but do the state restore at first" },
{ KILLCMD, "kill", "notify daemon to quit, rescan or save_and_quit" },
{ CARDCMD, "monitor", "monitor control events" },
{ 0, NULL, NULL }
};

static void help(void)
{
	struct arg *n = args, *a;
	char *larg, sa[4], buf[32];
	int sarg;

	sa[0] = '-';
	sa[2] = ',';
	sa[3] = '\0';
	while (n->comment) {
		a = n;
		n++;
		sarg = a->sarg;
		if (sarg & (HEADER|TITLE)) {
			printf("%s%s\n", (sarg & HEADER) != 0 ? "\n" : "",
								a->comment);
			continue;
		}
		buf[0] = '\0';
		larg = a->larg;
		if (sarg & (EMPCMD|CARDCMD|KILLCMD)) {
			if (sarg & CARDCMD)
				strcat(buf, "<card>");
			else if (sarg & KILLCMD)
				strcat(buf, "<cmd>");
			printf("  %-8s  %-6s  %s\n", larg ? larg : "",
							buf, a->comment);
			continue;
		}
		sa[1] = a->sarg;
		sprintf(buf, "%s%s%s", sa[1] ? sa : "",
				larg ? "--" : "", larg ? larg : "");
		if (sarg & ENVARG)
			strcat(buf, " #=#");
		else if (sarg & (FILEARG|INTARG))
			strcat(buf, " #");
		printf("  %-15s  %s\n", buf, a->comment);
	}
}

#define NO_NICE (-100000)

static void do_nice(int use_nice, int sched_idle)
{
	struct sched_param sched_param;

	if (use_nice != NO_NICE && nice(use_nice) < 0)
		error("nice(%i): %s", use_nice, strerror(errno));
	if (sched_idle) {
		if (sched_getparam(0, &sched_param) >= 0) {
			sched_param.sched_priority = 0;
			if (!sched_setscheduler(0, SCHED_RR, &sched_param))
				error("sched_setparam failed: %s", strerror(errno));
		} else {
			error("sched_getparam failed: %s", strerror(errno));
		}
	}
}

int main(int argc, char *argv[])
{
	static const char *const devfiles[] = {
		"/dev/snd/controlC",
		"/dev/snd/pcmC",
		"/dev/snd/midiC",
		"/dev/snd/hwC",
		NULL
	};
	char *cfgfile = SYS_ASOUNDRC;
	char *initfile = DATADIR "/init/00main";
	char *pidfile = SYS_PIDFILE;
	char *cardname, ncardname[16];
	char *cmd;
	const char *const *tmp;
	int removestate = 0;
	int init_fallback = 1; /* new default behavior */
	int period = 5*60;
	int background = 0;
	int daemoncmd = 0;
	int use_nice = NO_NICE;
	int sched_idle = 0;
	struct arg *a;
	struct option *o;
	int i, j, k, res;
	struct option *long_option;
	char *short_option;

	long_option = calloc(ARRAY_SIZE(args), sizeof(struct option));
	if (long_option == NULL)
		exit(EXIT_FAILURE);
	short_option = malloc(128);
	if (short_option == NULL) {
		free(long_option);
		exit(EXIT_FAILURE);
	}
	for (i = j = k = 0; i < ARRAY_SIZE(args); i++) {
		a = &args[i];
		if ((a->sarg & 0xff) == 0)
			continue;
		o = &long_option[j];
		o->name = a->larg;
		o->has_arg = (a->sarg & (ENVARG|FILEARG|INTARG)) != 0;
		o->flag = NULL;
		o->val = a->sarg & 0xff;
		j++;
		short_option[k++] = o->val;
		if (o->has_arg)
			short_option[k++] = ':';
	}
	short_option[k] = '\0';
	command = argv[0];
	while (1) {
		int c;

		if ((c = getopt_long(argc, argv, short_option, long_option,
								  NULL)) < 0)
			break;
		switch (c) {
		case 'h':
			help();
			res = EXIT_SUCCESS;
			goto out;
		case 'f':
			cfgfile = optarg;
			break;
		case 'l':
			do_lock = 1;
			break;
		case 'L':
			do_lock = -1;
			break;
		case 'O':
			lockfile = optarg;
			break;
		case 'F':
			force_restore = 1;
			break;
		case 'g':
			ignore_nocards = 1;
			break;
		case 'E':
			if (putenv(optarg)) {
				fprintf(stderr, "environment string '%s' is wrong\n", optarg);
				res = EXIT_FAILURE;
				goto out;
			}
			break;
		case 'i':
			initfile = optarg;
			break;
		case 'I':
			init_fallback = 0;
			break;
		case 'r':
			statefile = optarg;
			break;
		case 'R':
			removestate = 1;
			break;
		case 'P':
			force_restore = 0;
			break;
		case 'p':
			period = atoi(optarg);
			if (period < 10)
				period = 5*60;
			else if (period > 24*60*60)
				period = 24*60*60;
			break;
		case 'e':
			pidfile = optarg;
			break;
		case 'b':
			background = 1;
			break;
		case 's':
			use_syslog = 1;
			break;
		case 'n':
			use_nice = atoi(optarg);
			if (use_nice < -20)
				use_nice = -20;
			else if (use_nice > 19)
				use_nice = 19;
			break;
		case 'c':
			sched_idle = 1;
			break;
		case 'd':
			debugflag = 1;
			break;
		case 'v':
			printf("alsactl version " SND_UTIL_VERSION_STR "\n");
			res = EXIT_SUCCESS;
			goto out;
		case '?':		// error msg already printed
			help();
			res = EXIT_FAILURE;
			goto out;
		default:		// should never happen
			fprintf(stderr, 
			"Invalid option '%c' (%d) not handled??\n", c, c);
		}
	}
	free(short_option);
	short_option = NULL;
	free(long_option);
	long_option = NULL;
	if (argc - optind <= 0) {
		fprintf(stderr, "alsactl: Specify command...\n");
		res = 0;
		goto out;
	}

	cardname = argc - optind > 1 ? argv[optind + 1] : NULL;
	for (tmp = devfiles; cardname != NULL && *tmp != NULL; tmp++) {
		int len = strlen(*tmp);
		if (!strncmp(cardname, *tmp, len)) {
			long l = strtol(cardname + len, NULL, 0);
			sprintf(ncardname, "%li", l);
			cardname = ncardname;
			break;
		}
	}

	/* the global system file should be always locked */
	if (strcmp(cfgfile, SYS_ASOUNDRC) == 0 && do_lock >= 0)
		do_lock = 1;

	/* when running in background, use syslog for reports */
	if (background) {
		use_syslog = 1;
		daemon(0, 0);
	}

	cmd = argv[optind];
	daemoncmd = strcmp(cmd, "daemon") == 0 || strcmp(cmd, "rdaemon") == 0;

	if (use_syslog) {
		openlog("alsactl", LOG_CONS|LOG_PID, LOG_DAEMON);
		if (daemoncmd)
			syslog(LOG_INFO, "alsactl " SND_UTIL_VERSION_STR " daemon started");
	}

	if (!strcmp(cmd, "init")) {
		res = init(initfile, cardname);
		snd_config_update_free_global();
	} else if (!strcmp(cmd, "store")) {
		res = save_state(cfgfile, cardname);
	} else if (!strcmp(cmd, "restore") ||
                   !strcmp(cmd, "rdaemon") ||
		   !strcmp(cmd, "nrestore")) {
		if (removestate)
			remove(statefile);
		res = load_state(cfgfile, initfile, cardname, init_fallback);
		if (!strcmp(cmd, "rdaemon")) {
			do_nice(use_nice, sched_idle);
			res = state_daemon(cfgfile, cardname, period, pidfile);
		}
		if (!strcmp(cmd, "nrestore"))
			res = state_daemon_kill(pidfile, "rescan");
	} else if (!strcmp(cmd, "daemon")) {
		do_nice(use_nice, sched_idle);
		res = state_daemon(cfgfile, cardname, period, pidfile);
	} else if (!strcmp(cmd, "kill")) {
		res = state_daemon_kill(pidfile, cardname);
	} else if (!strcmp(cmd, "monitor")) {
		res = monitor(cardname);
	} else {
		fprintf(stderr, "alsactl: Unknown command '%s'...\n", cmd);
		res = -ENODEV;
	}

	snd_config_update_free_global();
	if (use_syslog) {
		if (daemoncmd)
			syslog(LOG_INFO, "alsactl daemon stopped");
		closelog();
	}
	return res < 0 ? -res : 0;

out:
	free(short_option);
	free(long_option);
	return res;
}
