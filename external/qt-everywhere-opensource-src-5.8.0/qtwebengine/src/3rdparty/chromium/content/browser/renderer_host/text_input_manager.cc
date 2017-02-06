// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/text_input_manager.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"

namespace content {

namespace {

bool AreDifferentTextInputStates(const content::TextInputState& old_state,
                                 const content::TextInputState& new_state) {
#if defined(USE_AURA)
  return old_state.type != new_state.type || old_state.mode != new_state.mode ||
         old_state.flags != new_state.flags ||
         old_state.can_compose_inline != new_state.can_compose_inline;
#else
  // TODO(ekaramad): Implement the logic for other platforms (crbug.com/578168).
  NOTREACHED();
  return true;
#endif
}

}  // namespace

TextInputManager::TextInputManager() : active_view_(nullptr) {}

TextInputManager::~TextInputManager() {
  // If there is an active view, we should unregister it first so that the
  // the tab's top-level RWHV will be notified about |TextInputState.type|
  // resetting to none (i.e., we do not have an active RWHV anymore).
  if (active_view_)
    Unregister(active_view_);

  // Unregister all the remaining views.
  std::vector<RenderWidgetHostViewBase*> views;
  for (auto pair : text_input_state_map_)
    views.push_back(pair.first);

  for (auto view : views)
    Unregister(view);
}

const TextInputState* TextInputManager::GetTextInputState() {
  return !!active_view_ ? &text_input_state_map_[active_view_] : nullptr;
}

RenderWidgetHostImpl* TextInputManager::GetActiveWidget() const {
  return !!active_view_ ? static_cast<RenderWidgetHostImpl*>(
                              active_view_->GetRenderWidgetHost())
                        : nullptr;
}

void TextInputManager::UpdateTextInputState(
    RenderWidgetHostViewBase* view,
    const TextInputState& text_input_state) {
  DCHECK(IsRegistered(view));

  // Since |view| is registgered, we already have a previous value for its
  // TextInputState.
  bool changed = AreDifferentTextInputStates(text_input_state_map_[view],
                                             text_input_state);

  text_input_state_map_[view] = text_input_state;

  // |active_view_| is only updated when the state for |view| is not none.
  if (text_input_state.type != ui::TEXT_INPUT_TYPE_NONE)
    active_view_ = view;

  // If the state for |active_view_| is none, then we no longer have an
  // |active_view_|.
  if (active_view_ == view && text_input_state.type == ui::TEXT_INPUT_TYPE_NONE)
    active_view_ = nullptr;

  NotifyObserversAboutInputStateUpdate(view, changed);
}

void TextInputManager::ImeCancelComposition(RenderWidgetHostViewBase* view) {
  DCHECK(IsRegistered(view));
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnImeCancelComposition(this, view));
}

void TextInputManager::Register(RenderWidgetHostViewBase* view) {
  DCHECK(!IsRegistered(view));

  text_input_state_map_[view] = TextInputState();
}

void TextInputManager::Unregister(RenderWidgetHostViewBase* view) {
  DCHECK(IsRegistered(view));

  text_input_state_map_.erase(view);
  if (active_view_ == view) {
    active_view_ = nullptr;
    NotifyObserversAboutInputStateUpdate(view, true);
  }
  view->DidUnregisterFromTextInputManager(this);
}

bool TextInputManager::IsRegistered(RenderWidgetHostViewBase* view) const {
  return text_input_state_map_.count(view) == 1;
}

void TextInputManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void TextInputManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

size_t TextInputManager::GetRegisteredViewsCountForTesting() {
  return text_input_state_map_.size();
}

ui::TextInputType TextInputManager::GetTextInputTypeForViewForTesting(
    RenderWidgetHostViewBase* view) {
  DCHECK(IsRegistered(view));
  return text_input_state_map_[view].type;
}

void TextInputManager::NotifyObserversAboutInputStateUpdate(
    RenderWidgetHostViewBase* updated_view,
    bool did_update_state) {
  FOR_EACH_OBSERVER(
      Observer, observer_list_,
      OnUpdateTextInputStateCalled(this, updated_view, did_update_state));
}

}  // namespace content