// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/mock_ime_input_context_handler.h"

#include "chromeos/ime/composition_text.h"

namespace chromeos {

MockIMEInputContextHandler::MockIMEInputContextHandler()
    : commit_text_call_count_(0),
      update_preedit_text_call_count_(0),
      delete_surrounding_text_call_count_(0) {
}

MockIMEInputContextHandler::~MockIMEInputContextHandler() {
}

void MockIMEInputContextHandler::CommitText(const std::string& text) {
  ++commit_text_call_count_;
  last_commit_text_ = text;
}

void MockIMEInputContextHandler::UpdateCompositionText(
    const CompositionText& text,
    uint32 cursor_pos,
    bool visible) {
  ++update_preedit_text_call_count_;
  last_update_composition_arg_.composition_text.CopyFrom(text);
  last_update_composition_arg_.cursor_pos = cursor_pos;
  last_update_composition_arg_.is_visible = visible;
}

void MockIMEInputContextHandler::DeleteSurroundingText(int32 offset,
                                                       uint32 length) {
  ++delete_surrounding_text_call_count_;
  last_delete_surrounding_text_arg_.offset = offset;
  last_delete_surrounding_text_arg_.length = length;
}

void MockIMEInputContextHandler::Reset() {
  commit_text_call_count_ = 0;
  update_preedit_text_call_count_ = 0;
  delete_surrounding_text_call_count_ = 0;
  last_commit_text_.clear();
}

}  // namespace chromeos
