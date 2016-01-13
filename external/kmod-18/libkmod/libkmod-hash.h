#pragma once

#include <stdbool.h>

struct hash;

struct hash_iter {
	const struct hash *hash;
	unsigned int bucket;
	unsigned int entry;
};

struct hash *hash_new(unsigned int n_buckets, void (*free_value)(void *value));
void hash_free(struct hash *hash);
int hash_add(struct hash *hash, const char *key, const void *value);
int hash_add_unique(struct hash *hash, const char *key, const void *value);
int hash_del(struct hash *hash, const char *key);
void *hash_find(const struct hash *hash, const char *key);
unsigned int hash_get_count(const struct hash *hash);
void hash_iter_init(const struct hash *hash, struct hash_iter *iter);
bool hash_iter_next(struct hash_iter *iter, const char **key,
							const void **value);
