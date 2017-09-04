// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/ime/arc_ime_bridge_impl.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/arc/arc_bridge_service.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"

namespace arc {
namespace {

constexpr uint32_t kMinVersionForOnKeyboardsBoundsChanging = 3;
constexpr uint32_t kMinVersionForExtendSelectionAndDelete = 4;

ui::TextInputType ConvertTextInputType(mojom::TextInputType ipc_type) {
  // The two enum types are similar, but intentionally made not identical.
  // We cannot force them to be in sync. If we do, updates in ui::TextInputType
  // must always be propagated to the mojom::TextInputType mojo definition in
  // ARC container side, which is in a different repository than Chromium.
  // We don't want such dependency.
  //
  // That's why we need a lengthy switch statement instead of static_cast
  // guarded by a static assert on the two enums to be in sync.
  switch (ipc_type) {
    case mojom::TextInputType::NONE:
      return ui::TEXT_INPUT_TYPE_NONE;
    case mojom::TextInputType::TEXT:
      return ui::TEXT_INPUT_TYPE_TEXT;
    case mojom::TextInputType::PASSWORD:
      return ui::TEXT_INPUT_TYPE_PASSWORD;
    case mojom::TextInputType::SEARCH:
      return ui::TEXT_INPUT_TYPE_SEARCH;
    case mojom::TextInputType::EMAIL:
      return ui::TEXT_INPUT_TYPE_EMAIL;
    case mojom::TextInputType::NUMBER:
      return ui::TEXT_INPUT_TYPE_NUMBER;
    case mojom::TextInputType::TELEPHONE:
      return ui::TEXT_INPUT_TYPE_TELEPHONE;
    case mojom::TextInputType::URL:
      return ui::TEXT_INPUT_TYPE_URL;
    case mojom::TextInputType::DATE:
      return ui::TEXT_INPUT_TYPE_DATE;
    case mojom::TextInputType::TIME:
      return ui::TEXT_INPUT_TYPE_TIME;
    case mojom::TextInputType::DATETIME:
      return ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL;
    default:
      return ui::TEXT_INPUT_TYPE_TEXT;
  }
}

std::vector<mojom::CompositionSegmentPtr> ConvertSegments(
    const ui::CompositionText& composition) {
  std::vector<mojom::CompositionSegmentPtr> segments;
  for (const ui::CompositionUnderline& underline : composition.underlines) {
    mojom::CompositionSegmentPtr segment = mojom::CompositionSegment::New();
    segment->start_offset = underline.start_offset;
    segment->end_offset = underline.end_offset;
    segment->emphasized =
        (underline.thick ||
         (composition.selection.start() == underline.start_offset &&
          composition.selection.end() == underline.end_offset));
    segments.push_back(std::move(segment));
  }
  return segments;
}

}  // namespace

ArcImeBridgeImpl::ArcImeBridgeImpl(Delegate* delegate,
                                   ArcBridgeService* bridge_service)
    : binding_(this), delegate_(delegate), bridge_service_(bridge_service) {
  bridge_service_->ime()->AddObserver(this);
}

ArcImeBridgeImpl::~ArcImeBridgeImpl() {
  bridge_service_->ime()->RemoveObserver(this);
}

void ArcImeBridgeImpl::OnInstanceReady() {
  auto* instance = bridge_service_->ime()->GetInstanceForMethod("Init");
  DCHECK(instance);
  instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcImeBridgeImpl::SendSetCompositionText(
    const ui::CompositionText& composition) {
  auto* ime_instance =
      bridge_service_->ime()->GetInstanceForMethod("SetCompositionText");
  if (!ime_instance)
    return;

  ime_instance->SetCompositionText(base::UTF16ToUTF8(composition.text),
                                   ConvertSegments(composition));
}

void ArcImeBridgeImpl::SendConfirmCompositionText() {
  auto* ime_instance =
      bridge_service_->ime()->GetInstanceForMethod("ConfirmCompositionText");
  if (!ime_instance)
    return;

  ime_instance->ConfirmCompositionText();
}

void ArcImeBridgeImpl::SendInsertText(const base::string16& text) {
  auto* ime_instance =
      bridge_service_->ime()->GetInstanceForMethod("InsertText");
  if (!ime_instance)
    return;

  ime_instance->InsertText(base::UTF16ToUTF8(text));
}

void ArcImeBridgeImpl::SendOnKeyboardBoundsChanging(
    const gfx::Rect& new_bounds) {
  auto* ime_instance = bridge_service_->ime()->GetInstanceForMethod(
      "OnKeyboardBoundsChanging", kMinVersionForOnKeyboardsBoundsChanging);
  if (!ime_instance)
    return;

  ime_instance->OnKeyboardBoundsChanging(new_bounds);
}

void ArcImeBridgeImpl::SendExtendSelectionAndDelete(
    size_t before, size_t after) {
  auto* ime_instance = bridge_service_->ime()->GetInstanceForMethod(
      "ExtendSelectionAndDelete", kMinVersionForExtendSelectionAndDelete);
  if (!ime_instance)
    return;

  ime_instance->ExtendSelectionAndDelete(before, after);
}

void ArcImeBridgeImpl::OnTextInputTypeChanged(mojom::TextInputType type) {
  delegate_->OnTextInputTypeChanged(ConvertTextInputType(type));
}

void ArcImeBridgeImpl::OnCursorRectChanged(mojom::CursorRectPtr rect) {
  delegate_->OnCursorRectChanged(gfx::Rect(rect->left, rect->top,
                                           rect->right - rect->left,
                                           rect->bottom - rect->top));
}

void ArcImeBridgeImpl::OnCancelComposition() {
  delegate_->OnCancelComposition();
}

void ArcImeBridgeImpl::ShowImeIfNeeded() {
  delegate_->ShowImeIfNeeded();
}

}  // namespace arc
