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
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <poll.h>
#include <alsa/asoundlib.h>
#include "alsactl.h"

struct id_list {
	snd_ctl_elem_id_t **list;
	int size;
};

struct card {
	int index;
	int pfds;
	snd_ctl_t *handle;
	struct id_list whitelist;
	struct id_list blacklist;
};

static int quit = 0;
static int rescan = 0;
static int save_now = 0;

static void signal_handler_quit(int sig)
{
	quit = 1;
	signal(sig, signal_handler_quit);
}

static void signal_handler_save_and_quit(int sig)
{
	quit = save_now = 1;
	signal(sig, signal_handler_quit);
}

static void signal_handler_rescan(int sig)
{
	rescan = 1;
	signal(sig, signal_handler_rescan);
}

static void free_list(struct id_list *list)
{
	int i;

	for (i = 0; i < list->size; i++)
		free(list->list[i]);
	free(list->list);
}

static void card_free(struct card **card)
{
	struct card *c = *card;

	free_list(&c->blacklist);
	free_list(&c->whitelist);
	if (c->handle)
		snd_ctl_close(c->handle);
	free(c);
	*card = NULL;
}

static void add_card(struct card ***cards, int *count, const char *cardname)
{
	struct card *card, **cc;
	int i, index, findex;
	char device[16];

	index = snd_card_get_index(cardname);
	if (index < 0)
		return;
	for (i = 0, findex = -1; i < *count; i++) {
		if ((*cards)[i] == NULL) {
			findex = i;
		} else {
			if ((*cards)[i]->index == index)
				return;
		}
	}
	card = calloc(1, sizeof(*card));
	if (card == NULL)
		return;
	card->index = index;
	sprintf(device, "hw:%i", index);
	if (snd_ctl_open(&card->handle, device, SND_CTL_READONLY|SND_CTL_NONBLOCK) < 0) {
		card_free(&card);
		return;
	}
	card->pfds = snd_ctl_poll_descriptors_count(card->handle);
	if (card->pfds < 0) {
		card_free(&card);
		return;
	}
	if (snd_ctl_subscribe_events(card->handle, 1) < 0) {
		card_free(&card);
		return;
	}
	if (findex >= 0) {
		(*cards)[findex] = card;
	} else {
		cc = realloc(*cards, sizeof(void *) * (*count + 1));
		if (cc == NULL) {
			card_free(&card);
			return;
		}
		cc[*count] = card;
		*count = *count + 1;
		*cards = cc;
	}
}

static void add_cards(struct card ***cards, int *count)
{
	int card = -1;
	char cardname[16];

	while (1) {
		if (snd_card_next(&card) < 0)
			break;
		if (card < 0)
			break;
		if (card >= 0) {
			sprintf(cardname, "%i", card);
			add_card(cards, count, cardname);
		}
	}
}

static int compare_ids(snd_ctl_elem_id_t *id1, snd_ctl_elem_id_t *id2)
{
	if (id1 == NULL || id2 == NULL)
		return 0;
	return snd_ctl_elem_id_get_interface(id1) == snd_ctl_elem_id_get_interface(id2) &&
	       snd_ctl_elem_id_get_index(id1) == snd_ctl_elem_id_get_index(id2) &&
	       strcmp(snd_ctl_elem_id_get_name(id1), snd_ctl_elem_id_get_name(id2)) == 0 &&
	       snd_ctl_elem_id_get_device(id1) == snd_ctl_elem_id_get_device(id2) &&
	       snd_ctl_elem_id_get_subdevice(id1) == snd_ctl_elem_id_get_subdevice(id2);
}

static int in_list(struct id_list *list, snd_ctl_elem_id_t *id)
{
	int i;
	snd_ctl_elem_id_t *id1;

	for (i = 0; i < list->size; i++) {
		id1 = list->list[i];
		if (id1 == NULL)
			continue;
		if (compare_ids(id, id1))
			return 1;
	}
	return 0;
}

static void remove_from_list(struct id_list *list, snd_ctl_elem_id_t *id)
{
	int i;

	for (i = 0; i < list->size; i++) {
		if (compare_ids(id, list->list[i])) {
			free(list->list[i]);
			list->list[i] = NULL;
		}
	}
}

static void add_to_list(struct id_list *list, snd_ctl_elem_id_t *id)
{
	snd_ctl_elem_id_t *id1;
	snd_ctl_elem_id_t **n;
	int i;

	if (snd_ctl_elem_id_malloc(&id1))
		return;
	snd_ctl_elem_id_copy(id1, id);
	for (i = 0; i < list->size; i++) {
		if (list->list[i] == NULL) {
			list->list[i] = id1;
			return;
		}
	}
	n = realloc(list->list, sizeof(void *) * (list->size + 1));
	if (n == NULL)
		return;
	n[list->size] = id1;
	list->size++;
	list->list = n;
}

static int check_lists(struct card *card, snd_ctl_elem_id_t *id)
{
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_info_alloca(&info);

	if (in_list(&card->blacklist, id))
		return 0;
	if (in_list(&card->whitelist, id))
		return 1;
	snd_ctl_elem_info_set_id(info, id);
	if (snd_ctl_elem_info(card->handle, info) < 0)
		return 0;
	if (snd_ctl_elem_info_is_writable(info) ||
	    snd_ctl_elem_info_is_tlv_writable(info)) {
		add_to_list(&card->whitelist, id);
		return 1;
	} else {
		add_to_list(&card->blacklist, id);
		return 0;
	}
}

static int card_events(struct card *card)
{
	int res = 0;
	snd_ctl_event_t *ev;
	snd_ctl_event_type_t type;
	unsigned int mask;
	snd_ctl_elem_id_t *id;
	snd_ctl_event_alloca(&ev);
	snd_ctl_elem_id_alloca(&id);

	while (snd_ctl_read(card->handle, ev) == 1) {
		type = snd_ctl_event_get_type(ev);
		if (type != SND_CTL_EVENT_ELEM)
			continue;
		mask = snd_ctl_event_elem_get_mask(ev);
		snd_ctl_event_elem_get_id(ev, id);
		if (mask == SND_CTL_EVENT_MASK_REMOVE) {
			remove_from_list(&card->whitelist, id);
			remove_from_list(&card->blacklist, id);
			continue;
		}
		if (mask & SND_CTL_EVENT_MASK_INFO) {
			remove_from_list(&card->whitelist, id);
			remove_from_list(&card->blacklist, id);
		}
		if (mask & (SND_CTL_EVENT_MASK_VALUE|
			    SND_CTL_EVENT_MASK_ADD|
			    SND_CTL_EVENT_MASK_TLV)) {
			if (check_lists(card, id))
				res = 1;
		}
	}
	return res;
}

static long read_pid_file(const char *pidfile)
{
	int fd, err;
	char pid_txt[12];

	fd = open(pidfile, O_RDONLY);
	if (fd >= 0) {
		err = read(fd, pid_txt, 11);
		if (err != 11)
			err = err < 0 ? -errno : -EIO;
		close(fd);
		pid_txt[11] = '\0';
		return atol(pid_txt);
	} else {
		return -errno;
	}
}

static int write_pid_file(const char *pidfile)
{
	int fd, err;
	char pid_txt[12];

	sprintf(pid_txt, "%10li\n", (long)getpid());
	fd = open(pidfile, O_WRONLY|O_CREAT|O_EXCL, 0600);
	if (fd >= 0) {
		err = write(fd, pid_txt, 11);
		if (err != 11) {
			err = err < 0 ? -errno : -EIO;
			unlink(pidfile);
		} else {
			err = 0;
		}
		close(fd);
	} else {
		err = -errno;
	}
	return err;
}

int state_daemon_kill(const char *pidfile, const char *cmd)
{
	long pid;
	int sig = SIGHUP;

	if (cmd == NULL) {
		error("Specify kill command (quit, rescan or save_and_quit)");
		return -EINVAL;
	}
	if (strcmp(cmd, "rescan") == 0)
		sig = SIGUSR1;
	else if (strcmp(cmd, "save_and_quit") == 0)
		sig = SIGUSR2;
	else if (strcmp(cmd, "quit") == 0)
		sig = SIGTERM;
	if (sig == SIGHUP) {
		error("Unknown kill command '%s'", cmd);
		return -EINVAL;
	}
	pid = read_pid_file(pidfile);
	if (pid > 0) {
		if (kill(pid, sig) >= 0)
			return 0;
		return -errno;
	}
	return 0;
}

static int check_another_instance(const char *pidfile)
{
	long pid;

	pid = read_pid_file(pidfile);
	if (pid >= 0) {
		/* invoke new card rescan */
		if (kill(pid, SIGUSR1) >= 0) {
			usleep(1000);
			pid = read_pid_file(pidfile);
			if (pid >= 0)
				return 1;
		}
	}
	return 0;
}

int state_daemon(const char *file, const char *cardname, int period,
		 const char *pidfile)
{
	int count = 0, pcount, psize = 0, i, j, k, changed = 0;
	time_t last_write, now;
	unsigned short revents;
	struct card **cards = NULL;
	struct pollfd *pfd = NULL, *pfdn;

	if (check_another_instance(pidfile))
		return 0;
	rescan = 1;
	signal(SIGABRT, signal_handler_quit);
	signal(SIGTERM, signal_handler_quit);
	signal(SIGINT, signal_handler_quit);
	signal(SIGUSR1, signal_handler_rescan);
	signal(SIGUSR2, signal_handler_save_and_quit);
	write_pid_file(pidfile);
	time(&last_write);
	while (!quit || save_now) {
		if (save_now)
			goto save;
		if (rescan) {
			if (cardname) {
				add_card(&cards, &count, cardname);
			} else {
				add_cards(&cards, &count);
			}
			snd_config_update_free_global();
			rescan = 0;
		}
		for (i = pcount = 0; i < count; i++) {
			if (cards[i] == NULL)
				continue;
			pcount += cards[i]->pfds;
		}
		if (pcount > psize) {
			pfdn = realloc(pfd, sizeof(struct pollfd) * pcount);
			if (pfdn) {
				psize = pcount;
				pfd = pfdn;
			} else {
				error("No enough memory...");
				goto out;
			}
		}
		for (i = j = 0; i < count; i++) {
			if (cards[i] == NULL)
				continue;
			k = snd_ctl_poll_descriptors(cards[i]->handle, pfd + j, pcount - j);
			if (k != cards[i]->pfds) {
				error("poll prepare failed: %i", k);
				goto out;
			}
			j += k;
		}
		i = poll(pfd, j, (period / 2) * 1000);
		if (i < 0 && errno == EINTR)
			continue;
		if (i < 0) {
			error("poll failed: %s", strerror(errno));
			break;
		}
		time(&now);
		for (i = j = 0; i < count; i++) {
			if (cards[i] == NULL)
				continue;
			k = snd_ctl_poll_descriptors_revents(cards[i]->handle,
					pfd + j, cards[i]->pfds, &revents);
			if (k < 0) {
				error("poll post failed: %i\n", k);
				goto out;
			}
			j += cards[i]->pfds;
			if (revents & (POLLERR|POLLNVAL)) {
				card_free(&cards[i]);
			} else if (revents & POLLIN) {
				if (card_events(cards[i])) {
					/* delay the write */
					if (!changed)
						last_write = now;
					changed = 1;
				}
			}
		}
		if ((now - last_write >= period && changed) || save_now) {
save:
			changed = save_now = 0;
			save_state(file, cardname);
		}
	}
out:
	free(pfd);
	remove(pidfile);
	for (i = 0; i < count; i++)
		card_free(&cards[i]);
	free(cards);
	return 0;
}
