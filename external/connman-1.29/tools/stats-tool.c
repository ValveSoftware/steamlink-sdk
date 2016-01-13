/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2010-2014  BMW Car IT GmbH.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h>

#ifdef TEMP_FAILURE_RETRY
#define TFR TEMP_FAILURE_RETRY
#else
#define TFR
#endif

#define MAGIC 0xFA00B916

struct connman_stats_data {
	unsigned int rx_packets;
	unsigned int tx_packets;
	unsigned int rx_bytes;
	unsigned int tx_bytes;
	unsigned int rx_errors;
	unsigned int tx_errors;
	unsigned int rx_dropped;
	unsigned int tx_dropped;
	unsigned int time;
};

struct stats_file_header {
	unsigned int magic;
	unsigned int begin;
	unsigned int end;
	unsigned int home;
	unsigned int roaming;
};

struct stats_record {
	time_t ts;
	unsigned int roaming;
	struct connman_stats_data data;
};

struct stats_file {
	int fd;
	char *name;
	char *addr;
	size_t len;
	size_t max_len;

	/* cached values */
	int max_nr;
	int nr;
	struct stats_record *first;
	struct stats_record *last;
	struct stats_record *home_first;
	struct stats_record *roaming_first;
};

struct stats_iter {
	struct stats_file *file;
	struct stats_record *begin;
	struct stats_record *end;
	struct stats_record *it;
};

static gint option_create = 0;
static gint option_interval = 3;
static bool option_dump = false;
static bool option_summary = false;
static char *option_info_file_name = NULL;
static time_t option_start_ts = -1;
static char *option_last_file_name = NULL;

static bool parse_start_ts(const char *key, const char *value,
					gpointer user_data, GError **error)
{
	GTimeVal time_val;

	if (!g_time_val_from_iso8601(value, &time_val))
		return false;

	option_start_ts = time_val.tv_sec;

	return true;
}

static GOptionEntry options[] = {
	{ "create", 'c', 0, G_OPTION_ARG_INT, &option_create,
			"Create a .data file with NR faked entries", "NR" },
	{ "interval", 'i', 0, G_OPTION_ARG_INT, &option_interval,
			"Interval in seconds (used with create)", "INTERVAL" },
	{ "dump", 'd', 0, G_OPTION_ARG_NONE, &option_dump,
			"Dump contents of .data file" },
	{ "summary", 's', 0, G_OPTION_ARG_NONE, &option_summary,
			"Summary of .data file" },
	{ "info", 'f', 0, G_OPTION_ARG_FILENAME, &option_info_file_name,
			".info file name" },
	{ "startts", 't', 0, G_OPTION_ARG_CALLBACK, parse_start_ts,
			"Set start time for creating .data file "
			"(example 2010-11-05T23:00:12Z)", "TS"},
	{ "last", 'l', 0, G_OPTION_ARG_FILENAME, &option_last_file_name,
			  "Start values from last .data file" },
	{ NULL },
};

static struct stats_file_header *get_hdr(struct stats_file *file)
{
	return (struct stats_file_header *)file->addr;
}

static struct stats_record *get_begin(struct stats_file *file)
{
	unsigned int off = get_hdr(file)->begin;

	return (struct stats_record *)(file->addr + off);
}

static struct stats_record *get_end(struct stats_file *file)
{
	unsigned int off = get_hdr(file)->end;

	return (struct stats_record *)(file->addr + off);
}

static struct stats_record *get_home(struct stats_file *file)
{
	struct stats_file_header *hdr;

	hdr = get_hdr(file);

	if (hdr->home == UINT_MAX)
		return NULL;

	return (struct stats_record *)(file->addr + hdr->home);
}

static struct stats_record *get_roaming(struct stats_file *file)
{
	struct stats_file_header *hdr;

	hdr = get_hdr(file);

	if (hdr->roaming == UINT_MAX)
		return NULL;

	return (struct stats_record *)(file->addr + hdr->roaming);
}

static void set_end(struct stats_file *file, struct stats_record *end)
{
	struct stats_file_header *hdr;

	hdr = get_hdr(file);
	hdr->end = (char *)end - file->addr;
}

static int get_index(struct stats_file *file, struct stats_record *rec)
{
	return rec - file->first;
}

static struct stats_record *get_next(struct stats_file *file,
					struct stats_record *cur)
{
	cur++;

	if (cur > file->last)
		cur = file->first;

	return cur;
}

static struct stats_record *get_iterator_begin(struct stats_file *file)
{
	return get_next(file, get_begin(file));
}

static struct stats_record *get_iterator_end(struct stats_file *file)
{
	return get_next(file, get_end(file));
}

static void stats_print_record(struct stats_record *rec)
{
	char buffer[30];

	strftime(buffer, 30, "%d-%m-%Y %T", localtime(&rec->ts));
	printf("%p %lld %s %01d %d %d %d %d %d %d %d %d %d\n",
		rec, (long long int)rec->ts, buffer,
		rec->roaming,
		rec->data.rx_packets,
		rec->data.tx_packets,
		rec->data.rx_bytes,
		rec->data.tx_bytes,
		rec->data.rx_errors,
		rec->data.tx_errors,
		rec->data.rx_dropped,
		rec->data.tx_dropped,
		rec->data.time);
}

static void stats_hdr_info(struct stats_file *file)
{
	struct stats_file_header *hdr;
	struct stats_record *begin, *end, *home, *roaming;
	unsigned int home_idx, roaming_idx;

	hdr = get_hdr(file);
	begin = get_begin(file);
	end = get_end(file);

	home = get_home(file);
	if (!home)
		home_idx = UINT_MAX;
	else
		home_idx = get_index(file, home);

	roaming = get_roaming(file);
	if (!roaming)
		roaming_idx = UINT_MAX;
	else
		roaming_idx = get_index(file, roaming);

	printf("Data Structure Sizes\n");
	printf("  sizeof header   %zd/0x%02zx\n",
		sizeof(struct stats_file_header),
		sizeof(struct stats_file_header));
	printf("  sizeof entry    %zd/0%02zx\n\n",
		sizeof(struct stats_record),
		sizeof(struct stats_record));

	printf("File\n");
	printf("  addr            %p\n",  file->addr);
	printf("  len             %zd\n", file->len);

	printf("  max nr entries  %d\n", file->max_nr);
	printf("  nr entries      %d\n\n", file->nr);

	printf("Header\n");
	printf("  magic           0x%08x\n", hdr->magic);
	printf("  begin           [%d] 0x%08x\n",
		get_index(file, begin), hdr->begin);
	printf("  end             [%d] 0x%08x\n",
		get_index(file, end), hdr->end);
	printf("  home            [%d] 0x%08x\n",
		home_idx, hdr->home);
	printf("  roaming         [%d] 0x%08x\n\n",
		roaming_idx, hdr->roaming);


	printf("Pointers\n");
	printf("  hdr             %p\n", hdr);
	printf("  begin           %p\n", begin);
	printf("  end             %p\n", end);
	printf("  home            %p\n", home);
	printf("  romaing         %p\n", roaming);
	printf("  first           %p\n", file->first);
	printf("  last            %p\n\n", file->last);
}

static void stats_print_entries(struct stats_file *file)
{
	struct stats_record *it;
	int i;

	printf("[ idx] ptr ts ts rx_packets tx_packets rx_bytes "
		"tx_bytes rx_errors tx_errors rx_dropped tx_dropped time\n\n");

	for (i = 0, it = file->first; it <= file->last; it++, i++) {
		printf("[%04d] ", i);
		stats_print_record(it);
	}
}

static void stats_print_rec_diff(struct stats_record *begin,
					struct stats_record *end)
{
	printf("\trx_packets: %d\n",
		end->data.rx_packets - begin->data.rx_packets);
	printf("\ttx_packets: %d\n",
		end->data.tx_packets - begin->data.tx_packets);
	printf("\trx_bytes:   %d\n",
		end->data.rx_bytes - begin->data.rx_bytes);
	printf("\ttx_bytes:   %d\n",
		end->data.tx_bytes - begin->data.tx_bytes);
	printf("\trx_errors:  %d\n",
		end->data.rx_errors - begin->data.rx_errors);
	printf("\ttx_errors:  %d\n",
		end->data.tx_errors - begin->data.tx_errors);
	printf("\trx_dropped: %d\n",
		end->data.rx_dropped - begin->data.rx_dropped);
	printf("\ttx_dropped: %d\n",
		end->data.tx_dropped - begin->data.tx_dropped);
	printf("\ttime:       %d\n",
		end->data.time - begin->data.time);
}

static void stats_print_diff(struct stats_file *file)
{
	struct stats_record *begin, *end;

	begin = get_begin(file);
	begin = get_next(file, begin);
	end = get_end(file);

	printf("\n(begin + 1)\n");
	printf("\t[%04d] ", get_index(file, begin));
	stats_print_record(begin);
	printf("end\n");
	printf("\t[%04d] ", get_index(file, end));
	stats_print_record(end);

	if (file->home_first && get_home(file)) {
		printf("\nhome\n");
		stats_print_rec_diff(file->home_first, get_home(file));
	}

	if (file->roaming_first && get_roaming(file)) {
		printf("\roaming\n");
		stats_print_rec_diff(file->roaming_first, get_roaming(file));
	}
}

static void update_max_nr_entries(struct stats_file *file)
{
	file->max_nr = (file->len - sizeof(struct stats_file_header)) /
		sizeof(struct stats_record);
}

static void update_nr_entries(struct stats_file *file)
{
	struct stats_record *begin, *end;
	int nr;

	begin = get_begin(file);
	end = get_end(file);

	nr = get_index(file, end) - get_index(file, begin);

	if (nr < 0)
		nr += file->max_nr;

	file->nr = nr;
}

static void update_first(struct stats_file *file)
{
	file->first = (struct stats_record *)(file->addr +
					sizeof(struct stats_file_header));
}

static void update_last(struct stats_file *file)
{
	struct stats_record *last;

	last = file->first;
	last += file->max_nr - 1;

	file->last = last;
}

static int stats_file_update_cache(struct stats_file *file)
{
	struct stats_record *it, *end;

	update_max_nr_entries(file);
	update_nr_entries(file);
	update_first(file);
	update_last(file);
	file->home_first = NULL;
	file->roaming_first = NULL;

	end = get_iterator_end(file);
	for (it = get_iterator_begin(file);
			it != end;
			it = get_next(file, it)) {

		if (!file->home_first && it->roaming == 0)
			file->home_first = it;

		if (!file->roaming_first && it->roaming == 1)
			file->roaming_first = it;

		if (file->home_first && file->roaming_first)
			break;
	}

	return 0;
}

static int stats_file_remap(struct stats_file *file, size_t size)
{
	size_t page_size, new_size;
	void *addr;
	int err;

	page_size = sysconf(_SC_PAGESIZE);
	new_size = (size + page_size - 1) & ~(page_size - 1);

	err = ftruncate(file->fd, new_size);
	if (err < 0) {
		fprintf(stderr, "ftrunctate error %s for %s",
				strerror(errno), file->name);
		return -errno;
	}

	if (!file->addr) {
		addr = mmap(NULL, new_size, PROT_READ | PROT_WRITE,
				MAP_SHARED, file->fd, 0);
	} else {
		addr = mremap(file->addr, file->len, new_size, MREMAP_MAYMOVE);
	}

	if (addr == MAP_FAILED) {
		fprintf(stderr, "mmap error %s for %s\n",
			strerror(errno), file->name);
		return -errno;
	}

	file->addr = addr;
	file->len = new_size;
	file->max_len = new_size;

	return 0;
}

static int stats_open(struct stats_file *file, const char *name)
{
	struct stats_file_header *hdr;
	struct stat tm;
	int err;
	size_t size = 0;

	bzero(file, sizeof(struct stats_file));

	if (name) {
		file->name = g_strdup(name);

		file->fd = TFR(open(file->name,
					O_RDWR | O_CREAT | O_CLOEXEC, 0644));
		if (file->fd == -1) {
			fprintf(stderr, "open error %s for %s\n",
				strerror(errno), file->name);
			return -errno;
		}

		err = fstat(file->fd, &tm);
		if (err < 0) {
			fprintf(stderr, "fstat error %s for %s\n",
				strerror(errno), file->name);
			return err;
		}

		size = (size_t)tm.st_size;
	} else {
		file->name = g_strdup("stats.XXXXXX.tmp");
		file->fd = g_mkstemp_full(file->name, O_RDWR | O_CREAT, 0644);
		if (file->fd == -1) {
			fprintf(stderr, "creating tmp failed\n");
			return -1;
		}
	}

	if (size == 0)
		size = sysconf(_SC_PAGESIZE);

	err = stats_file_remap(file, size);
	if (err < 0) {
		fprintf(stderr, "remap failed\n");
		return err;
	}

	/* Initialize new file */
	hdr = get_hdr(file);
	if (hdr->magic != MAGIC ||
			hdr->begin < sizeof(struct stats_file_header) ||
			hdr->end < sizeof(struct stats_file_header) ||
			hdr->home < sizeof(struct stats_file_header) ||
			hdr->roaming < sizeof(struct stats_file_header) ||
			hdr->begin > file->len ||
			hdr->end > file->len) {
		hdr->magic = MAGIC;
		hdr->begin = sizeof(struct stats_file_header);
		hdr->end = sizeof(struct stats_file_header);
		hdr->home = UINT_MAX;
		hdr->roaming = UINT_MAX;

	}
	stats_file_update_cache(file);

	return 0;
}

static void stats_close(struct stats_file *file)
{
	munmap(file->addr, file->len);
	close(file->fd);
	g_free(file->name);
}

static int stats_create(struct stats_file *file, unsigned int nr,
			unsigned int interval, time_t start_ts,
			struct stats_record *start)
{
	unsigned int i;
	int err;
	struct stats_record *cur, *next;
	struct stats_file_header *hdr;
	unsigned int pkt;
	unsigned int step_ts;
	unsigned int roaming = FALSE;

	hdr = get_hdr(file);

	hdr->magic = MAGIC;
	hdr->begin = sizeof(struct stats_file_header);
	hdr->end = sizeof(struct stats_file_header);
	hdr->home = UINT_MAX;
	hdr->roaming = UINT_MAX;

	stats_file_update_cache(file);

	if (start) {
		struct stats_record *rec;

		rec = get_end(file);
		memcpy(rec, start, sizeof(struct stats_record));
	} else {
		get_end(file)->ts = start_ts;
	}

	for (i = 0; i < nr; i++) {
		if (file->last == get_end(file)) {
			err = stats_file_remap(file, file->len +
						sysconf(_SC_PAGESIZE));
			if (err < 0)
				return err;

			stats_file_update_cache(file);
		}
		cur = get_end(file);
		next = get_next(file, cur);

		step_ts = (rand() % interval);
		if (step_ts == 0)
			step_ts = 1;

		next->ts = cur->ts + step_ts;
		next->roaming = roaming;
		next->data.time = cur->data.time + step_ts;

		next->data.rx_packets = cur->data.rx_packets;
		next->data.rx_bytes = cur->data.rx_bytes;

		if (rand() % 3 == 0) {
			pkt = rand() % 5;
			next->data.rx_packets += pkt;
			next->data.rx_bytes += pkt * (rand() % 1500);
		}

		next->data.tx_packets = cur->data.tx_packets;
		next->data.tx_bytes = cur->data.tx_bytes;

		if (rand() % 3 == 0) {
			pkt = rand() % 5;
			next->data.tx_packets += pkt;
			next->data.tx_bytes += pkt * (rand() % 1500);
		}

		set_end(file, next);

		if ((rand() % 50) == 0)
			roaming = roaming ? FALSE : TRUE;

	}

	return 0;
}

static struct stats_record *get_next_record(struct stats_iter *iter)
{
	if (iter->it != iter->end) {
		struct stats_record *tmp;

		tmp = iter->it;
		iter->it = get_next(iter->file, iter->it);

		return tmp;
	}

	return NULL;
}

static int append_record(struct stats_file *file,
				struct stats_record *rec)
{
	struct stats_record *cur, *next;
	int err;

	if (file->last == get_end(file)) {
		err = stats_file_remap(file, file->len +
					sysconf(_SC_PAGESIZE));
		if (err < 0)
			return err;

		stats_file_update_cache(file);
	}

	cur = get_end(file);
	next = get_next(file, cur);

	memcpy(next, rec, sizeof(struct stats_record));

	set_end(file, next);

	return 0;
}

static struct stats_record *process_file(struct stats_iter *iter,
					struct stats_file *temp_file,
					struct stats_record *cur,
					GDate *date_change_step_size,
					int account_period_offset)
{
	struct stats_record *home, *roaming;
	struct stats_record *next;

	home = NULL;
	roaming = NULL;

	if (!cur)
		cur = get_next_record(iter);
	next = get_next_record(iter);

	while (next) {
		GDate date_cur;
		GDate date_next;
		int append;

		append = FALSE;

		if (cur->roaming)
			roaming = cur;
		else
			home = cur;

		g_date_set_time_t(&date_cur, cur->ts);
		g_date_set_time_t(&date_next, next->ts);

		if (g_date_compare(&date_cur, date_change_step_size) < 0) {
			/* month period size */
			GDateDay day_cur, day_next;
			GDateMonth month_cur, month_next;

			month_cur = g_date_get_month(&date_cur);
			month_next = g_date_get_month(&date_next);

			day_cur = g_date_get_day(&date_cur);
			day_next = g_date_get_day(&date_next);

			if (day_cur == day_next && month_cur != month_next)
				append = TRUE;
			else if (day_cur < account_period_offset
					&& day_next >= account_period_offset)
				append = TRUE;
		} else {
			/* day period size */
			if (g_date_days_between(&date_cur, &date_next) > 0)
				append = TRUE;
		}

		if (append) {
			if (home) {
				append_record(temp_file, home);
				home = NULL;
			}

			if (roaming) {
				append_record(temp_file, roaming);
				roaming = NULL;
			}
		}

		cur = next;
		next = get_next_record(iter);
	}

	return cur;
}

static int summarize(struct stats_file *data_file,
			struct stats_file *history_file,
			struct stats_file *temp_file,
			int account_period_offset)
{
	struct stats_iter data_iter;
	struct stats_iter history_iter;
	struct stats_record *cur, *next;

	GDate today, date_change_step_size;

	/*
	 * First calculate the date when switch from monthly
	 * accounting period size to daily size
	 */
	g_date_set_time_t(&today, time(NULL));

	date_change_step_size = today;
	if (g_date_get_day(&today) - account_period_offset >= 0)
		g_date_subtract_months(&date_change_step_size, 2);
	else
		g_date_subtract_months(&date_change_step_size, 3);

	g_date_set_day(&date_change_step_size, account_period_offset);


	/* Now process history file */
	cur = NULL;

	if (history_file) {
		history_iter.file = history_file;
		history_iter.begin = get_iterator_begin(history_iter.file);
		history_iter.end = get_iterator_end(history_iter.file);
		history_iter.it = history_iter.begin;

		cur = process_file(&history_iter, temp_file, NULL,
					&date_change_step_size,
					account_period_offset);
	}

	data_iter.file = data_file;
	data_iter.begin = get_iterator_begin(data_iter.file);
	data_iter.end = get_iterator_end(data_iter.file);
	data_iter.it = data_iter.begin;

	/*
	 * Ensure date_file records are newer than the history_file
	 * record
	 */
	if (cur) {
		next = get_next_record(&data_iter);
		while (next && cur->ts > next->ts)
			next = get_next_record(&data_iter);
	}

	/* And finally process the new data records */
	cur = process_file(&data_iter, temp_file, cur,
				&date_change_step_size, account_period_offset);

	if (cur)
		append_record(temp_file, cur);

	return 0;
}

static void swap_and_close_files(struct stats_file *history_file,
			struct stats_file *temp_file)
{
	munmap(history_file->addr, history_file->len);
	munmap(temp_file->addr, temp_file->len);

	TFR(close(temp_file->fd));

	unlink(history_file->name);

	if (link(temp_file->name, history_file->name) < 0)
		return;

	unlink(temp_file->name);
	TFR(close(history_file->fd));
}

static void history_file_update(struct stats_file *data_file,
				const char *history_file_name)
{
	struct stats_file _history_file;
	struct stats_file tempory_file;

	struct stats_file *history_file = NULL;

	if (stats_open(&_history_file, history_file_name) == 0)
		history_file = &_history_file;

	if (stats_open(&tempory_file, NULL) < 0) {
		if (history_file)
			stats_close(history_file);
		return;
	}

	summarize(data_file, history_file, &tempory_file, 13);

	swap_and_close_files(history_file, &tempory_file);
}

int main(int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;

	struct stats_file_header *hdr;
	struct stats_file data, *data_file;
	struct stats_record *rec;
	time_t start_ts;
	int err;

	rec = NULL;

	data_file = &data;

	putenv("TZ=GMT0");

	context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, options, NULL);

	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		if (error) {
			g_printerr("%s\n", error->message);
			g_error_free(error);
		} else
			g_printerr("An unknown error occurred\n");
		exit(1);
	}

	g_option_context_free(context);

	if (argc < 2) {
		printf("Usage: %s [FILENAME]\n", argv[0]);
		exit(0);
	}

	err = stats_open(data_file, argv[1]);
	if (err < 0) {
		fprintf(stderr, "failed open file %s\n", argv[1]);
		exit(1);
	}

	if (option_last_file_name) {
		struct stats_file last;
		if (stats_open(&last, option_last_file_name) < 0) {
			fprintf(stderr, "failed open file %s\n",
				option_last_file_name);
			exit(1);
		}

		rec = get_end(&last);
	}

	if (option_start_ts == -1)
		start_ts = time(NULL);
	else
		start_ts = option_start_ts;

	if (option_create > 0)
		stats_create(data_file, option_create, option_interval,
				start_ts, rec);

	hdr = get_hdr(data_file);
	if (hdr->magic != MAGIC) {
		fprintf(stderr, "header file magic test failed\n");
		goto err;
	}

	stats_file_update_cache(data_file);

	stats_hdr_info(data_file);

	if (option_dump)
		stats_print_entries(data_file);

	if (option_summary)
		stats_print_diff(data_file);

	if (option_info_file_name)
		history_file_update(data_file, option_info_file_name);

err:
	stats_close(data_file);

	return 0;
}
