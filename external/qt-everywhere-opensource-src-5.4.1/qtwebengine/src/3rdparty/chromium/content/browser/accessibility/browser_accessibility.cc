// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/common/accessibility_messages.h"

namespace content {

#if !defined(OS_MACOSX) && \
    !defined(OS_WIN) && \
    !defined(OS_ANDROID)
// We have subclassess of BrowserAccessibility on Mac and Win. For any other
// platform, instantiate the base class.
// static
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibility();
}
#endif

BrowserAccessibility::BrowserAccessibility()
    : manager_(NULL),
      node_(NULL) {
}

BrowserAccessibility::~BrowserAccessibility() {
}

void BrowserAccessibility::Init(BrowserAccessibilityManager* manager,
    ui::AXNode* node) {
  manager_ = manager;
  node_ = node;
}

void BrowserAccessibility::OnDataChanged() {
  GetStringAttribute(ui::AX_ATTR_NAME, &name_);
  GetStringAttribute(ui::AX_ATTR_VALUE, &value_);
}

bool BrowserAccessibility::PlatformIsLeaf() const {
  if (InternalChildCount() == 0)
    return true;

  // All of these roles may have children that we use as internal
  // implementation details, but we want to expose them as leaves
  // to platform accessibility APIs.
  switch (GetRole()) {
    case ui::AX_ROLE_EDITABLE_TEXT:
    case ui::AX_ROLE_SLIDER:
    case ui::AX_ROLE_STATIC_TEXT:
    case ui::AX_ROLE_TEXT_AREA:
    case ui::AX_ROLE_TEXT_FIELD:
      return true;
    default:
      return false;
  }
}

uint32 BrowserAccessibility::PlatformChildCount() const {
  return PlatformIsLeaf() ? 0 : InternalChildCount();
}

bool BrowserAccessibility::IsNative() const {
  return false;
}

bool BrowserAccessibility::IsDescendantOf(
    BrowserAccessibility* ancestor) {
  if (this == ancestor) {
    return true;
  } else if (GetParent()) {
    return GetParent()->IsDescendantOf(ancestor);
  }

  return false;
}

BrowserAccessibility* BrowserAccessibility::PlatformGetChild(
    uint32 child_index) const {
  DCHECK(child_index < InternalChildCount());
  return InternalGetChild(child_index);
}

BrowserAccessibility* BrowserAccessibility::GetPreviousSibling() {
  if (GetParent() && GetIndexInParent() > 0)
    return GetParent()->InternalGetChild(GetIndexInParent() - 1);

  return NULL;
}

BrowserAccessibility* BrowserAccessibility::GetNextSibling() {
  if (GetParent() &&
      GetIndexInParent() >= 0 &&
      GetIndexInParent() < static_cast<int>(
          GetParent()->InternalChildCount() - 1)) {
    return GetParent()->InternalGetChild(GetIndexInParent() + 1);
  }

  return NULL;
}

uint32 BrowserAccessibility::InternalChildCount() const {
  if (!node_ || !manager_)
    return 0;
  return static_cast<uint32>(node_->child_count());
}

BrowserAccessibility* BrowserAccessibility::InternalGetChild(
    uint32 child_index) const {
  if (!node_ || !manager_)
    return NULL;
  return manager_->GetFromAXNode(node_->children()[child_index]);
}

BrowserAccessibility* BrowserAccessibility::GetParent() const {
  if (!node_ || !manager_)
    return NULL;
  ui::AXNode* parent = node_->parent();
  return parent ? manager_->GetFromAXNode(parent) : NULL;
}

int32 BrowserAccessibility::GetIndexInParent() const {
  return node_ ? node_->index_in_parent() : -1;
}

int32 BrowserAccessibility::GetId() const {
  return node_ ? node_->id() : -1;
}

const ui::AXNodeData& BrowserAccessibility::GetData() const {
  CR_DEFINE_STATIC_LOCAL(ui::AXNodeData, empty_data, ());
  if (node_)
    return node_->data();
  else
    return empty_data;
}

gfx::Rect BrowserAccessibility::GetLocation() const {
  return GetData().location;
}

int32 BrowserAccessibility::GetRole() const {
  return GetData().role;
}

int32 BrowserAccessibility::GetState() const {
  return GetData().state;
}

const std::vector<std::pair<std::string, std::string> >&
BrowserAccessibility::GetHtmlAttributes() const {
  return GetData().html_attributes;
}

gfx::Rect BrowserAccessibility::GetLocalBoundsRect() const {
  gfx::Rect bounds = GetLocation();

  // Walk up the parent chain. Every time we encounter a Web Area, offset
  // based on the scroll bars and then offset based on the origin of that
  // nested web area.
  BrowserAccessibility* parent = GetParent();
  bool need_to_offset_web_area =
      (GetRole() == ui::AX_ROLE_WEB_AREA ||
       GetRole() == ui::AX_ROLE_ROOT_WEB_AREA);
  while (parent) {
    if (need_to_offset_web_area &&
        parent->GetLocation().width() > 0 &&
        parent->GetLocation().height() > 0) {
      bounds.Offset(parent->GetLocation().x(), parent->GetLocation().y());
      need_to_offset_web_area = false;
    }

    // On some platforms, we don't want to take the root scroll offsets
    // into account.
    if (parent->GetRole() == ui::AX_ROLE_ROOT_WEB_AREA &&
        !manager()->UseRootScrollOffsetsWhenComputingBounds()) {
      break;
    }

    if (parent->GetRole() == ui::AX_ROLE_WEB_AREA ||
        parent->GetRole() == ui::AX_ROLE_ROOT_WEB_AREA) {
      int sx = 0;
      int sy = 0;
      if (parent->GetIntAttribute(ui::AX_ATTR_SCROLL_X, &sx) &&
          parent->GetIntAttribute(ui::AX_ATTR_SCROLL_Y, &sy)) {
        bounds.Offset(-sx, -sy);
      }
      need_to_offset_web_area = true;
    }
    parent = parent->GetParent();
  }

  return bounds;
}

gfx::Rect BrowserAccessibility::GetGlobalBoundsRect() const {
  gfx::Rect bounds = GetLocalBoundsRect();

  // Adjust the bounds by the top left corner of the containing view's bounds
  // in screen coordinates.
  bounds.Offset(manager_->GetViewBounds().OffsetFromOrigin());

  return bounds;
}

gfx::Rect BrowserAccessibility::GetLocalBoundsForRange(int start, int len)
    const {
  if (GetRole() != ui::AX_ROLE_STATIC_TEXT) {
    // Apply recursively to all static text descendants. For example, if
    // you call it on a div with two text node children, it just calls
    // GetLocalBoundsForRange on each of the two children (adjusting
    // |start| for each one) and unions the resulting rects.
    gfx::Rect bounds;
    for (size_t i = 0; i < InternalChildCount(); ++i) {
      BrowserAccessibility* child = InternalGetChild(i);
      int child_len = child->GetStaticTextLenRecursive();
      if (start < child_len && start + len > 0) {
        gfx::Rect child_rect = child->GetLocalBoundsForRange(start, len);
        bounds.Union(child_rect);
      }
      start -= child_len;
    }
    return bounds;
  }

  int end = start + len;
  int child_start = 0;
  int child_end = 0;

  gfx::Rect bounds;
  for (size_t i = 0; i < InternalChildCount() && child_end < start + len; ++i) {
    BrowserAccessibility* child = InternalGetChild(i);
    DCHECK_EQ(child->GetRole(), ui::AX_ROLE_INLINE_TEXT_BOX);
    std::string child_text;
    child->GetStringAttribute(ui::AX_ATTR_VALUE, &child_text);
    int child_len = static_cast<int>(child_text.size());
    child_start = child_end;
    child_end += child_len;

    if (child_end < start)
      continue;

    int overlap_start = std::max(start, child_start);
    int overlap_end = std::min(end, child_end);

    int local_start = overlap_start - child_start;
    int local_end = overlap_end - child_start;

    gfx::Rect child_rect = child->GetLocation();
    int text_direction = child->GetIntAttribute(
        ui::AX_ATTR_TEXT_DIRECTION);
    const std::vector<int32>& character_offsets = child->GetIntListAttribute(
        ui::AX_ATTR_CHARACTER_OFFSETS);
    int start_pixel_offset =
        local_start > 0 ? character_offsets[local_start - 1] : 0;
    int end_pixel_offset =
        local_end > 0 ? character_offsets[local_end - 1] : 0;

    gfx::Rect child_overlap_rect;
    switch (text_direction) {
      case ui::AX_TEXT_DIRECTION_NONE:
      case ui::AX_TEXT_DIRECTION_LR: {
        int left = child_rect.x() + start_pixel_offset;
        int right = child_rect.x() + end_pixel_offset;
        child_overlap_rect = gfx::Rect(left, child_rect.y(),
                                       right - left, child_rect.height());
        break;
      }
      case ui::AX_TEXT_DIRECTION_RL: {
        int right = child_rect.right() - start_pixel_offset;
        int left = child_rect.right() - end_pixel_offset;
        child_overlap_rect = gfx::Rect(left, child_rect.y(),
                                       right - left, child_rect.height());
        break;
      }
      case ui::AX_TEXT_DIRECTION_TB: {
        int top = child_rect.y() + start_pixel_offset;
        int bottom = child_rect.y() + end_pixel_offset;
        child_overlap_rect = gfx::Rect(child_rect.x(), top,
                                       child_rect.width(), bottom - top);
        break;
      }
      case ui::AX_TEXT_DIRECTION_BT: {
        int bottom = child_rect.bottom() - start_pixel_offset;
        int top = child_rect.bottom() - end_pixel_offset;
        child_overlap_rect = gfx::Rect(child_rect.x(), top,
                                       child_rect.width(), bottom - top);
        break;
      }
      default:
        NOTREACHED();
    }

    if (bounds.width() == 0 && bounds.height() == 0)
      bounds = child_overlap_rect;
    else
      bounds.Union(child_overlap_rect);
  }

  return bounds;
}

gfx::Rect BrowserAccessibility::GetGlobalBoundsForRange(int start, int len)
    const {
  gfx::Rect bounds = GetLocalBoundsForRange(start, len);

  // Adjust the bounds by the top left corner of the containing view's bounds
  // in screen coordinates.
  bounds.Offset(manager_->GetViewBounds().OffsetFromOrigin());

  return bounds;
}

BrowserAccessibility* BrowserAccessibility::BrowserAccessibilityForPoint(
    const gfx::Point& point) {
  // The best result found that's a child of this object.
  BrowserAccessibility* child_result = NULL;
  // The best result that's an indirect descendant like grandchild, etc.
  BrowserAccessibility* descendant_result = NULL;

  // Walk the children recursively looking for the BrowserAccessibility that
  // most tightly encloses the specified point. Walk backwards so that in
  // the absence of any other information, we assume the object that occurs
  // later in the tree is on top of one that comes before it.
  for (int i = static_cast<int>(PlatformChildCount()) - 1; i >= 0; --i) {
    BrowserAccessibility* child = PlatformGetChild(i);

    // Skip table columns because cells are only contained in rows,
    // not columns.
    if (child->GetRole() == ui::AX_ROLE_COLUMN)
      continue;

    if (child->GetGlobalBoundsRect().Contains(point)) {
      BrowserAccessibility* result = child->BrowserAccessibilityForPoint(point);
      if (result == child && !child_result)
        child_result = result;
      if (result != child && !descendant_result)
        descendant_result = result;
    }

    if (child_result && descendant_result)
      break;
  }

  // Explanation of logic: it's possible that this point overlaps more than
  // one child of this object. If so, as a heuristic we prefer if the point
  // overlaps a descendant of one of the two children and not the other.
  // As an example, suppose you have two rows of buttons - the buttons don't
  // overlap, but the rows do. Without this heuristic, we'd greedily only
  // consider one of the containers.
  if (descendant_result)
    return descendant_result;
  if (child_result)
    return child_result;

  return this;
}

void BrowserAccessibility::Destroy() {
  // Allow the object to fire a TextRemoved notification.
  name_.clear();
  value_.clear();

  manager_->NotifyAccessibilityEvent(ui::AX_EVENT_HIDE, this);
  node_ = NULL;
  manager_ = NULL;

  NativeReleaseReference();
}

void BrowserAccessibility::NativeReleaseReference() {
  delete this;
}

bool BrowserAccessibility::HasBoolAttribute(
    ui::AXBoolAttribute attribute) const {
  const ui::AXNodeData& data = GetData();
  for (size_t i = 0; i < data.bool_attributes.size(); ++i) {
    if (data.bool_attributes[i].first == attribute)
      return true;
  }

  return false;
}


bool BrowserAccessibility::GetBoolAttribute(
    ui::AXBoolAttribute attribute) const {
  const ui::AXNodeData& data = GetData();
  for (size_t i = 0; i < data.bool_attributes.size(); ++i) {
    if (data.bool_attributes[i].first == attribute)
      return data.bool_attributes[i].second;
  }

  return false;
}

bool BrowserAccessibility::GetBoolAttribute(
    ui::AXBoolAttribute attribute, bool* value) const {
  const ui::AXNodeData& data = GetData();
  for (size_t i = 0; i < data.bool_attributes.size(); ++i) {
    if (data.bool_attributes[i].first == attribute) {
      *value = data.bool_attributes[i].second;
      return true;
    }
  }

  return false;
}

bool BrowserAccessibility::HasFloatAttribute(
    ui::AXFloatAttribute attribute) const {
  const ui::AXNodeData& data = GetData();
  for (size_t i = 0; i < data.float_attributes.size(); ++i) {
    if (data.float_attributes[i].first == attribute)
      return true;
  }

  return false;
}

float BrowserAccessibility::GetFloatAttribute(
    ui::AXFloatAttribute attribute) const {
  const ui::AXNodeData& data = GetData();
  for (size_t i = 0; i < data.float_attributes.size(); ++i) {
    if (data.float_attributes[i].first == attribute)
      return data.float_attributes[i].second;
  }

  return 0.0;
}

bool BrowserAccessibility::GetFloatAttribute(
    ui::AXFloatAttribute attribute, float* value) const {
  const ui::AXNodeData& data = GetData();
  for (size_t i = 0; i < data.float_attributes.size(); ++i) {
    if (data.float_attributes[i].first == attribute) {
      *value = data.float_attributes[i].second;
      return true;
    }
  }

  return false;
}

bool BrowserAccessibility::HasIntAttribute(
    ui::AXIntAttribute attribute) const {
  const ui::AXNodeData& data = GetData();
  for (size_t i = 0; i < data.int_attributes.size(); ++i) {
    if (data.int_attributes[i].first == attribute)
      return true;
  }

  return false;
}

int BrowserAccessibility::GetIntAttribute(ui::AXIntAttribute attribute) const {
  const ui::AXNodeData& data = GetData();
  for (size_t i = 0; i < data.int_attributes.size(); ++i) {
    if (data.int_attributes[i].first == attribute)
      return data.int_attributes[i].second;
  }

  return 0;
}

bool BrowserAccessibility::GetIntAttribute(
    ui::AXIntAttribute attribute, int* value) const {
  const ui::AXNodeData& data = GetData();
  for (size_t i = 0; i < data.int_attributes.size(); ++i) {
    if (data.int_attributes[i].first == attribute) {
      *value = data.int_attributes[i].second;
      return true;
    }
  }

  return false;
}

bool BrowserAccessibility::HasStringAttribute(
    ui::AXStringAttribute attribute) const {
  const ui::AXNodeData& data = GetData();
  for (size_t i = 0; i < data.string_attributes.size(); ++i) {
    if (data.string_attributes[i].first == attribute)
      return true;
  }

  return false;
}

const std::string& BrowserAccessibility::GetStringAttribute(
    ui::AXStringAttribute attribute) const {
  const ui::AXNodeData& data = GetData();
  CR_DEFINE_STATIC_LOCAL(std::string, empty_string, ());
  for (size_t i = 0; i < data.string_attributes.size(); ++i) {
    if (data.string_attributes[i].first == attribute)
      return data.string_attributes[i].second;
  }

  return empty_string;
}

bool BrowserAccessibility::GetStringAttribute(
    ui::AXStringAttribute attribute, std::string* value) const {
  const ui::AXNodeData& data = GetData();
  for (size_t i = 0; i < data.string_attributes.size(); ++i) {
    if (data.string_attributes[i].first == attribute) {
      *value = data.string_attributes[i].second;
      return true;
    }
  }

  return false;
}

base::string16 BrowserAccessibility::GetString16Attribute(
    ui::AXStringAttribute attribute) const {
  std::string value_utf8;
  if (!GetStringAttribute(attribute, &value_utf8))
    return base::string16();
  return base::UTF8ToUTF16(value_utf8);
}

bool BrowserAccessibility::GetString16Attribute(
    ui::AXStringAttribute attribute,
    base::string16* value) const {
  std::string value_utf8;
  if (!GetStringAttribute(attribute, &value_utf8))
    return false;
  *value = base::UTF8ToUTF16(value_utf8);
  return true;
}

void BrowserAccessibility::SetStringAttribute(
    ui::AXStringAttribute attribute, const std::string& value) {
  if (!node_)
    return;
  ui::AXNodeData data = GetData();
  for (size_t i = 0; i < data.string_attributes.size(); ++i) {
    if (data.string_attributes[i].first == attribute) {
      data.string_attributes[i].second = value;
      node_->SetData(data);
      return;
    }
  }
  if (!value.empty()) {
    data.string_attributes.push_back(std::make_pair(attribute, value));
    node_->SetData(data);
  }
}

bool BrowserAccessibility::HasIntListAttribute(
    ui::AXIntListAttribute attribute) const {
  const ui::AXNodeData& data = GetData();
  for (size_t i = 0; i < data.intlist_attributes.size(); ++i) {
    if (data.intlist_attributes[i].first == attribute)
      return true;
  }

  return false;
}

const std::vector<int32>& BrowserAccessibility::GetIntListAttribute(
    ui::AXIntListAttribute attribute) const {
  const ui::AXNodeData& data = GetData();
  CR_DEFINE_STATIC_LOCAL(std::vector<int32>, empty_vector, ());
  for (size_t i = 0; i < data.intlist_attributes.size(); ++i) {
    if (data.intlist_attributes[i].first == attribute)
      return data.intlist_attributes[i].second;
  }

  return empty_vector;
}

bool BrowserAccessibility::GetIntListAttribute(
    ui::AXIntListAttribute attribute,
    std::vector<int32>* value) const {
  const ui::AXNodeData& data = GetData();
  for (size_t i = 0; i < data.intlist_attributes.size(); ++i) {
    if (data.intlist_attributes[i].first == attribute) {
      *value = data.intlist_attributes[i].second;
      return true;
    }
  }

  return false;
}

bool BrowserAccessibility::GetHtmlAttribute(
    const char* html_attr, std::string* value) const {
  for (size_t i = 0; i < GetHtmlAttributes().size(); ++i) {
    const std::string& attr = GetHtmlAttributes()[i].first;
    if (LowerCaseEqualsASCII(attr, html_attr)) {
      *value = GetHtmlAttributes()[i].second;
      return true;
    }
  }

  return false;
}

bool BrowserAccessibility::GetHtmlAttribute(
    const char* html_attr, base::string16* value) const {
  std::string value_utf8;
  if (!GetHtmlAttribute(html_attr, &value_utf8))
    return false;
  *value = base::UTF8ToUTF16(value_utf8);
  return true;
}

bool BrowserAccessibility::GetAriaTristate(
    const char* html_attr,
    bool* is_defined,
    bool* is_mixed) const {
  *is_defined = false;
  *is_mixed = false;

  base::string16 value;
  if (!GetHtmlAttribute(html_attr, &value) ||
      value.empty() ||
      EqualsASCII(value, "undefined")) {
    return false;  // Not set (and *is_defined is also false)
  }

  *is_defined = true;

  if (EqualsASCII(value, "true"))
    return true;

  if (EqualsASCII(value, "mixed"))
    *is_mixed = true;

  return false;  // Not set
}

bool BrowserAccessibility::HasState(ui::AXState state_enum) const {
  return (GetState() >> state_enum) & 1;
}

bool BrowserAccessibility::IsEditableText() const {
  // These roles don't have readonly set, but they're not editable text.
  if (GetRole() == ui::AX_ROLE_SCROLL_AREA ||
      GetRole() == ui::AX_ROLE_COLUMN ||
      GetRole() == ui::AX_ROLE_TABLE_HEADER_CONTAINER) {
    return false;
  }

  // Note: WebAXStateReadonly being false means it's either a text control,
  // or contenteditable. We also check for editable text roles to cover
  // another element that has role=textbox set on it.
  return (!HasState(ui::AX_STATE_READ_ONLY) ||
          GetRole() == ui::AX_ROLE_TEXT_FIELD ||
          GetRole() == ui::AX_ROLE_TEXT_AREA);
}

std::string BrowserAccessibility::GetTextRecursive() const {
  if (!name_.empty()) {
    return name_;
  }

  std::string result;
  for (uint32 i = 0; i < PlatformChildCount(); ++i)
    result += PlatformGetChild(i)->GetTextRecursive();
  return result;
}

int BrowserAccessibility::GetStaticTextLenRecursive() const {
  if (GetRole() == ui::AX_ROLE_STATIC_TEXT)
    return static_cast<int>(GetStringAttribute(ui::AX_ATTR_VALUE).size());

  int len = 0;
  for (size_t i = 0; i < InternalChildCount(); ++i)
    len += InternalGetChild(i)->GetStaticTextLenRecursive();
  return len;
}

}  // namespace content
