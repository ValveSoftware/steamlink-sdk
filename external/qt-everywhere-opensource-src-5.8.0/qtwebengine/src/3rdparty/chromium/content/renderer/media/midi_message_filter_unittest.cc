// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/midi_message_filter.h"

#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
using BlinkState = blink::WebMIDIAccessorClient::MIDIPortState;
}  // namespace

TEST(MidiMessageFilterTest, CastMidiPortState) {
  // Check if static_cast of ToMIDIPortState() just works fine for all states.
  EXPECT_EQ(
      BlinkState::MIDIPortStateDisconnected,
      MidiMessageFilter::ToBlinkState(media::midi::MIDI_PORT_DISCONNECTED));
  EXPECT_EQ(BlinkState::MIDIPortStateConnected,
            MidiMessageFilter::ToBlinkState(media::midi::MIDI_PORT_CONNECTED));
  // Web MIDI API manages DeviceState and ConnectionState separately.
  // "open", "pending", or "closed" are managed separately for ConnectionState
  // by Blink per MIDIAccess instance. So, MIDI_PORT_OPENED in content can be
  // converted to MIDIPortStateConnected.
  EXPECT_EQ(BlinkState::MIDIPortStateConnected,
            MidiMessageFilter::ToBlinkState(media::midi::MIDI_PORT_OPENED));

  // Check if we do not have any unknown MidiPortState that is added later.
  EXPECT_EQ(media::midi::MIDI_PORT_OPENED, media::midi::MIDI_PORT_STATE_LAST);
}

}  // namespace content
