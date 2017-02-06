// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_IME_ARC_IME_BRIDGE_IMPL_H_
#define COMPONENTS_ARC_IME_ARC_IME_BRIDGE_IMPL_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/ime.mojom.h"
#include "components/arc/ime/arc_ime_bridge.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
struct CompositionText;
}  // namespace ui

namespace arc {

// This class encapsulates the detail of IME related IPC between
// Chromium and the ARC container.
class ArcImeBridgeImpl : public ArcImeBridge,
                         public mojom::ImeHost,
                         public InstanceHolder<mojom::ImeInstance>::Observer {
 public:
  ArcImeBridgeImpl(Delegate* delegate, ArcBridgeService* bridge_service);
  ~ArcImeBridgeImpl() override;

  // arc::InstanceHolder<mojom::ImeInstance>::Observer overrides:
  void OnInstanceReady() override;

  // ArcImeBridge overrides:
  void SendSetCompositionText(const ui::CompositionText& composition) override;
  void SendConfirmCompositionText() override;
  void SendInsertText(const base::string16& text) override;
  void SendOnKeyboardBoundsChanging(const gfx::Rect& new_bounds) override;
  void SendExtendSelectionAndDelete(size_t before, size_t after) override;

  // arc::mojom::ImeHost overrides:
  void OnTextInputTypeChanged(arc::mojom::TextInputType type) override;
  void OnCursorRectChanged(arc::mojom::CursorRectPtr rect) override;
  void OnCancelComposition() override;
  void ShowImeIfNeeded() override;

 private:
  mojo::Binding<mojom::ImeHost> binding_;
  Delegate* const delegate_;
  ArcBridgeService* const bridge_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcImeBridgeImpl);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_IME_ARC_IME_BRIDGE_IMPL_H_
