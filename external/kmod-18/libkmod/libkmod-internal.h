#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <syslog.h>
#include <limits.h>

#include "missing.h"
#include "macro.h"
#include "libkmod.h"

static _always_inline_ _printf_format_(2, 3) void
	kmod_log_null(struct kmod_ctx *ctx, const char *format, ...) {}

#define kmod_log_cond(ctx, prio, arg...) \
	do { \
		if (kmod_get_log_priority(ctx) >= prio) \
		kmod_log(ctx, prio, __FILE__, __LINE__, __func__, ## arg);\
	} while (0)

#ifdef ENABLE_LOGGING
#  ifdef ENABLE_DEBUG
#    define DBG(ctx, arg...) kmod_log_cond(ctx, LOG_DEBUG, ## arg)
#  else
#    define DBG(ctx, arg...) kmod_log_null(ctx, ## arg)
#  endif
#  define INFO(ctx, arg...) kmod_log_cond(ctx, LOG_INFO, ## arg)
#  define ERR(ctx, arg...) kmod_log_cond(ctx, LOG_ERR, ## arg)
#else
#  define DBG(ctx, arg...) kmod_log_null(ctx, ## arg)
#  define INFO(ctx, arg...) kmod_log_null(ctx, ## arg)
#  define ERR(ctx, arg...) kmod_log_null(ctx, ## arg)
#endif

#define KMOD_EXPORT __attribute__ ((visibility("default")))

#define KCMD_LINE_SIZE 4096

#ifndef HAVE_SECURE_GETENV
#  ifdef HAVE___SECURE_GETENV
#    define secure_getenv __secure_getenv
#  else
#    warning neither secure_getenv nor __secure_getenv is available
#    define secure_getenv getenv
#  endif
#endif

void kmod_log(const struct kmod_ctx *ctx,
		int priority, const char *file, int line, const char *fn,
		const char *format, ...) __attribute__((format(printf, 6, 7))) __attribute__((nonnull(1, 3, 5)));

struct list_node {
	struct list_node *next, *prev;
};

struct kmod_list {
	struct list_node node;
	void *data;
};

struct kmod_list *kmod_list_append(struct kmod_list *list, const void *data) _must_check_ __attribute__((nonnull(2)));
struct kmod_list *kmod_list_prepend(struct kmod_list *list, const void *data) _must_check_ __attribute__((nonnull(2)));
struct kmod_list *kmod_list_remove(struct kmod_list *list) _must_check_;
struct kmod_list *kmod_list_remove_data(struct kmod_list *list,
					const void *data) _must_check_ __attribute__((nonnull(2)));
struct kmod_list *kmod_list_remove_n_latest(struct kmod_list *list,
						unsigned int n) _must_check_;
struct kmod_list *kmod_list_insert_after(struct kmod_list *list, const void *data) __attribute__((nonnull(2)));
struct kmod_list *kmod_list_insert_before(struct kmod_list *list, const void *data) __attribute__((nonnull(2)));
struct kmod_list *kmod_list_append_list(struct kmod_list *list1, struct kmod_list *list2) _must_check_;

#undef kmod_list_foreach
#define kmod_list_foreach(list_entry, first_entry) \
	for (list_entry = ((first_entry) == NULL) ? NULL : (first_entry); \
		list_entry != NULL; \
		list_entry = (list_entry->node.next == &((first_entry)->node)) ? NULL : \
		container_of(list_entry->node.next, struct kmod_list, node))

#undef kmod_list_foreach_reverse
#define kmod_list_foreach_reverse(list_entry, first_entry) \
	for (list_entry = (((first_entry) == NULL) ? NULL : container_of(first_entry->node.prev, struct kmod_list, node)); \
		list_entry != NULL; \
		list_entry = ((list_entry == first_entry) ? NULL :	\
		container_of(list_entry->node.prev, struct kmod_list, node)))

/* libkmod.c */
const char *kmod_get_dirname(const struct kmod_ctx *ctx) __attribute__((nonnull(1)));

int kmod_lookup_alias_from_config(struct kmod_ctx *ctx, const char *name, struct kmod_list **list) __attribute__((nonnull(1, 2, 3)));
int kmod_lookup_alias_from_symbols_file(struct kmod_ctx *ctx, const char *name, struct kmod_list **list) __attribute__((nonnull(1, 2, 3)));
int kmod_lookup_alias_from_aliases_file(struct kmod_ctx *ctx, const char *name, struct kmod_list **list) __attribute__((nonnull(1, 2, 3)));
int kmod_lookup_alias_from_moddep_file(struct kmod_ctx *ctx, const char *name, struct kmod_list **list) __attribute__((nonnull(1, 2, 3)));
int kmod_lookup_alias_from_builtin_file(struct kmod_ctx *ctx, const char *name, struct kmod_list **list) __attribute__((nonnull(1, 2, 3)));
int kmod_lookup_alias_from_commands(struct kmod_ctx *ctx, const char *name, struct kmod_list **list) __attribute__((nonnull(1, 2, 3)));
void kmod_set_modules_visited(struct kmod_ctx *ctx, bool visited) __attribute__((nonnull((1))));
void kmod_set_modules_required(struct kmod_ctx *ctx, bool required) __attribute__((nonnull((1))));

char *kmod_search_moddep(struct kmod_ctx *ctx, const char *name) __attribute__((nonnull(1,2)));

struct kmod_module *kmod_pool_get_module(struct kmod_ctx *ctx, const char *key) __attribute__((nonnull(1,2)));
void kmod_pool_add_module(struct kmod_ctx *ctx, struct kmod_module *mod, const char *key) __attribute__((nonnull(1, 2, 3)));
void kmod_pool_del_module(struct kmod_ctx *ctx, struct kmod_module *mod, const char *key) __attribute__((nonnull(1, 2, 3)));

const struct kmod_config *kmod_get_config(const struct kmod_ctx *ctx) __attribute__((nonnull(1)));

/* libkmod-config.c */
struct kmod_config_path {
	unsigned long long stamp;
	char path[];
};

struct kmod_config {
	struct kmod_ctx *ctx;
	struct kmod_list *aliases;
	struct kmod_list *blacklists;
	struct kmod_list *options;
	struct kmod_list *remove_commands;
	struct kmod_list *install_commands;
	struct kmod_list *softdeps;

	struct kmod_list *paths;
};

int kmod_config_new(struct kmod_ctx *ctx, struct kmod_config **config, const char * const *config_paths) __attribute__((nonnull(1, 2,3)));
void kmod_config_free(struct kmod_config *config) __attribute__((nonnull(1)));
const char *kmod_blacklist_get_modname(const struct kmod_list *l) __attribute__((nonnull(1)));
const char *kmod_alias_get_name(const struct kmod_list *l) __attribute__((nonnull(1)));
const char *kmod_alias_get_modname(const struct kmod_list *l) __attribute__((nonnull(1)));
const char *kmod_option_get_options(const struct kmod_list *l) __attribute__((nonnull(1)));
const char *kmod_option_get_modname(const struct kmod_list *l) __attribute__((nonnull(1)));
const char *kmod_command_get_command(const struct kmod_list *l) __attribute__((nonnull(1)));
const char *kmod_command_get_modname(const struct kmod_list *l) __attribute__((nonnull(1)));

const char *kmod_softdep_get_name(const struct kmod_list *l) __attribute__((nonnull(1)));
const char * const *kmod_softdep_get_pre(const struct kmod_list *l, unsigned int *count) __attribute__((nonnull(1, 2)));
const char * const *kmod_softdep_get_post(const struct kmod_list *l, unsigned int *count);


/* libkmod-module.c */
int kmod_module_new_from_alias(struct kmod_ctx *ctx, const char *alias, const char *name, struct kmod_module **mod);
int kmod_module_parse_depline(struct kmod_module *mod, char *line) __attribute__((nonnull(1, 2)));
void kmod_module_set_install_commands(struct kmod_module *mod, const char *cmd) __attribute__((nonnull(1)));
void kmod_module_set_remove_commands(struct kmod_module *mod, const char *cmd) __attribute__((nonnull(1)));
void kmod_module_set_visited(struct kmod_module *mod, bool visited) __attribute__((nonnull(1)));
void kmod_module_set_builtin(struct kmod_module *mod, bool builtin) __attribute__((nonnull((1))));
void kmod_module_set_required(struct kmod_module *mod, bool required) __attribute__((nonnull(1)));

/* libkmod-hash.c */

#include "libkmod-hash.h"

/* libkmod-file.c */
struct kmod_file *kmod_file_open(const struct kmod_ctx *ctx, const char *filename) _must_check_ __attribute__((nonnull(1,2)));
struct kmod_elf *kmod_file_get_elf(struct kmod_file *file) __attribute__((nonnull(1)));
void *kmod_file_get_contents(const struct kmod_file *file) _must_check_ __attribute__((nonnull(1)));
off_t kmod_file_get_size(const struct kmod_file *file) _must_check_ __attribute__((nonnull(1)));
bool kmod_file_get_direct(const struct kmod_file *file) _must_check_ __attribute__((nonnull(1)));
int kmod_file_get_fd(const struct kmod_file *file) _must_check_ __attribute__((nonnull(1)));
void kmod_file_unref(struct kmod_file *file) __attribute__((nonnull(1)));

/* libkmod-elf.c */
struct kmod_elf;
struct kmod_modversion {
	uint64_t crc;
	enum kmod_symbol_bind bind;
	char *symbol;
};

struct kmod_elf *kmod_elf_new(const void *memory, off_t size) _must_check_ __attribute__((nonnull(1)));
void kmod_elf_unref(struct kmod_elf *elf) __attribute__((nonnull(1)));
const void *kmod_elf_get_memory(const struct kmod_elf *elf) _must_check_ __attribute__((nonnull(1)));
int kmod_elf_get_strings(const struct kmod_elf *elf, const char *section, char ***array) _must_check_ __attribute__((nonnull(1,2,3)));
int kmod_elf_get_modversions(const struct kmod_elf *elf, struct kmod_modversion **array) _must_check_ __attribute__((nonnull(1,2)));
int kmod_elf_get_symbols(const struct kmod_elf *elf, struct kmod_modversion **array) _must_check_ __attribute__((nonnull(1,2)));
int kmod_elf_get_dependency_symbols(const struct kmod_elf *elf, struct kmod_modversion **array) _must_check_ __attribute__((nonnull(1,2)));
int kmod_elf_strip_section(struct kmod_elf *elf, const char *section) _must_check_ __attribute__((nonnull(1,2)));
int kmod_elf_strip_vermagic(struct kmod_elf *elf) _must_check_ __attribute__((nonnull(1)));

/*
 * Debug mock lib need to find section ".gnu.linkonce.this_module" in order to
 * get modname
 */
int kmod_elf_get_section(const struct kmod_elf *elf, const char *section, const void **buf, uint64_t *buf_size) _must_check_ __attribute__((nonnull(1,2,3,4)));

/* libkmod-signature.c */
struct kmod_signature_info {
	const char *signer;
	size_t signer_len;
	const char *key_id;
	size_t key_id_len;
	const char *algo, *hash_algo, *id_type;
};
bool kmod_module_signature_info(const struct kmod_file *file, struct kmod_signature_info *sig_info) _must_check_ __attribute__((nonnull(1, 2)));
/* util functions */
#include "libkmod-util.h"
