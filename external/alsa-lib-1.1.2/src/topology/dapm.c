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

/* mapping of widget text names to types */
static const struct map_elem widget_map[] = {
	{"input", SND_SOC_TPLG_DAPM_INPUT},
	{"output", SND_SOC_TPLG_DAPM_OUTPUT},
	{"mux", SND_SOC_TPLG_DAPM_MUX},
	{"mixer", SND_SOC_TPLG_DAPM_MIXER},
	{"pga", SND_SOC_TPLG_DAPM_PGA},
	{"out_drv", SND_SOC_TPLG_DAPM_OUT_DRV},
	{"adc", SND_SOC_TPLG_DAPM_ADC},
	{"dac", SND_SOC_TPLG_DAPM_DAC},
	{"switch", SND_SOC_TPLG_DAPM_SWITCH},
	{"pre", SND_SOC_TPLG_DAPM_PRE},
	{"post", SND_SOC_TPLG_DAPM_POST},
	{"aif_in", SND_SOC_TPLG_DAPM_AIF_IN},
	{"aif_out", SND_SOC_TPLG_DAPM_AIF_OUT},
	{"dai_in", SND_SOC_TPLG_DAPM_DAI_IN},
	{"dai_out", SND_SOC_TPLG_DAPM_DAI_OUT},
	{"dai_link", SND_SOC_TPLG_DAPM_DAI_LINK},
};

/* mapping of widget kcontrol text names to types */
static const struct map_elem widget_control_map[] = {
	{"volsw", SND_SOC_TPLG_DAPM_CTL_VOLSW},
	{"enum_double", SND_SOC_TPLG_DAPM_CTL_ENUM_DOUBLE},
	{"enum_virt", SND_SOC_TPLG_DAPM_CTL_ENUM_VIRT},
	{"enum_value", SND_SOC_TPLG_DAPM_CTL_ENUM_VALUE},
};

static int lookup_widget(const char *w)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(widget_map); i++) {
		if (strcmp(widget_map[i].name, w) == 0)
			return widget_map[i].id;
	}

	return -EINVAL;
}

static int tplg_parse_dapm_mixers(snd_config_t *cfg, struct tplg_elem *elem)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *value = NULL;

	tplg_dbg(" DAPM Mixer Controls: %s\n", elem->id);

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);

		/* get value */
		if (snd_config_get_string(n, &value) < 0)
			continue;

		tplg_ref_add(elem, SND_TPLG_TYPE_MIXER, value);
		tplg_dbg("\t\t %s\n", value);
	}

	return 0;
}

static int tplg_parse_dapm_enums(snd_config_t *cfg, struct tplg_elem *elem)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *value = NULL;

	tplg_dbg(" DAPM Enum Controls: %s\n", elem->id);

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);

		/* get value */
		if (snd_config_get_string(n, &value) < 0)
			continue;

		tplg_ref_add(elem, SND_TPLG_TYPE_ENUM, value);
		tplg_dbg("\t\t %s\n", value);
	}

	return 0;
}

static int tplg_parse_dapm_bytes(snd_config_t *cfg, struct tplg_elem *elem)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *value = NULL;

	tplg_dbg(" DAPM Bytes Controls: %s\n", elem->id);

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);

		/* get value */
		if (snd_config_get_string(n, &value) < 0)
			continue;

		tplg_ref_add(elem, SND_TPLG_TYPE_BYTES, value);
		tplg_dbg("\t\t %s\n", value);
	}

	return 0;
}

/* move referenced controls to the widget */
static int copy_dapm_control(struct tplg_elem *elem, struct tplg_elem *ref)
{
	struct snd_soc_tplg_dapm_widget *widget = elem->widget;

	tplg_dbg("Control '%s' used by '%s'\n", ref->id, elem->id);
	tplg_dbg("\tparent size: %d + %d -> %d, priv size -> %d\n",
		elem->size, ref->size, elem->size + ref->size,
		widget->priv.size);

	widget = realloc(widget, elem->size + ref->size);
	if (!widget)
		return -ENOMEM;

	elem->widget = widget;

	/* append the control to the end of the widget */
	memcpy((void*)widget + elem->size, ref->obj, ref->size);
	elem->size += ref->size;

	widget->num_kcontrols++;
	ref->compound_elem = 1;
	return 0;
}

/* check referenced controls for a widget */
static int tplg_build_widget(snd_tplg_t *tplg,
	struct tplg_elem *elem)
{
	struct tplg_ref *ref;
	struct list_head *base, *pos;
	int err = 0;

	base = &elem->ref_list;

	/* for each ref in this control elem */
	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);

		switch (ref->type) {
		case SND_TPLG_TYPE_MIXER:
			if (!ref->elem)
				ref->elem = tplg_elem_lookup(&tplg->mixer_list,
						ref->id, SND_TPLG_TYPE_MIXER);
			if (ref->elem)
				err = copy_dapm_control(elem, ref->elem);
			break;

		case SND_TPLG_TYPE_ENUM:
			if (!ref->elem)
				ref->elem = tplg_elem_lookup(&tplg->enum_list,
						ref->id, SND_TPLG_TYPE_ENUM);
			if (ref->elem)
				err = copy_dapm_control(elem, ref->elem);
			break;

		case SND_TPLG_TYPE_BYTES:
			if (!ref->elem)
				ref->elem = tplg_elem_lookup(&tplg->bytes_ext_list,
						ref->id, SND_TPLG_TYPE_BYTES);
			if (ref->elem)
				err = copy_dapm_control(elem, ref->elem);
			break;

		case SND_TPLG_TYPE_DATA:
			err = tplg_copy_data(tplg, elem, ref);
			if (err < 0)
				return err;
			break;

		default:
			break;
		}

		if (!ref->elem) {
			SNDERR("error: cannot find '%s'"
				" referenced by widget '%s'\n",
				ref->id, elem->id);
			return -EINVAL;
		}

		if (err < 0)
			return err;
	}

	return 0;
}

int tplg_build_widgets(snd_tplg_t *tplg)
{

	struct list_head *base, *pos;
	struct tplg_elem *elem;
	int err;

	base = &tplg->widget_list;
	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		if (!elem->widget || elem->type != SND_TPLG_TYPE_DAPM_WIDGET) {
			SNDERR("error: invalid widget '%s'\n",
				elem->id);
			return -EINVAL;
		}

		err = tplg_build_widget(tplg, elem);
		if (err < 0)
			return err;

		/* add widget to manifest */
		tplg->manifest.widget_elems++;
	}

	return 0;
}

int tplg_build_routes(snd_tplg_t *tplg)
{
	struct list_head *base, *pos;
	struct tplg_elem *elem;
	struct snd_soc_tplg_dapm_graph_elem *route;

	base = &tplg->route_list;

	list_for_each(pos, base) {
		elem = list_entry(pos, struct tplg_elem, list);

		if (!elem->route || elem->type != SND_TPLG_TYPE_DAPM_GRAPH) {
			SNDERR("error: invalid route '%s'\n",
				elem->id);
			return -EINVAL;
		}

		route = elem->route;
		tplg_dbg("\nCheck route: sink '%s', control '%s', source '%s'\n",
			route->sink, route->control, route->source);

		/* validate sink */
		if (strlen(route->sink) <= 0) {
			SNDERR("error: no sink\n");
			return -EINVAL;

		}
		if (!tplg_elem_lookup(&tplg->widget_list, route->sink,
			SND_TPLG_TYPE_DAPM_WIDGET)) {
			SNDERR("warning: undefined sink widget/stream '%s'\n",
				route->sink);
		}

		/* validate control name */
		if (strlen(route->control)) {
			if (!tplg_elem_lookup(&tplg->mixer_list,
				route->control, SND_TPLG_TYPE_MIXER) &&
			!tplg_elem_lookup(&tplg->enum_list,
				route->control, SND_TPLG_TYPE_ENUM)) {
				SNDERR("warning: Undefined mixer/enum control '%s'\n",
					route->control);
			}
		}

		/* validate source */
		if (strlen(route->source) <= 0) {
			SNDERR("error: no source\n");
			return -EINVAL;

		}
		if (!tplg_elem_lookup(&tplg->widget_list, route->source,
			SND_TPLG_TYPE_DAPM_WIDGET)) {
			SNDERR("warning: Undefined source widget/stream '%s'\n",
				route->source);
		}

		/* add graph to manifest */
		tplg->manifest.graph_elems++;
	}

	return 0;
}

struct tplg_elem* tplg_elem_new_route(snd_tplg_t *tplg)
{
	struct tplg_elem *elem;
	struct snd_soc_tplg_dapm_graph_elem *line;

	elem = tplg_elem_new();
	if (!elem)
		return NULL;

	list_add_tail(&elem->list, &tplg->route_list);
	strcpy(elem->id, "line");
	elem->type = SND_TPLG_TYPE_DAPM_GRAPH;
	elem->size = sizeof(*line);

	line = calloc(1, sizeof(*line));
	if (!line) {
		tplg_elem_free(elem);
		return NULL;
	}
	elem->route = line;

	return elem;
}

#define LINE_SIZE	1024

/* line is defined as '"source, control, sink"' */
static int tplg_parse_line(const char *text,
	struct snd_soc_tplg_dapm_graph_elem *line)
{
	char buf[LINE_SIZE];
	unsigned int len, i;
	const char *source = NULL, *sink = NULL, *control = NULL;

	elem_copy_text(buf, text, LINE_SIZE);

	len = strlen(buf);
	if (len <= 2) {
		SNDERR("error: invalid route \"%s\"\n", buf);
		return -EINVAL;
	}

	/* find first , */
	for (i = 1; i < len; i++) {
		if (buf[i] == ',')
			goto second;
	}
	SNDERR("error: invalid route \"%s\"\n", buf);
	return -EINVAL;

second:
	/* find second , */
	sink = buf;
	control = &buf[i + 2];
	buf[i] = 0;

	for (; i < len; i++) {
		if (buf[i] == ',')
			goto done;
	}

	SNDERR("error: invalid route \"%s\"\n", buf);
	return -EINVAL;

done:
	buf[i] = 0;
	source = &buf[i + 2];

	strcpy(line->source, source);
	strcpy(line->control, control);
	strcpy(line->sink, sink);
	return 0;
}


static int tplg_parse_routes(snd_tplg_t *tplg, snd_config_t *cfg)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	struct tplg_elem *elem;
	struct snd_soc_tplg_dapm_graph_elem *line;
	int err;

	snd_config_for_each(i, next, cfg) {
		const char *val;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_string(n, &val) < 0)
			continue;

		elem = tplg_elem_new_route(tplg);
		if (!elem)
			return -ENOMEM;

		line = elem->route;

		err = tplg_parse_line(val, line);
		if (err < 0)
			return err;

		tplg_dbg("route: sink '%s', control '%s', source '%s'\n",
				line->sink, line->control, line->source);
	}

	return 0;
}

int tplg_parse_dapm_graph(snd_tplg_t *tplg, snd_config_t *cfg,
	void *private ATTRIBUTE_UNUSED)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int err;
	const char *graph_id;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("error: compound is expected for dapm graph definition\n");
		return -EINVAL;
	}

	snd_config_get_id(cfg, &graph_id);

	snd_config_for_each(i, next, cfg) {
		const char *id;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0) {
			continue;
		}

		if (strcmp(id, "lines") == 0) {
			err = tplg_parse_routes(tplg, n);
			if (err < 0) {
				SNDERR("error: failed to parse dapm graph %s\n",
					graph_id);
				return err;
			}
			continue;
		}
	}

	return 0;
}

/* DAPM Widget */
int tplg_parse_dapm_widget(snd_tplg_t *tplg,
	snd_config_t *cfg, void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_dapm_widget *widget;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;
	int widget_type, err;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_DAPM_WIDGET);
	if (!elem)
		return -ENOMEM;

	tplg_dbg(" Widget: %s\n", elem->id);

	widget = elem->widget;
	elem_copy_text(widget->name, elem->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	widget->size = elem->size;

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

		if (strcmp(id, "type") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			widget_type = lookup_widget(val);
			if (widget_type < 0){
				SNDERR("Widget '%s': Unsupported widget type %s\n",
					elem->id, val);
				return -EINVAL;
			}

			widget->id = widget_type;
			tplg_dbg("\t%s: %s\n", id, val);
			continue;
		}

		if (strcmp(id, "no_pm") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			if (strcmp(val, "true") == 0)
				widget->reg = -1;

			tplg_dbg("\t%s: %s\n", id, val);
			continue;
		}

		if (strcmp(id, "shift") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			widget->shift = atoi(val);
			tplg_dbg("\t%s: %d\n", id, widget->shift);
			continue;
		}

		if (strcmp(id, "reg") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			widget->reg = atoi(val);
			tplg_dbg("\t%s: %d\n", id, widget->reg);
			continue;
		}

		if (strcmp(id, "invert") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			widget->invert = atoi(val);
			tplg_dbg("\t%s: %d\n", id, widget->invert);
			continue;
		}

		if (strcmp(id, "subseq") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			widget->subseq= atoi(val);
			tplg_dbg("\t%s: %d\n", id, widget->subseq);
			continue;
		}

		if (strcmp(id, "event_type") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			widget->event_type = atoi(val);
			tplg_dbg("\t%s: %d\n", id, widget->event_type);
			continue;
		}

		if (strcmp(id, "event_flags") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;

			widget->event_flags = atoi(val);
			tplg_dbg("\t%s: %d\n", id, widget->event_flags);
			continue;
		}

		if (strcmp(id, "enum") == 0) {
			err = tplg_parse_dapm_enums(n, elem);
			if (err < 0)
				return err;

			continue;
		}

		if (strcmp(id, "mixer") == 0) {
			err = tplg_parse_dapm_mixers(n, elem);
			if (err < 0)
				return err;

			continue;
		}

		if (strcmp(id, "bytes") == 0) {
			err = tplg_parse_dapm_bytes(n, elem);
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
	}

	return 0;
}

int tplg_add_route(snd_tplg_t *tplg, struct snd_tplg_graph_elem *t)
{
	struct tplg_elem *elem;
	struct snd_soc_tplg_dapm_graph_elem *line;

	if (!t->src || !t->sink)
		return -EINVAL;

	elem = tplg_elem_new_route(tplg);
	if (!elem)
		return -ENOMEM;

	line = elem->route;
	elem_copy_text(line->source, t->src, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	if (t->ctl)
		elem_copy_text(line->control, t->ctl,
			SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	elem_copy_text(line->sink, t->sink, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

	return 0;
}

int tplg_add_graph_object(snd_tplg_t *tplg, snd_tplg_obj_template_t *t)
{
	struct snd_tplg_graph_template *gt =  t->graph;
	int i, ret;

	for (i = 0; i < gt->count; i++) {
		ret = tplg_add_route(tplg, gt->elem + i);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int tplg_add_widget_object(snd_tplg_t *tplg, snd_tplg_obj_template_t *t)
{
	struct snd_tplg_widget_template *wt = t->widget;
	struct snd_soc_tplg_dapm_widget *w;
	struct tplg_elem *elem;
	int i, ret = 0;

	tplg_dbg("Widget: %s\n", wt->name);

	elem = tplg_elem_new_common(tplg, NULL, wt->name,
		SND_TPLG_TYPE_DAPM_WIDGET);
	if (!elem)
		return -ENOMEM;

	/* init new widget */
	w = elem->widget;
	w->size = elem->size;

	w->id = wt->id;
	elem_copy_text(w->name, wt->name, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	if (wt->sname)
		elem_copy_text(w->sname, wt->sname, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	w->reg = wt->reg;
	w->shift = wt->shift;
	w->mask = wt->mask;
	w->subseq = wt->subseq;
	w->invert = wt->invert;
	w->ignore_suspend = wt->ignore_suspend;
	w->event_flags = wt->event_flags;
	w->event_type = wt->event_type;

	if (wt->priv != NULL) {
		w = realloc(w,
			elem->size + wt->priv->size);
		if (!w) {
			tplg_elem_free(elem);
			return -ENOMEM;
		}

		elem->widget = w;
		elem->size += wt->priv->size;

		memcpy(w->priv.data, wt->priv->data,
			wt->priv->size);
		w->priv.size = wt->priv->size;
	}

	/* add controls to the widget's reference list */
	for (i = 0 ; i < wt->num_ctls; i++) {
		struct snd_tplg_ctl_template *ct = wt->ctl[i];
		struct tplg_elem *elem_ctl;
		struct snd_tplg_mixer_template *mt;
		struct snd_tplg_bytes_template *bt;
		struct snd_tplg_enum_template *et;

		if (!ct) {
			tplg_elem_free(elem);
			return -EINVAL;
		}

		switch (ct->type) {
		case SND_SOC_TPLG_TYPE_MIXER:
			mt = container_of(ct, struct snd_tplg_mixer_template, hdr);
			ret = tplg_add_mixer(tplg, mt, &elem_ctl);
			break;

		case SND_SOC_TPLG_TYPE_BYTES:
			bt = container_of(ct, struct snd_tplg_bytes_template, hdr);
			ret = tplg_add_bytes(tplg, bt, &elem_ctl);
			break;

		case SND_SOC_TPLG_TYPE_ENUM:
			et = container_of(ct, struct snd_tplg_enum_template, hdr);
			ret = tplg_add_enum(tplg, et, &elem_ctl);
			break;

		default:
			SNDERR("error: widget %s: invalid type %d for ctl %d\n",
				wt->name, ct->type, i);
			ret = -EINVAL;
			break;
		}

		if (ret < 0) {
			tplg_elem_free(elem);
			return ret;
		}

		ret = tplg_ref_add_elem(elem, elem_ctl);
		if (ret < 0) {
			tplg_elem_free(elem);
			return ret;
		}
	}

	return 0;
}
