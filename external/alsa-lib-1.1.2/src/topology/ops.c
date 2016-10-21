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

/* mapping of kcontrol text names to types */
static const struct map_elem control_map[] = {
	{"volsw", SND_SOC_TPLG_CTL_VOLSW},
	{"volsw_sx", SND_SOC_TPLG_CTL_VOLSW_SX},
	{"volsw_xr_sx", SND_SOC_TPLG_CTL_VOLSW_XR_SX},
	{"enum", SND_SOC_TPLG_CTL_ENUM},
	{"bytes", SND_SOC_TPLG_CTL_BYTES},
	{"enum_value", SND_SOC_TPLG_CTL_ENUM_VALUE},
	{"range", SND_SOC_TPLG_CTL_RANGE},
	{"strobe", SND_SOC_TPLG_CTL_STROBE},
};

static int lookup_ops(const char *c)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(control_map); i++) {
		if (strcmp(control_map[i].name, c) == 0)
			return control_map[i].id;
	}

	/* cant find string name in our table so we use its ID number */
	return atoi(c);
}

/* Parse Control operations. Ops can come from standard names above or
 * bespoke driver controls with numbers >= 256
 */
int tplg_parse_ops(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
	snd_config_t *cfg, void *private)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	struct snd_soc_tplg_ctl_hdr *hdr = private;
	const char *id, *value;

	tplg_dbg("\tOps\n");
	hdr->size = sizeof(*hdr);

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);

		/* get id */
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* get value - try strings then ints */
		if (snd_config_get_string(n, &value) < 0)
			continue;

		if (strcmp(id, "info") == 0)
			hdr->ops.info = lookup_ops(value);
		else if (strcmp(id, "put") == 0)
			hdr->ops.put = lookup_ops(value);
		else if (strcmp(id, "get") == 0)
			hdr->ops.get = lookup_ops(value);

		tplg_dbg("\t\t%s = %s\n", id, value);
	}

	return 0;
}

/* Parse External Control operations. Ops can come from standard names above or
 * bespoke driver controls with numbers >= 256
 */
int tplg_parse_ext_ops(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
	snd_config_t *cfg, void *private)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	struct snd_soc_tplg_bytes_control *be = private;
	const char *id, *value;

	tplg_dbg("\tExt Ops\n");

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);

		/* get id */
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* get value - try strings then ints */
		if (snd_config_get_string(n, &value) < 0)
			continue;

		if (strcmp(id, "info") == 0)
			be->ext_ops.info = lookup_ops(value);
		else if (strcmp(id, "put") == 0)
			be->ext_ops.put = lookup_ops(value);
		else if (strcmp(id, "get") == 0)
			be->ext_ops.get = lookup_ops(value);

		tplg_dbg("\t\t%s = %s\n", id, value);
	}

	return 0;
}
