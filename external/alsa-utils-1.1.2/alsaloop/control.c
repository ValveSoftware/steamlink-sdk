/*
 *  A simple PCM loopback utility
 *  Copyright (c) 2010 by Jaroslav Kysela <perex@perex.cz>
 *
 *     Author: Jaroslav Kysela <perex@perex.cz>
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

#include <ctype.h>
#include <syslog.h>
#include <alsa/asoundlib.h>
#include "alsaloop.h"

static char *id_str(snd_ctl_elem_id_t *id)
{
	static char str[128];

	sprintf(str, "%i,%s,%i,%i,%s,%i",
		snd_ctl_elem_id_get_numid(id),
		snd_ctl_elem_iface_name(snd_ctl_elem_id_get_interface(id)),
		snd_ctl_elem_id_get_device(id),
		snd_ctl_elem_id_get_subdevice(id),
		snd_ctl_elem_id_get_name(id),
		snd_ctl_elem_id_get_index(id));
	return str;
}

int control_parse_id(const char *str, snd_ctl_elem_id_t *id)
{
	int c, size, numid;
	char *ptr;

	while (*str == ' ' || *str == '\t')
		str++;
	if (!(*str))
		return -EINVAL;
	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);	/* default */
	while (*str) {
		if (!strncasecmp(str, "numid=", 6)) {
			str += 6;
			numid = atoi(str);
			if (numid <= 0) {
				logit(LOG_CRIT, "Invalid numid %d\n", numid);
				return -EINVAL;
			}
			snd_ctl_elem_id_set_numid(id, atoi(str));
			while (isdigit(*str))
				str++;
		} else if (!strncasecmp(str, "iface=", 6)) {
			str += 6;
			if (!strncasecmp(str, "card", 4)) {
				snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_CARD);
				str += 4;
			} else if (!strncasecmp(str, "mixer", 5)) {
				snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
				str += 5;
			} else if (!strncasecmp(str, "pcm", 3)) {
				snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_PCM);
				str += 3;
			} else if (!strncasecmp(str, "rawmidi", 7)) {
				snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_RAWMIDI);
				str += 7;
			} else if (!strncasecmp(str, "timer", 5)) {
				snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_TIMER);
				str += 5;
			} else if (!strncasecmp(str, "sequencer", 9)) {
				snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_SEQUENCER);
				str += 9;
			} else {
				return -EINVAL;
			}
		} else if (!strncasecmp(str, "name=", 5)) {
			char buf[64];
			str += 5;
			ptr = buf;
			size = 0;
			if (*str == '\'' || *str == '\"') {
				c = *str++;
				while (*str && *str != c) {
					if (size < (int)sizeof(buf)) {
						*ptr++ = *str;
						size++;
					}
					str++;
				}
				if (*str == c)
					str++;
			} else {
				while (*str && *str != ',') {
					if (size < (int)sizeof(buf)) {
						*ptr++ = *str;
						size++;
					}
					str++;
				}
			}
			*ptr = '\0';
			snd_ctl_elem_id_set_name(id, buf);
		} else if (!strncasecmp(str, "index=", 6)) {
			str += 6;
			snd_ctl_elem_id_set_index(id, atoi(str));
			while (isdigit(*str))
				str++;
		} else if (!strncasecmp(str, "device=", 7)) {
			str += 7;
			snd_ctl_elem_id_set_device(id, atoi(str));
			while (isdigit(*str))
				str++;
		} else if (!strncasecmp(str, "subdevice=", 10)) {
			str += 10;
			snd_ctl_elem_id_set_subdevice(id, atoi(str));
			while (isdigit(*str))
				str++;
		}
		if (*str == ',') {
			str++;
		} else {
			if (*str)
				return -EINVAL;
		}
	}
	return 0;
}

int control_id_match(snd_ctl_elem_id_t *id1, snd_ctl_elem_id_t *id2)
{
	if (snd_ctl_elem_id_get_interface(id1) !=
	    snd_ctl_elem_id_get_interface(id2))
		return 0;
	if (snd_ctl_elem_id_get_device(id1) !=
	    snd_ctl_elem_id_get_device(id2))
		return 0;
	if (snd_ctl_elem_id_get_subdevice(id1) !=
	    snd_ctl_elem_id_get_subdevice(id2))
		return 0;
	if (strcmp(snd_ctl_elem_id_get_name(id1),
		   snd_ctl_elem_id_get_name(id2)) != 0)
		return 0;
	if (snd_ctl_elem_id_get_index(id1) !=
	    snd_ctl_elem_id_get_index(id2))
		return 0;
	return 1;
}

static int control_init1(struct loopback_handle *lhandle,
			 struct loopback_control *ctl)
{
	int err;

	snd_ctl_elem_info_set_id(ctl->info, ctl->id);
	snd_ctl_elem_value_set_id(ctl->value, ctl->id);
	if (lhandle->ctl == NULL) {
		logit(LOG_WARNING, "Unable to read control info for '%s'\n", id_str(ctl->id));
		return -EIO;
	}
	err = snd_ctl_elem_info(lhandle->ctl, ctl->info);
	if (err < 0) {
		logit(LOG_WARNING, "Unable to read control info '%s': %s\n", id_str(ctl->id), snd_strerror(err));
		return err;
	}
	err = snd_ctl_elem_read(lhandle->ctl, ctl->value);
	if (err < 0) {
		logit(LOG_WARNING, "Unable to read control value (init1) '%s': %s\n", id_str(ctl->id), snd_strerror(err));
		return err;
	}
	return 0;
}

static int copy_value(struct loopback_control *dst,
		      struct loopback_control *src)
{
	snd_ctl_elem_type_t type;
	unsigned int count;
	int i;

	type = snd_ctl_elem_info_get_type(dst->info);
	count = snd_ctl_elem_info_get_count(dst->info);
	switch (type) {
	case SND_CTL_ELEM_TYPE_BOOLEAN:
		for (i = 0; i < count; i++)
			snd_ctl_elem_value_set_boolean(dst->value,
				i, snd_ctl_elem_value_get_boolean(src->value, i));
		break;
	case SND_CTL_ELEM_TYPE_INTEGER:
		for (i = 0; i < count; i++) {
			snd_ctl_elem_value_set_integer(dst->value,
				i, snd_ctl_elem_value_get_integer(src->value, i));
		}
		break;
	default:
		logit(LOG_CRIT, "Unable to copy control value for type %s\n", snd_ctl_elem_type_name(type));
		return -EINVAL;
	}
	return 0;
}

static int oss_set(struct loopback *loop,
		   struct loopback_ossmixer *ossmix,
		   int enable)
{
	char buf[128], file[128];
	int fd;

	if (loop->capt->card_number < 0)
		return 0;
	if (!enable) {
		sprintf(buf, "%s \"\" 0\n", ossmix->oss_id);
	} else {
		sprintf(buf, "%s \"%s\" %i\n", ossmix->oss_id, ossmix->alsa_id, ossmix->alsa_index);
	}
	sprintf(file, "/proc/asound/card%i/oss_mixer", loop->capt->card_number);
	if (verbose)
		snd_output_printf(loop->output, "%s: Initialize OSS volume %s: %s", loop->id, file, buf);
	fd = open(file, O_WRONLY);
	if (fd >= 0 && write(fd, buf, strlen(buf)) == strlen(buf)) {
		close(fd);
		return 0;
	}
	if (fd >= 0)
		close(fd);
	logit(LOG_INFO, "%s: Unable to initialize OSS Mixer ID '%s'\n", loop->id, ossmix->oss_id);
	return -1;
}

static int control_init2(struct loopback *loop,
			 struct loopback_mixer *mix)
{
	snd_ctl_elem_type_t type;
	unsigned int count;
	int err;

	snd_ctl_elem_info_copy(mix->dst.info, mix->src.info);
	snd_ctl_elem_info_set_id(mix->dst.info, mix->dst.id);
	snd_ctl_elem_value_clear(mix->dst.value);
	snd_ctl_elem_value_set_id(mix->dst.value, mix->dst.id);
	type = snd_ctl_elem_info_get_type(mix->dst.info);
	count = snd_ctl_elem_info_get_count(mix->dst.info);
	snd_ctl_elem_remove(loop->capt->ctl, mix->dst.id);
	switch (type) {
	case SND_CTL_ELEM_TYPE_BOOLEAN:
		err = snd_ctl_elem_add_boolean(loop->capt->ctl,
					       mix->dst.id, count);
		copy_value(&mix->dst, &mix->src);
		break;
	case SND_CTL_ELEM_TYPE_INTEGER:
		err = snd_ctl_elem_add_integer(loop->capt->ctl,
				mix->dst.id, count,
				snd_ctl_elem_info_get_min(mix->dst.info),
				snd_ctl_elem_info_get_max(mix->dst.info),
				snd_ctl_elem_info_get_step(mix->dst.info));
		copy_value(&mix->dst, &mix->src);
		break;
	default:
		logit(LOG_CRIT, "Unable to handle control type %s\n", snd_ctl_elem_type_name(type));
		err = -EINVAL;
		break;
	}
	if (err < 0) {
		logit(LOG_CRIT, "Unable to create control '%s': %s\n", id_str(mix->dst.id), snd_strerror(err));
		return err;
	}
	err = snd_ctl_elem_unlock(loop->capt->ctl, mix->dst.id);
	if (err < 0) {
		logit(LOG_CRIT, "Unable to unlock control info '%s': %s\n", id_str(mix->dst.id), snd_strerror(err));
		return err;
	}
	err = snd_ctl_elem_info(loop->capt->ctl, mix->dst.info);
	if (err < 0) {
		logit(LOG_CRIT, "Unable to read control info '%s': %s\n", id_str(mix->dst.id), snd_strerror(err));
		return err;
	}
	if (snd_ctl_elem_info_is_tlv_writable(mix->dst.info)) {
		unsigned int tlv[64];
		err = snd_ctl_elem_tlv_read(loop->play->ctl,
					    mix->src.id,
					    tlv, sizeof(tlv));
		if (err < 0) {
			logit(LOG_CRIT, "Unable to read TLV for '%s': %s\n", id_str(mix->src.id), snd_strerror(err));
			tlv[0] = tlv[1] = 0;
		}
		err = snd_ctl_elem_tlv_write(loop->capt->ctl,
					     mix->dst.id,
					     tlv);
		if (err < 0) {
			logit(LOG_CRIT, "Unable to write TLV for '%s': %s\n", id_str(mix->src.id), snd_strerror(err));
			return err;
		}
	}
	err = snd_ctl_elem_write(loop->capt->ctl, mix->dst.value);
	if (err < 0) {
		logit(LOG_CRIT, "Unable to write control value '%s': %s\n", id_str(mix->dst.id), snd_strerror(err));
		return err;
	}
	return 0;
}

int control_init(struct loopback *loop)
{
	struct loopback_mixer *mix;
	struct loopback_ossmixer *ossmix;
	int err;

	for (ossmix = loop->oss_controls; ossmix; ossmix = ossmix->next)
		oss_set(loop, ossmix, 0);
	for (mix = loop->controls; mix; mix = mix->next) {
		err = control_init1(loop->play, &mix->src);
		if (err < 0) {
			logit(LOG_WARNING, "%s: Disabling playback control '%s'\n", loop->id, id_str(mix->src.id));
			mix->skip = 1;
			continue;
		}
		err = control_init2(loop, mix);
		if (err < 0)
			return err;
	}
	for (ossmix = loop->oss_controls; ossmix; ossmix = ossmix->next) {
		err = oss_set(loop, ossmix, 1);
		if (err < 0) {
			ossmix->skip = 1;
			logit(LOG_WARNING, "%s: Disabling OSS mixer ID '%s'\n", loop->id, ossmix->oss_id);
		}
	}
	return 0;
}

int control_done(struct loopback *loop)
{
	struct loopback_mixer *mix;
	struct loopback_ossmixer *ossmix;
	int err;

	if (loop->capt->ctl == NULL)
		return 0;
	for (ossmix = loop->oss_controls; ossmix; ossmix = ossmix->next) {
		err = oss_set(loop, ossmix, 0);
		if (err < 0)
			logit(LOG_WARNING, "%s: Unable to remove OSS control '%s'\n", loop->id, ossmix->oss_id);
	}
	for (mix = loop->controls; mix; mix = mix->next) {
		if (mix->skip)
			continue;
		err = snd_ctl_elem_remove(loop->capt->ctl, mix->dst.id);
		if (err < 0)
			logit(LOG_WARNING, "%s: Unable to remove control '%s': %s\n", loop->id, id_str(mix->dst.id), snd_strerror(err));
	}
	return 0;
}

static int control_event1(struct loopback *loop,
			  struct loopback_mixer *mix,
			  snd_ctl_event_t *ev,
			  int capture)
{
	unsigned int mask = snd_ctl_event_elem_get_mask(ev);
	int err;

	if (mask == SND_CTL_EVENT_MASK_REMOVE)
		return 0;
	if ((mask & SND_CTL_EVENT_MASK_VALUE) == 0)
		return 0;
	if (!capture) {
		snd_ctl_elem_value_set_id(mix->src.value, mix->src.id);
		err = snd_ctl_elem_read(loop->play->ctl, mix->src.value);
		if (err < 0) {
			logit(LOG_CRIT, "Unable to read control value (event1) '%s': %s\n", id_str(mix->src.id), snd_strerror(err));
			return err;
		}
		copy_value(&mix->dst, &mix->src);
		err = snd_ctl_elem_write(loop->capt->ctl, mix->dst.value);
		if (err < 0) {
			logit(LOG_CRIT, "Unable to write control value (event1) '%s': %s\n", id_str(mix->dst.id), snd_strerror(err));
			return err;
		}
	} else {
		err = snd_ctl_elem_read(loop->capt->ctl, mix->dst.value);
		if (err < 0) {
			logit(LOG_CRIT, "Unable to read control value (event2) '%s': %s\n", id_str(mix->dst.id), snd_strerror(err));
			return err;
		}
		copy_value(&mix->src, &mix->dst);
		err = snd_ctl_elem_write(loop->play->ctl, mix->src.value);
		if (err < 0) {
			logit(LOG_CRIT, "Unable to write control value (event2) '%s': %s\n", id_str(mix->src.id), snd_strerror(err));
			return err;
		}
	}
	return 0;
}

int control_event(struct loopback_handle *lhandle, snd_ctl_event_t *ev)
{
	snd_ctl_elem_id_t *id2;
	struct loopback_mixer *mix;
	int capt = lhandle == lhandle->loopback->capt;
	int err;

	snd_ctl_elem_id_alloca(&id2);
	snd_ctl_event_elem_get_id(ev, id2);
	for (mix = lhandle->loopback->controls; mix; mix = mix->next) {
		if (mix->skip)
			continue;
		if (control_id_match(id2, capt ? mix->dst.id : mix->src.id)) {
			err = control_event1(lhandle->loopback, mix, ev, capt);
			if (err < 0)
				return err;
		}
	}
	return 0;
}
