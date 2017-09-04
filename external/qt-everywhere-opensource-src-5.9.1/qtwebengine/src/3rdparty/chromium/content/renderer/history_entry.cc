// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "content/renderer/history_entry.h"

#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

using blink::WebFrame;
using blink::WebHistoryItem;

namespace content {

HistoryEntry::HistoryNode* HistoryEntry::HistoryNode::AddChild(
    const WebHistoryItem& item) {
  children_->push_back(new HistoryNode(entry_, item));
  return children_->back();
}

HistoryEntry::HistoryNode* HistoryEntry::HistoryNode::AddChild() {
  return AddChild(WebHistoryItem());
}

HistoryEntry::HistoryNode* HistoryEntry::HistoryNode::CloneAndReplace(
    const base::WeakPtr<HistoryEntry>& new_entry,
    const WebHistoryItem& new_item,
    bool clone_children_of_target,
    RenderFrameImpl* target_frame,
    RenderFrameImpl* current_frame) {
  bool is_target_frame = target_frame == current_frame;
  const WebHistoryItem& item_for_create = is_target_frame ? new_item : item_;
  HistoryNode* new_history_node = new HistoryNode(new_entry, item_for_create);

  // Use the last committed history item for the frame rather than item_, since
  // the latter may not accurately reflect which URL is currently committed in
  // the frame.  See https://crbug.com/612713#c12.
  const WebHistoryItem& current_item = current_frame->current_history_item();
  if (is_target_frame && clone_children_of_target && !current_item.isNull()) {
    // TODO(creis): Setting the document sequence number here appears to be
    // unnecessary.  Remove this block if this DCHECK never fires.
    DCHECK_EQ(current_item.documentSequenceNumber(),
              new_history_node->item().documentSequenceNumber());
  }

  // TODO(creis): This needs to be updated to handle HistoryEntry in
  // subframe processes, where the main frame isn't guaranteed to be in the
  // same process.
  if (current_frame && (clone_children_of_target || !is_target_frame)) {
    for (WebFrame* child = current_frame->GetWebFrame()->firstChild(); child;
         child = child->nextSibling()) {
      RenderFrameImpl* child_render_frame =
          RenderFrameImpl::FromWebFrame(child);
      // TODO(creis): A child frame may be a RenderFrameProxy.  We should still
      // process its children, but that will be possible when we move this code
      // to the browser process in https://crbug.com/236848.
      if (!child_render_frame)
        continue;
      HistoryNode* child_history_node =
          entry_->GetHistoryNodeForFrame(child_render_frame);
      if (!child_history_node)
        continue;
      HistoryNode* new_child_node =
          child_history_node->CloneAndReplace(new_entry,
                                              new_item,
                                              clone_children_of_target,
                                              target_frame,
                                              child_render_frame);
      new_history_node->children_->push_back(new_child_node);
    }
  }
  return new_history_node;
}

void HistoryEntry::HistoryNode::set_item(const WebHistoryItem& item) {
  DCHECK(!item.isNull());
  entry_->unique_names_to_items_[item.target().utf8()] = this;
  unique_names_.push_back(item.target().utf8());
  item_ = item;
}

HistoryEntry::HistoryNode::HistoryNode(const base::WeakPtr<HistoryEntry>& entry,
                                       const WebHistoryItem& item)
    : entry_(entry) {
  if (!item.isNull())
    set_item(item);
  children_.reset(new ScopedVector<HistoryNode>);
}

HistoryEntry::HistoryNode::~HistoryNode() {
  if (!entry_ || item_.isNull())
    return;

  for (const std::string& name : unique_names_) {
    if (entry_->unique_names_to_items_[name] == this)
      entry_->unique_names_to_items_.erase(name);
  }
}

void HistoryEntry::HistoryNode::RemoveChildren() {
  children_.reset(new ScopedVector<HistoryNode>);
}

HistoryEntry::HistoryEntry() : weak_ptr_factory_(this) {
  root_.reset(
      new HistoryNode(weak_ptr_factory_.GetWeakPtr(), WebHistoryItem()));
}

HistoryEntry::~HistoryEntry() {
}

HistoryEntry::HistoryEntry(const WebHistoryItem& root)
    : weak_ptr_factory_(this) {
  root_.reset(new HistoryNode(weak_ptr_factory_.GetWeakPtr(), root));
}

HistoryEntry* HistoryEntry::CloneAndReplace(const WebHistoryItem& new_item,
                                            bool clone_children_of_target,
                                            RenderFrameImpl* target_frame,
                                            RenderViewImpl* render_view) {
  HistoryEntry* new_entry = new HistoryEntry();
  new_entry->root_.reset(
      root_->CloneAndReplace(new_entry->weak_ptr_factory_.GetWeakPtr(),
                             new_item, clone_children_of_target, target_frame,
                             render_view->GetMainRenderFrame()));
  return new_entry;
}

HistoryEntry::HistoryNode* HistoryEntry::GetHistoryNodeForFrame(
    RenderFrameImpl* frame) {
  if (!frame->GetWebFrame()->parent())
    return root_history_node();
  return unique_names_to_items_[frame->GetWebFrame()->uniqueName().utf8()];
}

WebHistoryItem HistoryEntry::GetItemForFrame(RenderFrameImpl* frame) {
  if (HistoryNode* history_node = GetHistoryNodeForFrame(frame))
    return history_node->item();
  return WebHistoryItem();
}

}  // namespace content
