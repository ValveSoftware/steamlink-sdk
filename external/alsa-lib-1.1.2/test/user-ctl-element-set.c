/*
 * user-control-element-set.c - a program to test in-kernel implementation of
 *				user-defined control element set.
 *
 * Copyright (c) 2015-2016 Takashi Sakamoto
 *
 * Licensed under the terms of the GNU General Public License, version 2.
 */

#include "../include/asoundlib.h"

struct elem_set_trial {
	snd_ctl_t *handle;

	snd_ctl_elem_type_t type;
	unsigned int member_count;
	unsigned int element_count;

	snd_ctl_elem_id_t *id;
	int dimension[4];

	int (*add_elem_set)(struct elem_set_trial *trial,
			    snd_ctl_elem_info_t *info);
	int (*check_elem_props)(struct elem_set_trial *trial,
				snd_ctl_elem_info_t *info);
	void (*change_elem_members)(struct elem_set_trial *trial,
				    snd_ctl_elem_value_t *elem_data);
};

/* Operations for elements in an element set with boolean type. */
static int add_bool_elem_set(struct elem_set_trial *trial,
			     snd_ctl_elem_info_t *info)
{
	return snd_ctl_add_boolean_elem_set(trial->handle, info,
				trial->element_count, trial->member_count);
}

static void change_bool_elem_members(struct elem_set_trial *trial,
				     snd_ctl_elem_value_t *elem_data)
{
	int val;
	unsigned int i;

	for (i = 0; i < trial->member_count; ++i) {
		val = snd_ctl_elem_value_get_boolean(elem_data, i);
		snd_ctl_elem_value_set_boolean(elem_data, i, !val);
	}
}

/* Operations for elements in an element set with integer type. */
static int add_int_elem_set(struct elem_set_trial *trial,
			    snd_ctl_elem_info_t *info)
{
	return snd_ctl_add_integer_elem_set(trial->handle, info,
				trial->element_count, trial->member_count,
				0, 99, 1);
}

static int check_int_elem_props(struct elem_set_trial *trial,
				snd_ctl_elem_info_t *info)
{
	if (snd_ctl_elem_info_get_min(info) != 0)
		return -EIO;
	if (snd_ctl_elem_info_get_max(info) != 99)
		return -EIO;
	if (snd_ctl_elem_info_get_step(info) != 1)
		return -EIO;

	return 0;
}

static void change_int_elem_members(struct elem_set_trial *trial,
				    snd_ctl_elem_value_t *elem_data)
{
	long val;
	unsigned int i;

	for (i = 0; i < trial->member_count; ++i) {
		val = snd_ctl_elem_value_get_integer(elem_data, i);
		snd_ctl_elem_value_set_integer(elem_data, i, ++val);
	}
}

/* Operations for elements in an element set with enumerated type. */
static const char *const labels[] = {
	"trusty",
	"utopic",
	"vivid",
	"willy",
	"xenial",
};

static int add_enum_elem_set(struct elem_set_trial *trial,
			     snd_ctl_elem_info_t *info)
{
	return snd_ctl_add_enumerated_elem_set(trial->handle, info,
				trial->element_count, trial->member_count,
				sizeof(labels) / sizeof(labels[0]),
				labels);
}

static int check_enum_elem_props(struct elem_set_trial *trial,
				 snd_ctl_elem_info_t *info)
{
	unsigned int items;
	unsigned int i;
	const char *label;
	int err;

	items = snd_ctl_elem_info_get_items(info);
	if (items != sizeof(labels) / sizeof(labels[0]))
		return -EIO;

	/* Enumerate and validate all of labels registered to this element. */
	for (i = 0; i < items; ++i) {
		snd_ctl_elem_info_set_item(info, i);
		err = snd_ctl_elem_info(trial->handle, info);
		if (err < 0)
			return err;

		label = snd_ctl_elem_info_get_item_name(info);
		if (strncmp(label, labels[i], strlen(labels[i])) != 0)
			return -EIO;
	}

	return 0;
}

static void change_enum_elem_members(struct elem_set_trial *trial,
				     snd_ctl_elem_value_t *elem_data)
{
	unsigned int val;
	unsigned int i;

	for (i = 0; i < trial->member_count; ++i) {
		val = snd_ctl_elem_value_get_enumerated(elem_data, i);
		snd_ctl_elem_value_set_enumerated(elem_data, i, ++val);
	}
}

/* Operations for elements in an element set with bytes type. */
static int add_bytes_elem_set(struct elem_set_trial *trial,
			      snd_ctl_elem_info_t *info)
{
	return snd_ctl_add_bytes_elem_set(trial->handle, info,
				trial->element_count, trial->member_count);
}

static void change_bytes_elem_members(struct elem_set_trial *trial,
				      snd_ctl_elem_value_t *elem_data)
{
	unsigned char val;
	unsigned int i;

	for (i = 0; i < trial->member_count; ++i) {
		val = snd_ctl_elem_value_get_byte(elem_data, i);
		snd_ctl_elem_value_set_byte(elem_data, i, ++val);
	}
}

/* Operations for elements in an element set with iec958 type. */
static int add_iec958_elem_set(struct elem_set_trial *trial,
			       snd_ctl_elem_info_t *info)
{
	int err;

	snd_ctl_elem_info_get_id(info, trial->id);

	err = snd_ctl_elem_add_iec958(trial->handle, trial->id);
	if (err < 0)
	        return err;

	/*
	 * In historical reason, the above API is not allowed to fill all of
	 * fields in identification data.
	 */
	return snd_ctl_elem_info(trial->handle, info);
}

static void change_iec958_elem_members(struct elem_set_trial *trial,
				       snd_ctl_elem_value_t *elem_data)
{
	snd_aes_iec958_t data;

	/* To suppress GCC warnings. */
	trial->element_count = 1;

	snd_ctl_elem_value_get_iec958(elem_data, &data);
	/* This is an arbitrary number. */
	data.pad = 10;
	snd_ctl_elem_value_set_iec958(elem_data, &data);
}

/* Operations for elements in an element set with integer64 type. */
static int add_int64_elem_set(struct elem_set_trial *trial,
			      snd_ctl_elem_info_t *info)
{
	return snd_ctl_add_integer64_elem_set(trial->handle, info,
				trial->element_count, trial->member_count,
				100, 10000, 30);
}

static int check_int64_elem_props(struct elem_set_trial *trial,
				  snd_ctl_elem_info_t *info)
{
	if (snd_ctl_elem_info_get_min64(info) != 100)
		return -EIO;
	if (snd_ctl_elem_info_get_max64(info) != 10000)
		return -EIO;
	if (snd_ctl_elem_info_get_step64(info) != 30)
		return -EIO;

	return 0;
}

static void change_int64_elem_members(struct elem_set_trial *trial,
				      snd_ctl_elem_value_t *elem_data)
{
	long long val;
	unsigned int i;

	for (i = 0; i < trial->member_count; ++i) {
		val = snd_ctl_elem_value_get_integer64(elem_data, i);
		snd_ctl_elem_value_set_integer64(elem_data, i, ++val);
	}
}

/* Common operations. */
static int add_elem_set(struct elem_set_trial *trial)
{
	snd_ctl_elem_info_t *info;
	char name[64] = {0};
	int err;

	snprintf(name, 64, "userspace-control-element-%s",
		 snd_ctl_elem_type_name(trial->type));

	snd_ctl_elem_info_alloca(&info);
	snd_ctl_elem_info_set_interface(info, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_info_set_name(info, name);
	snd_ctl_elem_info_set_dimension(info, trial->dimension);

	err = trial->add_elem_set(trial, info);
	if (err >= 0)
		snd_ctl_elem_info_get_id(info, trial->id);

	return err;
}

static int check_event(struct elem_set_trial *trial, unsigned int mask,
		       unsigned int expected_count)
{
	struct pollfd pfds;
	int count;
	unsigned short revents;
	snd_ctl_event_t *event;
	int err;

	snd_ctl_event_alloca(&event);

	if (snd_ctl_poll_descriptors_count(trial->handle) != 1)
		return -ENXIO;

	if (snd_ctl_poll_descriptors(trial->handle, &pfds, 1) != 1)
		return -ENXIO;

	while (expected_count > 0) {
		count = poll(&pfds, 1, 1000);
		if (count < 0)
			return errno;
		/* Some events are already supplied. */
		if (count == 0)
			return -ETIMEDOUT;

		err = snd_ctl_poll_descriptors_revents(trial->handle, &pfds,
						       count, &revents);
		if (err < 0)
			return err;
		if (revents & POLLERR)
			return -EIO;
		if (!(revents & POLLIN))
			continue;

		err = snd_ctl_read(trial->handle, event);
		if (err < 0)
			return err;
		if (snd_ctl_event_get_type(event) != SND_CTL_EVENT_ELEM)
			continue;
		/*
		 * I expect each event is generated separately to the same
		 * element.
		 */
		if (!(snd_ctl_event_elem_get_mask(event) & mask))
			continue;
		--expected_count;
	}

	if (expected_count != 0)
		return -EIO;

	return 0;
}

static int check_elem_set_props(struct elem_set_trial *trial)
{
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_info_t *info;
	unsigned int numid;
	unsigned int index;
	unsigned int i;
	unsigned int j;
	int err;

	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_info_alloca(&info);

	snd_ctl_elem_info_set_id(info, trial->id);
	numid = snd_ctl_elem_id_get_numid(trial->id);
	index = snd_ctl_elem_id_get_index(trial->id);

	for (i = 0; i < trial->element_count; ++i) {
		snd_ctl_elem_info_set_index(info, index + i);

		/*
		 * In Linux 4.0 or former, ioctl(SNDRV_CTL_IOCTL_ELEM_ADD)
		 * doesn't fill all of fields for identification.
		 */
		if (numid > 0)
			snd_ctl_elem_info_set_numid(info, numid + i);

		err = snd_ctl_elem_info(trial->handle, info);
		if (err < 0)
			return err;

		/* Check some common properties. */
		if (snd_ctl_elem_info_get_type(info) != trial->type)
			return -EIO;
		if (snd_ctl_elem_info_get_count(info) != trial->member_count)
			return -EIO;
		for (j = 0; j < 4; ++j) {
			if (snd_ctl_elem_info_get_dimension(info, j) !=
							trial->dimension[j])
				return -EIO;
		}

		/*
		 * In a case of IPC, this is the others. But in this case,
		 * it's myself.
		 */
		if (snd_ctl_elem_info_get_owner(info) != getpid())
			return -EIO;

		/*
		 * Just adding an element set by userspace applications,
		 * included elements are initially locked.
		 */
		if (!snd_ctl_elem_info_is_locked(info))
			return -EIO;

		/* Check type-specific properties. */
		if (trial->check_elem_props != NULL) {
			err = trial->check_elem_props(trial, info);
			if (err < 0)
				return err;
		}

		snd_ctl_elem_info_get_id(info, id);
		err = snd_ctl_elem_unlock(trial->handle, id);
		if (err < 0)
			return err;
	}

	return 0;
}

static int check_elems(struct elem_set_trial *trial)
{
	snd_ctl_elem_value_t *data;
	unsigned int numid;
	unsigned int index;
	unsigned int i;
	int err;

	snd_ctl_elem_value_alloca(&data);

	snd_ctl_elem_value_set_id(data, trial->id);
	numid = snd_ctl_elem_id_get_numid(trial->id);
	index = snd_ctl_elem_id_get_index(trial->id);

	for (i = 0; i < trial->element_count; ++i) {
		snd_ctl_elem_value_set_index(data, index + i);

		/*
		 * In Linux 4.0 or former, ioctl(SNDRV_CTL_IOCTL_ELEM_ADD)
		 * doesn't fill all of fields for identification.
		 */
		if (numid > 0)
			snd_ctl_elem_value_set_numid(data, numid + i);

		err = snd_ctl_elem_read(trial->handle, data);
		if (err < 0)
			return err;

		/* Change members of an element in this element set. */
		trial->change_elem_members(trial, data);

		err = snd_ctl_elem_write(trial->handle, data);
		if (err < 0)
			return err;
	}

	return 0;
}

static int check_tlv(struct elem_set_trial *trial)
{
	unsigned int orig[8], curr[8];
	int err;

	/*
	 * See a layout of 'struct snd_ctl_tlv'. I don't know the reason to
	 * construct this buffer with the same layout. It should be abstracted
	 * inner userspace library...
	 */
	orig[0] = snd_ctl_elem_id_get_numid(trial->id);
	orig[1] = 6 * sizeof(orig[0]);
	orig[2] = 'a';
	orig[3] = 'b';
	orig[4] = 'c';
	orig[5] = 'd';
	orig[6] = 'e';
	orig[7] = 'f';

	/*
	 * In in-kernel implementation, write and command operations are the
	 * same  for an element set added by userspace applications. Here, I
	 * use write.
	 */
	err = snd_ctl_elem_tlv_write(trial->handle, trial->id,
				     (const unsigned int *)orig);
	if (err < 0)
		return err;

	err = snd_ctl_elem_tlv_read(trial->handle, trial->id, curr,
				    sizeof(curr));
	if (err < 0)
		return err;

	if (memcmp(curr, orig, sizeof(orig)) != 0)
		return -EIO;

	return 0;
}

int main(void)
{
	struct elem_set_trial trial = {0};
	unsigned int i;
	int err;

	snd_ctl_elem_id_alloca(&trial.id);

	err = snd_ctl_open(&trial.handle, "hw:0", 0);
	if (err < 0)
		return EXIT_FAILURE;

	err = snd_ctl_subscribe_events(trial.handle, 1);
	if (err < 0)
		return EXIT_FAILURE;

	/* Test all of types. */
	for (i = 0; i < SND_CTL_ELEM_TYPE_LAST; ++i) {
		trial.type = i + 1;

		/* Assign type-dependent operations. */
		switch (trial.type) {
		case SND_CTL_ELEM_TYPE_BOOLEAN:
			trial.element_count = 900;
			trial.member_count = 128;
			trial.dimension[0] = 4;
			trial.dimension[1] = 4;
			trial.dimension[2] = 8;
			trial.dimension[3] = 0;
			trial.add_elem_set = add_bool_elem_set;
			trial.check_elem_props = NULL;
			trial.change_elem_members = change_bool_elem_members;
			break;
		case SND_CTL_ELEM_TYPE_INTEGER:
			trial.element_count = 900;
			trial.member_count = 128;
			trial.dimension[0] = 128;
			trial.dimension[1] = 0;
			trial.dimension[2] = 0;
			trial.dimension[3] = 0;
			trial.add_elem_set = add_int_elem_set;
			trial.check_elem_props = check_int_elem_props;
			trial.change_elem_members = change_int_elem_members;
			break;
		case SND_CTL_ELEM_TYPE_ENUMERATED:
			trial.element_count = 900;
			trial.member_count = 128;
			trial.dimension[0] = 16;
			trial.dimension[1] = 8;
			trial.dimension[2] = 0;
			trial.dimension[3] = 0;
			trial.add_elem_set = add_enum_elem_set;
			trial.check_elem_props = check_enum_elem_props;
			trial.change_elem_members = change_enum_elem_members;
			break;
		case SND_CTL_ELEM_TYPE_BYTES:
			trial.element_count = 900;
			trial.member_count = 512;
			trial.dimension[0] = 8;
			trial.dimension[1] = 4;
			trial.dimension[2] = 8;
			trial.dimension[3] = 2;
			trial.add_elem_set = add_bytes_elem_set;
			trial.check_elem_props = NULL;
			trial.change_elem_members = change_bytes_elem_members;
			break;
		case SND_CTL_ELEM_TYPE_IEC958:
			trial.element_count = 1;
			trial.member_count = 1;
			trial.dimension[0] = 0;
			trial.dimension[1] = 0;
			trial.dimension[2] = 0;
			trial.dimension[3] = 0;
			trial.add_elem_set = add_iec958_elem_set;
			trial.check_elem_props = NULL;
			trial.change_elem_members = change_iec958_elem_members;
			break;
		case SND_CTL_ELEM_TYPE_INTEGER64:
		default:
			trial.element_count = 900;
			trial.member_count = 64;
			trial.dimension[0] = 0;
			trial.dimension[1] = 0;
			trial.dimension[2] = 0;
			trial.dimension[3] = 0;
			trial.add_elem_set = add_int64_elem_set;
			trial.check_elem_props = check_int64_elem_props;
			trial.change_elem_members = change_int64_elem_members;
			break;
		}

		/* Test an operation to add an element set. */
		err = add_elem_set(&trial);
		if (err < 0) {
			printf("Fail to add an element set with %s type.\n",
			       snd_ctl_elem_type_name(trial.type));
			break;
		}
		err = check_event(&trial, SND_CTL_EVENT_MASK_ADD,
				  trial.element_count);
		if (err < 0) {
			printf("Fail to check some events to add elements with "
			       "%s type.\n",
			       snd_ctl_elem_type_name(trial.type));
			break;
		}

		/* Check properties of each element in this element set. */
		err = check_elem_set_props(&trial);
		if (err < 0) {
			printf("Fail to check propetries of each element with "
			       "%s type.\n",
			       snd_ctl_elem_type_name(trial.type));
			break;
		}

		/*
		 * Test operations to change the state of members in each
		 * element in the element set.
		 */
		err = check_elems(&trial);
		if (err < 0) {
			printf("Fail to change status of each element with %s "
			       "type.\n",
			       snd_ctl_elem_type_name(trial.type));
			break;
		}
		err = check_event(&trial, SND_CTL_EVENT_MASK_VALUE,
				  trial.element_count);
		if (err < 0) {
			printf("Fail to check some events to change status of "
			       "each elements with %s type.\n",
			       snd_ctl_elem_type_name(trial.type));
			break;
		}

		/*
		 * Test an operation to change threshold data of this element set,
		 * except for IEC958 type.
		 */
		if (trial.type != SND_CTL_ELEM_TYPE_IEC958) {
			err = check_tlv(&trial);
			if (err < 0) {
				printf("Fail to change threshold level of an "
				       "element set with %s type.\n",
				       snd_ctl_elem_type_name(trial.type));
				break;
			}
			err = check_event(&trial, SND_CTL_EVENT_MASK_TLV, 1);
			if (err < 0) {
				printf("Fail to check an event to change "
				       "threshold level of an an element set "
				       "with %s type.\n",
				       snd_ctl_elem_type_name(trial.type));
				break;
			}
		}

		/* Test an operation to remove elements in this element set. */
		err = snd_ctl_elem_remove(trial.handle, trial.id);
		if (err < 0) {
			printf("Fail to remove elements with %s type.\n",
			       snd_ctl_elem_type_name(trial.type));
			break;
		}
		err = check_event(&trial, SND_CTL_EVENT_MASK_REMOVE,
						  trial.element_count);
		if (err < 0) {
			printf("Fail to check some events to remove each "
			       "element with %s type.\n",
			       snd_ctl_elem_type_name(trial.type));
			break;
		}
	}

	if (err < 0) {
		printf("%s\n", snd_strerror(err));

		/* To ensure. */
		snd_ctl_elem_remove(trial.handle, trial.id);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
