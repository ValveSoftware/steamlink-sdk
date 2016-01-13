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
#include "third_party/WebKit/public/web/WebFrame.h"

using blink::WebFrame;
using blink::WebHistoryItem;

namespace content {

HistoryEntry::HistoryNode* HistoryEntry::HistoryNode::AddChild(
    const WebHistoryItem& item,
    int64_t frame_id) {
  children_->push_back(new HistoryNode(entry_, item, frame_id));
  return children_->back();
}

HistoryEntry::HistoryNode* HistoryEntry::HistoryNode::AddChild() {
  return AddChild(WebHistoryItem(), kInvalidFrameRoutingID);
}

HistoryEntry::HistoryNode* HistoryEntry::HistoryNode::CloneAndReplace(
    HistoryEntry* new_entry,
    const WebHistoryItem& new_item,
    bool clone_children_of_target,
    RenderFrameImpl* target_frame,
    RenderFrameImpl* current_frame) {
  bool is_target_frame = target_frame == current_frame;
  const WebHistoryItem& item_for_create = is_target_frame ? new_item : item_;
  HistoryNode* new_history_node = new HistoryNode(
      new_entry, item_for_create, current_frame->GetRoutingID());

  if (is_target_frame && clone_children_of_target && !item_.isNull()) {
    new_history_node->item().setDocumentSequenceNumber(
        item_.documentSequenceNumber());
  }

  if (clone_children_of_target || !is_target_frame) {
    for (WebFrame* child = current_frame->GetWebFrame()->firstChild(); child;
         child = child->nextSibling()) {
      RenderFrameImpl* child_render_frame =
          RenderFrameImpl::FromWebFrame(child);
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
  // The previous HistoryItem might not have had a target set, or it might be
  // different than the current one.
  entry_->unique_names_to_items_[item.target().utf8()] = this;
  item_ = item;
}

HistoryEntry::HistoryNode::HistoryNode(HistoryEntry* entry,
                                       const WebHistoryItem& item,
                                       int64_t frame_id)
    : entry_(entry), item_(item) {
  if (frame_id != kInvalidFrameRoutingID)
    entry_->frames_to_items_[frame_id] = this;
  if (!item.isNull())
    entry_->unique_names_to_items_[item.target().utf8()] = this;
  children_.reset(new ScopedVector<HistoryNode>);
}

HistoryEntry::HistoryNode::~HistoryNode() {
}

void HistoryEntry::HistoryNode::RemoveChildren() {
  // TODO(japhet): This is inefficient. Figure out a cleaner way to ensure
  // this HistoryNode isn't cached anywhere.
  std::vector<uint64_t> frames_to_remove;
  std::vector<std::string> unique_names_to_remove;
  for (size_t i = 0; i < children().size(); i++) {
    children().at(i)->RemoveChildren();

    HistoryEntry::FramesToItems::iterator frames_end =
        entry_->frames_to_items_.end();
    HistoryEntry::UniqueNamesToItems::iterator unique_names_end =
        entry_->unique_names_to_items_.end();
    for (HistoryEntry::FramesToItems::iterator it =
             entry_->frames_to_items_.begin();
         it != frames_end;
         ++it) {
      if (it->second == children().at(i))
        frames_to_remove.push_back(it->first);
    }
    for (HistoryEntry::UniqueNamesToItems::iterator it =
             entry_->unique_names_to_items_.begin();
         it != unique_names_end;
         ++it) {
      if (it->second == children().at(i))
        unique_names_to_remove.push_back(it->first);
    }
  }
  for (unsigned i = 0; i < frames_to_remove.size(); i++)
    entry_->frames_to_items_.erase(frames_to_remove[i]);
  for (unsigned i = 0; i < unique_names_to_remove.size(); i++)
    entry_->unique_names_to_items_.erase(unique_names_to_remove[i]);
  children_.reset(new ScopedVector<HistoryNode>);
}

HistoryEntry::HistoryEntry() {
  root_.reset(new HistoryNode(this, WebHistoryItem(), kInvalidFrameRoutingID));
}

HistoryEntry::~HistoryEntry() {
}

HistoryEntry::HistoryEntry(const WebHistoryItem& root, int64_t frame_id) {
  root_.reset(new HistoryNode(this, root, frame_id));
}

HistoryEntry* HistoryEntry::CloneAndReplace(const WebHistoryItem& new_item,
                                            bool clone_children_of_target,
                                            RenderFrameImpl* target_frame,
                                            RenderViewImpl* render_view) {
  HistoryEntry* new_entry = new HistoryEntry();
  new_entry->root_.reset(
      root_->CloneAndReplace(new_entry,
                             new_item,
                             clone_children_of_target,
                             target_frame,
                             render_view->main_render_frame()));
  return new_entry;
}

HistoryEntry::HistoryNode* HistoryEntry::GetHistoryNodeForFrame(
    RenderFrameImpl* frame) {
  if (HistoryNode* history_node = frames_to_items_[frame->GetRoutingID()])
    return history_node;
  return unique_names_to_items_[frame->GetWebFrame()->uniqueName().utf8()];
}

WebHistoryItem HistoryEntry::GetItemForFrame(RenderFrameImpl* frame) {
  if (HistoryNode* history_node = GetHistoryNodeForFrame(frame))
    return history_node->item();
  return WebHistoryItem();
}

}  // namespace content
