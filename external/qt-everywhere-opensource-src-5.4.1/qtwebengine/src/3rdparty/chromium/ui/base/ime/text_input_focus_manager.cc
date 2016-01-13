// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/text_input_focus_manager.h"

#include "base/logging.h"
#include "base/memory/singleton.h"

namespace ui {

TextInputFocusManager* TextInputFocusManager::GetInstance() {
  TextInputFocusManager* instance = Singleton<TextInputFocusManager>::get();
  DCHECK(instance->thread_checker_.CalledOnValidThread());
  return instance;
}

TextInputClient* TextInputFocusManager::GetFocusedTextInputClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return focused_text_input_client_;
}

void TextInputFocusManager::FocusTextInputClient(
    TextInputClient* text_input_client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  focused_text_input_client_ = text_input_client;
}

void TextInputFocusManager::BlurTextInputClient(
    TextInputClient* text_input_client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (focused_text_input_client_ == text_input_client)
    focused_text_input_client_ = NULL;
}

TextInputFocusManager::TextInputFocusManager()
    : focused_text_input_client_(NULL) {}

TextInputFocusManager::~TextInputFocusManager() {}

}  // namespace ui
