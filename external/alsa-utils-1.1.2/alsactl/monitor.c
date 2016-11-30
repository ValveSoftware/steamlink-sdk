/*
 *  Advanced Linux Sound Architecture Control Program
 *  Copyright (c) by Takashi Iwai <tiwai@suse.de>
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
 */

#include "aconfig.h"
#include "version.h"
#include <stdio.h>
#include <alsa/asoundlib.h>

static int open_ctl(const char *name, snd_ctl_t **ctlp)
{
	snd_ctl_t *ctl;
	int err;

	err = snd_ctl_open(&ctl, name, SND_CTL_READONLY);
	if (err < 0) {
		fprintf(stderr, "Cannot open ctl %s\n", name);
		return err;
	}
	err = snd_ctl_subscribe_events(ctl, 1);
	if (err < 0) {
		fprintf(stderr, "Cannot open subscribe events to ctl %s\n", name);
		snd_ctl_close(ctl);
		return err;
	}
	*ctlp = ctl;
	return 0;
}

static int print_event(int card, snd_ctl_t *ctl)
{
	snd_ctl_event_t *event;
	unsigned int mask;
	int err;

	snd_ctl_event_alloca(&event);
	err = snd_ctl_read(ctl, event);
	if (err < 0)
		return err;

	if (snd_ctl_event_get_type(event) != SND_CTL_EVENT_ELEM)
		return 0;

	if (card >= 0)
		printf("card %d, ", card);
	printf("#%d (%i,%i,%i,%s,%i)",
	       snd_ctl_event_elem_get_numid(event),
	       snd_ctl_event_elem_get_interface(event),
	       snd_ctl_event_elem_get_device(event),
	       snd_ctl_event_elem_get_subdevice(event),
	       snd_ctl_event_elem_get_name(event),
	       snd_ctl_event_elem_get_index(event));

	mask = snd_ctl_event_elem_get_mask(event);
	if (mask == SND_CTL_EVENT_MASK_REMOVE) {
		printf(" REMOVE\n");
		return 0;
	}

	if (mask & SND_CTL_EVENT_MASK_VALUE)
		printf(" VALUE");
	if (mask & SND_CTL_EVENT_MASK_INFO)
		printf(" INFO");
	if (mask & SND_CTL_EVENT_MASK_ADD)
		printf(" ADD");
	if (mask & SND_CTL_EVENT_MASK_TLV)
		printf(" TLV");
	printf("\n");
	return 0;
}

#define MAX_CARDS	256

int monitor(const char *name)
{
	snd_ctl_t *ctls[MAX_CARDS];
	int ncards = 0;
	int show_cards;
	int i, err = 0;

	if (!name) {
		int card = -1;
		while (snd_card_next(&card) >= 0 && card >= 0) {
			char cardname[16];
			if (ncards >= MAX_CARDS) {
				fprintf(stderr, "alsactl: too many cards\n");
				err = -E2BIG;
				goto error;
			}
			sprintf(cardname, "hw:%d", card);
			err = open_ctl(cardname, &ctls[ncards]);
			if (err < 0)
				goto error;
			ncards++;
		}
		show_cards = 1;
	} else {
		err = open_ctl(name, &ctls[0]);
		if (err < 0)
			goto error;
		ncards++;
		show_cards = 0;
	}

	for (;ncards > 0;) {
		struct pollfd fds[ncards];

		for (i = 0; i < ncards; i++)
			snd_ctl_poll_descriptors(ctls[i], &fds[i], 1);

		err = poll(fds, ncards, -1);
		if (err <= 0) {
			err = 0;
			break;
		}

		for (i = 0; i < ncards; i++) {
			unsigned short revents;
			snd_ctl_poll_descriptors_revents(ctls[i], &fds[i], 1,
							 &revents);
			if (revents & POLLIN)
				print_event(show_cards ? i : -1, ctls[i]);
		}
	}

 error:
	for (i = 0; i < ncards; i++)
		snd_ctl_close(ctls[i]);
	return err;
}
