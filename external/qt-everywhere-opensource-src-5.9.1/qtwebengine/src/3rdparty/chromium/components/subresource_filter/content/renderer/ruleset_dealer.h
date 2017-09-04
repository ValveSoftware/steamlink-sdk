// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_RULESET_DEALER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_RULESET_DEALER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_thread_observer.h"
#include "ipc/ipc_platform_file.h"

namespace IPC {
class Message;
}  // namespace IPC

namespace subresource_filter {

class MemoryMappedRuleset;

// Memory maps the subresource filtering ruleset file received over IPC from the
// RulesetDistributor, and makes it available to all SubresourceFilterAgents
// within the current render process.
//
// Although most OS'es will take care of memory mapping pages of the ruleset
// only on-demand, and of swapping out pages eagerly when they are no longer
// used, this class has extra logic to make sure memory is conserved as much as
// possible, and syscall overhead is minimized:
//
//   * The ruleset is only memory mapped on-demand the first time a
//     SubresourceFilterAgent actually needs it, and unmapped once the last
//     SubresourceFilterAgent drops its reference to it.
//
//   * If the ruleset is used by multiple agents within the same renderer
//     process, they will use the same, cached, MemoryMappedRuleset instance,
//     and will not call mmap() multiple times.
//
// See the distribution pipeline diagram in content_ruleset_distributor.h.
class RulesetDealer : public content::RenderThreadObserver {
 public:
  RulesetDealer();
  ~RulesetDealer() override;

  // Sets the |ruleset_file| to memory map and distribute from now on.
  void SetRulesetFile(base::File ruleset_file);

  // Returns whether a subsequent call to GetRuleset() would return a non-null
  // ruleset, but without memory mapping the ruleset.
  bool IsRulesetAvailable() const;

  // Returns the set |ruleset_file|. Normally, the same instance is used by all
  // frames in a given renderer process. That intance is mapped lazily and
  // umapped eagerly as soon as the last reference to it is dropped.
  scoped_refptr<const MemoryMappedRuleset> GetRuleset();

 private:
  friend class SubresourceFilterRulesetDealerTest;

  // Exposed for testing only.
  bool has_cached_ruleset() const { return !!weak_cached_ruleset_.get(); }

  // content::RenderThreadObserver:
  bool OnControlMessageReceived(const IPC::Message& message) override;
  void OnSetRulesetForProcess(const IPC::PlatformFileForTransit& file);

  base::File ruleset_file_;
  base::WeakPtr<MemoryMappedRuleset> weak_cached_ruleset_;

  DISALLOW_COPY_AND_ASSIGN(RulesetDealer);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_RULESET_DEALER_H_
