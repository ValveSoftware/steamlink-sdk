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
#include <ctype.h>

/* Get private data buffer of an element */
struct snd_soc_tplg_private *get_priv_data(struct tplg_elem *elem)
{
	struct snd_soc_tplg_private *priv = NULL;

	switch (elem->type) {
	case SND_TPLG_TYPE_MANIFEST:
		priv = &elem->manifest->priv;
		break;

	case SND_TPLG_TYPE_MIXER:
		priv = &elem->mixer_ctrl->priv;
		break;

	case SND_TPLG_TYPE_ENUM:
		priv = &elem->enum_ctrl->priv;
		break;

	case SND_TPLG_TYPE_BYTES:
		priv = &elem->bytes_ext->priv;
		break;

	case SND_TPLG_TYPE_DAPM_WIDGET:
		priv = &elem->widget->priv;
		break;

	default:
		SNDERR("error: '%s': no support for private data for type %d\n",
			elem->id, elem->type);
	}

	return priv;
}

/* Get Private data from a file. */
static int tplg_parse_data_file(snd_config_t *cfg, struct tplg_elem *elem)
{
	struct snd_soc_tplg_private *priv = NULL;
	const char *value = NULL;
	char filename[MAX_FILE];
	char *env = getenv(ALSA_CONFIG_TPLG_VAR);
	FILE *fp;
	size_t size, bytes_read;
	int ret = 0;

	tplg_dbg("data DataFile: %s\n", elem->id);

	if (snd_config_get_string(cfg, &value) < 0)
		return -EINVAL;

	/* prepend alsa config directory to path */
	snprintf(filename, sizeof(filename), "%s/%s",
		env ? env : ALSA_TPLG_DIR, value);

	fp = fopen(filename, "r");
	if (fp == NULL) {
		SNDERR("error: invalid data file path '%s'\n",
			filename);
		ret = -errno;
		goto err;
	}

	fseek(fp, 0L, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	if (size <= 0) {
		SNDERR("error: invalid data file size %zu\n", size);
		ret = -EINVAL;
		goto err;
	}
	if (size > TPLG_MAX_PRIV_SIZE) {
		SNDERR("error: data file too big %zu\n", size);
		ret = -EINVAL;
		goto err;
	}

	priv = calloc(1, sizeof(*priv) + size);
	if (!priv) {
		ret = -ENOMEM;
		goto err;
	}

	bytes_read = fread(&priv->data, 1, size, fp);
	if (bytes_read != size) {
		ret = -errno;
		goto err;
	}

	elem->data = priv;
	priv->size = size;
	elem->size = sizeof(*priv) + size;
	return 0;

err:
	if (priv)
		free(priv);
	return ret;
}

static void dump_priv_data(struct tplg_elem *elem)
{
	struct snd_soc_tplg_private *priv = elem->data;
	unsigned char *p = (unsigned char *)priv->data;
	unsigned int i, j = 0;

	tplg_dbg(" elem size = %d, priv data size = %d\n",
		elem->size, priv->size);

	for (i = 0; i < priv->size; i++) {
		if (j++ % 8 == 0)
			tplg_dbg("\n");

		tplg_dbg(" 0x%x", *p++);
	}

	tplg_dbg("\n\n");
}

/* get number of hex value elements in CSV list */
static int get_hex_num(const char *str)
{
	int commas = 0, values = 0, len = strlen(str);
	const char *end = str + len;

	/* we expect "0x0, 0x0, 0x0" */
	while (str < end) {

		/* skip white space */
		if (isspace(*str)) {
			str++;
			continue;
		}

		/* find delimeters */
		if (*str == ',') {
			commas++;
			str++;
			continue;
		}

		/* find 0x[0-9] values */
		if (*str == '0' && str + 2 <= end) {
			if (str[1] == 'x' && str[2] >= '0' && str[2] <= 'f') {
				values++;
				str += 3;
			} else {
				str++;
			}
		}

		str++;
	}

	/* there should always be one less comma than value */
	if (values -1 != commas)
		return -EINVAL;

	return values;
}

/* get uuid from a string made by 16 characters separated by commas */
static int get_uuid(const char *str, unsigned char *uuid_le)
{
	unsigned long int  val;
	char *tmp, *s = NULL;
	int values = 0, ret = 0;

	tmp = strdup(str);
	if (tmp == NULL)
		return -ENOMEM;

	s = strtok(tmp, ",");

	while (s != NULL) {
		errno = 0;
		val = strtoul(s, NULL, 0);
		if ((errno == ERANGE && val == ULONG_MAX)
			|| (errno != 0 && val == 0)
			|| (val > UCHAR_MAX)) {
			SNDERR("error: invalid value for uuid\n");
			ret = -EINVAL;
			goto out;
		}

		 *(uuid_le + values) = (unsigned char)val;

		values++;
		if (values >= 16)
			break;

		s = strtok(NULL, ",");
	}

	if (values < 16) {
		SNDERR("error: less than 16 integers for uuid\n");
		ret = -EINVAL;
	}

out:
	free(tmp);
	return ret;
}

static int write_hex(char *buf, char *str, int width)
{
	long val;
	void *p = &val;

        errno = 0;
	val = strtol(str, NULL, 16);

	if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
		|| (errno != 0 && val == 0)) {
		return -EINVAL;
        }

	switch (width) {
	case 1:
		*(unsigned char *)buf = *(unsigned char *)p;
		break;
	case 2:
		*(unsigned short *)buf = *(unsigned short *)p;
		break;
	case 4:
		*(unsigned int *)buf = *(unsigned int *)p;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int copy_data_hex(char *data, int off, const char *str, int width)
{
	char *tmp, *s = NULL, *p = data;
	int ret;

	tmp = strdup(str);
	if (tmp == NULL)
		return -ENOMEM;

	p += off;
	s = strtok(tmp, ",");

	while (s != NULL) {
		ret = write_hex(p, s, width);
		if (ret < 0) {
			free(tmp);
			return ret;
		}

		s = strtok(NULL, ",");
		p += width;
	}

	free(tmp);
	return 0;
}

static int tplg_parse_data_hex(snd_config_t *cfg, struct tplg_elem *elem,
	int width)
{
	struct snd_soc_tplg_private *priv;
	const char *value = NULL;
	int size, esize, off, num;
	int ret;

	tplg_dbg(" data: %s\n", elem->id);

	if (snd_config_get_string(cfg, &value) < 0)
		return -EINVAL;

	num = get_hex_num(value);
	if (num <= 0) {
		SNDERR("error: malformed hex variable list %s\n", value);
		return -EINVAL;
	}

	size = num * width;
	priv = elem->data;

	if (size > TPLG_MAX_PRIV_SIZE) {
		SNDERR("error: data too big %d\n", size);
		return -EINVAL;
	}

	if (priv != NULL) {
		off = priv->size;
		esize = elem->size + size;
		priv = realloc(priv, esize);
	} else {
		off = 0;
		esize = sizeof(*priv) + size;
		priv = calloc(1, esize);
	}

	if (!priv)
		return -ENOMEM;

	elem->data = priv;
	priv->size += size;
	elem->size = esize;

	ret = copy_data_hex(priv->data, off, value, width);

	dump_priv_data(elem);
	return ret;
}

/* get the token integer value from its id */
static int get_token_value(const char *token_id,
	struct tplg_vendor_tokens *tokens)
{
	unsigned int i;

	for (i = 0; i < tokens->num_tokens; i++) {
		if (strcmp(token_id, tokens->token[i].id) == 0)
			return tokens->token[i].value;
	}

	SNDERR("error: cannot find token id '%s'\n", token_id);
	return -1;
}

/* get the vendor tokens referred by the vendor tuples */
static struct tplg_elem *get_tokens(snd_tplg_t *tplg, struct tplg_elem *elem)
{
	struct tplg_ref *ref;
	struct list_head *base, *pos;

	base = &elem->ref_list;
	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);

		if (!ref->id || ref->type != SND_TPLG_TYPE_TOKEN)
			continue;

		if (!ref->elem) {
			ref->elem = tplg_elem_lookup(&tplg->token_list,
						ref->id, SND_TPLG_TYPE_TOKEN);
		}

		return ref->elem;
	}

	return NULL;
}

/* check if a data element has tuples */
static bool has_tuples(struct tplg_elem *elem)
{
	struct tplg_ref *ref;
	struct list_head *base, *pos;

	base = &elem->ref_list;
	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);
		if (ref->id && ref->type == SND_TPLG_TYPE_TUPLE)
			return true;
	}

	return false;
}

/* get size of a tuple element from its type */
static unsigned int get_tuple_size(int type)
{
	switch (type) {

	case SND_SOC_TPLG_TUPLE_TYPE_UUID:
		return sizeof(struct snd_soc_tplg_vendor_uuid_elem);

	case SND_SOC_TPLG_TUPLE_TYPE_STRING:
		return sizeof(struct snd_soc_tplg_vendor_string_elem);

	default:
		return sizeof(struct snd_soc_tplg_vendor_value_elem);
	}
}

/* fill a data element's private buffer with its tuples */
static int copy_tuples(struct tplg_elem *elem,
	struct tplg_vendor_tuples *tuples, struct tplg_vendor_tokens *tokens)
{
	struct snd_soc_tplg_private *priv = elem->data;
	struct tplg_tuple_set *tuple_set;
	struct tplg_tuple *tuple;
	struct snd_soc_tplg_vendor_array *array;
	struct snd_soc_tplg_vendor_uuid_elem *uuid;
	struct snd_soc_tplg_vendor_string_elem *string;
	struct snd_soc_tplg_vendor_value_elem *value;
	int set_size, size, off;
	unsigned int i, j;
	int token_val;

	if (priv) {
		SNDERR("error: %s has more data than tuples\n", elem->id);
		return -EINVAL;
	}

	size = 0;
	for (i = 0; i < tuples->num_sets ; i++) {
		tuple_set = tuples->set[i];
		set_size = sizeof(struct snd_soc_tplg_vendor_array)
			+ get_tuple_size(tuple_set->type)
			* tuple_set->num_tuples;
		size += set_size;
		if (size > TPLG_MAX_PRIV_SIZE) {
			SNDERR("error: data too big %d\n", size);
			return -EINVAL;
		}

		if (priv != NULL)
			priv = realloc(priv, sizeof(*priv) + size);
		else
			priv = calloc(1, sizeof(*priv) + size);
		if (!priv)
			return -ENOMEM;

		off = priv->size;
		priv->size = size;

		array = (struct snd_soc_tplg_vendor_array *)(priv->data + off);
		array->size = set_size;
		array->type = tuple_set->type;
		array->num_elems = tuple_set->num_tuples;

		/* fill the private data buffer */
		for (j = 0; j < tuple_set->num_tuples; j++) {
			tuple = &tuple_set->tuple[j];
			token_val = get_token_value(tuple->token, tokens);
			if (token_val  < 0)
				return -EINVAL;

			switch (tuple_set->type) {
			case SND_SOC_TPLG_TUPLE_TYPE_UUID:
				uuid = &array->uuid[j];
				uuid->token = token_val;
				memcpy(uuid->uuid, tuple->uuid, 16);
				break;

			case SND_SOC_TPLG_TUPLE_TYPE_STRING:
				string = &array->string[j];
				string->token = token_val;
				elem_copy_text(string->string, tuple->string,
					SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
				break;

			default:
				value = &array->value[j];
				value->token = token_val;
				value->value = tuple->value;
				break;
			}
		}
	}

	elem->data = priv;
	return 0;
}

/* build a data element from its tuples */
static int build_tuples(snd_tplg_t *tplg, struct tplg_elem *elem)
{
	struct tplg_ref *ref;
	struct list_head *base, *pos;
	struct tplg_elem *tuples, *tokens;

	base = &elem->ref_list;
	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);

		if (!ref->id || ref->type != SND_TPLG_TYPE_TUPLE)
			continue;

		tplg_dbg("look up tuples %s\n", ref->id);

		if (!ref->elem)
			ref->elem = tplg_elem_lookup(&tplg->tuple_list,
						ref->id, SND_TPLG_TYPE_TUPLE);
		tuples = ref->elem;
		if (!tuples)
			return -EINVAL;

		tplg_dbg("found tuples %s\n", tuples->id);
		tokens = get_tokens(tplg, tuples);
		if (!tokens)
			return -EINVAL;

		tplg_dbg("found tokens %s\n", tokens->id);
		/* a data object can only have one tuples object */
		return copy_tuples(elem, tuples->tuples, tokens->tokens);
	}

	return 0;
}

static int parse_tuple_set(snd_config_t *cfg,
	struct tplg_tuple_set **s)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *value;
	struct tplg_tuple_set *set;
	unsigned int type, num_tuples = 0;
	struct tplg_tuple *tuple;
	unsigned long int tuple_val;

	snd_config_get_id(cfg, &id);

	if (strncmp(id, "uuid", 4) == 0)
		type = SND_SOC_TPLG_TUPLE_TYPE_UUID;
	else if (strncmp(id, "string", 5) == 0)
		type = SND_SOC_TPLG_TUPLE_TYPE_STRING;
	else if (strncmp(id, "bool", 4) == 0)
		type = SND_SOC_TPLG_TUPLE_TYPE_BOOL;
	else if (strncmp(id, "byte", 4) == 0)
		type = SND_SOC_TPLG_TUPLE_TYPE_BYTE;
	else if (strncmp(id, "short", 5) == 0)
		type = SND_SOC_TPLG_TUPLE_TYPE_SHORT;
	else if (strncmp(id, "word", 4) == 0)
		type = SND_SOC_TPLG_TUPLE_TYPE_WORD;
	else {
		SNDERR("error: invalid tuple type '%s'\n", id);
		return -EINVAL;
	}

	snd_config_for_each(i, next, cfg)
		num_tuples++;
	if (!num_tuples)
		return 0;

	tplg_dbg("\t %d %s tuples:\n", num_tuples, id);
	set = calloc(1, sizeof(*set) + num_tuples * sizeof(struct tplg_tuple));
	if (!set)
		return -ENOMEM;

	set->type = type;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);

		/* get id */
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* get value */
		if (snd_config_get_string(n, &value) < 0)
			continue;

		tuple = &set->tuple[set->num_tuples];
		elem_copy_text(tuple->token, id,
				SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

		switch (type) {
		case SND_SOC_TPLG_TUPLE_TYPE_UUID:
			if (get_uuid(value, tuple->uuid) < 0)
				goto err;
			break;

		case SND_SOC_TPLG_TUPLE_TYPE_STRING:
			elem_copy_text(tuple->string, value,
				SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
			tplg_dbg("\t\t%s = %s\n", tuple->token, tuple->string);
			break;

		case SND_SOC_TPLG_TUPLE_TYPE_BOOL:
			if (strcmp(value, "true") == 0)
				tuple->value = 1;
			tplg_dbg("\t\t%s = %d\n", tuple->token, tuple->value);
			break;

		case SND_SOC_TPLG_TUPLE_TYPE_BYTE:
		case SND_SOC_TPLG_TUPLE_TYPE_SHORT:
		case SND_SOC_TPLG_TUPLE_TYPE_WORD:
			errno = 0;
			/* no support for negative value */
			tuple_val = strtoul(value, NULL, 0);
			if ((errno == ERANGE && tuple_val == ULONG_MAX)
				|| (errno != 0 && tuple_val == 0)) {
				SNDERR("error: tuple %s:strtoul fail\n", id);
				goto err;
			}

			if ((type == SND_SOC_TPLG_TUPLE_TYPE_WORD
					&& tuple_val > UINT_MAX)
				|| (type == SND_SOC_TPLG_TUPLE_TYPE_SHORT
					&& tuple_val > USHRT_MAX)
				|| (type == SND_SOC_TPLG_TUPLE_TYPE_BYTE
					&& tuple_val > UCHAR_MAX)) {
				SNDERR("error: tuple %s: invalid value\n", id);
				goto err;
			}

			tuple->value = (unsigned int) tuple_val;
			tplg_dbg("\t\t%s = 0x%x\n", tuple->token, tuple->value);
			break;

		default:
			break;
		}

		set->num_tuples++;
	}

	*s = set;
	return 0;

err:
	free(set);
	return -EINVAL;
}

static int parse_tuple_sets(snd_config_t *cfg,
	struct tplg_vendor_tuples *tuples)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	unsigned int num_tuple_sets = 0;
	int err;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		snd_config_get_id(cfg, &id);
		SNDERR("error: compound type expected for %s", id);
		return -EINVAL;
	}

	snd_config_for_each(i, next, cfg) {
		num_tuple_sets++;
	}

	if (!num_tuple_sets)
		return 0;

	tuples->set = calloc(1, num_tuple_sets * sizeof(void *));
	if (!tuples->set)
		return -ENOMEM;

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_type(n) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("error: compound type expected for %s, is %d",
			id, snd_config_get_type(n));
			return -EINVAL;
		}

		err = parse_tuple_set(n, &tuples->set[tuples->num_sets]);
		if (err < 0)
			return err;

		/* overlook empty tuple sets */
		if (tuples->set[tuples->num_sets])
			tuples->num_sets++;
	}

	return 0;
}

/* Parse private data references for the element, either a single data section
 * or a list of data sections.
 */
int tplg_parse_data_refs(snd_config_t *cfg,
	struct tplg_elem *elem)
{
	snd_config_type_t  type;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *val = NULL;

	type = snd_config_get_type(cfg);

	/* refer to a single data section */
	if (type == SND_CONFIG_TYPE_STRING) {
		if (snd_config_get_string(cfg, &val) < 0)
			return -EINVAL;

		tplg_dbg("\tdata: %s\n", val);
		return tplg_ref_add(elem, SND_TPLG_TYPE_DATA, val);
	}

	if (type != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("error: compound type expected for %s", elem->id);
		return -EINVAL;
	}

	/* refer to a list of data sections */
	snd_config_for_each(i, next, cfg) {
		const char *val;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_string(n, &val) < 0)
			continue;

		tplg_dbg("\tdata: %s\n", val);
		tplg_ref_add(elem, SND_TPLG_TYPE_DATA, val);
	}

	return 0;
}

/* Parse vendor tokens
 */
int tplg_parse_tokens(snd_tplg_t *tplg, snd_config_t *cfg,
	void *private ATTRIBUTE_UNUSED)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *value;
	struct tplg_elem *elem;
	struct tplg_vendor_tokens *tokens;
	int num_tokens = 0;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_TOKEN);
	if (!elem)
		return -ENOMEM;

	snd_config_for_each(i, next, cfg) {
		num_tokens++;
	}

	if (!num_tokens)
		return 0;

	tplg_dbg(" Vendor tokens: %s, %d tokens\n", elem->id, num_tokens);

	tokens = calloc(1, sizeof(*tokens)
			+ num_tokens * sizeof(struct tplg_token));
	if (!tokens)
		return -ENOMEM;
	elem->tokens = tokens;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (snd_config_get_string(n, &value) < 0)
			continue;

		elem_copy_text(tokens->token[tokens->num_tokens].id, id,
				SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
		tokens->token[tokens->num_tokens].value = atoi(value);
		tplg_dbg("\t\t %s : %d\n", tokens->token[tokens->num_tokens].id,
			tokens->token[tokens->num_tokens].value);
		tokens->num_tokens++;
	}

	return 0;
}

/* Parse vendor tuples.
 */
int tplg_parse_tuples(snd_tplg_t *tplg, snd_config_t *cfg,
	void *private ATTRIBUTE_UNUSED)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *value;
	struct tplg_elem *elem;
	struct tplg_vendor_tuples *tuples;
	int err;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_TUPLE);
	if (!elem)
		return -ENOMEM;

	tplg_dbg(" Vendor Tuples: %s\n", elem->id);

	tuples = calloc(1, sizeof(*tuples));
	if (!tuples)
		return -ENOMEM;
	elem->tuples = tuples;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "tokens") == 0) {
			if (snd_config_get_string(n, &value) < 0)
				return -EINVAL;
			tplg_ref_add(elem, SND_TPLG_TYPE_TOKEN, value);
			tplg_dbg("\t refer to vendor tokens: %s\n", value);
		}

		if (strcmp(id, "tuples") == 0) {
			err = parse_tuple_sets(n, tuples);
			if (err < 0)
				return err;
		}
	}

	return 0;
}

/* Free handler of tuples */
void tplg_free_tuples(void *obj)
{
	struct tplg_vendor_tuples *tuples = (struct tplg_vendor_tuples *)obj;
	unsigned int i;

	if (!tuples || !tuples->set)
		return;

	for (i = 0; i < tuples->num_sets; i++)
		free(tuples->set[i]);

	free(tuples->set);
}

/* Parse manifest's data references
 */
int tplg_parse_manifest_data(snd_tplg_t *tplg, snd_config_t *cfg,
	void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_manifest *manifest;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	int err;

	if (!list_empty(&tplg->manifest_list)) {
		SNDERR("error: already has manifest data\n");
		return -EINVAL;
	}

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_MANIFEST);
	if (!elem)
		return -ENOMEM;

	manifest = elem->manifest;
	manifest->size = elem->size;

	tplg_dbg(" Manifest: %s\n", elem->id);

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;


		if (strcmp(id, "data") == 0) {
			err = tplg_parse_data_refs(n, elem);
			if (err < 0)
				return err;
			continue;
		}
	}

	return 0;
}

/* merge private data of manifest */
int tplg_build_manifest_data(snd_tplg_t *tplg)
{
	struct list_head *base, *pos;
	struct tplg_elem *elem = NULL;
	struct tplg_ref *ref;
	struct snd_soc_tplg_manifest *manifest;
	int err = 0;

	base = &tplg->manifest_list;
	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		break;
	}

	if (!elem) /* no manifest data */
		return 0;

	base = &elem->ref_list;

	/* for each ref in this manifest elem */
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

	manifest = elem->manifest;
	if (!manifest->priv.size) /* no manifest data */
		return 0;

	tplg->manifest_pdata = malloc(manifest->priv.size);
	if (!tplg->manifest_pdata)
		return -ENOMEM;

	tplg->manifest.priv.size = manifest->priv.size;
	memcpy(tplg->manifest_pdata, manifest->priv.data, manifest->priv.size);
	return 0;
}

/* Parse Private data.
 *
 * Object private data can either be from file or defined as bytes, shorts,
 * words, tuples.
 */
int tplg_parse_data(snd_tplg_t *tplg, snd_config_t *cfg,
	void *private ATTRIBUTE_UNUSED)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *val = NULL;
	int err = 0;
	struct tplg_elem *elem;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_DATA);
	if (!elem)
		return -ENOMEM;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0) {
			continue;
		}

		if (strcmp(id, "file") == 0) {
			err = tplg_parse_data_file(n, elem);
			if (err < 0) {
				SNDERR("error: failed to parse data file\n");
				return err;
			}
			continue;
		}

		if (strcmp(id, "bytes") == 0) {
			err = tplg_parse_data_hex(n, elem, 1);
			if (err < 0) {
				SNDERR("error: failed to parse data bytes\n");
				return err;
			}
			continue;
		}

		if (strcmp(id, "shorts") == 0) {
			err = tplg_parse_data_hex(n, elem, 2);
			if (err < 0) {
				SNDERR("error: failed to parse data shorts\n");
				return err;
			}
			continue;
		}

		if (strcmp(id, "words") == 0) {
			err = tplg_parse_data_hex(n, elem, 4);
			if (err < 0) {
				SNDERR("error: failed to parse data words\n");
				return err;
			}
			continue;
		}

		if (strcmp(id, "tuples") == 0) {
			if (snd_config_get_string(n, &val) < 0)
				return -EINVAL;
			tplg_dbg(" Data: %s\n", val);
			tplg_ref_add(elem, SND_TPLG_TYPE_TUPLE, val);
			continue;
		}

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

			elem->vendor_type = atoi(val);
			tplg_dbg("\t%s: %d\n", id, elem->index);
			continue;
		}
	}

	return err;
}

/* Find a referenced data element and copy its data to the parent
 * element's private data buffer.
 * An element can refer to multiple data sections. Data of these sections
 * will be merged in the their reference order.
 */
int tplg_copy_data(snd_tplg_t *tplg, struct tplg_elem *elem,
		   struct tplg_ref *ref)
{
	struct tplg_elem *ref_elem;
	struct snd_soc_tplg_private *priv, *old_priv;
	int priv_data_size, old_priv_data_size;
	void *obj;

	ref_elem = tplg_elem_lookup(&tplg->pdata_list,
				     ref->id, SND_TPLG_TYPE_DATA);
	if (!ref_elem) {
		SNDERR("error: cannot find data '%s' referenced by"
		" element '%s'\n", ref->id, elem->id);
		return -EINVAL;
	}

	tplg_dbg("Data '%s' used by '%s'\n", ref->id, elem->id);
	/* overlook empty private data */
	if (!ref_elem->data || !ref_elem->data->size) {
		ref->elem = ref_elem;
		return 0;
	}

	old_priv = get_priv_data(elem);
	if (!old_priv)
		return -EINVAL;
	old_priv_data_size = old_priv->size;

	priv_data_size = ref_elem->data->size;
	obj = realloc(elem->obj,
			elem->size + priv_data_size);
	if (!obj)
		return -ENOMEM;
	elem->obj = obj;

	priv = get_priv_data(elem);
	if (!priv)
		return -EINVAL;

	/* merge the new data block */
	elem->size += priv_data_size;
	priv->size = priv_data_size + old_priv_data_size;
	ref_elem->compound_elem = 1;
	memcpy(priv->data + old_priv_data_size,
	       ref_elem->data->data, priv_data_size);

	ref->elem = ref_elem;
	return 0;
}

/* check data objects and build those with tuples */
int tplg_build_data(snd_tplg_t *tplg)
{
	struct list_head *base, *pos;
	struct tplg_elem *elem;
	int err = 0;

	base = &tplg->pdata_list;
	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		if (has_tuples(elem)) {
			err = build_tuples(tplg, elem);
			if (err < 0)
				return err;
		}
	}

	return 0;
}
