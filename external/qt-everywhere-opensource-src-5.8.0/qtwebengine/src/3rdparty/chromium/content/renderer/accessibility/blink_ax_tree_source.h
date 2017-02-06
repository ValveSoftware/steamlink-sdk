// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_BLINK_AX_TREE_SOURCE_H_
#define CONTENT_RENDERER_ACCESSIBILITY_BLINK_AX_TREE_SOURCE_H_

#include <stdint.h>

#include "content/common/ax_content_node_data.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_source.h"

namespace content {

class RenderFrameImpl;

class BlinkAXTreeSource
    : public ui::AXTreeSource<blink::WebAXObject,
                              AXContentNodeData,
                              AXContentTreeData> {
 public:
  BlinkAXTreeSource(RenderFrameImpl* render_frame);
  ~BlinkAXTreeSource() override;

  // It may be necessary to call SetRoot if you're using a WebScopedAXContext,
  // because BlinkAXTreeSource can't get the root of the tree from the
  // WebDocument if accessibility isn't enabled globally.
  void SetRoot(blink::WebAXObject root);

  // Walks up the ancestor chain to see if this is a descendant of the root.
  bool IsInTree(blink::WebAXObject node) const;

  // Set the id of the node with accessibility focus. The node with
  // accessibility focus will force loading inline text box children,
  // which aren't always loaded by default on all platforms.
  int accessibility_focus_id() { return accessibility_focus_id_; }
  void set_accessibility_focus_id(int id) { accessibility_focus_id_ = id; }

  // AXTreeSource implementation.
  bool GetTreeData(AXContentTreeData* tree_data) const override;
  blink::WebAXObject GetRoot() const override;
  blink::WebAXObject GetFromId(int32_t id) const override;
  int32_t GetId(blink::WebAXObject node) const override;
  void GetChildren(
      blink::WebAXObject node,
      std::vector<blink::WebAXObject>* out_children) const override;
  blink::WebAXObject GetParent(blink::WebAXObject node) const override;
  void SerializeNode(blink::WebAXObject node,
                     AXContentNodeData* out_data) const override;
  bool IsValid(blink::WebAXObject node) const override;
  bool IsEqual(blink::WebAXObject node1,
               blink::WebAXObject node2) const override;
  blink::WebAXObject GetNull() const override;

  blink::WebDocument GetMainDocument() const;

 private:
  RenderFrameImpl* render_frame_;
  blink::WebAXObject root_;

  int accessibility_focus_id_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_BLINK_AX_TREE_SOURCE_H_
