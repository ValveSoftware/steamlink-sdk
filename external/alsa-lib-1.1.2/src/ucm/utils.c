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

void uc_mgr_error(const char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	fprintf(stderr, "ucm: ");
	vfprintf(stderr, fmt, va);
	va_end(va);
}

void uc_mgr_stdout(const char *fmt,...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(stdout, fmt, va);
	va_end(va);
}

int uc_mgr_config_load(const char *file, snd_config_t **cfg)
{
	FILE *fp;
	snd_input_t *in;
	snd_config_t *top;
	int err;

	fp = fopen(file, "r");
	if (fp == NULL) {
		err = -errno;
		goto __err;
	}
	err = snd_input_stdio_attach(&in, fp, 1);
	if (err < 0) {
	      __err:
		uc_error("could not open configuration file %s", file);
		return err;
	}
	err = snd_config_top(&top);
	if (err < 0)
		return err;
	err = snd_config_load(top, in);
	if (err < 0) {
		uc_error("could not load configuration file %s", file);
		snd_config_delete(top);
		return err;
	}
	err = snd_input_close(in);
	if (err < 0) {
		snd_config_delete(top);
		return err;
	}
	*cfg = top;
	return 0;
}

void uc_mgr_free_value(struct list_head *base)
{
	struct list_head *pos, *npos;
	struct ucm_value *val;
	
	list_for_each_safe(pos, npos, base) {
		val = list_entry(pos, struct ucm_value, list);
		free(val->name);
		free(val->data);
		list_del(&val->list);
		free(val);
	}
}

void uc_mgr_free_dev_list(struct dev_list *dev_list)
{
	struct list_head *pos, *npos;
	struct dev_list_node *dlist;
	
	list_for_each_safe(pos, npos, &dev_list->list) {
		dlist = list_entry(pos, struct dev_list_node, list);
		free(dlist->name);
		list_del(&dlist->list);
		free(dlist);
	}
}

void uc_mgr_free_sequence_element(struct sequence_element *seq)
{
	if (seq == NULL)
		return;
	switch (seq->type) {
	case SEQUENCE_ELEMENT_TYPE_CSET:
	case SEQUENCE_ELEMENT_TYPE_EXEC:
		free(seq->data.exec);
		break;
	default:
		break;
	}
	free(seq);
}

void uc_mgr_free_sequence(struct list_head *base)
{
	struct list_head *pos, *npos;
	struct sequence_element *seq;
	
	list_for_each_safe(pos, npos, base) {
		seq = list_entry(pos, struct sequence_element, list);
		list_del(&seq->list);
		uc_mgr_free_sequence_element(seq);
	}
}

void uc_mgr_free_transition_element(struct transition_sequence *tseq)
{
	free(tseq->name);
	uc_mgr_free_sequence(&tseq->transition_list);
	free(tseq);
}

void uc_mgr_free_transition(struct list_head *base)
{
	struct list_head *pos, *npos;
	struct transition_sequence *tseq;
	
	list_for_each_safe(pos, npos, base) {
		tseq = list_entry(pos, struct transition_sequence, list);
		list_del(&tseq->list);
		uc_mgr_free_transition_element(tseq);
	}
}

void uc_mgr_free_modifier(struct list_head *base)
{
	struct list_head *pos, *npos;
	struct use_case_modifier *mod;
	
	list_for_each_safe(pos, npos, base) {
		mod = list_entry(pos, struct use_case_modifier, list);
		free(mod->name);
		free(mod->comment);
		uc_mgr_free_sequence(&mod->enable_list);
		uc_mgr_free_sequence(&mod->disable_list);
		uc_mgr_free_transition(&mod->transition_list);
		uc_mgr_free_dev_list(&mod->dev_list);
		uc_mgr_free_value(&mod->value_list);
		list_del(&mod->list);
		free(mod);
	}
}

void uc_mgr_free_device(struct list_head *base)
{
	struct list_head *pos, *npos;
	struct use_case_device *dev;
	
	list_for_each_safe(pos, npos, base) {
		dev = list_entry(pos, struct use_case_device, list);
		free(dev->name);
		free(dev->comment);
		uc_mgr_free_sequence(&dev->enable_list);
		uc_mgr_free_sequence(&dev->disable_list);
		uc_mgr_free_transition(&dev->transition_list);
		uc_mgr_free_dev_list(&dev->dev_list);
		uc_mgr_free_value(&dev->value_list);
		list_del(&dev->list);
		free(dev);
	}
}

void uc_mgr_free_verb(snd_use_case_mgr_t *uc_mgr)
{
	struct list_head *pos, *npos;
	struct use_case_verb *verb;

	list_for_each_safe(pos, npos, &uc_mgr->verb_list) {
		verb = list_entry(pos, struct use_case_verb, list);
		free(verb->name);
		free(verb->comment);
		uc_mgr_free_sequence(&verb->enable_list);
		uc_mgr_free_sequence(&verb->disable_list);
		uc_mgr_free_transition(&verb->transition_list);
		uc_mgr_free_value(&verb->value_list);
		uc_mgr_free_device(&verb->device_list);
		uc_mgr_free_modifier(&verb->modifier_list);
		list_del(&verb->list);
		free(verb);
	}
	uc_mgr_free_sequence(&uc_mgr->default_list);
	uc_mgr_free_value(&uc_mgr->value_list);
	free(uc_mgr->comment);
	uc_mgr->comment = NULL;
	uc_mgr->active_verb = NULL;
	INIT_LIST_HEAD(&uc_mgr->active_devices);
	INIT_LIST_HEAD(&uc_mgr->active_modifiers);
	if (uc_mgr->ctl != NULL) {
		snd_ctl_close(uc_mgr->ctl);
		uc_mgr->ctl = NULL;
	}
	free(uc_mgr->ctl_dev);
	uc_mgr->ctl_dev = NULL;
}

void uc_mgr_free(snd_use_case_mgr_t *uc_mgr)
{
	uc_mgr_free_verb(uc_mgr);
	free(uc_mgr->card_name);
	free(uc_mgr);
}
