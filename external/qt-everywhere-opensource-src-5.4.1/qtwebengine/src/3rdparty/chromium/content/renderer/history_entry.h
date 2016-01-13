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

#ifndef CONTENT_RENDERER_HISTORY_ENTRY_H_
#define CONTENT_RENDERER_HISTORY_ENTRY_H_

#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"

namespace blink {
class WebFrame;
}

namespace content {
class RenderFrameImpl;
class RenderViewImpl;

const int kInvalidFrameRoutingID = -1;

class CONTENT_EXPORT HistoryEntry {
 public:
  class HistoryNode {
   public:
    HistoryNode(HistoryEntry* entry,
                const blink::WebHistoryItem& item,
                int64_t frame_id);
    ~HistoryNode();

    HistoryNode* AddChild(const blink::WebHistoryItem& item, int64_t frame_id);
    HistoryNode* AddChild();
    HistoryNode* CloneAndReplace(HistoryEntry* new_entry,
                                 const blink::WebHistoryItem& new_item,
                                 bool clone_children_of_target,
                                 RenderFrameImpl* target_frame,
                                 RenderFrameImpl* current_frame);
    blink::WebHistoryItem& item() { return item_; }
    void set_item(const blink::WebHistoryItem& item);
    std::vector<HistoryNode*>& children() const { return children_->get(); }
    void RemoveChildren();

   private:
    HistoryEntry* entry_;
    scoped_ptr<ScopedVector<HistoryNode> > children_;
    blink::WebHistoryItem item_;
  };

  HistoryEntry(const blink::WebHistoryItem& root, int64_t frame_id);
  HistoryEntry();
  ~HistoryEntry();

  HistoryEntry* CloneAndReplace(const blink::WebHistoryItem& newItem,
                                bool clone_children_of_target,
                                RenderFrameImpl* target_frame,
                                RenderViewImpl* render_view);

  HistoryNode* GetHistoryNodeForFrame(RenderFrameImpl* frame);
  blink::WebHistoryItem GetItemForFrame(RenderFrameImpl* frame);
  const blink::WebHistoryItem& root() const { return root_->item(); }
  HistoryNode* root_history_node() const { return root_.get(); }

 private:

  scoped_ptr<HistoryNode> root_;

  typedef base::hash_map<uint64_t, HistoryNode*> FramesToItems;
  FramesToItems frames_to_items_;

  typedef base::hash_map<std::string, HistoryNode*> UniqueNamesToItems;
  UniqueNamesToItems unique_names_to_items_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_HISTORY_ENTRY_H_
