/**
 * \file control/control.c
 * \brief CTL interface - parse ASCII identifiers and values
 * \author Jaroslav Kysela <perex@perex.cz>
 * \date 2010
 */
/*
 *  Control Interface - ASCII parser
 *  Copyright (c) 2010 by Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "control_local.h"

/* Function to convert from percentage to volume. val = percentage */

#ifdef HAVE_SOFT_FLOAT
static inline long int convert_prange1(long val, long min, long max)
{
	long temp = val * (max - min);
	return temp / 100 + min + ((temp % 100) == 0 ? 0 : 1);
}
#else

#define convert_prange1(val, min, max) \
	ceil((val) * ((max) - (min)) * 0.01 + (min))
#endif

#define check_range(val, min, max) \
	((val < min) ? (min) : ((val > max) ? (max) : (val)))

static long get_integer(const char **ptr, long min, long max)
{
	long val = min;
	char *p = (char *)*ptr, *s;

	if (*p == ':')
		p++;
	if (*p == '\0' || (!isdigit(*p) && *p != '-'))
		goto out;

	s = p;
	val = strtol(s, &p, 0);
	if (*p == '.') {
		p++;
		(void)strtol(p, &p, 10);
	}
	if (*p == '%') {
		val = (long)convert_prange1(strtod(s, NULL), min, max);
		p++;
	}
	val = check_range(val, min, max);
	if (*p == ',')
		p++;
 out:
	*ptr = p;
	return val;
}

static long long get_integer64(const char **ptr, long long min, long long max)
{
	long long val = min;
	char *p = (char *)*ptr, *s;

	if (*p == ':')
		p++;
	if (*p == '\0' || (!isdigit(*p) && *p != '-'))
		goto out;

	s = p;
	val = strtol(s, &p, 0);
	if (*p == '.') {
		p++;
		(void)strtol(p, &p, 10);
	}
	if (*p == '%') {
		val = (long long)convert_prange1(strtod(s, NULL), min, max);
		p++;
	}
	val = check_range(val, min, max);
	if (*p == ',')
		p++;
 out:
	*ptr = p;
	return val;
}

/**
 * \brief return ASCII CTL element identifier name
 * \param id CTL identifier
 * \return ascii identifier of CTL element
 *
 * The string is allocated using strdup().
 */
char *snd_ctl_ascii_elem_id_get(snd_ctl_elem_id_t *id)
{
	unsigned int index, device, subdevice;
	char buf[256], buf1[32];

	snprintf(buf, sizeof(buf), "numid=%u,iface=%s,name='%s'",
				snd_ctl_elem_id_get_numid(id),
				snd_ctl_elem_iface_name(
					snd_ctl_elem_id_get_interface(id)),
				snd_ctl_elem_id_get_name(id));
	buf[sizeof(buf)-1] = '\0';
	index = snd_ctl_elem_id_get_index(id);
	device = snd_ctl_elem_id_get_device(id);
	subdevice = snd_ctl_elem_id_get_subdevice(id);
	if (index) {
		snprintf(buf1, sizeof(buf1), ",index=%i", index);
		if (strlen(buf) + strlen(buf1) < sizeof(buf))
			strcat(buf, buf1);
	}
	if (device) {
		snprintf(buf1, sizeof(buf1), ",device=%i", device);
		if (strlen(buf) + strlen(buf1) < sizeof(buf))
			strcat(buf, buf1);
	}
	if (subdevice) {
		snprintf(buf1, sizeof(buf1), ",subdevice=%i", subdevice);
		if (strlen(buf) + strlen(buf1) < sizeof(buf))
			strcat(buf, buf1);
	}
	return strdup(buf);
}

#ifndef DOC_HIDDEN
/* used by UCM parser, too */
int __snd_ctl_ascii_elem_id_parse(snd_ctl_elem_id_t *dst, const char *str,
				  const char **ret_ptr)
{
	int c, size, numid;
	int err = -EINVAL;
	char *ptr;

	while (isspace(*str))
		str++;
	if (!(*str))
		goto out;
	snd_ctl_elem_id_set_interface(dst, SND_CTL_ELEM_IFACE_MIXER);	/* default */
	while (*str) {
		if (!strncasecmp(str, "numid=", 6)) {
			str += 6;
			numid = atoi(str);
			if (numid <= 0) {
				fprintf(stderr, "amixer: Invalid numid %d\n", numid);
				goto out;
			}
			snd_ctl_elem_id_set_numid(dst, atoi(str));
			while (isdigit(*str))
				str++;
		} else if (!strncasecmp(str, "iface=", 6)) {
			str += 6;
			if (!strncasecmp(str, "card", 4)) {
				snd_ctl_elem_id_set_interface(dst, SND_CTL_ELEM_IFACE_CARD);
				str += 4;
			} else if (!strncasecmp(str, "mixer", 5)) {
				snd_ctl_elem_id_set_interface(dst, SND_CTL_ELEM_IFACE_MIXER);
				str += 5;
			} else if (!strncasecmp(str, "pcm", 3)) {
				snd_ctl_elem_id_set_interface(dst, SND_CTL_ELEM_IFACE_PCM);
				str += 3;
			} else if (!strncasecmp(str, "rawmidi", 7)) {
				snd_ctl_elem_id_set_interface(dst, SND_CTL_ELEM_IFACE_RAWMIDI);
				str += 7;
			} else if (!strncasecmp(str, "timer", 5)) {
				snd_ctl_elem_id_set_interface(dst, SND_CTL_ELEM_IFACE_TIMER);
				str += 5;
			} else if (!strncasecmp(str, "sequencer", 9)) {
				snd_ctl_elem_id_set_interface(dst, SND_CTL_ELEM_IFACE_SEQUENCER);
				str += 9;
			} else {
				goto out;
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
			snd_ctl_elem_id_set_name(dst, buf);
		} else if (!strncasecmp(str, "index=", 6)) {
			str += 6;
			snd_ctl_elem_id_set_index(dst, atoi(str));
			while (isdigit(*str))
				str++;
		} else if (!strncasecmp(str, "device=", 7)) {
			str += 7;
			snd_ctl_elem_id_set_device(dst, atoi(str));
			while (isdigit(*str))
				str++;
		} else if (!strncasecmp(str, "subdevice=", 10)) {
			str += 10;
			snd_ctl_elem_id_set_subdevice(dst, atoi(str));
			while (isdigit(*str))
				str++;
		}
		if (*str == ',') {
			str++;
		} else {
			/* when ret_ptr is given, allow to terminate gracefully
			 * at the next space letter
			 */
			if (ret_ptr && isspace(*str))
				break;
			if (*str)
				goto out;
		}
	}			
	err = 0;

 out:
	if (ret_ptr)
		*ret_ptr = str;
	return err;
}
#endif

/**
 * \brief parse ASCII string as CTL element identifier
 * \param dst destination CTL identifier
 * \param str source ASCII string
 * \return zero on success, otherwise a negative error code
 */
int snd_ctl_ascii_elem_id_parse(snd_ctl_elem_id_t *dst, const char *str)
{
	return __snd_ctl_ascii_elem_id_parse(dst, str, NULL);
}

static int get_ctl_enum_item_index(snd_ctl_t *handle,
				   snd_ctl_elem_info_t *info,
				   const char **ptrp)
{ 
	char *ptr = (char *)*ptrp;
	int items, i, len;
	const char *name;
  
	items = snd_ctl_elem_info_get_items(info);
	if (items <= 0)
		return -1;

	for (i = 0; i < items; i++) {
		snd_ctl_elem_info_set_item(info, i);
		if (snd_ctl_elem_info(handle, info) < 0)
			return -1;
		name = snd_ctl_elem_info_get_item_name(info);
		len = strlen(name);
		if (! strncmp(name, ptr, len)) {
			if (! ptr[len] || ptr[len] == ',' || ptr[len] == '\n') {
				ptr += len;
				*ptrp = ptr;
				return i;
			}
		}
	}
	return -1;
}

/**
 * \brief parse ASCII string as CTL element value
 * \param handle CTL handle
 * \param dst destination CTL element value
 * \param info CTL element info structure
 * \param value source ASCII string
 * \return zero on success, otherwise a negative error code
 *
 * Note: For toggle command, the dst must contain previous (current)
 * state (do the #snd_ctl_elem_read call to obtain it).
 */
int snd_ctl_ascii_value_parse(snd_ctl_t *handle,
			      snd_ctl_elem_value_t *dst,
			      snd_ctl_elem_info_t *info,
			      const char *value)
{
	const char *ptr = value;
	snd_ctl_elem_id_t myid = {0};
	snd_ctl_elem_type_t type;
	unsigned int idx, count;
	long tmp;
	long long tmp64;

	snd_ctl_elem_info_get_id(info, &myid);
	type = snd_ctl_elem_info_get_type(info);
	count = snd_ctl_elem_info_get_count(info);
	snd_ctl_elem_value_set_id(dst, &myid);
	
	for (idx = 0; idx < count && idx < 128 && ptr && *ptr; idx++) {
		if (*ptr == ',')
			goto skip;
		switch (type) {
		case SND_CTL_ELEM_TYPE_BOOLEAN:
			tmp = 0;
			if (!strncasecmp(ptr, "on", 2) ||
			    !strncasecmp(ptr, "up", 2)) {
				tmp = 1;
				ptr += 2;
			} else if (!strncasecmp(ptr, "yes", 3)) {
				tmp = 1;
				ptr += 3;
			} else if (!strncasecmp(ptr, "toggle", 6)) {
				tmp = snd_ctl_elem_value_get_boolean(dst, idx);
				tmp = tmp > 0 ? 0 : 1;
				ptr += 6;
			} else if (isdigit(*ptr)) {
				tmp = atoi(ptr) > 0 ? 1 : 0;
				while (isdigit(*ptr))
					ptr++;
			} else {
				while (*ptr && *ptr != ',')
					ptr++;
			}
			snd_ctl_elem_value_set_boolean(dst, idx, tmp);
			break;
		case SND_CTL_ELEM_TYPE_INTEGER:
			tmp = get_integer(&ptr,
					  snd_ctl_elem_info_get_min(info),
					  snd_ctl_elem_info_get_max(info));
			snd_ctl_elem_value_set_integer(dst, idx, tmp);
			break;
		case SND_CTL_ELEM_TYPE_INTEGER64:
			tmp64 = get_integer64(&ptr,
					  snd_ctl_elem_info_get_min64(info),
					  snd_ctl_elem_info_get_max64(info));
			snd_ctl_elem_value_set_integer64(dst, idx, tmp64);
			break;
		case SND_CTL_ELEM_TYPE_ENUMERATED:
			tmp = get_ctl_enum_item_index(handle, info, &ptr);
			if (tmp < 0)
				tmp = get_integer(&ptr, 0,
					snd_ctl_elem_info_get_items(info) - 1);
			snd_ctl_elem_value_set_enumerated(dst, idx, tmp);
			break;
		case SND_CTL_ELEM_TYPE_BYTES:
			tmp = get_integer(&ptr, 0, 255);
			snd_ctl_elem_value_set_byte(dst, idx, tmp);
			break;
		default:
			break;
		}
	skip:
		if (!strchr(value, ','))
			ptr = value;
		else if (*ptr == ',')
			ptr++;
	}
	return 0;
}
