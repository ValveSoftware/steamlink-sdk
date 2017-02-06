// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/subresource_filter_agent.h"

#include <climits>

#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "components/subresource_filter/content/renderer/ruleset_dealer.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "content/public/renderer/render_frame.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/platform/WebDocumentSubresourceFilter.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace subresource_filter {

namespace {

// Placeholder subresource filter implementation that treats the entire ruleset
// as the UTF-8 representation of a string, and will disallow subresource loads
// from URL paths having this string as a suffix.
// TODO(engedy): Replace this with the actual filtering logic.
class PathSuffixFilter : public blink::WebDocumentSubresourceFilter {
 public:
  explicit PathSuffixFilter(const scoped_refptr<MemoryMappedRuleset>& ruleset)
      : ruleset_(ruleset) {
    static_assert(CHAR_BIT == 8, "Assumed char was 8 bits.");
    disallowed_suffix_ = base::StringPiece(
        reinterpret_cast<const char*>(ruleset_->data()), ruleset_->length());
  }
  ~PathSuffixFilter() override {}

  bool allowLoad(const blink::WebURL& resourceUrl,
                 blink::WebURLRequest::RequestContext) override {
    return disallowed_suffix_.empty() ||
           !base::EndsWith(GURL(resourceUrl).path(), disallowed_suffix_,
                           base::CompareCase::SENSITIVE);
  }

 private:
  scoped_refptr<MemoryMappedRuleset> ruleset_;
  base::StringPiece disallowed_suffix_;

  DISALLOW_COPY_AND_ASSIGN(PathSuffixFilter);
};

}  // namespace

SubresourceFilterAgent::SubresourceFilterAgent(
    content::RenderFrame* render_frame,
    RulesetDealer* ruleset_dealer)
    : content::RenderFrameObserver(render_frame),
      ruleset_dealer_(ruleset_dealer),
      activation_state_for_provisional_load_(ActivationState::DISABLED) {}

SubresourceFilterAgent::~SubresourceFilterAgent() = default;

void SubresourceFilterAgent::OnDestruct() {
  delete this;
}

void SubresourceFilterAgent::DidStartProvisionalLoad() {
  activation_state_for_provisional_load_ = ActivationState::DISABLED;
}

void SubresourceFilterAgent::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_page_navigation) {
  if (activation_state_for_provisional_load_ == ActivationState::ENABLED &&
      ruleset_dealer_->ruleset()) {
    blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
    web_frame->dataSource()->setSubresourceFilter(
        new PathSuffixFilter(ruleset_dealer_->ruleset()));
  }
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

void SubresourceFilterAgent::OnActivateForProvisionalLoad(
    ActivationState activation_state) {
  activation_state_for_provisional_load_ = activation_state;
}

}  // namespace subresource_filter
