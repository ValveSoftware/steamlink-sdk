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

int tplg_ref_add(struct tplg_elem *elem, int type, const char* id)
{
	struct tplg_ref *ref;

	ref = calloc(1, sizeof(*ref));
	if (!ref)
		return -ENOMEM;

	strncpy(ref->id, id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	ref->id[SNDRV_CTL_ELEM_ID_NAME_MAXLEN - 1] = 0;
	ref->type = type;

	list_add_tail(&ref->list, &elem->ref_list);
	return 0;
}

/* directly add a reference elem */
int tplg_ref_add_elem(struct tplg_elem *elem, struct tplg_elem *elem_ref)
{
	struct tplg_ref *ref;

	ref = calloc(1, sizeof(*ref));
	if (!ref)
		return -ENOMEM;

	ref->type = elem_ref->type;
	ref->elem = elem_ref;
	elem_copy_text(ref->id,  elem_ref->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	list_add_tail(&ref->list, &elem->ref_list);
	return 0;
}

void tplg_ref_free_list(struct list_head *base)
{
	struct list_head *pos, *npos;
	struct tplg_ref *ref;

	list_for_each_safe(pos, npos, base) {
		ref = list_entry(pos, struct tplg_ref, list);
		list_del(&ref->list);
		free(ref);
	}
}

struct tplg_elem *tplg_elem_new(void)
{
	struct tplg_elem *elem;

	elem = calloc(1, sizeof(*elem));
	if (!elem)
		return NULL;

	INIT_LIST_HEAD(&elem->ref_list);
	return elem;
}

void tplg_elem_free(struct tplg_elem *elem)
{
	tplg_ref_free_list(&elem->ref_list);

	/* free struct snd_tplg_ object,
	 * the union pointers share the same address
	 */
	if (elem->obj) {
		if (elem->free)
			elem->free(elem->obj);

		free(elem->obj);
	}

	free(elem);
}

void tplg_elem_free_list(struct list_head *base)
{
	struct list_head *pos, *npos;
	struct tplg_elem *elem;

	list_for_each_safe(pos, npos, base) {
		elem = list_entry(pos, struct tplg_elem, list);
		list_del(&elem->list);
		tplg_elem_free(elem);
	}
}

struct tplg_elem *tplg_elem_lookup(struct list_head *base, const char* id,
	unsigned int type)
{
	struct list_head *pos;
	struct tplg_elem *elem;

	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);

		if (!strcmp(elem->id, id) && elem->type == type)
			return elem;
	}

	return NULL;
}

/* create a new common element and object */
struct tplg_elem* tplg_elem_new_common(snd_tplg_t *tplg,
	snd_config_t *cfg, const char *name, enum snd_tplg_type type)
{
	struct tplg_elem *elem;
	const char *id;
	int obj_size = 0;
	void *obj;

	if (!cfg && !name)
		return NULL;

	elem = tplg_elem_new();
	if (!elem)
		return NULL;

	/* do we get name from cfg */
	if (cfg) {
		snd_config_get_id(cfg, &id);
		elem_copy_text(elem->id, id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
		elem->id[SNDRV_CTL_ELEM_ID_NAME_MAXLEN - 1] = 0;
	} else if (name != NULL)
		elem_copy_text(elem->id, name, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	switch (type) {
	case SND_TPLG_TYPE_DATA:
		list_add_tail(&elem->list, &tplg->pdata_list);
		break;
	case SND_TPLG_TYPE_MANIFEST:
		list_add_tail(&elem->list, &tplg->manifest_list);
		obj_size = sizeof(struct snd_soc_tplg_manifest);
		break;
	case SND_TPLG_TYPE_TEXT:
		list_add_tail(&elem->list, &tplg->text_list);
		break;
	case SND_TPLG_TYPE_TLV:
		list_add_tail(&elem->list, &tplg->tlv_list);
		elem->size = sizeof(struct snd_soc_tplg_ctl_tlv);
		break;
	case SND_TPLG_TYPE_BYTES:
		list_add_tail(&elem->list, &tplg->bytes_ext_list);
		obj_size = sizeof(struct snd_soc_tplg_bytes_control);
		break;
	case SND_TPLG_TYPE_ENUM:
		list_add_tail(&elem->list, &tplg->enum_list);
		obj_size = sizeof(struct snd_soc_tplg_enum_control);
		break;
	case SND_TPLG_TYPE_MIXER:
		list_add_tail(&elem->list, &tplg->mixer_list);
		obj_size = sizeof(struct snd_soc_tplg_mixer_control);
		break;
	case SND_TPLG_TYPE_DAPM_WIDGET:
		list_add_tail(&elem->list, &tplg->widget_list);
		obj_size = sizeof(struct snd_soc_tplg_dapm_widget);
		break;
	case SND_TPLG_TYPE_STREAM_CONFIG:
		list_add_tail(&elem->list, &tplg->pcm_config_list);
		obj_size = sizeof(struct snd_soc_tplg_stream);
		break;
	case SND_TPLG_TYPE_STREAM_CAPS:
		list_add_tail(&elem->list, &tplg->pcm_caps_list);
		obj_size = sizeof(struct snd_soc_tplg_stream_caps);
		break;
	case SND_TPLG_TYPE_PCM:
		list_add_tail(&elem->list, &tplg->pcm_list);
		obj_size = sizeof(struct snd_soc_tplg_pcm);
		break;
	case SND_TPLG_TYPE_BE:
		list_add_tail(&elem->list, &tplg->be_list);
		obj_size = sizeof(struct snd_soc_tplg_link_config);
		break;
	case SND_TPLG_TYPE_CC:
		list_add_tail(&elem->list, &tplg->cc_list);
		obj_size = sizeof(struct snd_soc_tplg_link_config);
		break;
	case SND_TPLG_TYPE_TOKEN:
		list_add_tail(&elem->list, &tplg->token_list);
		break;
	case SND_TPLG_TYPE_TUPLE:
		list_add_tail(&elem->list, &tplg->tuple_list);
		elem->free = tplg_free_tuples;
		break;
	default:
		free(elem);
		return NULL;
	}

	/* create new object too if required */
	if (obj_size > 0) {
		obj = calloc(1, obj_size);
		if (obj == NULL) {
			free(elem);
			return NULL;
		}

		elem->obj = obj;
		elem->size = obj_size;
	}

	elem->type = type;
	return elem;
}
