// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_RESULT_H_
#define MEDIA_MIDI_MIDI_RESULT_H_

namespace media {

// Result codes for MIDI.
enum MidiResult {
  MIDI_OK,
  MIDI_NOT_SUPPORTED,
  MIDI_INITIALIZATION_ERROR,

  // |MIDI_RESULT_LAST| is used in content/common/media/midi_messages.h with
  // IPC_ENUM_TRAITS_MAX_VALUE macro. Keep the value up to date. Otherwise
  // a new value can not be passed to the renderer.
  MIDI_RESULT_LAST = MIDI_INITIALIZATION_ERROR,
};

}  // namespace media

#endif  // MEDIA_MIDI_MIDI_RESULT_H_
