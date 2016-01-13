// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_MOCK_IME_ENGINE_HANDLER_H_
#define UI_BASE_IME_CHROMEOS_MOCK_IME_ENGINE_HANDLER_H_

#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ui_base_export.h"
#include "ui/events/event.h"

namespace chromeos {

class UI_BASE_EXPORT MockIMEEngineHandler : public IMEEngineHandlerInterface {
 public:
  MockIMEEngineHandler();
  virtual ~MockIMEEngineHandler();

  virtual void FocusIn(const InputContext& input_context) OVERRIDE;
  virtual void FocusOut() OVERRIDE;
  virtual void Enable() OVERRIDE;
  virtual void Disable() OVERRIDE;
  virtual void PropertyActivate(const std::string& property_name) OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void ProcessKeyEvent(const ui::KeyEvent& key_event,
                               const KeyEventDoneCallback& callback) OVERRIDE;
  virtual void CandidateClicked(uint32 index) OVERRIDE;
  virtual void SetSurroundingText(const std::string& text, uint32 cursor_pos,
                                  uint32 anchor_pos) OVERRIDE;

  int focus_in_call_count() const { return focus_in_call_count_; }
  int focus_out_call_count() const { return focus_out_call_count_; }
  int reset_call_count() const { return reset_call_count_; }
  int set_surrounding_text_call_count() const {
    return set_surrounding_text_call_count_;
  }
  int process_key_event_call_count() const {
    return process_key_event_call_count_;
  }

  const InputContext& last_text_input_context() const {
    return last_text_input_context_;
  }

  std::string last_activated_property() const {
    return last_activated_property_;
  }

  std::string last_set_surrounding_text() const {
    return last_set_surrounding_text_;
  }

  uint32 last_set_surrounding_cursor_pos() const {
    return last_set_surrounding_cursor_pos_;
  }

  uint32 last_set_surrounding_anchor_pos() const {
    return last_set_surrounding_anchor_pos_;
  }

  const ui::KeyEvent* last_processed_key_event() const {
    return last_processed_key_event_.get();
  }

  const KeyEventDoneCallback& last_passed_callback() const {
    return last_passed_callback_;
  }

 private:
  int focus_in_call_count_;
  int focus_out_call_count_;
  int set_surrounding_text_call_count_;
  int process_key_event_call_count_;
  int reset_call_count_;
  InputContext last_text_input_context_;
  std::string last_activated_property_;
  std::string last_set_surrounding_text_;
  uint32 last_set_surrounding_cursor_pos_;
  uint32 last_set_surrounding_anchor_pos_;
  scoped_ptr<ui::KeyEvent> last_processed_key_event_;
  KeyEventDoneCallback last_passed_callback_;
};

}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_MOCK_IME_ENGINE_HANDLER_H_
