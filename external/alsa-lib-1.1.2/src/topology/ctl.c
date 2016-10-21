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

#define ENUM_VAL_SIZE 	(SNDRV_CTL_ELEM_ID_NAME_MAXLEN >> 2)

struct ctl_access_elem {
	const char *name;
	unsigned int value;
};

/* CTL access strings and codes */
static const struct ctl_access_elem ctl_access[] = {
	{"read", SNDRV_CTL_ELEM_ACCESS_READ},
	{"write", SNDRV_CTL_ELEM_ACCESS_WRITE},
	{"read_write", SNDRV_CTL_ELEM_ACCESS_READWRITE},
	{"volatile", SNDRV_CTL_ELEM_ACCESS_VOLATILE},
	{"timestamp", SNDRV_CTL_ELEM_ACCESS_TIMESTAMP},
	{"tlv_read", SNDRV_CTL_ELEM_ACCESS_TLV_READ},
	{"tlv_write", SNDRV_CTL_ELEM_ACCESS_TLV_WRITE},
	{"tlv_read_write", SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE},
	{"tlv_command", SNDRV_CTL_ELEM_ACCESS_TLV_COMMAND},
	{"inactive", SNDRV_CTL_ELEM_ACCESS_INACTIVE},
	{"lock", SNDRV_CTL_ELEM_ACCESS_LOCK},
	{"owner", SNDRV_CTL_ELEM_ACCESS_OWNER},
	{"tlv_callback", SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK},
};

/* find CTL access strings and conver to values */
static int parse_access_values(snd_config_t *cfg,
	struct snd_soc_tplg_ctl_hdr *hdr)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *value = NULL;
	unsigned int j;

	tplg_dbg(" Access:\n");

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);

		/* get value */
		if (snd_config_get_string(n, &value) < 0)
			continue;

		/* match access value and set flags */
		for (j = 0; j < ARRAY_SIZE(ctl_access); j++) {
			if (strcmp(value, ctl_access[j].name) == 0) {
				hdr->access |= ctl_access[j].value;
				tplg_dbg("\t%s\n", value);
				break;
			}
		}
	}

	return 0;
}

/* Parse Access */
int parse_access(snd_config_t *cfg,
	struct snd_soc_tplg_ctl_hdr *hdr)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	int err = 0;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "access") == 0) {
			err = parse_access_values(n, hdr);
			if (err < 0) {
				SNDERR("error: failed to parse access");
				return err;
			}
			continue;
		}
	}

	return err;
}

/* copy referenced TLV to the mixer control */
static int copy_tlv(struct tplg_elem *elem, struct tplg_elem *ref)
{
	struct snd_soc_tplg_mixer_control *mixer_ctrl =  elem->mixer_ctrl;
	struct snd_soc_tplg_ctl_tlv *tlv = ref->tlv;

	tplg_dbg("TLV '%s' used by '%s\n", ref->id, elem->id);

	/* TLV has a fixed size */
	mixer_ctrl->hdr.tlv = *tlv;
	return 0;
}

/* check referenced TLV for a mixer control */
static int tplg_build_mixer_control(snd_tplg_t *tplg,
				struct tplg_elem *elem)
{
	struct tplg_ref *ref;
	struct list_head *base, *pos;
	int err = 0;

	base = &elem->ref_list;

	/* for each ref in this control elem */
	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);
		if (ref->id == NULL || ref->elem)
			continue;

		if (ref->type == SND_TPLG_TYPE_TLV) {
			ref->elem = tplg_elem_lookup(&tplg->tlv_list,
						ref->id, SND_TPLG_TYPE_TLV);
			if (ref->elem)
				 err = copy_tlv(elem, ref->elem);

		} else if (ref->type == SND_TPLG_TYPE_DATA) {
			err = tplg_copy_data(tplg, elem, ref);
			if (err < 0)
				return err;
		}

		if (!ref->elem) {
			SNDERR("error: cannot find '%s' referenced by"
				" control '%s'\n", ref->id, elem->id);
			return -EINVAL;
		} else if (err < 0)
			return err;
	}

	return 0;
}

static void copy_enum_texts(struct tplg_elem *enum_elem,
	struct tplg_elem *ref_elem)
{
	struct snd_soc_tplg_enum_control *ec = enum_elem->enum_ctrl;

	memcpy(ec->texts, ref_elem->texts,
		SND_SOC_TPLG_NUM_TEXTS * SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
}

/* check referenced text for a enum control */
static int tplg_build_enum_control(snd_tplg_t *tplg,
				struct tplg_elem *elem)
{
	struct tplg_ref *ref;
	struct list_head *base, *pos;
	int err = 0;

	base = &elem->ref_list;

	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);
		if (ref->id == NULL || ref->elem)
			continue;

		if (ref->type == SND_TPLG_TYPE_TEXT) {
			ref->elem = tplg_elem_lookup(&tplg->text_list,
						ref->id, SND_TPLG_TYPE_TEXT);
			if (ref->elem)
				copy_enum_texts(elem, ref->elem);

		} else if (ref->type == SND_TPLG_TYPE_DATA) {
			err = tplg_copy_data(tplg, elem, ref);
			if (err < 0)
				return err;
		}
		if (!ref->elem) {
			SNDERR("error: cannot find '%s' referenced by"
				" control '%s'\n", ref->id, elem->id);
			return -EINVAL;
		} else if (err < 0)
			return err;
	}

	return 0;
}

/* check referenced private data for a byte control */
static int tplg_build_bytes_control(snd_tplg_t *tplg, struct tplg_elem *elem)
{
	struct tplg_ref *ref;
	struct list_head *base, *pos;
	int err;

	base = &elem->ref_list;

	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);
		if (ref->id == NULL || ref->elem)
			continue;

		 if (ref->type == SND_TPLG_TYPE_DATA) {
			err = tplg_copy_data(tplg, elem, ref);
			if (err < 0)
				return err;
		}
	}

	return 0;
}

int tplg_build_controls(snd_tplg_t *tplg)
{
	struct list_head *base, *pos;
	struct tplg_elem *elem;
	int err = 0;

	base = &tplg->mixer_list;
	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		err = tplg_build_mixer_control(tplg, elem);
		if (err < 0)
			return err;

		/* add control to manifest */
		tplg->manifest.control_elems++;
	}

	base = &tplg->enum_list;
	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		err = tplg_build_enum_control(tplg, elem);
		if (err < 0)
			return err;

		/* add control to manifest */
		tplg->manifest.control_elems++;
	}

	base = &tplg->bytes_ext_list;
	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		err = tplg_build_bytes_control(tplg, elem);
		if (err < 0)
			return err;

		/* add control to manifest */
		tplg->manifest.control_elems++;
	}

	return 0;
}


/*
 * Parse TLV of DBScale type.
 *
 * Parse DBScale describing min, step, mute in DB.
 */
static int tplg_parse_tlv_dbscale(snd_config_t *cfg, struct tplg_elem *elem)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	struct snd_soc_tplg_ctl_tlv *tplg_tlv;
	struct snd_soc_tplg_tlv_dbscale *scale;
	const char *id = NULL, *value = NULL;

	tplg_dbg(" scale: %s\n", elem->id);

	tplg_tlv = calloc(1, sizeof(*tplg_tlv));
	if (!tplg_tlv)
		return -ENOMEM;

	elem->tlv = tplg_tlv;
	tplg_tlv->size = sizeof(struct snd_soc_tplg_ctl_tlv);
	tplg_tlv->type = SNDRV_CTL_TLVT_DB_SCALE;
	scale = &tplg_tlv->scale;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);

		/* get ID */
		if (snd_config_get_id(n, &id) < 0) {
			SNDERR("error: cant get ID\n");
			return -EINVAL;
		}

		/* get value */
		if (snd_config_get_string(n, &value) < 0)
			continue;

		tplg_dbg("\t%s = %s\n", id, value);

		/* get TLV data */
		if (strcmp(id, "min") == 0)
			scale->min = atoi(value);
		else if (strcmp(id, "step") == 0)
			scale->step = atoi(value);
		else if (strcmp(id, "mute") == 0)
			scale->mute = atoi(value);
		else
			SNDERR("error: unknown key %s\n", id);
	}

	return 0;
}

/* Parse TLV */
int tplg_parse_tlv(snd_tplg_t *tplg, snd_config_t *cfg,
	void *private ATTRIBUTE_UNUSED)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	int err = 0;
	struct tplg_elem *elem;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_TLV);
	if (!elem)
		return -ENOMEM;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "scale") == 0) {
			err = tplg_parse_tlv_dbscale(n, elem);
			if (err < 0) {
				SNDERR("error: failed to DBScale");
				return err;
			}
			continue;
		}
	}

	return err;
}

/* Parse Control Bytes */
int tplg_parse_control_bytes(snd_tplg_t *tplg,
	snd_config_t *cfg, void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_bytes_control *be;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;
	int err;
	bool access_set = false, tlv_set = false;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_BYTES);
	if (!elem)
		return -ENOMEM;

	be = elem->bytes_ext;
	be->size = elem->size;
	elem_copy_text(be->hdr.name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	be->hdr.type = SND_SOC_TPLG_TYPE_BYTES;

	tplg_dbg(" Control Bytes: %s\n", elem->id);

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;

		if (strcmp(id, "index") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			elem->index = atoi(val);
			tplg_dbg("\t%s: %d\n", id, elem->index);
			continue;
		}

		if (strcmp(id, "base") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			be->base = atoi(val);
			tplg_dbg("\t%s: %d\n", id, be->base);
			continue;
		}

		if (strcmp(id, "num_regs") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			be->num_regs = atoi(val);
			tplg_dbg("\t%s: %d\n", id, be->num_regs);
			continue;
		}

		if (strcmp(id, "max") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			be->max = atoi(val);
			tplg_dbg("\t%s: %d\n", id, be->max);
			continue;
		}

		if (strcmp(id, "mask") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			be->mask = strtol(val, NULL, 16);
			tplg_dbg("\t%s: %d\n", id, be->mask);
			continue;
		}

		if (strcmp(id, "data") == 0) {
			err = tplg_parse_data_refs(n, elem);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "tlv") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			err = tplg_ref_add(elem, SND_TPLG_TYPE_TLV, val);
			if (err < 0)
				return err;

			tlv_set = true;
			tplg_dbg("\t%s: %s\n", id, val);
			continue;
		}

		if (strcmp(id, "ops") == 0) {
			err = tplg_parse_compound(tplg, n, tplg_parse_ops,
				&be->hdr);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "extops") == 0) {
			err = tplg_parse_compound(tplg, n, tplg_parse_ext_ops,
				be);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "access") == 0) {
			err = parse_access(cfg, &be->hdr);
			if (err < 0)
				return err;
			access_set = true;
			continue;
		}
	}

	/* set CTL access to default values if none are provided */
	if (!access_set) {

		be->hdr.access = SNDRV_CTL_ELEM_ACCESS_READWRITE;
		if (tlv_set)
			be->hdr.access |= SNDRV_CTL_ELEM_ACCESS_TLV_READ;
	}

	return 0;
}

/* Parse Control Enums. */
int tplg_parse_control_enum(snd_tplg_t *tplg, snd_config_t *cfg,
	void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_enum_control *ec;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;
	int err, j;
	bool access_set = false;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_ENUM);
	if (!elem)
		return -ENOMEM;

	ec = elem->enum_ctrl;
	elem_copy_text(ec->hdr.name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	ec->hdr.type = SND_SOC_TPLG_TYPE_ENUM;
	ec->size = elem->size;
	tplg->channel_idx = 0;

	/* set channel reg to default state */
	for (j = 0; j < SND_SOC_TPLG_MAX_CHAN; j++)
		ec->channel[j].reg = -1;

	tplg_dbg(" Control Enum: %s\n", elem->id);

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;

		if (strcmp(id, "index") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			elem->index = atoi(val);
			tplg_dbg("\t%s: %d\n", id, elem->index);
			continue;
		}

		if (strcmp(id, "texts") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			tplg_ref_add(elem, SND_TPLG_TYPE_TEXT, val);
			tplg_dbg("\t%s: %s\n", id, val);
			continue;
		}

		if (strcmp(id, "channel") == 0) {
			if (ec->num_channels >= SND_SOC_TPLG_MAX_CHAN) {
				SNDERR("error: too many channels %s\n",
					elem->id);
				return -EINVAL;
			}

			err = tplg_parse_compound(tplg, n, tplg_parse_channel,
				ec->channel);
			if (err < 0)
				return err;

			ec->num_channels = tplg->channel_idx;
			continue;
		}

		if (strcmp(id, "ops") == 0) {
			err = tplg_parse_compound(tplg, n, tplg_parse_ops,
				&ec->hdr);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "data") == 0) {
			err = tplg_parse_data_refs(n, elem);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "access") == 0) {
			err = parse_access(cfg, &ec->hdr);
			if (err < 0)
				return err;
			access_set = true;
			continue;
		}
	}

	/* set CTL access to default values if none are provided */
	if (!access_set) {
		ec->hdr.access = SNDRV_CTL_ELEM_ACCESS_READWRITE;
	}

	return 0;
}

/* Parse Controls.
 *
 * Mixer control. Supports multiple channels.
 */
int tplg_parse_control_mixer(snd_tplg_t *tplg,
	snd_config_t *cfg, void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_mixer_control *mc;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;
	int err, j;
	bool access_set = false, tlv_set = false;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_MIXER);
	if (!elem)
		return -ENOMEM;

	/* init new mixer */
	mc = elem->mixer_ctrl;
	elem_copy_text(mc->hdr.name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	mc->hdr.type = SND_SOC_TPLG_TYPE_MIXER;
	mc->size = elem->size;
	tplg->channel_idx = 0;

	/* set channel reg to default state */
	for (j = 0; j < SND_SOC_TPLG_MAX_CHAN; j++)
		mc->channel[j].reg = -1;

	tplg_dbg(" Control Mixer: %s\n", elem->id);

	/* giterate trough each mixer elment */
	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;

		if (strcmp(id, "index") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			elem->index = atoi(val);
			tplg_dbg("\t%s: %d\n", id, elem->index);
			continue;
		}

		if (strcmp(id, "channel") == 0) {
			if (mc->num_channels >= SND_SOC_TPLG_MAX_CHAN) {
				SNDERR("error: too many channels %s\n",
					elem->id);
				return -EINVAL;
			}

			err = tplg_parse_compound(tplg, n, tplg_parse_channel,
				mc->channel);
			if (err < 0)
				return err;

			mc->num_channels = tplg->channel_idx;
			continue;
		}

		if (strcmp(id, "max") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			mc->max = atoi(val);
			tplg_dbg("\t%s: %d\n", id, mc->max);
			continue;
		}

		if (strcmp(id, "invert") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			if (strcmp(val, "true") == 0)
				mc->invert = 1;
			else if (strcmp(val, "false") == 0)
				mc->invert = 0;

			tplg_dbg("\t%s: %d\n", id, mc->invert);
			continue;
		}

		if (strcmp(id, "ops") == 0) {
			err = tplg_parse_compound(tplg, n, tplg_parse_ops,
				&mc->hdr);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "tlv") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			err = tplg_ref_add(elem, SND_TPLG_TYPE_TLV, val);
			if (err < 0)
				return err;

			tlv_set = true;
			tplg_dbg("\t%s: %s\n", id, val);
			continue;
		}

		if (strcmp(id, "data") == 0) {
			err = tplg_parse_data_refs(n, elem);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "access") == 0) {
			err = parse_access(cfg, &mc->hdr);
			if (err < 0)
				return err;
			access_set = true;
			continue;
		}
	}

	/* set CTL access to default values if none are provided */
	if (!access_set) {

		mc->hdr.access = SNDRV_CTL_ELEM_ACCESS_READWRITE;
		if (tlv_set)
			mc->hdr.access |= SNDRV_CTL_ELEM_ACCESS_TLV_READ;
	}

	return 0;
}

static int init_ctl_hdr(struct snd_soc_tplg_ctl_hdr *hdr,
		struct snd_tplg_ctl_template *t)
{
	hdr->size = sizeof(struct snd_soc_tplg_ctl_hdr);
	hdr->type = t->type;

	elem_copy_text(hdr->name, t->name,
		SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	/* clean up access flag */
	if (t->access == 0)
		t->access = SNDRV_CTL_ELEM_ACCESS_READWRITE;
	t->access &= (SNDRV_CTL_ELEM_ACCESS_READWRITE |
		SNDRV_CTL_ELEM_ACCESS_VOLATILE |
		SNDRV_CTL_ELEM_ACCESS_INACTIVE |
		SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE |
		SNDRV_CTL_ELEM_ACCESS_TLV_COMMAND |
		SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK);

	hdr->access = t->access;
	hdr->ops.get = t->ops.get;
	hdr->ops.put = t->ops.put;
	hdr->ops.info = t->ops.info;

	/* TLV */
	if (hdr->access & SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE
		&& !(hdr->access & SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK)) {

		struct snd_tplg_tlv_template *tlvt = t->tlv;
		struct snd_soc_tplg_ctl_tlv *tlv = &hdr->tlv;
		struct snd_tplg_tlv_dbscale_template *scalet;
		struct snd_soc_tplg_tlv_dbscale *scale;

		if (!tlvt) {
			SNDERR("error: missing TLV data\n");
			return -EINVAL;
		}

		tlv->size = sizeof(struct snd_soc_tplg_ctl_tlv);
		tlv->type = tlvt->type;

		switch (tlvt->type) {
		case SNDRV_CTL_TLVT_DB_SCALE:
			scalet = container_of(tlvt,
				struct snd_tplg_tlv_dbscale_template, hdr);
			scale = &tlv->scale;
			scale->min = scalet->min;
			scale->step = scalet->step;
			scale->mute = scalet->mute;
			break;

		/* TODO: add support for other TLV types */
		default:
			SNDERR("error: unsupported TLV type %d\n", tlv->type);
			break;
		}
	}

	return 0;
}

int tplg_add_mixer(snd_tplg_t *tplg, struct snd_tplg_mixer_template *mixer,
	struct tplg_elem **e)
{
	struct snd_soc_tplg_private *priv = mixer->priv;
	struct snd_soc_tplg_mixer_control *mc;
	struct tplg_elem *elem;
	int ret, i, num_channels;

	tplg_dbg(" Control Mixer: %s\n", mixer->hdr.name);

	if (mixer->hdr.type != SND_SOC_TPLG_TYPE_MIXER) {
		SNDERR("error: invalid mixer type %d\n", mixer->hdr.type);
		return -EINVAL;
	}

	elem = tplg_elem_new_common(tplg, NULL, mixer->hdr.name,
		SND_TPLG_TYPE_MIXER);
	if (!elem)
		return -ENOMEM;

	/* init new mixer */
	mc = elem->mixer_ctrl;
	mc->size = elem->size;
	ret =  init_ctl_hdr(&mc->hdr, &mixer->hdr);
	if (ret < 0) {
		tplg_elem_free(elem);
		return ret;
	}

	mc->min = mixer->min;
	mc->max = mixer->max;
	mc->platform_max = mixer->platform_max;
	mc->invert = mixer->invert;

	/* set channel reg to default state */
	for (i = 0; i < SND_SOC_TPLG_MAX_CHAN; i++)
		mc->channel[i].reg = -1;

	num_channels = mixer->map ? mixer->map->num_channels : 0;
	mc->num_channels = num_channels;

	for (i = 0; i < num_channels; i++) {
		struct snd_tplg_channel_elem *channel = &mixer->map->channel[i];

		mc->channel[i].size = channel->size;
		mc->channel[i].reg = channel->reg;
		mc->channel[i].shift = channel->shift;
		mc->channel[i].id = channel->id;
	}

	/* priv data */
	if (priv) {
		mc = realloc(mc, elem->size + priv->size);
		if (!mc) {
			tplg_elem_free(elem);
			return -ENOMEM;
		}

		elem->mixer_ctrl = mc;
		elem->size += priv->size;
		mc->priv.size = priv->size;
		memcpy(mc->priv.data, priv->data,  priv->size);
        }

	if (e)
		*e = elem;
	return 0;
}

int tplg_add_enum(snd_tplg_t *tplg, struct snd_tplg_enum_template *enum_ctl,
	struct tplg_elem **e)
{
	struct snd_soc_tplg_enum_control *ec;
	struct tplg_elem *elem;
	int ret, i, num_items;

	tplg_dbg(" Control Enum: %s\n", enum_ctl->hdr.name);

	if (enum_ctl->hdr.type != SND_SOC_TPLG_TYPE_ENUM) {
		SNDERR("error: invalid enum type %d\n", enum_ctl->hdr.type);
		return -EINVAL;
	}

	elem = tplg_elem_new_common(tplg, NULL, enum_ctl->hdr.name,
		SND_TPLG_TYPE_ENUM);
	if (!elem)
		return -ENOMEM;

	ec = elem->enum_ctrl;
	ec->size = elem->size;
	ret = init_ctl_hdr(&ec->hdr, &enum_ctl->hdr);
	if (ret < 0) {
		tplg_elem_free(elem);
		return ret;
	}

	num_items =  enum_ctl->items < SND_SOC_TPLG_NUM_TEXTS ?
		enum_ctl->items : SND_SOC_TPLG_NUM_TEXTS;
	ec->items = num_items;
	ec->mask = enum_ctl->mask;
	ec->count = enum_ctl->items;

	if (enum_ctl->texts != NULL) {
		for (i = 0; i < num_items; i++) {
			if (enum_ctl->texts[i] != NULL)
				strncpy(ec->texts[i], enum_ctl->texts[i],
					SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
		}
	}

	if (enum_ctl->values != NULL) {
		for (i = 0; i < num_items; i++) {
			if (enum_ctl->values[i])
				continue;

			memcpy(&ec->values[i * sizeof(int) * ENUM_VAL_SIZE],
				enum_ctl->values[i],
				sizeof(int) * ENUM_VAL_SIZE);
		}
	}

	if (enum_ctl->priv != NULL) {
		ec = realloc(ec,
			elem->size + enum_ctl->priv->size);
		if (!ec) {
			tplg_elem_free(elem);
			return -ENOMEM;
		}

		elem->enum_ctrl = ec;
		elem->size += enum_ctl->priv->size;

		memcpy(ec->priv.data, enum_ctl->priv->data,
			enum_ctl->priv->size);

		ec->priv.size = enum_ctl->priv->size;
	}

	if (e)
		*e = elem;
	return 0;
}

int tplg_add_bytes(snd_tplg_t *tplg, struct snd_tplg_bytes_template *bytes_ctl,
	struct tplg_elem **e)
{
	struct snd_soc_tplg_bytes_control *be;
	struct tplg_elem *elem;
	int ret;

	tplg_dbg(" Control Bytes: %s\n", bytes_ctl->hdr.name);

	if (bytes_ctl->hdr.type != SND_SOC_TPLG_TYPE_BYTES) {
		SNDERR("error: invalid bytes type %d\n", bytes_ctl->hdr.type);
		return -EINVAL;
	}

	elem = tplg_elem_new_common(tplg, NULL, bytes_ctl->hdr.name,
		SND_TPLG_TYPE_BYTES);
	if (!elem)
		return -ENOMEM;

	be = elem->bytes_ext;
	be->size = elem->size;
	ret = init_ctl_hdr(&be->hdr, &bytes_ctl->hdr);
	if (ret < 0) {
		tplg_elem_free(elem);
		return ret;
	}

	be->max = bytes_ctl->max;
	be->mask = bytes_ctl->mask;
	be->base = bytes_ctl->base;
	be->num_regs = bytes_ctl->num_regs;
	be->ext_ops.put = bytes_ctl->ext_ops.put;
	be->ext_ops.get = bytes_ctl->ext_ops.get;

	if (bytes_ctl->priv != NULL) {
		be = realloc(be,
			elem->size + bytes_ctl->priv->size);
		if (!be) {
			tplg_elem_free(elem);
			return -ENOMEM;
		}
		elem->bytes_ext = be;
		elem->size += bytes_ctl->priv->size;

		memcpy(be->priv.data, bytes_ctl->priv->data,
			bytes_ctl->priv->size);

		be->priv.size = bytes_ctl->priv->size;
	}

	/* check on TLV bytes control */
	if (be->hdr.access & SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK) {
		if ((be->hdr.access & SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE)
			!= SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE) {
			SNDERR("error: Invalid TLV bytes control access 0x%x\n",
				be->hdr.access);
			tplg_elem_free(elem);
			return -EINVAL;
		}

		if (!be->max) {
			tplg_elem_free(elem);
			return -EINVAL;
		}
	}

	if (e)
		*e = elem;
	return 0;
}

int tplg_add_mixer_object(snd_tplg_t *tplg, snd_tplg_obj_template_t *t)
{
	return tplg_add_mixer(tplg, t->mixer, NULL);
}

int tplg_add_enum_object(snd_tplg_t *tplg, snd_tplg_obj_template_t *t)
{
	return tplg_add_enum(tplg, t->enum_ctl, NULL);
}

int tplg_add_bytes_object(snd_tplg_t *tplg, snd_tplg_obj_template_t *t)
{
	return tplg_add_bytes(tplg, t->bytes_ctl, NULL);
}
