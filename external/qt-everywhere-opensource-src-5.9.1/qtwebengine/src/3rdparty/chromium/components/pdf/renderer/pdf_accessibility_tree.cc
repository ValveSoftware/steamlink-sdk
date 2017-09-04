// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "components/pdf/renderer/pdf_accessibility_tree.h"
#include "content/public/renderer/render_accessibility.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace pdf {

namespace {

// Don't try to apply font size thresholds to automatically identify headings
// if the median font size is not at least this many points.
const double kMinimumFontSize = 5;

// Don't try to apply line break thresholds to automatically identify
// line breaks if the median line break is not at least this many points.
const double kMinimumLineSpacing = 5;

// Ratio between the font size of one text run and the median on the page
// for that text run to be considered to be a heading instead of normal text.
const double kHeadingFontSizeRatio = 1.2;

// Ratio between the line spacing between two lines and the median on the
// page for that line spacing to be considered a paragraph break.
const double kParagraphLineSpacingRatio = 1.2;

gfx::RectF ToGfxRectF(const PP_FloatRect& r) {
  return gfx::RectF(r.point.x, r.point.y, r.size.width, r.size.height);
}

}

PdfAccessibilityTree::PdfAccessibilityTree(
    content::RendererPpapiHost* host,
    PP_Instance instance)
    : host_(host),
      instance_(instance),
      zoom_(1.0) {
}

PdfAccessibilityTree::~PdfAccessibilityTree() {
}

void PdfAccessibilityTree::SetAccessibilityViewportInfo(
    const PP_PrivateAccessibilityViewportInfo& viewport_info) {
  zoom_ = viewport_info.zoom;
  CHECK_GT(zoom_, 0);
  scroll_ = ToVector2dF(viewport_info.scroll);
  scroll_.Scale(1.0 / zoom_);
  offset_ = ToVector2dF(viewport_info.offset);
  offset_.Scale(1.0 / zoom_);

  content::RenderAccessibility* render_accessibility = GetRenderAccessibility();
  if (render_accessibility && tree_.size() > 1) {
    ui::AXNode* root = tree_.root();
    ui::AXNodeData root_data = root->data();
    root_data.transform = base::WrapUnique(MakeTransformFromViewInfo());
    root->SetData(root_data);
    render_accessibility->OnPluginRootNodeUpdated();
  }
}

void PdfAccessibilityTree::SetAccessibilityDocInfo(
    const PP_PrivateAccessibilityDocInfo& doc_info) {
  if (!GetRenderAccessibility())
    return;

  doc_info_ = doc_info;
  doc_node_ = CreateNode(ui::AX_ROLE_GROUP);

  // Because all of the coordinates are expressed relative to the
  // doc's coordinates, the origin of the doc must be (0, 0). Its
  // width and height will be updated as we add each page so that the
  // doc's bounding box surrounds all pages.
  doc_node_->location = gfx::RectF(0, 0, 1, 1);
}

void PdfAccessibilityTree::SetAccessibilityPageInfo(
    const PP_PrivateAccessibilityPageInfo& page_info,
    const std::vector<PP_PrivateAccessibilityTextRunInfo>& text_runs,
    const std::vector<PP_PrivateAccessibilityCharInfo>& chars) {
  content::RenderAccessibility* render_accessibility = GetRenderAccessibility();
  if (!render_accessibility)
    return;

  uint32_t page_index = page_info.page_index;
  CHECK_GE(page_index, 0U);
  CHECK_LT(page_index, doc_info_.page_count);

  ui::AXNodeData* page_node = CreateNode(ui::AX_ROLE_REGION);
  page_node->AddStringAttribute(
      ui::AX_ATTR_NAME,
      l10n_util::GetPluralStringFUTF8(
          IDS_PDF_PAGE_INDEX, page_index + 1));

  gfx::RectF page_bounds = ToRectF(page_info.bounds);
  page_node->location = page_bounds;
  doc_node_->location.Union(page_node->location);
  doc_node_->child_ids.push_back(page_node->id);

  double heading_font_size_threshold = 0;
  double line_spacing_threshold = 0;
  ComputeParagraphAndHeadingThresholds(text_runs,
                                       &heading_font_size_threshold,
                                       &line_spacing_threshold);

  ui::AXNodeData* para_node = nullptr;
  ui::AXNodeData* static_text_node = nullptr;
  std::string static_text;
  uint32_t char_index = 0;
  for (size_t i = 0; i < text_runs.size(); ++i) {
    // Get the text of the next text run
    const auto& text_run = text_runs[i];
    std::string chars_utf8 = GetTextRunCharsAsUTF8(text_run, chars, char_index);
    std::vector<int32_t> char_offsets = GetTextRunCharOffsets(
        text_run, chars, char_index);
    static_text += chars_utf8;
    char_index += text_run.len;

    // If we don't have a paragraph, create one.
    if (!para_node) {
      para_node = CreateNode(ui::AX_ROLE_PARAGRAPH);
      page_node->child_ids.push_back(para_node->id);

      if (heading_font_size_threshold > 0 &&
          text_run.font_size > heading_font_size_threshold) {
        para_node->role = ui::AX_ROLE_HEADING;
        para_node->AddIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL, 2);
        para_node->AddStringAttribute(ui::AX_ATTR_HTML_TAG, "h2");
      }

      // This node is for the text inside the paragraph, it includes
      // the text of all of the text runs.
      static_text_node = CreateNode(ui::AX_ROLE_STATIC_TEXT);
      para_node->child_ids.push_back(static_text_node->id);
    }

    // Add this text run to the current static text node.
    ui::AXNodeData* inline_text_box_node = CreateNode(
        ui::AX_ROLE_INLINE_TEXT_BOX);
    static_text_node->child_ids.push_back(inline_text_box_node->id);

    inline_text_box_node->AddStringAttribute(ui::AX_ATTR_NAME, chars_utf8);
    gfx::RectF text_run_bounds = ToGfxRectF(text_run.bounds);
    text_run_bounds += page_bounds.OffsetFromOrigin();
    inline_text_box_node->location = text_run_bounds;
    inline_text_box_node->AddIntListAttribute(ui::AX_ATTR_CHARACTER_OFFSETS,
                                              char_offsets);

    para_node->location.Union(inline_text_box_node->location);
    static_text_node->location.Union(inline_text_box_node->location);

    if (i == text_runs.size() - 1) {
      static_text_node->AddStringAttribute(ui::AX_ATTR_NAME, static_text);
      break;
    }

    double line_spacing =
        text_runs[i + 1].bounds.point.y - text_run.bounds.point.y;
    if (text_run.font_size != text_runs[i + 1].font_size ||
        (line_spacing_threshold > 0 &&
         line_spacing > line_spacing_threshold)) {
      static_text_node->AddStringAttribute(ui::AX_ATTR_NAME, static_text);
      para_node = nullptr;
      static_text_node = nullptr;
      static_text.clear();
    }
  }

  if (page_index == doc_info_.page_count - 1)
    Finish();
}

void PdfAccessibilityTree::Finish() {
  doc_node_->transform = base::WrapUnique(MakeTransformFromViewInfo());

  ui::AXTreeUpdate update;
  update.root_id = doc_node_->id;
  for (const auto& node : nodes_)
    update.nodes.push_back(*node);

  CHECK(tree_.Unserialize(update)) << update.ToString() << tree_.error();
  content::RenderAccessibility* render_accessibility = GetRenderAccessibility();
  if (render_accessibility)
    render_accessibility->SetPluginTreeSource(this);
}

void PdfAccessibilityTree::ComputeParagraphAndHeadingThresholds(
    const std::vector<PP_PrivateAccessibilityTextRunInfo>& text_runs,
    double* out_heading_font_size_threshold,
    double* out_line_spacing_threshold) {
  // Scan over the font sizes and line spacing within this page and
  // set heuristic thresholds so that text larger than the median font
  // size can be marked as a heading, and spacing larger than the median
  // line spacing can be a paragraph break.
  std::vector<double> font_sizes;
  std::vector<double> line_spacings;
  for (size_t i = 0; i < text_runs.size(); ++i) {
    font_sizes.push_back(text_runs[i].font_size);
    if (i > 0) {
      const auto& cur = text_runs[i].bounds;
      const auto& prev = text_runs[i - 1].bounds;
      if (cur.point.y > prev.point.y + prev.size.height / 2)
        line_spacings.push_back(cur.point.y - prev.point.y);
    }
  }
  if (font_sizes.size() > 2) {
    std::sort(font_sizes.begin(), font_sizes.end());
    double median_font_size = font_sizes[font_sizes.size() / 2];
    if (median_font_size > kMinimumFontSize) {
      *out_heading_font_size_threshold =
          median_font_size * kHeadingFontSizeRatio;
    }
  }
  if (line_spacings.size() > 4) {
    std::sort(line_spacings.begin(), line_spacings.end());
    double median_line_spacing = line_spacings[line_spacings.size() / 2];
    if (median_line_spacing > kMinimumLineSpacing) {
      *out_line_spacing_threshold =
          median_line_spacing * kParagraphLineSpacingRatio;
    }
  }
}

std::string PdfAccessibilityTree::GetTextRunCharsAsUTF8(
    const PP_PrivateAccessibilityTextRunInfo& text_run,
    const std::vector<PP_PrivateAccessibilityCharInfo>& chars,
    int char_index) {
  std::string chars_utf8;
  for (uint32_t i = 0; i < text_run.len; ++i) {
    base::WriteUnicodeCharacter(chars[char_index + i].unicode_character,
                                &chars_utf8);
  }
  return chars_utf8;
}

std::vector<int32_t> PdfAccessibilityTree::GetTextRunCharOffsets(
    const PP_PrivateAccessibilityTextRunInfo& text_run,
    const std::vector<PP_PrivateAccessibilityCharInfo>& chars,
    int char_index) {
  std::vector<int32_t> char_offsets(text_run.len);
  double offset = 0.0;
  for (uint32_t j = 0; j < text_run.len; ++j) {
    offset += chars[char_index + j].char_width;
    char_offsets[j] = floor(offset);
  }
  return char_offsets;
}

gfx::Vector2dF PdfAccessibilityTree::ToVector2dF(const PP_Point& p) {
  return gfx::Vector2dF(p.x, p.y);
}

gfx::RectF PdfAccessibilityTree::ToRectF(const PP_Rect& r) {
  return gfx::RectF(r.point.x, r.point.y, r.size.width, r.size.height);
}

ui::AXNodeData* PdfAccessibilityTree::CreateNode(ui::AXRole role) {
  content::RenderAccessibility* render_accessibility = GetRenderAccessibility();
  DCHECK(render_accessibility);

  ui::AXNodeData* node = new ui::AXNodeData();
  node->id = render_accessibility->GenerateAXID();
  node->role = role;
  node->state = 1 << ui::AX_STATE_READ_ONLY;

  // All nodes other than the first one have coordinates relative to
  // the first node.
  if (nodes_.size() > 0)
    node->offset_container_id = nodes_[0]->id;

  nodes_.push_back(base::WrapUnique(node));

  return node;
}

float PdfAccessibilityTree::GetDeviceScaleFactor() const {
  content::RenderFrame* render_frame =
      host_->GetRenderFrameForInstance(instance_);
  DCHECK(render_frame);
  return render_frame->GetRenderView()->GetDeviceScaleFactor();
}

content::RenderAccessibility* PdfAccessibilityTree::GetRenderAccessibility() {
  content::RenderFrame* render_frame =
      host_->GetRenderFrameForInstance(instance_);
  return render_frame ? render_frame->GetRenderAccessibility() : nullptr;
}

gfx::Transform* PdfAccessibilityTree::MakeTransformFromViewInfo() {
  gfx::Transform* transform = new gfx::Transform();
  float scale_factor = zoom_ / GetDeviceScaleFactor();
  transform->Scale(scale_factor, scale_factor);
  transform->Translate(offset_);
  transform->Translate(-scroll_);
  return transform;
}

//
// AXTreeSource implementation.
//

bool PdfAccessibilityTree::GetTreeData(ui::AXTreeData* tree_data) const {
  return false;
}

ui::AXNode* PdfAccessibilityTree::GetRoot() const {
  return tree_.root();
}

ui::AXNode* PdfAccessibilityTree::GetFromId(int32_t id) const {
  return tree_.GetFromId(id);
}

int32_t PdfAccessibilityTree::GetId(const ui::AXNode* node) const {
  return node->id();
}

void PdfAccessibilityTree::GetChildren(
    const ui::AXNode* node,
    std::vector<const ui::AXNode*>* out_children) const {
  for (int i = 0; i < node->child_count(); ++i)
    out_children->push_back(node->ChildAtIndex(i));
}

ui::AXNode* PdfAccessibilityTree::GetParent(const ui::AXNode* node) const {
  return node->parent();
}

bool PdfAccessibilityTree::IsValid(const ui::AXNode* node) const {
  return node != nullptr;
}

bool PdfAccessibilityTree::IsEqual(const ui::AXNode* node1,
                                   const ui::AXNode* node2) const {
  return node1 == node2;
}

const ui::AXNode* PdfAccessibilityTree::GetNull() const {
  return nullptr;
}

void PdfAccessibilityTree::SerializeNode(
    const ui::AXNode* node, ui::AXNodeData* out_data) const {
  *out_data = node->data();
}

}  // namespace pdf
