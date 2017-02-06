// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 *     (http://www.torchmobile.com/)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CONTENT_RENDERER_HISTORY_CONTROLLER_H_
#define CONTENT_RENDERER_HISTORY_CONTROLLER_H_

#include <memory>
#include <utility>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/renderer/history_entry.h"
#include "third_party/WebKit/public/web/WebHistoryCommitType.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"

namespace blink {
class WebFrame;
class WebLocalFrame;
enum class WebCachePolicy;
}

namespace content {
class RenderFrameImpl;
class RenderViewImpl;
struct NavigationParams;

// A guide to history state in the renderer:
//
// HistoryController: Owned by RenderView, is the entry point for interacting
//     with history. Handles most of the operations to modify history state,
//     navigate to an existing back/forward entry, etc.
//
// HistoryEntry: Represents a single entry in the back/forward list,
//     encapsulating all frames in the page it represents. It provides access
//     to each frame's state via lookups by frame id or frame name.
// HistoryNode: Represents a single frame in a HistoryEntry. Owned by a
//     HistoryEntry. HistoryNodes form a tree that mirrors the frame tree in
//     the corresponding page.
// HistoryNodes represent the structure of the page, but don't hold any
// per-frame state except a list of child frames.
// WebHistoryItem (lives in blink): The state for a given frame. Can persist
//     across navigations. WebHistoryItem is reference counted, and each
//     HistoryNode holds a reference to its single corresponding
//     WebHistoryItem. Can be referenced by multiple HistoryNodes and can
//     therefore exist in multiple HistoryEntry instances.
//
// Suppose we have the following page, foo.com, which embeds foo.com/a in an
// iframe:
//
// HistoryEntry 0:
//     HistoryNode 0_0 (WebHistoryItem A (url: foo.com))
//         HistoryNode 0_1: (WebHistoryItem B (url: foo.com/a))
//
// Now we navigate the top frame to bar.com, which embeds bar.com/b and
// bar.com/c in iframes, and bar.com/b in turn embeds bar.com/d. We will
// create a new HistoryEntry with a tree containing 4 new HistoryNodes. The
// state will be:
//
// HistoryEntry 1:
//     HistoryNode 1_0 (WebHistoryItem C (url: bar.com))
//         HistoryNode 1_1: (WebHistoryItem D (url: bar.com/b))
//             HistoryNode 1_3: (WebHistoryItem F (url: bar.com/d))
//         HistoryNode 1_2: (WebHistoryItem E (url: bar.com/c))
//
//
// Finally, we navigate the first subframe from bar.com/b to bar.com/e, which
// embeds bar.com/f. We will create a new HistoryEntry and new HistoryNode for
// each frame. Any frame that navigates (bar.com/e and its child, bar.com/f)
// will receive a new WebHistoryItem. However, 2 frames were not navigated
// (bar.com and bar.com/c), so those two frames will reuse the existing
// WebHistoryItem:
//
// HistoryEntry 2:
//     HistoryNode 2_0 (WebHistoryItem C (url: bar.com))  *REUSED*
//         HistoryNode 2_1: (WebHistoryItem G (url: bar.com/e))
//            HistoryNode 2_3: (WebHistoryItem H (url: bar.com/f))
//         HistoryNode 2_2: (WebHistoryItem E (url: bar.com/c)) *REUSED*
//
class CONTENT_EXPORT HistoryController {
 public:
  explicit HistoryController(RenderViewImpl* render_view);
  ~HistoryController();

  void set_provisional_entry(std::unique_ptr<HistoryEntry> entry) {
    provisional_entry_ = std::move(entry);
  }

  // Return true if the main frame ended up loading a request as part of the
  // history navigation.
  bool GoToEntry(blink::WebLocalFrame* main_frame,
                 std::unique_ptr<HistoryEntry> entry,
                 std::unique_ptr<NavigationParams> navigation_params,
                 blink::WebCachePolicy cache_policy);

  void UpdateForCommit(RenderFrameImpl* frame,
                       const blink::WebHistoryItem& item,
                       blink::WebHistoryCommitType commit_type,
                       bool navigation_within_page);

  HistoryEntry* GetCurrentEntry();
  blink::WebHistoryItem GetItemForNewChildFrame(RenderFrameImpl* frame) const;
  void RemoveChildrenForRedirect(RenderFrameImpl* frame);

 private:
  typedef std::vector<std::pair<blink::WebFrame*, blink::WebHistoryItem> >
      HistoryFrameLoadVector;
  void RecursiveGoToEntry(blink::WebFrame* frame,
                          HistoryFrameLoadVector& sameDocumentLoads,
                          HistoryFrameLoadVector& differentDocumentLoads);

  void UpdateForInitialLoadInChildFrame(RenderFrameImpl* frame,
                                        const blink::WebHistoryItem& item);
  void CreateNewBackForwardItem(RenderFrameImpl* frame,
                                const blink::WebHistoryItem& item,
                                bool clone_children_of_target);

  RenderViewImpl* render_view_;

  // A HistoryEntry representing the currently-loaded page.
  std::unique_ptr<HistoryEntry> current_entry_;
  // A HistoryEntry representing the page that is being loaded, or an empty
  // scoped_ptr if no page is being loaded.
  std::unique_ptr<HistoryEntry> provisional_entry_;

  // The NavigationParams corresponding to the last back/forward load that was
  // initiated by |GoToEntry|. This is kept around so that it can be passed into
  // existing frames affected by a history navigation in GoToEntry(), and can be
  // passed into frames created after the commit that resulted from the
  // navigation in GetItemForNewChildFrame().
  //
  // This is reset in UpdateForCommit if we see a commit from a different
  // navigation, to avoid using stale parameters.
  std::unique_ptr<NavigationParams> navigation_params_;

  DISALLOW_COPY_AND_ASSIGN(HistoryController);
};

}  // namespace content

#endif  // CONTENT_RENDERER_HISTORY_CONTROLLER_H_
