// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_message_queue.h"

#include <algorithm>

#include "base/logging.h"
#include "media/midi/midi_message_util.h"

namespace media {
namespace {

const uint8 kSysEx = 0xf0;
const uint8 kEndOfSysEx = 0xf7;

bool IsDataByte(uint8 data) {
  return (data & 0x80) == 0;
}

bool IsFirstStatusByte(uint8 data) {
  return !IsDataByte(data) && data != kEndOfSysEx;
}

bool IsSystemRealTimeMessage(uint8 data) {
  return 0xf8 <= data && data <= 0xff;
}

}  // namespace

MidiMessageQueue::MidiMessageQueue(bool allow_running_status)
    : allow_running_status_(allow_running_status) {}

MidiMessageQueue::~MidiMessageQueue() {}

void MidiMessageQueue::Add(const std::vector<uint8>& data) {
  queue_.insert(queue_.end(), data.begin(), data.end());
}

void MidiMessageQueue::Add(const uint8* data, size_t length) {
  queue_.insert(queue_.end(), data, data + length);
}

void MidiMessageQueue::Get(std::vector<uint8>* message) {
  message->clear();

  while (true) {
    if (queue_.empty())
      return;

    const uint8 next = queue_.front();
    queue_.pop_front();

    // "System Real Time Messages" is a special kind of MIDI messages, which can
    // appear at arbitrary byte position of MIDI stream. Here we reorder
    // "System Real Time Messages" prior to |next_message_| so that each message
    // can be clearly separated as a complete MIDI message.
    if (IsSystemRealTimeMessage(next)) {
      message->push_back(next);
      return;
    }

    // Here |next_message_[0]| may contain the previous status byte when
    // |allow_running_status_| is true. Following condition fixes up
    // |next_message_| if running status condition is not fulfilled.
    if (!next_message_.empty() &&
        ((next_message_[0] == kSysEx && IsFirstStatusByte(next)) ||
         (next_message_[0] != kSysEx && !IsDataByte(next)))) {
      // An invalid data sequence is found or running status condition is not
      // fulfilled.
      next_message_.clear();
    }

    if (next_message_.empty()) {
      if (IsFirstStatusByte(next)) {
        next_message_.push_back(next);
      } else {
        // MIDI protocol doesn't provide any error correction mechanism in
        // physical layers, and incoming messages can be corrupted, and should
        // be corrected here.
      }
      continue;
    }

    // Here we can assume |next_message_| starts with a valid status byte.
    const uint8 status_byte = next_message_[0];
    next_message_.push_back(next);

    if (status_byte == kSysEx) {
      if (next == kEndOfSysEx) {
        std::swap(*message, next_message_);
        next_message_.clear();
        return;
      }
      continue;
    }

    DCHECK(IsDataByte(next));
    DCHECK_NE(kSysEx, status_byte);
    const size_t target_len = GetMidiMessageLength(status_byte);
    if (next_message_.size() < target_len)
      continue;
    if (next_message_.size() == target_len) {
      std::swap(*message, next_message_);
      next_message_.clear();
      if (allow_running_status_) {
        // Speculatively keep the status byte in case of running status. If this
        // assumption is not true, |next_message_| will be cleared anyway.
        next_message_.push_back(status_byte);
      }
      return;
    }

    NOTREACHED();
  }
}

}  // namespace media
