/*
 * xattrs.c --- Modify extended attributes via debugfs.
 *
 * Copyright (C) 2014 Oracle.  This file may be redistributed
 * under the terms of the GNU Public License.
 */

#include "config.h"
#include <stdio.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int optind;
extern char *optarg;
#endif
#include <ctype.h>

#include "debugfs.h"

/* Dump extended attributes */
static void dump_xattr_string(FILE *out, const char *str, int len)
{
	int printable = 0;
	int i;

	/* check: is string "printable enough?" */
	for (i = 0; i < len; i++)
		if (isprint(str[i]))
			printable++;

	if (printable <= len*7/8)
		printable = 0;

	for (i = 0; i < len; i++)
		if (printable)
			fprintf(out, isprint(str[i]) ? "%c" : "\\%03o",
				(unsigned char)str[i]);
		else
			fprintf(out, "%02x ", (unsigned char)str[i]);
}

static int dump_attr(char *name, char *value, size_t value_len, void *data)
{
	FILE *out = data;

	fprintf(out, "  ");
	dump_xattr_string(out, name, strlen(name));
	if (strcmp(name, "system.data") != 0) {
		fprintf(out, " = \"");
		dump_xattr_string(out, value, value_len);
		fprintf(out, "\"");
	}
	fprintf(out, " (%zu)\n", value_len);

	return 0;
}

void dump_inode_attributes(FILE *out, ext2_ino_t ino)
{
	struct ext2_xattr_handle *h;
	size_t sz;
	errcode_t err;

	err = ext2fs_xattrs_open(current_fs, ino, &h);
	if (err)
		return;

	err = ext2fs_xattrs_read(h);
	if (err)
		goto out;

	err = ext2fs_xattrs_count(h, &sz);
	if (err || sz == 0)
		goto out;

	fprintf(out, "Extended attributes:\n");
	err = ext2fs_xattrs_iterate(h, dump_attr, out);
	if (err)
		goto out;

out:
	err = ext2fs_xattrs_close(&h);
}

void do_list_xattr(int argc, char **argv)
{
	ext2_ino_t ino;

	if (argc != 2) {
		printf("%s: Usage: %s <file>\n", argv[0],
		       argv[0]);
		return;
	}

	if (check_fs_open(argv[0]))
		return;

	ino = string_to_inode(argv[1]);
	if (!ino)
		return;

	dump_inode_attributes(stdout, ino);
}

void do_get_xattr(int argc, char **argv)
{
	ext2_ino_t ino;
	struct ext2_xattr_handle *h;
	FILE *fp = NULL;
	char *buf = NULL;
	size_t buflen;
	int i;
	errcode_t err;

	reset_getopt();
	while ((i = getopt(argc, argv, "f:")) != -1) {
		switch (i) {
		case 'f':
			if (fp)
				fclose(fp);
			fp = fopen(optarg, "w");
			if (fp == NULL) {
				perror(optarg);
				return;
			}
			break;
		default:
			printf("%s: Usage: %s <file> <attr> [-f outfile]\n",
			       argv[0], argv[0]);
			goto out2;
		}
	}

	if (optind != argc - 2) {
		printf("%s: Usage: %s <file> <attr> [-f outfile]\n", argv[0],
		       argv[0]);
		goto out2;
	}

	if (check_fs_open(argv[0]))
		goto out2;

	ino = string_to_inode(argv[optind]);
	if (!ino)
		goto out2;

	err = ext2fs_xattrs_open(current_fs, ino, &h);
	if (err)
		goto out2;

	err = ext2fs_xattrs_read(h);
	if (err)
		goto out;

	err = ext2fs_xattr_get(h, argv[optind + 1], (void **)&buf, &buflen);
	if (err)
		goto out;

	if (fp) {
		fwrite(buf, buflen, 1, fp);
	} else {
		dump_xattr_string(stdout, buf, buflen);
		printf("\n");
	}

	ext2fs_free_mem(&buf);
out:
	ext2fs_xattrs_close(&h);
	if (err)
		com_err(argv[0], err, "while getting extended attribute");
out2:
	if (fp)
		fclose(fp);
}

void do_set_xattr(int argc, char **argv)
{
	ext2_ino_t ino;
	struct ext2_xattr_handle *h;
	FILE *fp = NULL;
	char *buf = NULL;
	size_t buflen;
	int i;
	errcode_t err;

	reset_getopt();
	while ((i = getopt(argc, argv, "f:")) != -1) {
		switch (i) {
		case 'f':
			if (fp)
				fclose(fp);
			fp = fopen(optarg, "r");
			if (fp == NULL) {
				perror(optarg);
				return;
			}
			break;
		default:
			goto print_usage;
		}
	}

	if (!(fp && optind == argc - 2) && !(!fp && optind == argc - 3)) {
	print_usage:
		printf("Usage:\t%s <file> <attr> <value>\n", argv[0]);
		printf("\t%s -f <value_file> <file> <attr>\n", argv[0]);
		goto out2;
	}

	if (check_fs_open(argv[0]))
		goto out2;
	if (check_fs_read_write(argv[0]))
		goto out2;
	if (check_fs_bitmaps(argv[0]))
		goto out2;

	ino = string_to_inode(argv[optind]);
	if (!ino)
		goto out2;

	err = ext2fs_xattrs_open(current_fs, ino, &h);
	if (err)
		goto out2;

	err = ext2fs_xattrs_read(h);
	if (err)
		goto out;

	if (fp) {
		err = ext2fs_get_mem(current_fs->blocksize, &buf);
		if (err)
			goto out;
		buflen = fread(buf, 1, current_fs->blocksize, fp);
	} else {
		buf = argv[optind + 2];
		buflen = strlen(argv[optind + 2]);
	}

	err = ext2fs_xattr_set(h, argv[optind + 1], buf, buflen);
	if (err)
		goto out;

	err = ext2fs_xattrs_write(h);
	if (err)
		goto out;

out:
	ext2fs_xattrs_close(&h);
	if (err)
		com_err(argv[0], err, "while setting extended attribute");
out2:
	if (fp) {
		fclose(fp);
		ext2fs_free_mem(&buf);
	}
}

void do_rm_xattr(int argc, char **argv)
{
	ext2_ino_t ino;
	struct ext2_xattr_handle *h;
	int i;
	errcode_t err;

	if (argc < 3) {
		printf("%s: Usage: %s <file> <attrs>...\n", argv[0], argv[0]);
		return;
	}

	if (check_fs_open(argv[0]))
		return;
	if (check_fs_read_write(argv[0]))
		return;
	if (check_fs_bitmaps(argv[0]))
		return;

	ino = string_to_inode(argv[1]);
	if (!ino)
		return;

	err = ext2fs_xattrs_open(current_fs, ino, &h);
	if (err)
		return;

	err = ext2fs_xattrs_read(h);
	if (err)
		goto out;

	for (i = 2; i < argc; i++) {
		err = ext2fs_xattr_remove(h, argv[i]);
		if (err)
			goto out;
	}

	err = ext2fs_xattrs_write(h);
	if (err)
		goto out;
out:
	ext2fs_xattrs_close(&h);
	if (err)
		com_err(argv[0], err, "while removing extended attribute");
}
