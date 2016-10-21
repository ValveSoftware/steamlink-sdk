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

#include <sys/stat.h>
#include "list.h"
#include "tplg_local.h"

/*
 * Parse compound
 */
int tplg_parse_compound(snd_tplg_t *tplg, snd_config_t *cfg,
	int (*fcn)(snd_tplg_t *, snd_config_t *, void *),
	void *private)
{
	const char *id;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int err = -EINVAL;

	if (snd_config_get_id(cfg, &id) < 0)
		return -EINVAL;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("error: compound type expected for %s", id);
		return -EINVAL;
	}

	/* parse compound */
	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);

		if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("error: compound type expected for %s, is %d",
				id, snd_config_get_type(cfg));
			return -EINVAL;
		}

		err = fcn(tplg, n, private);
		if (err < 0)
			return err;
	}

	return err;
}

static int tplg_parse_config(snd_tplg_t *tplg, snd_config_t *cfg)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	int err;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("error: compound type expected at top level");
		return -EINVAL;
	}

	/* parse topology config sections */
	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "SectionTLV") == 0) {
			err = tplg_parse_compound(tplg, n, tplg_parse_tlv,
				NULL);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "SectionControlMixer") == 0) {
			err = tplg_parse_compound(tplg, n,
				tplg_parse_control_mixer, NULL);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "SectionControlEnum") == 0) {
			err = tplg_parse_compound(tplg, n,
				tplg_parse_control_enum, NULL);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "SectionControlBytes") == 0) {
			err = tplg_parse_compound(tplg, n,
				tplg_parse_control_bytes, NULL);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "SectionWidget") == 0) {
			err = tplg_parse_compound(tplg, n,
				tplg_parse_dapm_widget, NULL);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "SectionPCMCapabilities") == 0) {
			err = tplg_parse_compound(tplg, n,
				tplg_parse_stream_caps, NULL);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "SectionPCM") == 0) {
			err = tplg_parse_compound(tplg, n,
				tplg_parse_pcm, NULL);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "SectionBE") == 0) {
			err = tplg_parse_compound(tplg, n, tplg_parse_be,
				NULL);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "SectionCC") == 0) {
			err = tplg_parse_compound(tplg, n, tplg_parse_cc,
				NULL);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "SectionGraph") == 0) {
			err = tplg_parse_compound(tplg, n,
				tplg_parse_dapm_graph, NULL);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "SectionText") == 0) {
			err = tplg_parse_compound(tplg, n, tplg_parse_text,
				NULL);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "SectionData") == 0) {
			err = tplg_parse_compound(tplg, n, tplg_parse_data,
				NULL);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "SectionVendorTokens") == 0) {
			err = tplg_parse_compound(tplg, n, tplg_parse_tokens,
				NULL);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "SectionVendorTuples") == 0) {
			err = tplg_parse_compound(tplg, n, tplg_parse_tuples,
				NULL);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "SectionManifest") == 0) {
			err = tplg_parse_compound(tplg, n,
						  tplg_parse_manifest_data,
				NULL);
			if (err < 0)
				return err;
			continue;
		}

		SNDERR("error: unknown section %s\n", id);
	}
	return 0;
}

static int tplg_load_config(const char *file, snd_config_t **cfg)
{
	FILE *fp;
	snd_input_t *in;
	snd_config_t *top;
	int ret;

	fp = fopen(file, "r");
	if (fp == NULL) {
		SNDERR("error: could not open configuration file %s",
			file);
		return -errno;
	}

	ret = snd_input_stdio_attach(&in, fp, 1);
	if (ret < 0) {
		SNDERR("error: could not attach stdio %s", file);
		goto err;
	}
	ret = snd_config_top(&top);
	if (ret < 0)
		goto err;

	ret = snd_config_load(top, in);
	if (ret < 0) {
		SNDERR("error: could not load configuration file %s",
			file);
		goto err_load;
	}

	ret = snd_input_close(in);
	if (ret < 0)
		goto err_load;

	*cfg = top;
	return 0;

err_load:
	snd_config_delete(top);
err:
	fclose(fp);
	return ret;
}

static int tplg_build_integ(snd_tplg_t *tplg)
{
	int err;

	err = tplg_build_data(tplg);
	if (err <  0)
		return err;

	err = tplg_build_manifest_data(tplg);
	if (err <  0)
		return err;

	err = tplg_build_controls(tplg);
	if (err <  0)
		return err;

	err = tplg_build_widgets(tplg);
	if (err <  0)
		return err;

	err = tplg_build_pcm(tplg, SND_TPLG_TYPE_PCM);
	if (err <  0)
		return err;

	err = tplg_build_link_cfg(tplg, SND_TPLG_TYPE_BE);
	if (err <  0)
		return err;

	err = tplg_build_link_cfg(tplg, SND_TPLG_TYPE_CC);
	if (err <  0)
		return err;

	err = tplg_build_routes(tplg);
	if (err <  0)
		return err;

	return err;
}

int snd_tplg_build_file(snd_tplg_t *tplg, const char *infile,
	const char *outfile)
{
	snd_config_t *cfg = NULL;
	int err = 0;

	tplg->out_fd =
		open(outfile, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (tplg->out_fd < 0) {
		SNDERR("error: failed to open %s err %d\n",
			outfile, -errno);
		return -errno;
	}

	err = tplg_load_config(infile, &cfg);
	if (err < 0) {
		SNDERR("error: failed to load topology file %s\n",
			infile);
		goto out_close;
	}

	err = tplg_parse_config(tplg, cfg);
	if (err < 0) {
		SNDERR("error: failed to parse topology\n");
		goto out;
	}

	err = tplg_build_integ(tplg);
	if (err < 0) {
		SNDERR("error: failed to check topology integrity\n");
		goto out;
	}

	err = tplg_write_data(tplg);
	if (err < 0) {
		SNDERR("error: failed to write data %d\n", err);
		goto out;
	}

out:
	snd_config_delete(cfg);
out_close:
	close(tplg->out_fd);
	return err;
}

int snd_tplg_add_object(snd_tplg_t *tplg, snd_tplg_obj_template_t *t)
{
	switch (t->type) {
	case SND_TPLG_TYPE_MIXER:
		return tplg_add_mixer_object(tplg, t);
	case SND_TPLG_TYPE_ENUM:
		return tplg_add_enum_object(tplg, t);
	case SND_TPLG_TYPE_BYTES:
		return tplg_add_bytes_object(tplg, t);
	case SND_TPLG_TYPE_DAPM_WIDGET:
		return tplg_add_widget_object(tplg, t);
	case SND_TPLG_TYPE_DAPM_GRAPH:
		return tplg_add_graph_object(tplg, t);
	case SND_TPLG_TYPE_PCM:
		return tplg_add_pcm_object(tplg, t);
	case SND_TPLG_TYPE_BE:
	case SND_TPLG_TYPE_CC:
		return tplg_add_link_object(tplg, t);
	default:
		SNDERR("error: invalid object type %d\n", t->type);
		return -EINVAL;
	};
}

int snd_tplg_build(snd_tplg_t *tplg, const char *outfile)
{
	int err;

	tplg->out_fd =
		open(outfile, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (tplg->out_fd < 0) {
		SNDERR("error: failed to open %s err %d\n",
			outfile, -errno);
		return -errno;
	}

	err = tplg_build_integ(tplg);
	if (err < 0) {
		SNDERR("error: failed to check topology integrity\n");
		goto out;
	}

	err = tplg_write_data(tplg);
	if (err < 0) {
		SNDERR("error: failed to write data %d\n", err);
		goto out;
	}

out:
	close(tplg->out_fd);
	return err;
}

int snd_tplg_set_manifest_data(snd_tplg_t *tplg, const void *data, int len)
{
	if (len <= 0)
		return 0;

	tplg->manifest.priv.size = len;

	tplg->manifest_pdata = malloc(len);
	if (!tplg->manifest_pdata)
		return -ENOMEM;

	memcpy(tplg->manifest_pdata, data, len);
	return 0;
}

int snd_tplg_set_version(snd_tplg_t *tplg, unsigned int version)
{
	tplg->version = version;

	return 0;
}

void snd_tplg_verbose(snd_tplg_t *tplg, int verbose)
{
	tplg->verbose = verbose;
}

static bool is_little_endian(void)
{
#ifdef __BYTE_ORDER
	#if __BYTE_ORDER == __LITTLE_ENDIAN
		return true;
	#endif
#endif
	return false;
}

snd_tplg_t *snd_tplg_new(void)
{
	snd_tplg_t *tplg;

	if (!is_little_endian()) {
		SNDERR("error: cannot support big-endian machines\n");
		return NULL;
	}

	tplg = calloc(1, sizeof(snd_tplg_t));
	if (!tplg)
		return NULL;

	tplg->manifest.size = sizeof(struct snd_soc_tplg_manifest);

	INIT_LIST_HEAD(&tplg->tlv_list);
	INIT_LIST_HEAD(&tplg->widget_list);
	INIT_LIST_HEAD(&tplg->pcm_list);
	INIT_LIST_HEAD(&tplg->be_list);
	INIT_LIST_HEAD(&tplg->cc_list);
	INIT_LIST_HEAD(&tplg->route_list);
	INIT_LIST_HEAD(&tplg->pdata_list);
	INIT_LIST_HEAD(&tplg->manifest_list);
	INIT_LIST_HEAD(&tplg->text_list);
	INIT_LIST_HEAD(&tplg->pcm_config_list);
	INIT_LIST_HEAD(&tplg->pcm_caps_list);
	INIT_LIST_HEAD(&tplg->mixer_list);
	INIT_LIST_HEAD(&tplg->enum_list);
	INIT_LIST_HEAD(&tplg->bytes_ext_list);
	INIT_LIST_HEAD(&tplg->token_list);
	INIT_LIST_HEAD(&tplg->tuple_list);

	return tplg;
}

void snd_tplg_free(snd_tplg_t *tplg)
{
	if (tplg->manifest_pdata)
		free(tplg->manifest_pdata);

	tplg_elem_free_list(&tplg->tlv_list);
	tplg_elem_free_list(&tplg->widget_list);
	tplg_elem_free_list(&tplg->pcm_list);
	tplg_elem_free_list(&tplg->be_list);
	tplg_elem_free_list(&tplg->cc_list);
	tplg_elem_free_list(&tplg->route_list);
	tplg_elem_free_list(&tplg->pdata_list);
	tplg_elem_free_list(&tplg->manifest_list);
	tplg_elem_free_list(&tplg->text_list);
	tplg_elem_free_list(&tplg->pcm_config_list);
	tplg_elem_free_list(&tplg->pcm_caps_list);
	tplg_elem_free_list(&tplg->mixer_list);
	tplg_elem_free_list(&tplg->enum_list);
	tplg_elem_free_list(&tplg->bytes_ext_list);
	tplg_elem_free_list(&tplg->token_list);
	tplg_elem_free_list(&tplg->tuple_list);

	free(tplg);
}
