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

struct tplg_elem *lookup_pcm_dai_stream(struct list_head *base, const char* id)
{
	struct list_head *pos;
	struct tplg_elem *elem;
	struct snd_soc_tplg_pcm *pcm;

	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		if (elem->type != SND_TPLG_TYPE_PCM)
			return NULL;

		pcm = elem->pcm;

		if (pcm && !strcmp(pcm->dai_name, id))
			return elem;
	}

	return NULL;
}

/* copy referenced caps to the parent (pcm or be dai) */
static void copy_stream_caps(const char *id,
	struct snd_soc_tplg_stream_caps *caps, struct tplg_elem *ref_elem)
{
	struct snd_soc_tplg_stream_caps *ref_caps = ref_elem->stream_caps;

	tplg_dbg("Copy pcm caps (%ld bytes) from '%s' to '%s' \n",
		sizeof(*caps), ref_elem->id, id);

	*caps =  *ref_caps;
}

/* find and copy the referenced stream caps */
static int tplg_build_stream_caps(snd_tplg_t *tplg,
	const char *id, struct snd_soc_tplg_stream_caps *caps)
{
	struct tplg_elem *ref_elem = NULL;
	unsigned int i;

	for (i = 0; i < 2; i++) {
		ref_elem = tplg_elem_lookup(&tplg->pcm_caps_list,
			caps[i].name, SND_TPLG_TYPE_STREAM_CAPS);

		if (ref_elem != NULL)
			copy_stream_caps(id, &caps[i], ref_elem);
	}

	return 0;
}

/* build FE DAI/PCM configurations */
int tplg_build_pcm(snd_tplg_t *tplg, unsigned int type)
{
	struct list_head *base, *pos;
	struct tplg_elem *elem;
	int err = 0;

	base = &tplg->pcm_list;
	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		if (elem->type != type) {
			SNDERR("error: invalid elem '%s'\n", elem->id);
			return -EINVAL;
		}

		err = tplg_build_stream_caps(tplg, elem->id, elem->pcm->caps);
		if (err < 0)
			return err;

		/* add PCM to manifest */
		tplg->manifest.pcm_elems++;
	}

	return 0;
}

static int tplg_build_stream_cfg(snd_tplg_t *tplg,
	struct snd_soc_tplg_stream *stream, int num_streams)
{
	struct snd_soc_tplg_stream *strm;
	struct tplg_elem *ref_elem;
	int i;

	for (i = 0; i < num_streams; i++) {
		strm = stream + i;
		ref_elem = tplg_elem_lookup(&tplg->pcm_config_list,
			strm->name, SND_TPLG_TYPE_STREAM_CONFIG);

		if (ref_elem && ref_elem->stream_cfg)
			*strm = *ref_elem->stream_cfg;
	}

	return 0;
}

/* build BE/CC DAI link configurations */
int tplg_build_link_cfg(snd_tplg_t *tplg, unsigned int type)
{
	struct list_head *base, *pos;
	struct tplg_elem *elem;
	struct snd_soc_tplg_link_config *link;
	int err = 0;

	switch (type) {
	case SND_TPLG_TYPE_BE:
		base = &tplg->be_list;
		break;
	case SND_TPLG_TYPE_CC:
		base = &tplg->cc_list;
		break;
	default:
		return -EINVAL;
	}

	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		if (elem->type != type) {
			SNDERR("error: invalid elem '%s'\n", elem->id);
			return -EINVAL;
		}

		if (type == SND_TPLG_TYPE_BE)
			link = elem->be;
		else
			link = elem->cc;

		err = tplg_build_stream_cfg(tplg, link->stream,
			link->num_streams);
		if (err < 0)
			return err;
	}

	return 0;
}

static int split_format(struct snd_soc_tplg_stream_caps *caps, char *str)
{
	char *s = NULL;
	snd_pcm_format_t format;
	int i = 0;

	s = strtok(str, ",");
	while ((s != NULL) && (i < SND_SOC_TPLG_MAX_FORMATS)) {
		format = snd_pcm_format_value(s);
		if (format == SND_PCM_FORMAT_UNKNOWN) {
			SNDERR("error: unsupported stream format %s\n", s);
			return -EINVAL;
		}

		caps->formats |= 1 << format;
		s = strtok(NULL, ", ");
		i++;
	}

	return 0;
}

/* Parse pcm stream capabilities */
int tplg_parse_stream_caps(snd_tplg_t *tplg,
	snd_config_t *cfg, void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_stream_caps *sc;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val;
	char *s;
	int err;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_STREAM_CAPS);
	if (!elem)
		return -ENOMEM;

	sc = elem->stream_caps;
	sc->size = elem->size;
	elem_copy_text(sc->name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	tplg_dbg(" PCM Capabilities: %s\n", elem->id);

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;

		if (snd_config_get_string(n, &val) < 0)
			return -EINVAL;

		if (strcmp(id, "formats") == 0) {
			s = strdup(val);
			if (s == NULL)
				return -ENOMEM;

			err = split_format(sc, s);
			free(s);

			if (err < 0)
				return err;

			tplg_dbg("\t\t%s: %s\n", id, val);
			continue;
		}

		if (strcmp(id, "rate_min") == 0) {
			sc->rate_min = atoi(val);
			tplg_dbg("\t\t%s: %d\n", id, sc->rate_min);
			continue;
		}

		if (strcmp(id, "rate_max") == 0) {
			sc->rate_max = atoi(val);
			tplg_dbg("\t\t%s: %d\n", id, sc->rate_max);
			continue;
		}

		if (strcmp(id, "channels_min") == 0) {
			sc->channels_min = atoi(val);
			tplg_dbg("\t\t%s: %d\n", id, sc->channels_min);
			continue;
		}

		if (strcmp(id, "channels_max") == 0) {
			sc->channels_max = atoi(val);
			tplg_dbg("\t\t%s: %d\n", id, sc->channels_max);
			continue;
		}
	}

	return 0;
}

/* Parse the caps and config of a pcm stream */
static int tplg_parse_streams(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
			      snd_config_t *cfg, void *private)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	struct tplg_elem *elem = private;
	struct snd_soc_tplg_pcm *pcm;
	unsigned int *playback, *capture;
	struct snd_soc_tplg_stream_caps *caps;
	const char *id, *value;
	int stream;

	snd_config_get_id(cfg, &id);

	tplg_dbg("\t%s:\n", id);

	switch (elem->type) {
	case SND_TPLG_TYPE_PCM:
		pcm = elem->pcm;
		playback = &pcm->playback;
		capture = &pcm->capture;
		caps = pcm->caps;
		break;
	default:
		return -EINVAL;
	}

	if (strcmp(id, "playback") == 0) {
		stream = SND_SOC_TPLG_STREAM_PLAYBACK;
		*playback = 1;
	} else if (strcmp(id, "capture") == 0) {
		stream = SND_SOC_TPLG_STREAM_CAPTURE;
		*capture = 1;
	} else
		return -EINVAL;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);

		/* get id */
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "capabilities") == 0) {
			if (snd_config_get_string(n, &value) < 0)
				continue;
			/* store stream caps name, to find and merge
			 * the caps in building phase.
			 */
			elem_copy_text(caps[stream].name, value,
				SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

			tplg_dbg("\t\t%s\n\t\t\t%s\n", id, value);
			continue;
		}
	}

	return 0;
}

/* Parse name and id of a front-end DAI (ie. cpu dai of a FE DAI link) */
static int tplg_parse_fe_dai(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
			     snd_config_t *cfg, void *private)
{
	struct tplg_elem *elem = private;
	struct snd_soc_tplg_pcm *pcm = elem->pcm;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *value = NULL;
	unsigned long int id_val;

	snd_config_get_id(cfg, &id);
	tplg_dbg("\t\tFE DAI %s:\n", id);
	elem_copy_text(pcm->dai_name, id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);

		/* get id */
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "id") == 0) {
			if (snd_config_get_string(n, &value) < 0)
				continue;
			errno = 0;
			/* no support for negative value */
			id_val = strtoul(value, NULL, 0);
			if ((errno == ERANGE && id_val == ULONG_MAX)
				|| (errno != 0 && id_val == 0)
				|| id_val > UINT_MAX) {
				SNDERR("error: invalid fe dai ID\n");
				return -EINVAL;
			}

			pcm->dai_id = (int) id_val;
			tplg_dbg("\t\t\tindex: %d\n", pcm->dai_id);
		}
	}

	return 0;
}

/* Parse pcm (for front end DAI & DAI link) */
int tplg_parse_pcm(snd_tplg_t *tplg,
	snd_config_t *cfg, void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_pcm *pcm;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;
	int err;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_PCM);
	if (!elem)
		return -ENOMEM;

	pcm = elem->pcm;
	pcm->size = elem->size;
	elem_copy_text(pcm->pcm_name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	tplg_dbg(" PCM: %s\n", elem->id);

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

		if (strcmp(id, "id") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			pcm->pcm_id = atoi(val);
			tplg_dbg("\t%s: %d\n", id, pcm->pcm_id);
			continue;
		}

		if (strcmp(id, "pcm") == 0) {
			err = tplg_parse_compound(tplg, n,
				tplg_parse_streams, elem);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "dai") == 0) {
			err = tplg_parse_compound(tplg, n,
				tplg_parse_fe_dai, elem);
			if (err < 0)
				return err;
			continue;
		}
	}

	return 0;
}

int tplg_parse_be(snd_tplg_t *tplg,
	snd_config_t *cfg, void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_link_config *link;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_BE);
	if (!elem)
		return -ENOMEM;

	link = elem->be;
	link->size = elem->size;

	tplg_dbg(" BE: %s\n", elem->id);

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

		if (strcmp(id, "id") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			link->id = atoi(val);
			tplg_dbg("\t%s: %d\n", id, link->id);
			continue;
		}
	}

	return 0;
}

/* Parse cc */
int tplg_parse_cc(snd_tplg_t *tplg,
	snd_config_t *cfg, void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_link_config *link;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_CC);
	if (!elem)
		return -ENOMEM;

	link = elem->cc;
	link->size = elem->size;

	tplg_dbg(" CC: %s\n", elem->id);

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

		if (strcmp(id, "id") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			link->id = atoi(val);
			tplg_dbg("\t%s: %d\n", id, link->id);
			continue;
		}

	}

	return 0;
}

/* copy stream object */
static void tplg_add_stream_object(struct snd_soc_tplg_stream *strm,
				struct snd_tplg_stream_template *strm_tpl)
{
	elem_copy_text(strm->name, strm_tpl->name,
		SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	strm->format = strm_tpl->format;
	strm->rate = strm_tpl->rate;
	strm->period_bytes = strm_tpl->period_bytes;
	strm->buffer_bytes = strm_tpl->buffer_bytes;
	strm->channels = strm_tpl->channels;
}

static void tplg_add_stream_caps(struct snd_soc_tplg_stream_caps *caps,
	struct snd_tplg_stream_caps_template *caps_tpl)
{
	elem_copy_text(caps->name, caps_tpl->name,
		SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	caps->formats = caps_tpl->formats;
	caps->rates = caps_tpl->rates;
	caps->rate_min = caps_tpl->rate_min;
	caps->rate_max = caps_tpl->rate_max;
	caps->channels_min = caps_tpl->channels_min;
	caps->channels_max = caps_tpl->channels_max;
	caps->periods_min = caps_tpl->periods_min;
	caps->periods_max = caps_tpl->periods_max;
	caps->period_size_min = caps_tpl->period_size_min;
	caps->period_size_max = caps_tpl->period_size_max;
	caps->buffer_size_min = caps_tpl->buffer_size_min;
	caps->buffer_size_max = caps_tpl->buffer_size_max;
}

int tplg_add_pcm_object(snd_tplg_t *tplg, snd_tplg_obj_template_t *t)
{
	struct snd_tplg_pcm_template *pcm_tpl = t->pcm;
	struct snd_soc_tplg_pcm *pcm;
	struct tplg_elem *elem;
	int i;

	tplg_dbg("PCM: %s, DAI %s\n", pcm_tpl->pcm_name, pcm_tpl->dai_name);

	if (pcm_tpl->num_streams > SND_SOC_TPLG_STREAM_CONFIG_MAX)
		return -EINVAL;

	elem = tplg_elem_new_common(tplg, NULL, pcm_tpl->pcm_name,
		SND_TPLG_TYPE_PCM);
	if (!elem)
		return -ENOMEM;

	pcm = elem->pcm;
	pcm->size = elem->size;

	elem_copy_text(pcm->pcm_name, pcm_tpl->pcm_name,
		SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	elem_copy_text(pcm->dai_name, pcm_tpl->dai_name,
		SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	pcm->pcm_id = pcm_tpl->pcm_id;
	pcm->dai_id = pcm_tpl->dai_id;
	pcm->playback = pcm_tpl->playback;
	pcm->capture = pcm_tpl->capture;
	pcm->compress = pcm_tpl->compress;

	for (i = 0; i < 2; i++) {
		if (pcm_tpl->caps[i])
			tplg_add_stream_caps(&pcm->caps[i], pcm_tpl->caps[i]);
	}

	pcm->num_streams = pcm_tpl->num_streams;
	for (i = 0; i < pcm_tpl->num_streams; i++)
		tplg_add_stream_object(&pcm->stream[i], &pcm_tpl->stream[i]);

	return 0;
}

int tplg_add_link_object(snd_tplg_t *tplg, snd_tplg_obj_template_t *t)
{
	struct snd_tplg_link_template *link = t->link;
	struct snd_soc_tplg_link_config *lk;
	struct tplg_elem *elem;
	int i;

	if (t->type != SND_TPLG_TYPE_BE && t->type != SND_TPLG_TYPE_CC)
		return -EINVAL;

	/* here type can be either BE or CC. */
	elem = tplg_elem_new_common(tplg, NULL, link->name, t->type);
	if (!elem)
		return -ENOMEM;

	if (t->type == SND_TPLG_TYPE_BE) {
		tplg_dbg("BE Link: %s", link->name);
		lk = elem->be;
	} else {
		tplg_dbg("CC Link: %s", link->name);
		lk = elem->cc;
	}

	lk->size = elem->size;
	lk->id = link->id;
	lk->num_streams = link->num_streams;

	for (i = 0; i < link->num_streams; i++)
		tplg_add_stream_object(&lk->stream[i], &link->stream[i]);

	return 0;
}
