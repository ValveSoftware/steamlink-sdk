// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NAVIGATION_PUBLIC_CPP_VIEW_H_
#define SERVICES_NAVIGATION_PUBLIC_CPP_VIEW_H_

#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/navigation/public/interfaces/view.mojom.h"

namespace mus {
class Window;
}

namespace navigation {

class ViewDelegate;
class ViewObserver;

// Represents an item in a View's navigation list.
struct NavigationListItem {
  NavigationListItem(const base::string16& title, int offset)
      : title(title), offset(offset) {}
  ~NavigationListItem() {}

  base::string16 title;
  // The navigation offset from the current page in the navigation list.
  int offset;
};

class View : public mojom::ViewClient {
 public:
  explicit View(mojom::ViewFactoryPtr factory);
  View(mojom::ViewPtr view, mojom::ViewClientRequest request);
  View(const View&) = delete;
  void operator=(const View&) = delete;
  ~View() override;

  void set_delegate(ViewDelegate* delegate) { delegate_ = delegate; }

  void AddObserver(ViewObserver* observer);
  void RemoveObserver(ViewObserver* observer);

  // Loading.
  void NavigateToURL(const GURL& url);
  void NavigateToOffset(int offset);
  bool is_loading() const { return is_loading_; }
  const GURL& url() const { return url_; }
  const base::string16& title() const { return title_; }

  // Back/Forward.
  void GoBack();
  void GoForward();
  bool can_go_back() const { return can_go_back_; }
  bool can_go_forward() const { return can_go_forward_; }
  void GetBackMenuItems(std::vector<NavigationListItem>* items);
  void GetForwardMenuItems(std::vector<NavigationListItem>* items);

  // Reload/Stop.
  void Reload(bool bypass_cache);
  void Stop();

  // Interstitials.
  void ShowInterstitial(const std::string& html);
  void HideInterstitial();

  // When non-empty, specifies the size of an area in the bottom corner of the
  // View that should allow the enclosing top-level window to be resized via the
  // pointer.
  void SetResizerSize(const gfx::Size& size);

  // Embed the View visually within |parent|.
  void EmbedInWindow(mus::Window* parent);

 private:
  // mojom::ViewClient:
  void OpenURL(mojom::OpenURLParamsPtr params) override;
  void LoadingStateChanged(bool is_loading) override;
  void NavigationStateChanged(const GURL& url,
                              const mojo::String& title,
                              bool can_go_back,
                              bool can_go_forward) override;
  void LoadProgressChanged(double progress) override;
  void UpdateHoverURL(const GURL& url) override;
  void ViewCreated(mojom::ViewPtr view,
                   mojom::ViewClientRequest request,
                   bool is_popup,
                   const gfx::Rect& initial_bounds,
                   bool user_gesture) override;
  void Close() override;
  void NavigationPending(mojom::NavigationEntryPtr entry) override;
  void NavigationCommitted(mojom::NavigationCommittedDetailsPtr details,
                           int current_index) override;
  void NavigationEntryChanged(mojom::NavigationEntryPtr entry,
                              int entry_index) override;
  void NavigationListPruned(bool from_front, int count) override;

  mojom::ViewPtr view_;
  mojo::Binding<mojom::ViewClient> binding_;

  ViewDelegate* delegate_ = nullptr;
  base::ObserverList<ViewObserver> observers_;

  bool is_loading_ = false;
  GURL url_;
  base::string16 title_;
  bool can_go_back_ = false;
  bool can_go_forward_ = false;

  mojom::NavigationEntryPtr pending_navigation_;
  std::vector<mojom::NavigationEntryPtr> navigation_list_;
  int navigation_list_cursor_ = 0;
};

}  // namespace navigation

#endif  // SERVICES_NAVIGATION_PUBLIC_CPP_VIEW_H_
