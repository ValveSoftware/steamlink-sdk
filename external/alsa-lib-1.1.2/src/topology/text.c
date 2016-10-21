/*
  Copyright(c) 2014-2015 Intel Corporation
  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  Authors: Mengdong Lin <mengdong.lin@intel.com>
           Yao Jin <yao.jin@intel.com>
           Liam Girdwood <liam.r.girdwood@linux.intel.com>

*/

#include "list.h"
#include "tplg_local.h"

#define TEXT_SIZE_MAX \
	(SND_SOC_TPLG_NUM_TEXTS * SNDRV_CTL_ELEM_ID_NAME_MAXLEN)

static int parse_text_values(snd_config_t *cfg, struct tplg_elem *elem)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *value = NULL;
	int j = 0;

	tplg_dbg(" Text Values: %s\n", elem->id);

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);

		if (j == SND_SOC_TPLG_NUM_TEXTS) {
			tplg_dbg("error: text string number exceeds %d\n", j);
			return -ENOMEM;
		}

		/* get value */
		if (snd_config_get_string(n, &value) < 0)
			continue;

		elem_copy_text(&elem->texts[j][0], value,
			SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
		tplg_dbg("\t%s\n", &elem->texts[j][0]);

		j++;
	}

	return 0;
}

/* Parse Text data */
int tplg_parse_text(snd_tplg_t *tplg, snd_config_t *cfg,
	void *private ATTRIBUTE_UNUSED)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	int err = 0;
	struct tplg_elem *elem;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_TEXT);
	if (!elem)
		return -ENOMEM;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "values") == 0) {
			err = parse_text_values(n, elem);
			if (err < 0) {
				SNDERR("error: failed to parse text values");
				return err;
			}
			continue;
		}
	}

	return err;
}
