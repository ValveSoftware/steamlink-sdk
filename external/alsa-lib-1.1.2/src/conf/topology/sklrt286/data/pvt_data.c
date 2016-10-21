/*
 * Copyright(c) 2014-2016 Intel Corporation
 * All rights reserved.
 *
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
 * Authors: Shreyas Nc <shreyas.nc@intel.com>
 *
 */
#include "pvt.c"
#include "stdio.h"
#include "fcntl.h"
#include <local.h>
#include <limits.h>
#include <stdint.h>
#include <linux/types.h>
#include "global.h"
#include "list.h"

#include <sound/asound.h>
#include <sound/asoc.h>

int replace_space(char *path, char *newpath)
{
	char buffer[52];
	char *p;

	strcpy(buffer, path);

	while ((p = strchr(buffer, ' ')))
		p[0] = '-';

	strcpy(newpath, buffer);
	return 0;
}

/*
 * The private data structures are written into a
 * binary blob. These contain module private data
 * information
 */
int main(void)
{
	unsigned int i;
	FILE  *fd;
	char path[128];
	char new_path[128];
	struct snd_soc_tplg_private *priv = NULL;

	memset(path, 0, sizeof(path));
	memset(new_path, 0, sizeof(new_path));

	priv = calloc(1, sizeof(dfw_wrap) + sizeof(uint32_t));

	for (i = 0; i < ARRAY_SIZE(dfw_wrap); i++) {
		strcat(path, "../");
		strcat(path, dfw_wrap[i].name);
		strcat(path, ".bin");

		replace_space(path, new_path);

		priv->size = (uint32_t)sizeof(dfw_wrap[i].skl_dfw_mod);

		memcpy(priv->data, &dfw_wrap[i].skl_dfw_mod,
				priv->size);

		fd = fopen(new_path, "wb");

		if (fd == NULL)
			return -ENOENT;

		if (fwrite(priv->data, priv->size, 1, fd) != 1) {
			fclose(fd);
			return -1;
		}

		memset(path, 0, sizeof(path));
	}

	free(priv);
	return 0;
}
