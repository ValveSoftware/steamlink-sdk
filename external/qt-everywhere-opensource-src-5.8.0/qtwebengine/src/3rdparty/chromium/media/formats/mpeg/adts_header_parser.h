// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MPEG_ADTS_HEADER_PARSER_H_
#define MEDIA_FORMATS_MPEG_ADTS_HEADER_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include "media/base/media_export.h"

namespace media {

class AudioDecoderConfig;

// Parses ADTS header |adts_header| (4 bytes) and extracts the information into
// |config| structure if the parsing succeeds. Returns true if the parsing
// succeeds or false otherwise. The |is_sbr| flag stands for Spectral Band
// Replication. |orig_sample_rate| will return the sample frequency before
// doubling in SBR.
MEDIA_EXPORT bool ParseAdtsHeader(const uint8_t* adts_header,
                                  bool is_sbr,
                                  AudioDecoderConfig* config,
                                  size_t* orig_sample_rate);

}  // namespace media

#endif  // MEDIA_FORMATS_MPEG_ADTS_HEADER_PARSER_H_
