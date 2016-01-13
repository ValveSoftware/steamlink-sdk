/*
 * libkmod - interface to kernel module operations
 *
 * Copyright (C) 2011-2013  ProFUSION embedded systems
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <stdint.h>

/* Integers are stored as 32 bit unsigned in "network" order, i.e. MSB first.
   All files start with a magic number.

   Magic spells "BOOTFAST". Second one used on newer versioned binary files.
 */
/* #define INDEX_MAGIC_OLD 0xB007FA57 */
#define INDEX_MAGIC 0xB007F457

/* We use a version string to keep track of changes to the binary format
 * This is stored in the form: INDEX_MAJOR (hi) INDEX_MINOR (lo) just in
 * case we ever decide to have minor changes that are not incompatible.
 */

#define INDEX_VERSION_MAJOR 0x0002
#define INDEX_VERSION_MINOR 0x0001
#define INDEX_VERSION ((INDEX_VERSION_MAJOR<<16)|INDEX_VERSION_MINOR)

/* The index file maps keys to values. Both keys and values are ASCII strings.
   Each key can have multiple values. Values are sorted by an integer priority.

   The reader also implements a wildcard search (including range expressions)
   where the keys in the index are treated as patterns.
   This feature is required for module aliases.
*/

/* Implementation is based on a radix tree, or "trie".
   Each arc from parent to child is labelled with a character.
   Each path from the root represents a string.

   == Example strings ==

   ask
   ate
   on
   once
   one

   == Key ==
    + Normal node
    * Marked node, representing a key and it's values.

   +
   |-a-+-s-+-k-*
   |   |
   |   `-t-+-e-*
   |
   `-o-+-n-*-c-+-e-*
           |
           `-e-*

   Naive implementations tend to be very space inefficient; child pointers
   are stored in arrays indexed by character, but most child pointers are null.

   Our implementation uses a scheme described by Wikipedia as a Patrica trie,

       "easiest to understand as a space-optimized trie where
        each node with only one child is merged with its child"

   +
   |-a-+-sk-*
   |   |
   |   `-te-*
   |
   `-on-*-ce-*
        |
        `-e-*

   We still use arrays of child pointers indexed by a single character;
   the remaining characters of the label are stored as a "prefix" in the child.

   The paper describing the original Patrica trie works on individiual bits -
   each node has a maximum of two children, which increases space efficiency.
   However for this application it is simpler to use the ASCII character set.
   Since the index file is read-only, it can be compressed by omitting null
   child pointers at the start and end of arrays.
*/

#define INDEX_PRIORITY_MIN UINT32_MAX

struct index_value {
	struct index_value *next;
	unsigned int priority;
	unsigned int len;
	char value[0];
};

/* In-memory index (depmod only) */
struct index_file;
struct index_file *index_file_open(const char *filename);
void index_file_close(struct index_file *idx);
char *index_search(struct index_file *idx, const char *key);
void index_dump(struct index_file *in, int fd, const char *prefix);
struct index_value *index_searchwild(struct index_file *idx, const char *key);

void index_values_free(struct index_value *values);

/* Implementation using mmap */
struct index_mm;
struct index_mm *index_mm_open(struct kmod_ctx *ctx, const char *filename,
						unsigned long long *stamp);
void index_mm_close(struct index_mm *index);
char *index_mm_search(struct index_mm *idx, const char *key);
struct index_value *index_mm_searchwild(struct index_mm *idx, const char *key);
void index_mm_dump(struct index_mm *idx, int fd, const char *prefix);
