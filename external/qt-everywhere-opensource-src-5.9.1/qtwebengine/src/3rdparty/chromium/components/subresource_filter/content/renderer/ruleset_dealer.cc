// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/ruleset_dealer.h"

#include <utility>

#include "base/files/file.h"
#include "base/logging.h"
#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "ipc/ipc_message_macros.h"

namespace subresource_filter {

RulesetDealer::RulesetDealer() = default;
RulesetDealer::~RulesetDealer() = default;

void RulesetDealer::SetRulesetFile(base::File ruleset_file) {
  DCHECK(ruleset_file.IsValid());
  ruleset_file_ = std::move(ruleset_file);
  weak_cached_ruleset_.reset();
}

bool RulesetDealer::IsRulesetAvailable() const {
  return ruleset_file_.IsValid();
}

scoped_refptr<const MemoryMappedRuleset> RulesetDealer::GetRuleset() {
  if (!ruleset_file_.IsValid())
    return nullptr;

  scoped_refptr<const MemoryMappedRuleset> strong_ruleset_ref;
  if (weak_cached_ruleset_) {
    strong_ruleset_ref = weak_cached_ruleset_.get();
  } else {
    MemoryMappedRuleset* ruleset =
        new MemoryMappedRuleset(ruleset_file_.Duplicate());
    strong_ruleset_ref = ruleset;
    weak_cached_ruleset_ = ruleset->AsWeakPtr();
  }
  return strong_ruleset_ref;
}

bool RulesetDealer::OnControlMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RulesetDealer, message)
    IPC_MESSAGE_HANDLER(SubresourceFilterMsg_SetRulesetForProcess,
                        OnSetRulesetForProcess)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RulesetDealer::OnSetRulesetForProcess(
    const IPC::PlatformFileForTransit& platform_file) {
  SetRulesetFile(IPC::PlatformFileForTransitToFile(platform_file));
}

}  // namespace subresource_filter
