/*
 * Copyright (C) 2013-2015 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include "aconfig.h"
#include "gettext.h"

#include "common.h"
#include "alsa.h"
#include "bat-signal.h"

int retval_play;
int retval_record;

/* update chunk_fmt data to bat */
static int update_fmt_to_bat(struct bat *bat, struct chunk_fmt *fmt)
{
	bat->channels = fmt->channels;
	bat->rate = fmt->sample_rate;
	bat->sample_size = fmt->sample_length / 8;
	if (bat->sample_size > 4) {
		fprintf(bat->err, _("Invalid format: sample size=%d\n"),
				bat->sample_size);
		return -EINVAL;
	}
	bat->frame_size = fmt->blocks_align;

	return 0;
}

/* calculate frames and update to bat */
static int update_frames_to_bat(struct bat *bat,
		struct wav_chunk_header *header, FILE *fp)
{
	/* The number of analyzed captured frames is arbitrarily set to half of
	   the number of frames of the wav file or the number of frames of the
	   wav file when doing direct analysis (--local) */
	bat->frames = header->length / bat->frame_size;
	if (!bat->local)
		bat->frames /= 2;

	return 0;
}

static int read_chunk_fmt(struct bat *bat, char *file, FILE *fp, bool skip,
		struct wav_chunk_header *header)
{
	size_t err;
	int header_skip;
	struct chunk_fmt chunk_fmt;

	err = fread(&chunk_fmt, sizeof(chunk_fmt), 1, fp);
	if (err != 1) {
		fprintf(bat->err, _("Read chunk fmt error: %s:%zd\n"),
				file, err);
		return -EIO;
	}
	/* If the format header is larger, skip the rest */
	header_skip = header->length - sizeof(chunk_fmt);
	if (header_skip > 0) {
		err = fseek(fp, header_skip, SEEK_CUR);
		if (err == -1) {
			fprintf(bat->err, _("Seek fmt header error: %s:%zd\n"),
					file, err);
			return -EINVAL;
		}
	}
	/* If the file is opened for playback, update BAT data;
	   If the file is opened for analysis, no update */
	if (skip == false) {
		err = update_fmt_to_bat(bat, &chunk_fmt);
		if (err != 0)
			return err;
	}

	return 0;
}

int read_wav_header(struct bat *bat, char *file, FILE *fp, bool skip)
{
	struct wav_header riff_wave_header;
	struct wav_chunk_header chunk_header;
	int more_chunks = 1;
	size_t err;

	/* Read header of RIFF wav file */
	err = fread(&riff_wave_header, sizeof(riff_wave_header), 1, fp);
	if (err != 1) {
		fprintf(bat->err, _("Read header error: %s:%zd\n"), file, err);
		return -EIO;
	}
	if ((riff_wave_header.magic != WAV_RIFF)
			|| (riff_wave_header.type != WAV_WAVE)) {
		fprintf(bat->err, _("%s is not a riff/wave file\n"), file);
		return -EINVAL;
	}

	/* Read chunks in RIFF wav file */
	do {
		err = fread(&chunk_header, sizeof(chunk_header), 1, fp);
		if (err != 1) {
			fprintf(bat->err, _("Read chunk header error: "));
			fprintf(bat->err, _("%s:%zd\n"), file, err);
			return -EIO;
		}

		switch (chunk_header.type) {
		case WAV_FMT:
			/* WAV_FMT chunk, read and analyze */
			err = read_chunk_fmt(bat, file, fp, skip,
					&chunk_header);
			if (err != 0)
				return err;
			break;
		case WAV_DATA:
			/* WAV_DATA chunk, break looping */
			/* If the file is opened for playback, update BAT data;
			   If the file is opened for analysis, no update */
			if (skip == false) {
				err = update_frames_to_bat(bat, &chunk_header,
						fp);
				if (err != 0)
					return err;
			}
			/* Stop looking for chunks */
			more_chunks = 0;
			break;
		default:
			/* Unknown chunk, skip bytes */
			err = fseek(fp, chunk_header.length, SEEK_CUR);
			if (err == -1) {
				fprintf(bat->err, _("Fail to skip unknown"));
				fprintf(bat->err, _(" chunk of %s:%zd\n"),
						file, err);
				return -EINVAL;
			}
		}
	} while (more_chunks);

	return 0;
}

void prepare_wav_info(struct wav_container *wav, struct bat *bat)
{
	wav->header.magic = WAV_RIFF;
	wav->header.type = WAV_WAVE;
	wav->format.magic = WAV_FMT;
	wav->format.fmt_size = 16;
	wav->format.format = WAV_FORMAT_PCM;
	wav->format.channels = bat->channels;
	wav->format.sample_rate = bat->rate;
	wav->format.sample_length = bat->sample_size * 8;
	wav->format.blocks_align = bat->channels * bat->sample_size;
	wav->format.bytes_p_second = wav->format.blocks_align * bat->rate;
	wav->chunk.length = bat->frames * bat->frame_size;
	wav->chunk.type = WAV_DATA;
	wav->header.length = (wav->chunk.length) + sizeof(wav->chunk)
			+ sizeof(wav->format) + sizeof(wav->header) - 8;
}

int write_wav_header(FILE *fp, struct wav_container *wav, struct bat *bat)
{
	int err = 0;

	err = fwrite(&wav->header, 1, sizeof(wav->header), fp);
	if (err != sizeof(wav->header)) {
		fprintf(bat->err, _("Write file error: header %d\n"), err);
		return -EIO;
	}
	err = fwrite(&wav->format, 1, sizeof(wav->format), fp);
	if (err != sizeof(wav->format)) {
		fprintf(bat->err, _("Write file error: format %d\n"), err);
		return -EIO;
	}
	err = fwrite(&wav->chunk, 1, sizeof(wav->chunk), fp);
	if (err != sizeof(wav->chunk)) {
		fprintf(bat->err, _("Write file error: chunk %d\n"), err);
		return -EIO;
	}

	return 0;
}

/* update wav header when data size changed */
int update_wav_header(struct bat *bat, FILE *fp, int bytes)
{
	int err = 0;
	struct wav_container wav;

	prepare_wav_info(&wav, bat);
	wav.chunk.length = bytes;
	wav.header.length = (wav.chunk.length) + sizeof(wav.chunk)
		+ sizeof(wav.format) + sizeof(wav.header) - 8;
	rewind(fp);
	err = write_wav_header(fp, &wav, bat);

	return err;
}

/*
 * Generate buffer to be played either from input file or from generated data
 * Return value
 * <0 error
 * 0 ok
 * >0 break
 */
int generate_input_data(struct bat *bat, void *buffer, int bytes, int frames)
{
	int err;
	static int load;

	if (bat->playback.file != NULL) {
		/* From input file */
		load = 0;

		while (1) {
			err = fread(buffer + load, 1, bytes - load, bat->fp);
			if (0 == err) {
				if (feof(bat->fp)) {
					fprintf(bat->log,
							_("End of playing.\n"));
					return 1;
				}
			} else if (err < bytes - load) {
				if (ferror(bat->fp)) {
					fprintf(bat->err, _("Read file error"));
					fprintf(bat->err, _(": %d\n"), err);
					return -EIO;
				}
				load += err;
			} else {
				break;
			}
		}
	} else {
		/* Generate sine wave */
		if ((bat->sinus_duration) && (load > bat->sinus_duration))
			return 1;

		err = generate_sine_wave(bat, frames, buffer);
		if (err != 0)
			return err;

		load += frames;
	}

	return 0;
}
