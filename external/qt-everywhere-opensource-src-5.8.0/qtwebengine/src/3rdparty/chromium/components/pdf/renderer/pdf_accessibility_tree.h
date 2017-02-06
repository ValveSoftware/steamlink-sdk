// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_H_
#define COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_H_

#include <memory>
#include <vector>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_source.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace content {
class RenderAccessibility;
class RenderView;
}

namespace pdf {

class PdfAccessibilityTree
    : public ui::AXTreeSource<const ui::AXNode*,
                              ui::AXNodeData,
                              ui::AXTreeData> {
 public:
  explicit PdfAccessibilityTree(content::RenderView* render_view);
  ~PdfAccessibilityTree() override;

  void SetAccessibilityViewportInfo(
      const PP_PrivateAccessibilityViewportInfo& viewport_info);
  void SetAccessibilityDocInfo(
      const PP_PrivateAccessibilityDocInfo& doc_info);
  void SetAccessibilityPageInfo(
      const PP_PrivateAccessibilityPageInfo& page_info,
      const std::vector<PP_PrivateAccessibilityTextRunInfo>& text_runs,
      const std::vector<PP_PrivateAccessibilityCharInfo>& chars);

  // AXTreeSource implementation.
  bool GetTreeData(ui::AXTreeData* tree_data) const override;
  ui::AXNode* GetRoot() const override;
  ui::AXNode* GetFromId(int32_t id) const override;
  int32_t GetId(const ui::AXNode* node) const override;
  void GetChildren(const ui::AXNode* node,
                   std::vector<const ui::AXNode*>* out_children) const override;
  ui::AXNode* GetParent(const ui::AXNode* node) const override;
  bool IsValid(const ui::AXNode* node) const override;
  bool IsEqual(const ui::AXNode* node1, const ui::AXNode* node2) const override;
  const ui::AXNode* GetNull() const override;
  void SerializeNode(const ui::AXNode* node, ui::AXNodeData* out_data)
      const override;

 private:
  void Finish();

  void ComputeParagraphAndHeadingThresholds(
      const std::vector<PP_PrivateAccessibilityTextRunInfo>& text_runs,
      double* out_heading_font_size_threshold,
      double* out_line_spacing_threshold);
  std::string GetTextRunCharsAsUTF8(
      const PP_PrivateAccessibilityTextRunInfo& text_run,
      const std::vector<PP_PrivateAccessibilityCharInfo>& chars,
      int char_index);
  std::vector<int32_t> GetTextRunCharOffsets(
      const PP_PrivateAccessibilityTextRunInfo& text_run,
      const std::vector<PP_PrivateAccessibilityCharInfo>& chars,
      int char_index);
  gfx::Vector2dF ToVector2dF(const PP_Point& p);
  gfx::RectF ToRectF(const PP_Rect& r);
  ui::AXNodeData* CreateNode(ui::AXRole role);
  float GetDeviceScaleFactor() const;

  ui::AXTree tree_;
  content::RenderView* render_view_;
  content::RenderAccessibility* render_accessibility_;
  double zoom_;
  gfx::Vector2dF scroll_;
  gfx::Vector2dF offset_;
  PP_PrivateAccessibilityDocInfo doc_info_;
  ui::AXNodeData* doc_node_;
  std::vector<std::unique_ptr<ui::AXNodeData>> nodes_;
};

}  // namespace pdf;

#endif  // COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_H_
