// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/navigation/public/cpp/view.h"

#include "base/strings/utf_string_conversions.h"
#include "components/mus/public/cpp/window.h"
#include "services/navigation/public/cpp/view_delegate.h"
#include "services/navigation/public/cpp/view_observer.h"

namespace navigation {

////////////////////////////////////////////////////////////////////////////////
// View, public:

View::View(mojom::ViewFactoryPtr factory) : binding_(this) {
  mojom::ViewClientPtr client;
  binding_.Bind(GetProxy(&client));
  factory->CreateView(std::move(client), GetProxy(&view_));
}

View::View(mojom::ViewPtr view, mojom::ViewClientRequest request)
    : view_(std::move(view)), binding_(this, std::move(request)) {}

View::~View() {}

void View::AddObserver(ViewObserver* observer) {
  observers_.AddObserver(observer);
}

void View::RemoveObserver(ViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

void View::NavigateToURL(const GURL& url) {
  view_->NavigateTo(url);
}

void View::NavigateToOffset(int offset) {
  view_->NavigateToOffset(offset);
}

void View::GoBack() {
  if (can_go_back_)
    view_->GoBack();
}

void View::GoForward() {
  if (can_go_forward_)
    view_->GoForward();
}

void View::GetBackMenuItems(std::vector<NavigationListItem>* items) {
  DCHECK(items);
  for (int i = navigation_list_cursor_ - 1, offset = -1; i >= 0;
       --i, --offset) {
    std::string title = navigation_list_[i]->title;
    items->push_back(NavigationListItem(base::UTF8ToUTF16(title), offset));
  }
}

void View::GetForwardMenuItems(std::vector<NavigationListItem>* items) {
  DCHECK(items);
  for (int i = navigation_list_cursor_ + 1, offset = 1;
       i < static_cast<int>(navigation_list_.size()); ++i, ++offset) {
    std::string title = navigation_list_[i]->title;
    items->push_back(NavigationListItem(base::UTF8ToUTF16(title), offset));
  }
}

void View::Reload(bool bypass_cache) {
  view_->Reload(bypass_cache);
}

void View::Stop() {
  view_->Stop();
}

void View::ShowInterstitial(const std::string& html) {
  view_->ShowInterstitial(html);
}

void View::HideInterstitial() {
  view_->HideInterstitial();
}

void View::SetResizerSize(const gfx::Size& size) {
  view_->SetResizerSize(size);
}

void View::EmbedInWindow(mus::Window* parent) {
  mus::mojom::WindowTreeClientPtr client;
  view_->GetWindowTreeClient(GetProxy(&client));
  parent->Embed(std::move(client));
}

////////////////////////////////////////////////////////////////////////////////
// View, mojom::ViewClient implementation:

void View::OpenURL(mojom::OpenURLParamsPtr params) {
  if (delegate_)
    delegate_->OpenURL(this, std::move(params));
}

void View::LoadingStateChanged(bool is_loading) {
  is_loading_ = is_loading;
  FOR_EACH_OBSERVER(ViewObserver, observers_, LoadingStateChanged(this));
}

void View::NavigationStateChanged(const GURL& url,
                                  const mojo::String& title,
                                  bool can_go_back,
                                  bool can_go_forward) {
  url_ = url;
  title_ = base::UTF8ToUTF16(title.get());
  can_go_back_ = can_go_back;
  can_go_forward_ = can_go_forward;
  FOR_EACH_OBSERVER(ViewObserver, observers_, NavigationStateChanged(this));
}

void View::LoadProgressChanged(double progress) {
  FOR_EACH_OBSERVER(ViewObserver, observers_,
                    LoadProgressChanged(this, progress));
}

void View::UpdateHoverURL(const GURL& url) {
  FOR_EACH_OBSERVER(ViewObserver, observers_, HoverTargetURLChanged(this, url));
}

void View::ViewCreated(mojom::ViewPtr view,
                       mojom::ViewClientRequest request,
                       bool is_popup,
                       const gfx::Rect& initial_bounds,
                       bool user_gesture) {
  if (delegate_) {
    delegate_->ViewCreated(
        this, base::WrapUnique(new View(std::move(view), std::move(request))),
        is_popup, initial_bounds, user_gesture);
  }
}

void View::Close() {
  if (delegate_)
    delegate_->Close(this);
}

void View::NavigationPending(mojom::NavigationEntryPtr entry) {
  pending_navigation_ = std::move(entry);
}

void View::NavigationCommitted(mojom::NavigationCommittedDetailsPtr details,
                               int current_index) {
  switch (details->type) {
    case mojom::NavigationType::NEW_PAGE:
      navigation_list_.push_back(std::move(pending_navigation_));
      navigation_list_cursor_ = current_index;
      break;
    case mojom::NavigationType::EXISTING_PAGE:
      navigation_list_cursor_ = current_index;
      break;
    default:
      break;
  }
}

void View::NavigationEntryChanged(mojom::NavigationEntryPtr entry,
                                  int entry_index) {
  navigation_list_[entry_index] = std::move(entry);
}

void View::NavigationListPruned(bool from_front, int count) {
  DCHECK(count < static_cast<int>(navigation_list_.size()));
  if (from_front) {
    auto it = navigation_list_.begin() + count;
    navigation_list_.erase(navigation_list_.begin(), it);
  } else {
    auto it = navigation_list_.end() - count;
    navigation_list_.erase(it, navigation_list_.end());
  }
}

}  // namespace navigation
