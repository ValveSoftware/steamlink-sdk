// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/subresource_filter_agent.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "components/subresource_filter/content/renderer/document_subresource_filter.h"
#include "components/subresource_filter/content/renderer/ruleset_dealer.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/renderer/render_frame.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "url/gurl.h"

namespace subresource_filter {

SubresourceFilterAgent::SubresourceFilterAgent(
    content::RenderFrame* render_frame,
    RulesetDealer* ruleset_dealer)
    : content::RenderFrameObserver(render_frame),
      ruleset_dealer_(ruleset_dealer),
      activation_state_for_provisional_load_(ActivationState::DISABLED) {
  DCHECK(ruleset_dealer);
}

SubresourceFilterAgent::~SubresourceFilterAgent() = default;

std::vector<GURL> SubresourceFilterAgent::GetAncestorDocumentURLs() {
  std::vector<GURL> urls;
  // As a temporary workaround for --isolate-extensions, ignore the ancestor
  // hierarchy after crossing an extension/non-extension boundary. This,
  // however, will not be a satisfactory solution for OOPIF in general.
  // See: https://crbug.com/637415.
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  do {
    urls.push_back(frame->document().url());
    frame = frame->parent();
  } while (frame && frame->isWebLocalFrame());
  return urls;
}

void SubresourceFilterAgent::SetSubresourceFilterForCommittedLoad(
    std::unique_ptr<blink::WebDocumentSubresourceFilter> filter) {
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  web_frame->dataSource()->setSubresourceFilter(filter.release());
}

void SubresourceFilterAgent::
    SignalFirstSubresourceDisallowedForCommittedLoad() {
  render_frame()->Send(new SubresourceFilterHostMsg_DidDisallowFirstSubresource(
      render_frame()->GetRoutingID()));
}

void SubresourceFilterAgent::OnActivateForProvisionalLoad(
    ActivationState activation_state,
    const GURL& url) {
  activation_state_for_provisional_load_ = activation_state;
  url_for_provisional_load_ = url;
}

void SubresourceFilterAgent::RecordHistogramsOnLoadCommitted() {
  UMA_HISTOGRAM_ENUMERATION(
      "SubresourceFilter.DocumentLoad.ActivationState",
      static_cast<int>(activation_state_for_provisional_load_),
      static_cast<int>(ActivationState::LAST) + 1);

  if (activation_state_for_provisional_load_ != ActivationState::DISABLED) {
    UMA_HISTOGRAM_BOOLEAN("SubresourceFilter.DocumentLoad.RulesetIsAvailable",
                          ruleset_dealer_->IsRulesetAvailable());
  }
}

void SubresourceFilterAgent::RecordHistogramsOnLoadFinished() {
  DCHECK(filter_for_last_committed_load_);
  UMA_HISTOGRAM_COUNTS_1000(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Total",
      filter_for_last_committed_load_->num_loads_total());
  UMA_HISTOGRAM_COUNTS_1000(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Evaluated",
      filter_for_last_committed_load_->num_loads_evaluated());
  UMA_HISTOGRAM_COUNTS_1000(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.MatchedRules",
      filter_for_last_committed_load_->num_loads_matching_rules());
  UMA_HISTOGRAM_COUNTS_1000(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Disallowed",
      filter_for_last_committed_load_->num_loads_disallowed());
}

void SubresourceFilterAgent::OnDestruct() {
  delete this;
}

void SubresourceFilterAgent::DidStartProvisionalLoad() {
  // With PlzNavigate, DidStartProvisionalLoad and DidCommitProvisionalLoad will
  // both be called in response to the one commit IPC from the browser. That
  // means that they will come after OnActivateForProvisionalLoad. So we have to
  // have extra logic to check that the response to OnActivateForProvisionalLoad
  // isn't removed in that case.
  blink::WebDataSource* ds =
      render_frame() ? render_frame()->GetWebFrame()->provisionalDataSource()
                     : nullptr;
  if (!content::IsBrowserSideNavigationEnabled() ||
      (!ds ||
       static_cast<GURL>(ds->request().url()) != url_for_provisional_load_)) {
    activation_state_for_provisional_load_ = ActivationState::DISABLED;
  } else {
    url_for_provisional_load_ = GURL();
  }
  filter_for_last_committed_load_.reset();
}

void SubresourceFilterAgent::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_page_navigation) {
  RecordHistogramsOnLoadCommitted();
  if (activation_state_for_provisional_load_ != ActivationState::DISABLED &&
      ruleset_dealer_->IsRulesetAvailable()) {
    std::vector<GURL> ancestor_document_urls = GetAncestorDocumentURLs();
    base::Closure first_disallowed_load_callback(
        base::Bind(&SubresourceFilterAgent::
                       SignalFirstSubresourceDisallowedForCommittedLoad,
                   AsWeakPtr()));
    std::unique_ptr<DocumentSubresourceFilter> filter(
        new DocumentSubresourceFilter(activation_state_for_provisional_load_,
                                      ruleset_dealer_->GetRuleset(),
                                      ancestor_document_urls,
                                      first_disallowed_load_callback));
    filter_for_last_committed_load_ = filter->AsWeakPtr();
    SetSubresourceFilterForCommittedLoad(std::move(filter));
  }
  activation_state_for_provisional_load_ = ActivationState::DISABLED;
}

void SubresourceFilterAgent::DidFinishLoad() {
  if (!filter_for_last_committed_load_)
    return;

  RecordHistogramsOnLoadFinished();
}

bool SubresourceFilterAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SubresourceFilterAgent, message)
    IPC_MESSAGE_HANDLER(SubresourceFilterMsg_ActivateForProvisionalLoad,
                        OnActivateForProvisionalLoad)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

}  // namespace subresource_filter
