/*
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software  
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 *  Support for the verb/device/modifier core logic and API,
 *  command line tool and file parser was kindly sponsored by
 *  Texas Instruments Inc.
 *  Support for multiple active modifiers and devices,
 *  transition sequences, multiple client access and user defined use
 *  cases was kindly sponsored by Wolfson Microelectronics PLC.
 *
 *  Copyright (C) 2008-2010 SlimLogic Ltd
 *  Copyright (C) 2010 Wolfson Microelectronics PLC
 *  Copyright (C) 2010 Texas Instruments Inc.
 *  Copyright (C) 2010 Red Hat Inc.
 *  Authors: Liam Girdwood <lrg@slimlogic.co.uk>
 *	         Stefan Schmidt <stefan@slimlogic.co.uk>
 *	         Justin Xu <justinx@slimlogic.co.uk>
 *               Jaroslav Kysela <perex@perex.cz>
 */

#include "ucm_local.h"
#include <dirent.h>

/** The name of the environment variable containing the UCM directory */
#define ALSA_CONFIG_UCM_VAR "ALSA_CONFIG_UCM"

static int parse_sequence(snd_use_case_mgr_t *uc_mgr,
			  struct list_head *base,
			  snd_config_t *cfg);

/*
 * Parse string
 */
int parse_string(snd_config_t *n, char **res)
{
	int err;

	err = snd_config_get_string(n, (const char **)res);
	if (err < 0)
		return err;
	*res = strdup(*res);
	if (*res == NULL)
		return -ENOMEM;
	return 0;
}

/*
 * Parse safe ID
 */
int parse_is_name_safe(const char *name)
{
	if (strchr(name, '.')) {
		uc_error("char '.' not allowed in '%s'", name);
		return 0;
	}
	return 1;
}

int parse_get_safe_id(snd_config_t *n, const char **id)
{
	int err;

	err = snd_config_get_id(n, id);
	if (err < 0)
		return err;
	if (!parse_is_name_safe((char *)(*id)))
		return -EINVAL;
	return 0;
}

/*
 * Parse transition
 */
static int parse_transition(snd_use_case_mgr_t *uc_mgr,
			    struct list_head *tlist,
			    snd_config_t *cfg)
{
	struct transition_sequence *tseq;
	const char *id;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int err;

	if (snd_config_get_id(cfg, &id) < 0)
		return -EINVAL;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for %s", id);
		return -EINVAL;
	}

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);

		if (snd_config_get_id(n, &id) < 0)
			return -EINVAL;

		tseq = calloc(1, sizeof(*tseq));
		if (tseq == NULL)
			return -ENOMEM;
		INIT_LIST_HEAD(&tseq->transition_list);

		tseq->name = strdup(id);
		if (tseq->name == NULL) {
			free(tseq);
			return -ENOMEM;
		}
	
		err = parse_sequence(uc_mgr, &tseq->transition_list, n);
		if (err < 0) {
			uc_mgr_free_transition_element(tseq);
			return err;
		}

		list_add(&tseq->list, tlist);
	}
	return 0;
}

/*
 * Parse compound
 */
static int parse_compound(snd_use_case_mgr_t *uc_mgr, snd_config_t *cfg,
	  int (*fcn)(snd_use_case_mgr_t *, snd_config_t *, void *, void *),
	  void *data1, void *data2)
{
	const char *id;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int err;

	if (snd_config_get_id(cfg, &id) < 0)
		return -EINVAL;
	
	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for %s", id);
		return -EINVAL;
	}
	/* parse compound */
	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);

		if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
			uc_error("compound type expected for %s, is %d", id, snd_config_get_type(cfg));
			return -EINVAL;
		}
		
		err = fcn(uc_mgr, n, data1, data2);
		if (err < 0)
			return err;
	}

	return 0;
}

static int strip_legacy_dev_index(char *name)
{
	char *dot = strchr(name, '.');
	if (!dot)
		return 0;
	if (dot[1] != '0' || dot[2] != '\0') {
		uc_error("device name %s contains a '.',"
			 " and is not legacy foo.0 format", name);
		return -EINVAL;
	}
	*dot = '\0';
	return 0;
}

/*
 * Parse device list
 */
static int parse_device_list(snd_use_case_mgr_t *uc_mgr ATTRIBUTE_UNUSED,
			     struct dev_list *dev_list,
			     enum dev_list_type type,
			     snd_config_t *cfg)
{
	struct dev_list_node *sdev;
	const char *id;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int err;

	if (dev_list->type != DEVLIST_NONE) {
		uc_error("error: multiple supported or"
			" conflicting device lists");
		return -EEXIST;
	}

	if (snd_config_get_id(cfg, &id) < 0)
		return -EINVAL;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for %s", id);
		return -EINVAL;
	}

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);

		if (snd_config_get_id(n, &id) < 0)
			return -EINVAL;

		sdev = calloc(1, sizeof(struct dev_list_node));
		if (sdev == NULL)
			return -ENOMEM;
		err = parse_string(n, &sdev->name);
		if (err < 0) {
			free(sdev);
			return err;
		}
		err = strip_legacy_dev_index(sdev->name);
		if (err < 0) {
			free(sdev->name);
			free(sdev);
			return err;
		}
		list_add(&sdev->list, &dev_list->list);
	}

	dev_list->type = type;

	return 0;
}

/*
 * Parse sequences.
 *
 * Sequence controls elements  are in the following form:-
 *
 * cdev "hw:0"
 * cset "element_id_syntax value_syntax"
 * usleep time
 * exec "any unix command with arguments"
 *
 * e.g.
 *	cset "name='Master Playback Switch' 0,0"
 *      cset "iface=PCM,name='Disable HDMI',index=1 0"
 */
static int parse_sequence(snd_use_case_mgr_t *uc_mgr ATTRIBUTE_UNUSED,
			  struct list_head *base,
			  snd_config_t *cfg)
{
	struct sequence_element *curr;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int err, idx = 0;
	const char *cmd = NULL;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("error: compound is expected for sequence definition");
		return -EINVAL;
	}

	snd_config_for_each(i, next, cfg) {
		const char *id;
		idx ^= 1;
		n = snd_config_iterator_entry(i);
		err = snd_config_get_id(n, &id);
		if (err < 0)
			continue;
		if (idx == 1) {
			if (snd_config_get_type(n) != SND_CONFIG_TYPE_STRING) {
				uc_error("error: string type is expected for sequence command");
				return -EINVAL;
			}
			snd_config_get_string(n, &cmd);
			continue;
		}

		/* alloc new sequence element */
		curr = calloc(1, sizeof(struct sequence_element));
		if (curr == NULL)
			return -ENOMEM;
		list_add_tail(&curr->list, base);

		if (strcmp(cmd, "cdev") == 0) {
			curr->type = SEQUENCE_ELEMENT_TYPE_CDEV;
			err = parse_string(n, &curr->data.cdev);
			if (err < 0) {
				uc_error("error: cdev requires a string!");
				return err;
			}
			continue;
		}

		if (strcmp(cmd, "cset") == 0) {
			curr->type = SEQUENCE_ELEMENT_TYPE_CSET;
			err = parse_string(n, &curr->data.cset);
			if (err < 0) {
				uc_error("error: cset requires a string!");
				return err;
			}
			continue;
		}

		if (strcmp(cmd, "cset-bin-file") == 0) {
			curr->type = SEQUENCE_ELEMENT_TYPE_CSET_BIN_FILE;
			err = parse_string(n, &curr->data.cset);
			if (err < 0) {
				uc_error("error: cset-bin-file requires a string!");
				return err;
			}
			continue;
		}

		if (strcmp(cmd, "cset-tlv") == 0) {
			curr->type = SEQUENCE_ELEMENT_TYPE_CSET_TLV;
			err = parse_string(n, &curr->data.cset);
			if (err < 0) {
				uc_error("error: cset-tlv requires a string!");
				return err;
			}
			continue;
		}

		if (strcmp(cmd, "usleep") == 0) {
			curr->type = SEQUENCE_ELEMENT_TYPE_SLEEP;
			err = snd_config_get_integer(n, &curr->data.sleep);
			if (err < 0) {
				uc_error("error: usleep requires integer!");
				return err;
			}
			continue;
		}

		if (strcmp(cmd, "msleep") == 0) {
			curr->type = SEQUENCE_ELEMENT_TYPE_SLEEP;
			err = snd_config_get_integer(n, &curr->data.sleep);
			if (err < 0) {
				uc_error("error: msleep requires integer!");
				return err;
			}
			curr->data.sleep *= 1000L;
			continue;
		}

		if (strcmp(cmd, "exec") == 0) {
			curr->type = SEQUENCE_ELEMENT_TYPE_EXEC;
			err = parse_string(n, &curr->data.exec);
			if (err < 0) {
				uc_error("error: exec requires a string!");
				return err;
			}
			continue;
		}
		
		list_del(&curr->list);
		uc_mgr_free_sequence_element(curr);
	}

	return 0;
}

/*
 * Parse values.
 *
 * Parse values describing PCM, control/mixer settings and stream parameters.
 *
 * Value {
 *   TQ Voice
 *   CapturePCM "hw:1"
 *   PlaybackVolume "name='Master Playback Volume',index=2"
 *   PlaybackSwitch "name='Master Playback Switch',index=2"
 * }
 */
static int parse_value(snd_use_case_mgr_t *uc_mgr ATTRIBUTE_UNUSED,
			  struct list_head *base,
			  snd_config_t *cfg)
{
	struct ucm_value *curr;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	long l;
	long long ll;
	double d;
	snd_config_type_t type;
	int err;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("error: compound is expected for value definition");
		return -EINVAL;
	}
	snd_config_for_each(i, next, cfg) {
		const char *id;
		n = snd_config_iterator_entry(i);
		err = snd_config_get_id(n, &id);
		if (err < 0)
			continue;

		/* alloc new value */
		curr = calloc(1, sizeof(struct ucm_value));
		if (curr == NULL)
			return -ENOMEM;
		list_add_tail(&curr->list, base);
		curr->name = strdup(id);
		if (curr->name == NULL)
			return -ENOMEM;
		type = snd_config_get_type(n);
		switch (type) {
		case SND_CONFIG_TYPE_INTEGER:
			curr->data = malloc(16);
			if (curr->data == NULL)
				return -ENOMEM;
			snd_config_get_integer(n, &l);
			sprintf(curr->data, "%li", l);
			break;
		case SND_CONFIG_TYPE_INTEGER64:
			curr->data = malloc(32);
			if (curr->data == NULL)
				return -ENOMEM;
			snd_config_get_integer64(n, &ll);
			sprintf(curr->data, "%lli", ll);
			break;
		case SND_CONFIG_TYPE_REAL:
			curr->data = malloc(64);
			if (curr->data == NULL)
				return -ENOMEM;
			snd_config_get_real(n, &d);
			sprintf(curr->data, "%-16g", d);
			break;
		case SND_CONFIG_TYPE_STRING:
			err = parse_string(n, &curr->data);
			if (err < 0) {
				uc_error("error: unable to parse a string for id '%s'!", id);
				return err;
			}
			break;
		default:
			uc_error("error: invalid type %i in Value compound", type);
			return -EINVAL;
		}
	}

	return 0;
}

/*
 * Parse Modifier Use cases
 *
 *	# Each modifier is described in new section. N modifiers are allowed
 *	SectionModifier."Capture Voice" {
 *
 *		Comment "Record voice call"
 *
 *		SupportedDevice [
 *			"x"
 *			"y"
 *		]
 *
 *		ConflictingDevice [
 *			"x"
 *			"y"
 *		]
 *
 *		EnableSequence [
 *			....
 *		]
 *
 *		DisableSequence [
 *			...
 *		]
 *
 *              TransitionSequence."ToModifierName" [
 *			...
 *		]
 *
 *		# Optional TQ and ALSA PCMs
 *		Value {
 *			TQ Voice
 *			CapturePCM "hw:1"
 *			PlaybackVolume "name='Master Playback Volume',index=2"
 *			PlaybackSwitch "name='Master Playback Switch',index=2"
 *		}
 *
 *	 }
 *
 * SupportedDevice and ConflictingDevice cannot be specified together.
 * Both are optional.
 */
static int parse_modifier(snd_use_case_mgr_t *uc_mgr,
		snd_config_t *cfg,
		void *data1,
		void *data2)
{
	struct use_case_verb *verb = data1;
	struct use_case_modifier *modifier;
	const char *name;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int err;

	if (data2) {
		name = data2;
		if (!parse_is_name_safe(name))
			return -EINVAL;
	}
	else {
		if (parse_get_safe_id(cfg, &name) < 0)
			return -EINVAL;
	}

	/* allocate modifier */
	modifier = calloc(1, sizeof(*modifier));
	if (modifier == NULL)
		return -ENOMEM;
	INIT_LIST_HEAD(&modifier->enable_list);
	INIT_LIST_HEAD(&modifier->disable_list);
	INIT_LIST_HEAD(&modifier->transition_list);
	INIT_LIST_HEAD(&modifier->dev_list.list);
	INIT_LIST_HEAD(&modifier->value_list);
	list_add_tail(&modifier->list, &verb->modifier_list);
	modifier->name = strdup(name);

	snd_config_for_each(i, next, cfg) {
		const char *id;
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "Comment") == 0) {
			err = parse_string(n, &modifier->comment);
			if (err < 0) {
				uc_error("error: failed to get modifier comment");
				return err;
			}
			continue;
		}

		if (strcmp(id, "SupportedDevice") == 0) {
			err = parse_device_list(uc_mgr, &modifier->dev_list,
						DEVLIST_SUPPORTED, n);
			if (err < 0) {
				uc_error("error: failed to parse supported"
					" device list");
				return err;
			}
		}

		if (strcmp(id, "ConflictingDevice") == 0) {
			err = parse_device_list(uc_mgr, &modifier->dev_list,
						DEVLIST_CONFLICTING, n);
			if (err < 0) {
				uc_error("error: failed to parse conflicting"
					" device list");
				return err;
			}
		}

		if (strcmp(id, "EnableSequence") == 0) {
			err = parse_sequence(uc_mgr, &modifier->enable_list, n);
			if (err < 0) {
				uc_error("error: failed to parse modifier"
					" enable sequence");
				return err;
			}
			continue;
		}

		if (strcmp(id, "DisableSequence") == 0) {
			err = parse_sequence(uc_mgr, &modifier->disable_list, n);
			if (err < 0) {
				uc_error("error: failed to parse modifier"
					" disable sequence");
				return err;
			}
			continue;
		}

		if (strcmp(id, "TransitionSequence") == 0) {
			err = parse_transition(uc_mgr, &modifier->transition_list, n);
			if (err < 0) {
				uc_error("error: failed to parse transition"
					" modifier");
				return err;
			}
			continue;
		}

		if (strcmp(id, "Value") == 0) {
			err = parse_value(uc_mgr, &modifier->value_list, n);
			if (err < 0) {
				uc_error("error: failed to parse Value");
				return err;
			}
			continue;
		}
	}

	return 0;
}

/*
 * Parse Device Use Cases
 *
 *# Each device is described in new section. N devices are allowed
 *SectionDevice."Headphones" {
 *	Comment "Headphones connected to 3.5mm jack"
 *
 *	upportedDevice [
 *		"x"
 *		"y"
 *	]
 *
 *	ConflictingDevice [
 *		"x"
 *		"y"
 *	]
 *
 *	EnableSequence [
 *		....
 *	]
 *
 *	DisableSequence [
 *		...
 *	]
 *
 *      TransitionSequence."ToDevice" [
 *		...
 *	]
 *
 *	Value {
 *		PlaybackVolume "name='Master Playback Volume',index=2"
 *		PlaybackSwitch "name='Master Playback Switch',index=2"
 *	}
 * }
 */
static int parse_device(snd_use_case_mgr_t *uc_mgr,
			snd_config_t *cfg,
			void *data1,
			void *data2)
{
	struct use_case_verb *verb = data1;
	const char *name;
	struct use_case_device *device;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int err;

	if (data2) {
		name = data2;
		if (!parse_is_name_safe(name))
			return -EINVAL;
	}
	else {
		if (parse_get_safe_id(cfg, &name) < 0)
			return -EINVAL;
	}

	device = calloc(1, sizeof(*device));
	if (device == NULL)
		return -ENOMEM;
	INIT_LIST_HEAD(&device->enable_list);
	INIT_LIST_HEAD(&device->disable_list);
	INIT_LIST_HEAD(&device->transition_list);
	INIT_LIST_HEAD(&device->dev_list.list);
	INIT_LIST_HEAD(&device->value_list);
	list_add_tail(&device->list, &verb->device_list);
	device->name = strdup(name);

	snd_config_for_each(i, next, cfg) {
		const char *id;
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "Comment") == 0) {
			err = parse_string(n, &device->comment);
			if (err < 0) {
				uc_error("error: failed to get device comment");
				return err;
			}
			continue;
		}

		if (strcmp(id, "SupportedDevice") == 0) {
			err = parse_device_list(uc_mgr, &device->dev_list,
						DEVLIST_SUPPORTED, n);
			if (err < 0) {
				uc_error("error: failed to parse supported"
					" device list");
				return err;
			}
		}

		if (strcmp(id, "ConflictingDevice") == 0) {
			err = parse_device_list(uc_mgr, &device->dev_list,
						DEVLIST_CONFLICTING, n);
			if (err < 0) {
				uc_error("error: failed to parse conflicting"
					" device list");
				return err;
			}
		}

		if (strcmp(id, "EnableSequence") == 0) {
			uc_dbg("EnableSequence");
			err = parse_sequence(uc_mgr, &device->enable_list, n);
			if (err < 0) {
				uc_error("error: failed to parse device enable"
					 " sequence");
				return err;
			}
			continue;
		}

		if (strcmp(id, "DisableSequence") == 0) {
			uc_dbg("DisableSequence");
			err = parse_sequence(uc_mgr, &device->disable_list, n);
			if (err < 0) {
				uc_error("error: failed to parse device disable"
					 " sequence");
				return err;
			}
			continue;
		}

		if (strcmp(id, "TransitionSequence") == 0) {
			uc_dbg("TransitionSequence");
			err = parse_transition(uc_mgr, &device->transition_list, n);
			if (err < 0) {
				uc_error("error: failed to parse transition"
					" device");
				return err;
			}
			continue;
		}

		if (strcmp(id, "Value") == 0) {
			err = parse_value(uc_mgr, &device->value_list, n);
			if (err < 0) {
				uc_error("error: failed to parse Value");
				return err;
			}
			continue;
		}
	}
	return 0;
}

static int parse_compound_check_legacy(snd_use_case_mgr_t *uc_mgr,
	  snd_config_t *cfg,
	  int (*fcn)(snd_use_case_mgr_t *, snd_config_t *, void *, void *),
	  void *data1)
{
	const char *id, *idchild;
	int child_ctr = 0, legacy_format = 1;
	snd_config_iterator_t i, next;
	snd_config_t *child;
	int err;

	err = snd_config_get_id(cfg, &id);
	if (err < 0)
		return err;

	snd_config_for_each(i, next, cfg) {
		child_ctr++;
		if (child_ctr > 1) {
			break;
		}

		child = snd_config_iterator_entry(i);

		if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
			legacy_format = 0;
			break;
		}

		if (snd_config_get_id(child, &idchild) < 0)
			return -EINVAL;

		if (strcmp(idchild, "0")) {
			legacy_format = 0;
			break;
		}
	}
	if (child_ctr != 1) {
		legacy_format = 0;
	}

	if (legacy_format)
		return parse_compound(uc_mgr, cfg, fcn, data1, (void *)id);
	else
		return fcn(uc_mgr, cfg, data1, NULL);
}

static int parse_device_name(snd_use_case_mgr_t *uc_mgr,
			     snd_config_t *cfg,
			     void *data1,
			     void *data2 ATTRIBUTE_UNUSED)
{
	return parse_compound_check_legacy(uc_mgr, cfg, parse_device, data1);
}

static int parse_modifier_name(snd_use_case_mgr_t *uc_mgr,
			     snd_config_t *cfg,
			     void *data1,
			     void *data2 ATTRIBUTE_UNUSED)
{
	return parse_compound_check_legacy(uc_mgr, cfg, parse_modifier, data1);
}

/*
 * Parse Verb Section
 *
 * # Example Use case verb section for Voice call blah
 * # By Joe Blogs <joe@blogs.com>
 *
 * SectionVerb {
 *	# enable and disable sequences are compulsory
 *	EnableSequence [
 *		cset "name='Master Playback Switch',index=2 0,0"
 *		cset "name='Master Playback Volume',index=2 25,25"
 *		msleep 50
 *		cset "name='Master Playback Switch',index=2 1,1"
 *		cset "name='Master Playback Volume',index=2 50,50"
 *	]
 *
 *	DisableSequence [
 *		cset "name='Master Playback Switch',index=2 0,0"
 *		cset "name='Master Playback Volume',index=2 25,25"
 *		msleep 50
 *		cset "name='Master Playback Switch',index=2 1,1"
 *		cset "name='Master Playback Volume',index=2 50,50"
 *	]
 *
 *      # Optional transition verb
 *      TransitionSequence."ToCaseName" [
 *		msleep 1
 *      ]
 *
 *	# Optional TQ and ALSA PCMs
 *	Value {
 *		TQ HiFi
 *		CapturePCM "hw:0"
 *		PlaybackPCM "hw:0"
 *	}
 * }
 */
static int parse_verb(snd_use_case_mgr_t *uc_mgr,
		      struct use_case_verb *verb,
		      snd_config_t *cfg)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	int err;
	
	/* parse verb section */
	snd_config_for_each(i, next, cfg) {
		const char *id;
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "EnableSequence") == 0) {
			uc_dbg("Parse EnableSequence");
			err = parse_sequence(uc_mgr, &verb->enable_list, n);
			if (err < 0) {
				uc_error("error: failed to parse verb enable sequence");
				return err;
			}
			continue;
		}

		if (strcmp(id, "DisableSequence") == 0) {
			uc_dbg("Parse DisableSequence");
			err = parse_sequence(uc_mgr, &verb->disable_list, n);
			if (err < 0) {
				uc_error("error: failed to parse verb disable sequence");
				return err;
			}
			continue;
		}

		if (strcmp(id, "TransitionSequence") == 0) {
			uc_dbg("Parse TransitionSequence");
			err = parse_transition(uc_mgr, &verb->transition_list, n);
			if (err < 0) {
				uc_error("error: failed to parse transition sequence");
				return err;
			}
			continue;
		}

		if (strcmp(id, "Value") == 0) {
			uc_dbg("Parse Value");
			err = parse_value(uc_mgr, &verb->value_list, n);
			if (err < 0)
				return err;
			continue;
		}
	}

	return 0;
}

/*
 * Parse a Use case verb file.
 *
 * This file contains the following :-
 *  o Verb enable and disable sequences.
 *  o Supported Device enable and disable sequences for verb.
 *  o Supported Modifier enable and disable sequences for verb
 *  o Optional QoS for the verb and modifiers.
 *  o Optional PCM device ID for verb and modifiers
 *  o Alias kcontrols IDs for master and volumes and mutes.
 */
static int parse_verb_file(snd_use_case_mgr_t *uc_mgr,
			   const char *use_case_name,
			   const char *comment,
			   const char *file)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	struct use_case_verb *verb;
	snd_config_t *cfg;
	char filename[MAX_FILE];
	char *env = getenv(ALSA_CONFIG_UCM_VAR);
	int err;

	/* allocate verb */
	verb = calloc(1, sizeof(struct use_case_verb));
	if (verb == NULL)
		return -ENOMEM;
	INIT_LIST_HEAD(&verb->enable_list);
	INIT_LIST_HEAD(&verb->disable_list);
	INIT_LIST_HEAD(&verb->transition_list);
	INIT_LIST_HEAD(&verb->device_list);
	INIT_LIST_HEAD(&verb->modifier_list);
	INIT_LIST_HEAD(&verb->value_list);
	list_add_tail(&verb->list, &uc_mgr->verb_list);
	if (use_case_name == NULL)
		return -EINVAL;
	verb->name = strdup(use_case_name);
	if (verb->name == NULL)
		return -ENOMEM;

	if (comment != NULL) {
		verb->comment = strdup(comment);
		if (verb->comment == NULL)
			return -ENOMEM;
	}

	/* open Verb file for reading */
	snprintf(filename, sizeof(filename), "%s/%s/%s",
		env ? env : ALSA_USE_CASE_DIR,
		uc_mgr->card_name, file);
	filename[sizeof(filename)-1] = '\0';
	
	err = uc_mgr_config_load(filename, &cfg);
	if (err < 0) {
		uc_error("error: failed to open verb file %s : %d",
			filename, -errno);
		return err;
	}

	/* parse master config sections */
	snd_config_for_each(i, next, cfg) {
		const char *id;
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* find verb section and parse it */
		if (strcmp(id, "SectionVerb") == 0) {
			err = parse_verb(uc_mgr, verb, n);
			if (err < 0) {
				uc_error("error: %s failed to parse verb",
						file);
				return err;
			}
			continue;
		}

		/* find device sections and parse them */
		if (strcmp(id, "SectionDevice") == 0) {
			err = parse_compound(uc_mgr, n,
						parse_device_name, verb, NULL);
			if (err < 0) {
				uc_error("error: %s failed to parse device",
						file);
				return err;
			}
			continue;
		}

		/* find modifier sections and parse them */
		if (strcmp(id, "SectionModifier") == 0) {
			err = parse_compound(uc_mgr, n,
					     parse_modifier_name, verb, NULL);
			if (err < 0) {
				uc_error("error: %s failed to parse modifier",
						file);
				return err;
			}
			continue;
		}
	}

	/* use case verb must have at least 1 device */
	if (list_empty(&verb->device_list)) {
		uc_error("error: no use case device defined", file);
		return -EINVAL;
	}
	return 0;
}

/*
 * Parse master section for "Use Case" and "File" tags.
 */
static int parse_master_section(snd_use_case_mgr_t *uc_mgr, snd_config_t *cfg,
				void *data1 ATTRIBUTE_UNUSED,
				void *data2 ATTRIBUTE_UNUSED)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *use_case_name, *file = NULL, *comment = NULL;
	int err;

	if (snd_config_get_id(cfg, &use_case_name) < 0) {
		uc_error("unable to get name for use case section");
		return -EINVAL;
	}

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for use case section");
		return -EINVAL;
	}
	/* parse master config sections */
	snd_config_for_each(i, next, cfg) {
		const char *id;
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* get use case verb file name */
		if (strcmp(id, "File") == 0) {
			err = snd_config_get_string(n, &file);
			if (err < 0) {
				uc_error("failed to get File");
				return err;
			}
			continue;
		}

		/* get optional use case comment */
		if (strncmp(id, "Comment", 7) == 0) {
			err = snd_config_get_string(n, &comment);
			if (err < 0) {
				uc_error("error: failed to get Comment");
				return err;
			}
			continue;
		}

		uc_error("unknown field %s in master section");
	}

	uc_dbg("use_case_name %s file '%s'", use_case_name, file);

	/* do we have both use case name and file ? */
	if (!file) {
		uc_error("error: use case missing file");
		return -EINVAL;
	}

	/* parse verb file */
	return parse_verb_file(uc_mgr, use_case_name, comment, file);
}

/*
 * parse controls
 */
static int parse_controls(snd_use_case_mgr_t *uc_mgr, snd_config_t *cfg)
{
	int err;
	
	if (!list_empty(&uc_mgr->default_list)) {
		uc_error("Default list is not empty");
		return -EINVAL;
	}
	err = parse_sequence(uc_mgr, &uc_mgr->default_list, cfg);
	if (err < 0) {
		uc_error("Unable to parse SectionDefaults");
		return err;
	}
	
	return 0;
}

/*
 * Each sound card has a master sound card file that lists all the supported
 * use case verbs for that sound card. i.e.
 *
 * #Example master file for blah sound card
 * #By Joe Blogs <joe@bloggs.org>
 *
 * Comment "Nice Abstracted Soundcard"
 *
 * # The file is divided into Use case sections. One section per use case verb.
 *
 * SectionUseCase."Voice Call" {
 *	File "voice_call_blah"
 *	Comment "Make a voice phone call."
 * }
 *
 * SectionUseCase."HiFi" {
 *	File "hifi_blah"
 *	Comment "Play and record HiFi quality Music."
 * }
 *
 * # Define Value defaults
 *
 * ValueDefaults {
 *	PlaybackCTL "hw:CARD=0"
 *	CaptureCTL "hw:CARD=0"
 * }
 *
 * # This file also stores the default sound card state.
 *
 * SectionDefaults [
 *	cset "name='Master Playback Switch',index=2 1,1"
 *	cset "name='Master Playback Volume',index=2 25,25"
 *	cset "name='Master Mono Playback',index=1 0"
 *	cset "name='Master Mono Playback Volume',index=1 0"
 *	cset "name='PCM Switch',index=2 1,1"
 *      exec "some binary here"
 *      msleep 50
 *	........
 * ]
 *
 * # End of example file.
 */
static int parse_master_file(snd_use_case_mgr_t *uc_mgr, snd_config_t *cfg)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	int err;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for master file");
		return -EINVAL;
	}

	/* parse master config sections */
	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "Comment") == 0) {
			err = parse_string(n, &uc_mgr->comment);
			if (err < 0) {
				uc_error("error: failed to get master comment");
				return err;
			}
			continue;
		}

		/* find use case section and parse it */
		if (strcmp(id, "SectionUseCase") == 0) {
			err = parse_compound(uc_mgr, n,
					     parse_master_section,
					     NULL, NULL);
			if (err < 0)
				return err;
			continue;
		}

		/* find default control values section and parse it */
		if (strcmp(id, "SectionDefaults") == 0) {
			err = parse_controls(uc_mgr, n);
			if (err < 0)
				return err;
			continue;
		}

		/* get the default values */
		if (strcmp(id, "ValueDefaults") == 0) {
			err = parse_value(uc_mgr, &uc_mgr->value_list, n);
			if (err < 0) {
				uc_error("error: failed to parse ValueDefaults");
				return err;
			}
			continue;
		}

		uc_error("uknown master file field %s", id);
	}
	return 0;
}

static int load_master_config(const char *card_name, snd_config_t **cfg)
{
	char filename[MAX_FILE];
	char *env = getenv(ALSA_CONFIG_UCM_VAR);
	int err;

	snprintf(filename, sizeof(filename)-1,
		"%s/%s/%s.conf", env ? env : ALSA_USE_CASE_DIR,
		card_name, card_name);
	filename[MAX_FILE-1] = '\0';

	err = uc_mgr_config_load(filename, cfg);
	if (err < 0) {
		uc_error("error: could not parse configuration for card %s",
				card_name);
		return err;
	}

	return 0;
}

/* load master use case file for sound card */
int uc_mgr_import_master_config(snd_use_case_mgr_t *uc_mgr)
{
	snd_config_t *cfg;
	int err;

	err = load_master_config(uc_mgr->card_name, &cfg);
	if (err < 0)
		return err;
	err = parse_master_file(uc_mgr, cfg);
	snd_config_delete(cfg);
	if (err < 0)
		uc_mgr_free_verb(uc_mgr);

	return err;
}

static int filename_filter(const struct dirent *dirent)
{
	if (dirent == NULL)
		return 0;
	if (dirent->d_type == DT_DIR) {
		if (dirent->d_name[0] == '.') {
			if (dirent->d_name[1] == '\0')
				return 0;
			if (dirent->d_name[1] == '.' &&
			    dirent->d_name[2] == '\0')
				return 0;
		}
		return 1;
	}
	return 0;
}

/* scan all cards and comments */
int uc_mgr_scan_master_configs(const char **_list[])
{
	char filename[MAX_FILE], dfl[MAX_FILE];
	char *env = getenv(ALSA_CONFIG_UCM_VAR);
	const char **list;
	snd_config_t *cfg, *c;
	int i, cnt, err;
	ssize_t ss;
	struct dirent **namelist;

	snprintf(filename, sizeof(filename)-1,
		"%s", env ? env : ALSA_USE_CASE_DIR);
	filename[MAX_FILE-1] = '\0';

#if defined(_GNU_SOURCE) && !defined(__NetBSD__) && !defined(__FreeBSD__) && !defined(__sun)
#define SORTFUNC	versionsort
#else
#define SORTFUNC	alphasort
#endif
	err = scandir(filename, &namelist, filename_filter, SORTFUNC);
	if (err < 0) {
		err = -errno;
		uc_error("error: could not scan directory %s: %s",
				filename, strerror(-err));
		return err;
	}
	cnt = err;

	dfl[0] = '\0';
	if (strlen(filename) + 8 < sizeof(filename)) {
		strcat(filename, "/default");
		ss = readlink(filename, dfl, sizeof(dfl)-1);
		if (ss >= 0) {
			dfl[ss] = '\0';
			dfl[sizeof(dfl)-1] = '\0';
			if (dfl[0] && dfl[strlen(dfl)-1] == '/')
				dfl[strlen(dfl)-1] = '\0';
		} else {
			dfl[0] = '\0';
		}
	}

	list = calloc(1, cnt * 2 * sizeof(char *));
	if (list == NULL) {
		err = -ENOMEM;
		goto __err;
	}

	for (i = 0; i < cnt; i++) {
		err = load_master_config(namelist[i]->d_name, &cfg);
		if (err < 0)
			goto __err;
		err = snd_config_search(cfg, "Comment", &c);
		if (err >= 0) {
			err = parse_string(c, (char **)&list[i*2+1]);
			if (err < 0) {
				snd_config_delete(cfg);
				goto __err;
			}
		}
		snd_config_delete(cfg);
		list[i * 2] = strdup(namelist[i]->d_name);
		if (list[i * 2] == NULL) {
			err = -ENOMEM;
			goto __err;
		}
		if (strcmp(dfl, list[i * 2]) == 0) {
			/* default to top */
			const char *save1 = list[i * 2];
			const char *save2 = list[i * 2 + 1];
			memmove(list + 2, list, i * 2 * sizeof(char *));
			list[0] = save1;
			list[1] = save2;
		}
	}
	err = cnt * 2;

      __err:
	for (i = 0; i < cnt; i++) {
		free(namelist[i]);
		if (err < 0) {
			free((void *)list[i * 2]);
			free((void *)list[i * 2 + 1]);
		}
	}
	free(namelist);

	if (err >= 0)
		*_list = list;

	return err;
}
