// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/ime_bridge.h"

#include <map>
#include "base/logging.h"
#include "base/memory/singleton.h"

namespace chromeos {

static IMEBridge* g_ime_bridge = NULL;

// An implementation of IMEBridge.
class IMEBridgeImpl : public IMEBridge {
 public:
  IMEBridgeImpl()
    : input_context_handler_(NULL),
      engine_handler_(NULL),
      candidate_window_handler_(NULL),
      current_text_input_(ui::TEXT_INPUT_TYPE_NONE) {
  }

  virtual ~IMEBridgeImpl() {
  }

  // IMEBridge override.
  virtual IMEInputContextHandlerInterface*
      GetInputContextHandler() const OVERRIDE {
    return input_context_handler_;
  }

  // IMEBridge override.
  virtual void SetInputContextHandler(
      IMEInputContextHandlerInterface* handler) OVERRIDE {
    input_context_handler_ = handler;
  }

  // IMEBridge override.
  virtual void SetCurrentEngineHandler(
      IMEEngineHandlerInterface* handler) OVERRIDE {
    engine_handler_ = handler;
  }

  // IMEBridge override.
  virtual IMEEngineHandlerInterface* GetCurrentEngineHandler() const OVERRIDE {
    return engine_handler_;
  }

  // IMEBridge override.
  virtual IMECandidateWindowHandlerInterface* GetCandidateWindowHandler() const
      OVERRIDE {
    return candidate_window_handler_;
  }

  // IMEBridge override.
  virtual void SetCandidateWindowHandler(
      IMECandidateWindowHandlerInterface* handler) OVERRIDE {
    candidate_window_handler_ = handler;
  }

  // IMEBridge override.
  virtual void SetCurrentTextInputType(ui::TextInputType input_type) OVERRIDE {
    current_text_input_ = input_type;
  }

  // IMEBridge override.
  virtual ui::TextInputType GetCurrentTextInputType() const OVERRIDE {
    return current_text_input_;
  }

 private:
  IMEInputContextHandlerInterface* input_context_handler_;
  IMEEngineHandlerInterface* engine_handler_;
  IMECandidateWindowHandlerInterface* candidate_window_handler_;
  ui::TextInputType current_text_input_;

  DISALLOW_COPY_AND_ASSIGN(IMEBridgeImpl);
};

///////////////////////////////////////////////////////////////////////////////
// IMEBridge
IMEBridge::IMEBridge() {
}

IMEBridge::~IMEBridge() {
}

// static.
void IMEBridge::Initialize() {
  if (!g_ime_bridge)
    g_ime_bridge = new IMEBridgeImpl();
}

// static.
void IMEBridge::Shutdown() {
  delete g_ime_bridge;
  g_ime_bridge = NULL;
}

// static.
IMEBridge* IMEBridge::Get() {
  return g_ime_bridge;
}

}  // namespace chromeos
