// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NAVIGATION_STATE_IMPL_H_
#define CONTENT_RENDERER_NAVIGATION_STATE_IMPL_H_

#include <string>

#include "base/macros.h"
#include "content/common/navigation_params.h"
#include "content/public/renderer/navigation_state.h"

namespace content {

class CONTENT_EXPORT NavigationStateImpl : public NavigationState {
 public:
  ~NavigationStateImpl() override;

  static NavigationStateImpl* CreateBrowserInitiated(
      const CommonNavigationParams& common_params,
      const StartNavigationParams& start_params,
      const RequestNavigationParams& request_params);

  static NavigationStateImpl* CreateContentInitiated();

  // NavigationState implementation.
  ui::PageTransition GetTransitionType() override;
  bool WasWithinSamePage() override;
  bool IsContentInitiated() override;

  const CommonNavigationParams& common_params() const { return common_params_; }
  const StartNavigationParams& start_params() const { return start_params_; }
  const RequestNavigationParams& request_params() const {
    return request_params_;
  }
  bool request_committed() const { return request_committed_; }
  void set_request_committed(bool value) { request_committed_ = value; }
  void set_was_within_same_page(bool value) { was_within_same_page_ = value; }

  void set_transition_type(ui::PageTransition transition) {
    common_params_.transition = transition;
  }

 private:
  NavigationStateImpl(const CommonNavigationParams& common_params,
                      const StartNavigationParams& start_params,
                      const RequestNavigationParams& request_params,
                      bool is_content_initiated);

  bool request_committed_;
  bool was_within_same_page_;

  // True if this navigation was not initiated via WebFrame::LoadRequest.
  const bool is_content_initiated_;

  CommonNavigationParams common_params_;
  const StartNavigationParams start_params_;

  // Note: if IsContentInitiated() is false, whether this navigation should
  // replace the current entry in the back/forward history list is determined by
  // the should_replace_current_entry field in |history_params|. Otherwise, use
  // replacesCurrentHistoryItem() on the WebDataSource.
  //
  // TODO(davidben): It would be good to unify these and have only one source
  // for the two cases. We can plumb this through WebFrame::loadRequest to set
  // lockBackForwardList on the FrameLoadRequest. However, this breaks process
  // swaps because FrameLoader::loadWithNavigationAction treats loads before a
  // FrameLoader has committedFirstRealDocumentLoad as a replacement. (Added for
  // http://crbug.com/178380).
  const RequestNavigationParams request_params_;

  DISALLOW_COPY_AND_ASSIGN(NavigationStateImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_NAVIGATION_STATE_IMPL_H_
