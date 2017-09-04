// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/text_input_client_impl.h"

#include "ui/aura/mus/input_method_mus.h"
#include "ui/base/ime/text_input_client.h"

namespace aura {

TextInputClientImpl::TextInputClientImpl(ui::TextInputClient* text_input_client)
    : text_input_client_(text_input_client), binding_(this) {}

TextInputClientImpl::~TextInputClientImpl() {}

ui::mojom::TextInputClientPtr TextInputClientImpl::CreateInterfacePtrAndBind() {
  return binding_.CreateInterfacePtrAndBind();
}

void TextInputClientImpl::OnCompositionEvent(
    ui::mojom::CompositionEventPtr event) {
  switch (event->type) {
    case ui::mojom::CompositionEventType::INSERT_CHAR: {
      DCHECK((*event->key_event)->IsKeyEvent());
      ui::KeyEvent* key_event = (*event->key_event)->AsKeyEvent();
      DCHECK(key_event->is_char());
      text_input_client_->InsertChar(*key_event);
      break;
    }
    case ui::mojom::CompositionEventType::CONFIRM:
      text_input_client_->ConfirmCompositionText();
      break;
    case ui::mojom::CompositionEventType::CLEAR:
      text_input_client_->ClearCompositionText();
      break;
    case ui::mojom::CompositionEventType::UPDATE:
    case ui::mojom::CompositionEventType::INSERT_TEXT:
      // TODO(moshayedi): crbug.com/631524. Implement these types of composition
      // events once we have the necessary fields in ui.mojom.CompositionEvent.
      NOTIMPLEMENTED();
      break;
  }
}

}  // namespace aura
