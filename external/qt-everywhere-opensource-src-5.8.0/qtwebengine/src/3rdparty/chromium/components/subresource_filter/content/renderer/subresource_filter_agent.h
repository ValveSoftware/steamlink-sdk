// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_SUBRESOURCE_FILTER_AGENT_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_SUBRESOURCE_FILTER_AGENT_H_

#include "base/macros.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "content/public/renderer/render_frame_observer.h"

namespace subresource_filter {

class RulesetDealer;

// The renderer-side agent of the ContentSubresourceFilterDriver. There is one
// instance per RenderFrame, responsible for setting up the subresource filter
// for the ongoing provisional document load in the frame when instructed to do
// so by the driver.
class SubresourceFilterAgent : public content::RenderFrameObserver {
 public:
  explicit SubresourceFilterAgent(content::RenderFrame* render_frame,
                                  RulesetDealer* ruleset_dealer);
  ~SubresourceFilterAgent() override;

 private:
  // content::RenderFrameObserver:
  void OnDestruct() override;
  void DidStartProvisionalLoad() override;
  void DidCommitProvisionalLoad(bool is_new_navigation,
                                bool is_same_page_navigation) override;

  // content::RenderFrameObserver:
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnActivateForProvisionalLoad(ActivationState activation_state);

  // Owned by the ChromeContentRendererClient and outlives us.
  RulesetDealer* ruleset_dealer_;

  ActivationState activation_state_for_provisional_load_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterAgent);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_SUBRESOURCE_FILTER_AGENT_H_
