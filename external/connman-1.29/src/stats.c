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
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>

#include "connman.h"

#define MODE		(S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | \
			S_IXGRP | S_IROTH | S_IXOTH)

#ifdef TEMP_FAILURE_RETRY
#define TFR TEMP_FAILURE_RETRY
#else
#define TFR
#endif

#define MAGIC 0xFA00B916

/*
 * Statistics counters are stored into a ring buffer which is stored
 * into a file
 *
 * File properties:
 *   The ring buffer is mmap to a file
 *   Initialy only the smallest possible amount of disk space is allocated
 *   The files grow to the configured maximal size
 *   The grows by _SC_PAGESIZE step size
 *   For each service a file is created
 *   Each file has a header where the indexes are stored
 *
 * Entries properties:
 *   Each entry has a timestamp
 *   A flag to mark if the entry is either home (0) or roaming (1) entry
 *   The entries are fixed sized (stats_record)
 *
 * Ring buffer properties:
 *   There are to indexes 'begin', 'end', 'home' and 'roaming'
 *   'begin' points to the oldest entry
 *   'end' points to the newest/current entry
 *   'home' points to the current home entry
 *   'roaming' points to the current roaming entry
 *   If 'begin' == 'end' then the buffer is empty
 *   If 'end' + 1 == 'begin then it's full
 *   The ring buffer is valid in the range (begin, end]
 *   If 'home' has the value UINT_MAX', 'home' is invalid
 *   if 'roaming' has the value UINT_MAX', 'roaming' is invalid
 *   'first' points to the first entry in the ring buffer
 *   'last' points to the last entry in the ring buffer
 *
 * History file:
 *   Same format as the ring buffer file
 *   For a period of at least 2 months dayly records are keept
 *   If older, then only a monthly record is keept
 */


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
	struct stats_record *first;
	struct stats_record *last;
	struct stats_record *home;
	struct stats_record *roaming;

	/* history */
	char *history_name;
	int account_period_offset;
};

struct stats_iter {
	struct stats_file *file;
	struct stats_record *begin;
	struct stats_record *end;
	struct stats_record *it;
};

static GHashTable *stats_hash = NULL;

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

static void set_home(struct stats_file *file, struct stats_record *home)
{
	struct stats_file_header *hdr;

	hdr = get_hdr(file);
	hdr->home = (char *)home - file->addr;
}

static void set_roaming(struct stats_file *file, struct stats_record *roaming)
{
	struct stats_file_header *hdr;

	hdr = get_hdr(file);
	hdr->roaming = (char *)roaming - file->addr;
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

static void stats_free(gpointer user_data)
{
	struct stats_file *file = user_data;

	if (!file)
		return;

	msync(file->addr, file->len, MS_SYNC);

	munmap(file->addr, file->len);
	file->addr = NULL;

	TFR(close(file->fd));
	file->fd = -1;

	if (file->history_name) {
		g_free(file->history_name);
		file->history_name = NULL;
	}

	if (file->name) {
		g_free(file->name);
		file->name = NULL;
	}

	g_free(file);
}

static void update_first(struct stats_file *file)
{
	file->first = (struct stats_record *)
			(file->addr + sizeof(struct stats_file_header));
}

static void update_last(struct stats_file *file)
{
	unsigned int max_entries;

	max_entries = (file->len - sizeof(struct stats_file_header)) /
			sizeof(struct stats_record);
	file->last = file->first + max_entries - 1;
}

static void update_home(struct stats_file *file)
{
	file->home = get_home(file);
}

static void update_roaming(struct stats_file *file)
{
	file->roaming = get_roaming(file);
}

static void stats_file_update_cache(struct stats_file *file)
{
	update_first(file);
	update_last(file);
	update_home(file);
	update_roaming(file);
}

static int stats_file_remap(struct stats_file *file, size_t size)
{
	size_t page_size, new_size;
	void *addr;
	int err;

	DBG("file %p size %zu addr %p len %zu", file, size, file->addr,
		file->len);

	page_size = sysconf(_SC_PAGESIZE);
	new_size = (size + page_size - 1) & ~(page_size - 1);

	err = ftruncate(file->fd, new_size);
	if (err < 0) {
		connman_error("ftrunctate error %s for %s",
				strerror(errno), file->name);
		return -errno;
	}

	if (!file->addr) {
		/*
		 * Though the buffer is not shared between processes, we still
		 * have to take MAP_SHARED because MAP_PRIVATE does not
		 * guarantee that writes will hit the file without an explicit
		 * call to munmap or msync. For more details please read the
		 * mmap man pages.
		 */
		addr = mmap(NULL, new_size, PROT_READ | PROT_WRITE,
				MAP_SHARED, file->fd, 0);
	} else {
		addr = mremap(file->addr, file->len, new_size, MREMAP_MAYMOVE);
	}

	if (addr == MAP_FAILED) {
		connman_error("mmap error %s for %s",
				strerror(errno), file->name);
		if (errno == EINVAL) {
			connman_error("%s might be on a file system, such as "
					"JFFS2, that does not allow shared "
					"writable mappings.", file->name);
		}
		return -errno;
	}

	file->addr = addr;
	file->len = new_size;

	stats_file_update_cache(file);

	return 0;
}

static int stats_open(struct stats_file *file,
			const char *name)
{
	DBG("file %p name %s", file, name);

	file->name = g_strdup(name);

	file->fd = TFR(open(file->name, O_RDWR | O_CREAT | O_CLOEXEC, 0644));
	if (file->fd < 0) {
		connman_error("open error %s for %s",
				strerror(errno), file->name);
		g_free(file->name);
		file->name = NULL;
		return -errno;
	}

	return 0;
}

static int stats_open_temp(struct stats_file *file)
{
	file->name = g_strdup_printf("%s/stats.XXXXXX.tmp",
					STORAGEDIR);
	file->fd = g_mkstemp_full(file->name, O_RDWR | O_CREAT, 0644);
	if (file->fd < 0) {
		connman_error("create tempory file error %s for %s",
				strerror(errno), file->name);
		g_free(file->name);
		file->name = NULL;
		return -errno;
	}

	return 0;
}

static int stats_file_setup(struct stats_file *file)
{
	struct stats_file_header *hdr;
	struct stat st;
	size_t size = 0;
	int err;

	DBG("file %p fd %d name %s", file, file->fd, file->name);

	err = fstat(file->fd, &st);
	if (err < 0) {
		connman_error("fstat error %s for %s\n",
			strerror(errno), file->name);

		TFR(close(file->fd));
		g_free(file->name);
		file->name = NULL;

		return -errno;
	}

	size = (size_t)st.st_size;
	file->max_len = STATS_MAX_FILE_SIZE;

	if (size < (size_t)sysconf(_SC_PAGESIZE))
		size = sysconf(_SC_PAGESIZE);

	err = stats_file_remap(file, size);
	if (err < 0) {
		TFR(close(file->fd));
		g_free(file->name);
		file->name = NULL;

		return err;
	}

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

		stats_file_update_cache(file);
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
		bool append;

		append = false;

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

			if (day_cur == day_next && month_cur != month_next) {
				append = true;
			} else if (day_cur < account_period_offset &&
					day_next >= account_period_offset) {
				append = true;
			}
		} else {
			/* day period size */
			if (g_date_days_between(&date_cur, &date_next) > 0)
				append = true;
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
			struct stats_file *temp_file)
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
	if (g_date_get_day(&today) - data_file->account_period_offset >= 0)
		g_date_subtract_months(&date_change_step_size, 2);
	else
		g_date_subtract_months(&date_change_step_size, 3);

	g_date_set_day(&date_change_step_size,
			data_file->account_period_offset);


	/* Now process history file */
	cur = NULL;

	if (history_file) {
		history_iter.file = history_file;
		history_iter.begin = get_iterator_begin(history_iter.file);
		history_iter.end = get_iterator_end(history_iter.file);
		history_iter.it = history_iter.begin;

		cur = process_file(&history_iter, temp_file, NULL,
					&date_change_step_size,
					data_file->account_period_offset);
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
				&date_change_step_size,
				data_file->account_period_offset);

	if (cur)
		append_record(temp_file, cur);

	return 0;
}

static void stats_file_unmap(struct stats_file *file)
{
	msync(file->addr, file->len, MS_SYNC);
	munmap(file->addr, file->len);
	file->addr = NULL;
}

static void stats_file_cleanup(struct stats_file *file)
{
	file->fd = -1;
	g_free(file->name);
	file->name = NULL;
}

static int stats_file_close_swap(struct stats_file *history_file,
					struct stats_file *temp_file)
{
	int err;

	stats_file_unmap(history_file);
	stats_file_unmap(temp_file);

	TFR(close(temp_file->fd));

	unlink(history_file->name);

	err = link(temp_file->name, history_file->name);

	unlink(temp_file->name);

	TFR(close(history_file->fd));

	stats_file_cleanup(history_file);
	stats_file_cleanup(temp_file);

	return err;
}

static int stats_file_history_update(struct stats_file *data_file)
{
	struct stats_file _history_file, *history_file;
	struct stats_file _temp_file, *temp_file;
	int err;

	history_file = &_history_file;
	temp_file = &_temp_file;

	bzero(history_file, sizeof(struct stats_file));
	bzero(temp_file, sizeof(struct stats_file));

	err = stats_open(history_file, data_file->history_name);
	if (err < 0)
		return err;
	stats_file_setup(history_file);

	err = stats_open_temp(temp_file);
	if (err < 0) {
		stats_free(history_file);
		return err;
	}
	stats_file_setup(temp_file);

	summarize(data_file, history_file, temp_file);

	err = stats_file_close_swap(history_file, temp_file);

	return err;
}

int __connman_stats_service_register(struct connman_service *service)
{
	struct stats_file *file;
	char *name, *dir;
	int err;

	DBG("service %p", service);

	file = g_hash_table_lookup(stats_hash, service);
	if (!file) {
		file = g_try_new0(struct stats_file, 1);
		if (!file)
			return -ENOMEM;

		g_hash_table_insert(stats_hash, service, file);
	} else {
		return -EALREADY;
	}

	dir = g_strdup_printf("%s/%s", STORAGEDIR,
				__connman_service_get_ident(service));

	/* If the dir doesn't exist, create it */
	if (!g_file_test(dir, G_FILE_TEST_IS_DIR)) {
		if (mkdir(dir, MODE) < 0) {
			if (errno != EEXIST) {
				g_free(dir);

				err = -errno;
				goto err;
			}
		}
	}

	g_free(dir);

	name = g_strdup_printf("%s/%s/data", STORAGEDIR,
				__connman_service_get_ident(service));
	file->history_name = g_strdup_printf("%s/%s/history", STORAGEDIR,
				__connman_service_get_ident(service));

	/* TODO: Use a global config file instead of hard coded value. */
	file->account_period_offset = 1;

	err = stats_open(file, name);
	g_free(name);
	if (err < 0)
		goto err;

	err = stats_file_setup(file);
	if (err < 0)
		goto err;

	return 0;

err:
	g_hash_table_remove(stats_hash, service);

	return err;
}

void __connman_stats_service_unregister(struct connman_service *service)
{
	DBG("service %p", service);

	g_hash_table_remove(stats_hash, service);
}

int  __connman_stats_update(struct connman_service *service,
				bool roaming,
				struct connman_stats_data *data)
{
	struct stats_file *file;
	struct stats_record *next;
	int err;

	file = g_hash_table_lookup(stats_hash, service);
	if (!file)
		return -EEXIST;

	if (file->len < file->max_len &&
			file->last == get_end(file)) {
		DBG("grow file %s", file->name);

		err = stats_file_remap(file, file->len + sysconf(_SC_PAGESIZE));
		if (err < 0)
			return err;
	}

	next = get_next(file, get_end(file));

	if (next == get_begin(file)) {
		DBG("ring buffer is full, update history file");

		if (stats_file_history_update(file) < 0) {
			connman_warn("history file update failed %s",
					file->history_name);
		}
	}

	next->ts = time(NULL);
	next->roaming = roaming;
	memcpy(&next->data, data, sizeof(struct connman_stats_data));

	if (!roaming)
		set_home(file, next);
	else
		set_roaming(file, next);

	set_end(file, next);

	return 0;
}

int __connman_stats_get(struct connman_service *service,
				bool roaming,
				struct connman_stats_data *data)
{
	struct stats_file *file;
	struct stats_record *rec;

	file = g_hash_table_lookup(stats_hash, service);
	if (!file)
		return -EEXIST;

	if (!roaming)
		rec = file->home;
	else
		rec = file->roaming;

	if (rec) {
		memcpy(data, &rec->data,
			sizeof(struct connman_stats_data));
	}

	return 0;
}

int __connman_stats_init(void)
{
	DBG("");

	stats_hash = g_hash_table_new_full(g_direct_hash, g_direct_equal,
							NULL, stats_free);

	return 0;
}

void __connman_stats_cleanup(void)
{
	DBG("");

	g_hash_table_destroy(stats_hash);
	stats_hash = NULL;
}
