// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_message_util.h"

namespace media {

size_t GetMidiMessageLength(uint8 status_byte) {
  if (status_byte < 0x80)
    return 0;
  if (0x80 <= status_byte && status_byte <= 0xbf)
    return 3;
  if (0xc0 <= status_byte && status_byte <= 0xdf)
    return 2;
  if (0xe0 <= status_byte && status_byte <= 0xef)
    return 3;
  if (status_byte == 0xf0)
    return 0;
  if (status_byte == 0xf1)
    return 2;
  if (status_byte == 0xf2)
    return 3;
  if (status_byte == 0xf3)
    return 2;
  if (0xf4 <= status_byte && status_byte <= 0xf6)
    return 1;
  if (status_byte == 0xf7)
    return 0;
  // 0xf8 <= status_byte && status_byte <= 0xff
  return 1;
}

}  // namespace media
