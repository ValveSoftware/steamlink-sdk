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
  base::File file = IPC::PlatformFileForTransitToFile(platform_file);
  DCHECK(file.IsValid());
  ruleset_ = new MemoryMappedRuleset(std::move(file));
}

}  // namespace subresource_filter
