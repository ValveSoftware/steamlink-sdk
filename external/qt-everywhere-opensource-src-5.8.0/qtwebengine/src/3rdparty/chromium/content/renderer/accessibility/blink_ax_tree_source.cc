// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/blink_ax_tree_source.h"

#include <stddef.h>

#include <set>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/accessibility_messages.h"
#include "content/renderer/accessibility/blink_ax_enum_conversion.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/web_frame_utils.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebPlugin.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebView.h"

using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using blink::WebAXObject;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFrame;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebPlugin;
using blink::WebPluginContainer;
using blink::WebVector;
using blink::WebView;

namespace content {

namespace {

// Returns true if |ancestor| is the first unignored parent of |child|,
// which means that when walking up the parent chain from |child|,
// |ancestor| is the *first* ancestor that isn't marked as
// accessibilityIsIgnored().
bool IsParentUnignoredOf(WebAXObject ancestor,
                         WebAXObject child) {
  WebAXObject parent = child.parentObject();
  while (!parent.isDetached() && parent.accessibilityIsIgnored())
    parent = parent.parentObject();
  return parent.equals(ancestor);
}

std::string GetEquivalentAriaRoleString(const ui::AXRole role) {
  switch (role) {
    case ui::AX_ROLE_ARTICLE:
      return "article";
    case ui::AX_ROLE_BANNER:
      return "banner";
    case ui::AX_ROLE_BUTTON:
      return "button";
    case ui::AX_ROLE_COMPLEMENTARY:
      return "complementary";
    case ui::AX_ROLE_FIGURE:
      return "figure";
    case ui::AX_ROLE_FOOTER:
      return "contentinfo";
    case ui::AX_ROLE_HEADING:
      return "heading";
    case ui::AX_ROLE_IMAGE:
      return "img";
    case ui::AX_ROLE_MAIN:
      return "main";
    case ui::AX_ROLE_NAVIGATION:
      return "navigation";
    case ui::AX_ROLE_RADIO_BUTTON:
      return "radio";
    case ui::AX_ROLE_REGION:
      return "region";
    case ui::AX_ROLE_SLIDER:
      return "slider";
    default:
      break;
  }

  return std::string();
}

void AddIntListAttributeFromWebObjects(ui::AXIntListAttribute attr,
                                       WebVector<WebAXObject> objects,
                                       AXContentNodeData* dst) {
  std::vector<int32_t> ids;
  for(size_t i = 0; i < objects.size(); i++)
    ids.push_back(objects[i].axID());
  if (ids.size() > 0)
    dst->AddIntListAttribute(attr, ids);
}

}  // namespace

BlinkAXTreeSource::BlinkAXTreeSource(RenderFrameImpl* render_frame)
    : render_frame_(render_frame),
      accessibility_focus_id_(-1) {
}

BlinkAXTreeSource::~BlinkAXTreeSource() {
}

void BlinkAXTreeSource::SetRoot(blink::WebAXObject root) {
  root_ = root;
}

bool BlinkAXTreeSource::IsInTree(blink::WebAXObject node) const {
  const blink::WebAXObject& root = GetRoot();
  while (IsValid(node)) {
    if (node.equals(root))
      return true;
    node = GetParent(node);
  }
  return false;
}

bool BlinkAXTreeSource::GetTreeData(AXContentTreeData* tree_data) const {
  blink::WebDocument document = BlinkAXTreeSource::GetMainDocument();
  const blink::WebAXObject& root = GetRoot();

  tree_data->doctype = "html";
  tree_data->loaded = root.isLoaded();
  tree_data->loading_progress = root.estimatedLoadingProgress();
  tree_data->mimetype = document.isXHTMLDocument() ? "text/xhtml" : "text/html";
  tree_data->title = document.title().utf8();
  tree_data->url = document.url().string().utf8();

  WebAXObject focus = document.focusedAccessibilityObject();
  if (!focus.isNull())
    tree_data->focus_id = focus.axID();

  WebAXObject anchor_object, focus_object;
  int anchor_offset, focus_offset;
  root.selection(anchor_object, anchor_offset, focus_object, focus_offset);
  if (!anchor_object.isNull() && !focus_object.isNull() &&
      anchor_offset >= 0 && focus_offset >= 0) {
    int32_t anchor_id = anchor_object.axID();
    int32_t focus_id = focus_object.axID();
    tree_data->sel_anchor_object_id = anchor_id;
    tree_data->sel_anchor_offset = anchor_offset;
    tree_data->sel_focus_object_id = focus_id;
    tree_data->sel_focus_offset = focus_offset;
  }

  // Get the tree ID for this frame and the parent frame.
  WebLocalFrame* web_frame = document.frame();
  if (web_frame) {
    RenderFrame* render_frame = RenderFrame::FromWebFrame(web_frame);
    tree_data->routing_id = render_frame->GetRoutingID();

    // Get the tree ID for the parent frame.
    blink::WebFrame* parent_web_frame = web_frame->parent();
    if (parent_web_frame) {
      tree_data->parent_routing_id =
          GetRoutingIdForFrameOrProxy(parent_web_frame);
    }
  }

  return true;
}

blink::WebAXObject BlinkAXTreeSource::GetRoot() const {
  if (!root_.isNull())
    return root_;
  return GetMainDocument().accessibilityObject();
}

blink::WebAXObject BlinkAXTreeSource::GetFromId(int32_t id) const {
  return GetMainDocument().accessibilityObjectFromID(id);
}

int32_t BlinkAXTreeSource::GetId(blink::WebAXObject node) const {
  return node.axID();
}

void BlinkAXTreeSource::GetChildren(
    blink::WebAXObject parent,
    std::vector<blink::WebAXObject>* out_children) const {
  if (parent.role() == blink::WebAXRoleStaticText) {
    blink::WebAXObject ancestor = parent;
    while (!ancestor.isDetached()) {
      int32_t focus_id = GetMainDocument().focusedAccessibilityObject().axID();
      if (ancestor.axID() == accessibility_focus_id_ ||
          (ancestor.axID() == focus_id && ancestor.isEditable())) {
        parent.loadInlineTextBoxes();
        break;
      }
      ancestor = ancestor.parentObject();
    }
  }

  bool is_iframe = false;
  WebNode node = parent.node();
  if (!node.isNull() && node.isElementNode())
    is_iframe = node.to<WebElement>().hasHTMLTagName("iframe");

  for (unsigned i = 0; i < parent.childCount(); i++) {
    blink::WebAXObject child = parent.childAt(i);

    // The child may be invalid due to issues in blink accessibility code.
    if (child.isDetached())
      continue;

    // Skip children whose parent isn't |parent|.
    // As an exception, include children of an iframe element.
    if (!is_iframe && !IsParentUnignoredOf(parent, child))
      continue;

    out_children->push_back(child);
  }
}

blink::WebAXObject BlinkAXTreeSource::GetParent(
    blink::WebAXObject node) const {
  // Blink returns ignored objects when walking up the parent chain,
  // we have to skip those here. Also, stop when we get to the root
  // element.
  blink::WebAXObject root = GetRoot();
  do {
    if (node.equals(root))
      return blink::WebAXObject();
    node = node.parentObject();
  } while (!node.isDetached() && node.accessibilityIsIgnored());

  return node;
}

bool BlinkAXTreeSource::IsValid(blink::WebAXObject node) const {
  return !node.isDetached();  // This also checks if it's null.
}

bool BlinkAXTreeSource::IsEqual(blink::WebAXObject node1,
                                blink::WebAXObject node2) const {
  return node1.equals(node2);
}

blink::WebAXObject BlinkAXTreeSource::GetNull() const {
  return blink::WebAXObject();
}

void BlinkAXTreeSource::SerializeNode(blink::WebAXObject src,
                                      AXContentNodeData* dst) const {
  dst->role = AXRoleFromBlink(src.role());
  dst->state = AXStateFromBlink(src);
  dst->location = src.boundingBoxRect();
  dst->id = src.axID();

  blink::WebAXNameFrom nameFrom;
  blink::WebVector<blink::WebAXObject> nameObjects;
  blink::WebString web_name = src.name(nameFrom, nameObjects);
  if (!web_name.isEmpty()) {
    dst->AddStringAttribute(ui::AX_ATTR_NAME, web_name.utf8());
    dst->AddIntAttribute(ui::AX_ATTR_NAME_FROM, AXNameFromFromBlink(nameFrom));
    AddIntListAttributeFromWebObjects(
        ui::AX_ATTR_LABELLEDBY_IDS, nameObjects, dst);
  }

  blink::WebAXDescriptionFrom descriptionFrom;
  blink::WebVector<blink::WebAXObject> descriptionObjects;
  blink::WebString web_description = src.description(
      nameFrom, descriptionFrom, descriptionObjects);
  if (!web_description.isEmpty()) {
    dst->AddStringAttribute(ui::AX_ATTR_DESCRIPTION, web_description.utf8());
    dst->AddIntAttribute(ui::AX_ATTR_DESCRIPTION_FROM,
        AXDescriptionFromFromBlink(descriptionFrom));
    AddIntListAttributeFromWebObjects(
        ui::AX_ATTR_DESCRIBEDBY_IDS, descriptionObjects, dst);
  }

  blink::WebString web_placeholder = src.placeholder(nameFrom, descriptionFrom);
  if (!web_placeholder.isEmpty())
    dst->AddStringAttribute(ui::AX_ATTR_PLACEHOLDER, web_placeholder.utf8());

  std::string value;
  if (src.valueDescription().length()) {
    dst->AddStringAttribute(ui::AX_ATTR_VALUE, src.valueDescription().utf8());
  } else {
    dst->AddStringAttribute(ui::AX_ATTR_VALUE, src.stringValue().utf8());
  }

  if (dst->role == ui::AX_ROLE_COLOR_WELL)
    dst->AddIntAttribute(ui::AX_ATTR_COLOR_VALUE, src.colorValue());


  // Text attributes.
  if (src.backgroundColor())
    dst->AddIntAttribute(ui::AX_ATTR_BACKGROUND_COLOR, src.backgroundColor());

  if (src.color())
    dst->AddIntAttribute(ui::AX_ATTR_COLOR, src.color());

  if (src.fontFamily().length()) {
    WebAXObject parent = src.parentObject();
    if (parent.isNull() || parent.fontFamily() != src.fontFamily())
      dst->AddStringAttribute(ui::AX_ATTR_FONT_FAMILY, src.fontFamily().utf8());
  }

  // Font size is in pixels.
  if (src.fontSize())
    dst->AddFloatAttribute(ui::AX_ATTR_FONT_SIZE, src.fontSize());

  if (src.ariaCurrentState()) {
    dst->AddIntAttribute(ui::AX_ATTR_ARIA_CURRENT_STATE,
                         AXAriaCurrentStateFromBlink(src.ariaCurrentState()));
  }

  if (src.invalidState()) {
    dst->AddIntAttribute(ui::AX_ATTR_INVALID_STATE,
                         AXInvalidStateFromBlink(src.invalidState()));
  }
  if (src.invalidState() == blink::WebAXInvalidStateOther &&
      src.ariaInvalidValue().length()) {
    dst->AddStringAttribute(
        ui::AX_ATTR_ARIA_INVALID_VALUE, src.ariaInvalidValue().utf8());
  }

  if (src.textDirection()) {
    dst->AddIntAttribute(ui::AX_ATTR_TEXT_DIRECTION,
                         AXTextDirectionFromBlink(src.textDirection()));
  }

  if (src.textStyle()) {
    dst->AddIntAttribute(ui::AX_ATTR_TEXT_STYLE,
                         AXTextStyleFromBlink(src.textStyle()));
  }


  if (dst->role == ui::AX_ROLE_INLINE_TEXT_BOX) {
    WebVector<int> src_character_offsets;
    src.characterOffsets(src_character_offsets);
    std::vector<int32_t> character_offsets;
    character_offsets.reserve(src_character_offsets.size());
    for (size_t i = 0; i < src_character_offsets.size(); ++i)
      character_offsets.push_back(src_character_offsets[i]);
    dst->AddIntListAttribute(ui::AX_ATTR_CHARACTER_OFFSETS, character_offsets);

    WebVector<int> src_word_starts;
    WebVector<int> src_word_ends;
    src.wordBoundaries(src_word_starts, src_word_ends);
    std::vector<int32_t> word_starts;
    std::vector<int32_t> word_ends;
    word_starts.reserve(src_word_starts.size());
    word_ends.reserve(src_word_starts.size());
    for (size_t i = 0; i < src_word_starts.size(); ++i) {
      word_starts.push_back(src_word_starts[i]);
      word_ends.push_back(src_word_ends[i]);
    }
    dst->AddIntListAttribute(ui::AX_ATTR_WORD_STARTS, word_starts);
    dst->AddIntListAttribute(ui::AX_ATTR_WORD_ENDS, word_ends);
  }

  if (src.accessKey().length()) {
    dst->AddStringAttribute(ui::AX_ATTR_ACCESS_KEY, src.accessKey().utf8());
  }

  if (src.actionVerb().length()) {
    dst->AddStringAttribute(ui::AX_ATTR_ACTION, src.actionVerb().utf8());
  }

  if (src.ariaAutoComplete().length()) {
    dst->AddStringAttribute(
        ui::AX_ATTR_AUTO_COMPLETE,
        src.ariaAutoComplete().utf8());
  }

  if (src.isAriaReadOnly())
    dst->AddBoolAttribute(ui::AX_ATTR_ARIA_READONLY, true);

  if (src.isButtonStateMixed())
    dst->AddBoolAttribute(ui::AX_ATTR_STATE_MIXED, true);

  if (src.canSetValueAttribute())
    dst->AddBoolAttribute(ui::AX_ATTR_CAN_SET_VALUE, true);

  if (src.hasComputedStyle()) {
    dst->AddStringAttribute(
        ui::AX_ATTR_DISPLAY, src.computedStyleDisplay().utf8());
  }

  if (src.language().length()) {
    WebAXObject parent = src.parentObject();
    if (parent.isNull() || parent.language() != src.language())
      dst->AddStringAttribute(ui::AX_ATTR_LANGUAGE, src.language().utf8());
  }

  if (src.keyboardShortcut().length()) {
    dst->AddStringAttribute(
        ui::AX_ATTR_SHORTCUT,
        src.keyboardShortcut().utf8());
  }

  if (!src.nextOnLine().isDetached()) {
    dst->AddIntAttribute(ui::AX_ATTR_NEXT_ON_LINE_ID, src.nextOnLine().axID());
  }

  if (!src.previousOnLine().isDetached()) {
    dst->AddIntAttribute(ui::AX_ATTR_PREVIOUS_ON_LINE_ID,
                         src.previousOnLine().axID());
  }

  if (!src.ariaActiveDescendant().isDetached()) {
    dst->AddIntAttribute(ui::AX_ATTR_ACTIVEDESCENDANT_ID,
                         src.ariaActiveDescendant().axID());
  }

  if (!src.url().isEmpty())
    dst->AddStringAttribute(ui::AX_ATTR_URL, src.url().string().utf8());

  if (dst->role == ui::AX_ROLE_HEADING)
    dst->AddIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL, src.headingLevel());
  else if ((dst->role == ui::AX_ROLE_TREE_ITEM ||
            dst->role == ui::AX_ROLE_ROW) &&
           src.hierarchicalLevel() > 0) {
    dst->AddIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL,
                         src.hierarchicalLevel());
  }

  if (src.setSize())
    dst->AddIntAttribute(ui::AX_ATTR_SET_SIZE, src.setSize());

  if (src.posInSet())
    dst->AddIntAttribute(ui::AX_ATTR_POS_IN_SET, src.posInSet());

  if (src.canvasHasFallbackContent())
    dst->AddBoolAttribute(ui::AX_ATTR_CANVAS_HAS_FALLBACK, true);

  // Spelling, grammar and other document markers.
  WebVector<blink::WebAXMarkerType> src_marker_types;
  WebVector<int> src_marker_starts;
  WebVector<int> src_marker_ends;
  src.markers(src_marker_types, src_marker_starts, src_marker_ends);
  DCHECK_EQ(src_marker_types.size(), src_marker_starts.size());
  DCHECK_EQ(src_marker_starts.size(), src_marker_ends.size());

  if (src_marker_types.size()) {
    std::vector<int32_t> marker_types;
    std::vector<int32_t> marker_starts;
    std::vector<int32_t> marker_ends;
    marker_types.reserve(src_marker_types.size());
    marker_starts.reserve(src_marker_starts.size());
    marker_ends.reserve(src_marker_ends.size());
    for (size_t i = 0; i < src_marker_types.size(); ++i) {
      marker_types.push_back(
          static_cast<int32_t>(AXMarkerTypeFromBlink(src_marker_types[i])));
      marker_starts.push_back(src_marker_starts[i]);
      marker_ends.push_back(src_marker_ends[i]);
    }
    dst->AddIntListAttribute(ui::AX_ATTR_MARKER_TYPES, marker_types);
    dst->AddIntListAttribute(ui::AX_ATTR_MARKER_STARTS, marker_starts);
    dst->AddIntListAttribute(ui::AX_ATTR_MARKER_ENDS, marker_ends);
  }

  WebNode node = src.node();
  bool is_iframe = false;

  if (!node.isNull() && node.isElementNode()) {
    WebElement element = node.to<WebElement>();
    is_iframe = element.hasHTMLTagName("iframe");

    // TODO(ctguil): The tagName in WebKit is lower cased but
    // HTMLElement::nodeName calls localNameUpper. Consider adding
    // a WebElement method that returns the original lower cased tagName.
    dst->AddStringAttribute(
        ui::AX_ATTR_HTML_TAG,
        base::ToLowerASCII(element.tagName().utf8()));
    for (unsigned i = 0; i < element.attributeCount(); ++i) {
      std::string name = base::ToLowerASCII(
          element.attributeLocalName(i).utf8());
      std::string value = element.attributeValue(i).utf8();
      dst->html_attributes.push_back(std::make_pair(name, value));
    }

    if (src.isEditable()) {
      dst->AddIntAttribute(ui::AX_ATTR_TEXT_SEL_START, src.selectionStart());
      dst->AddIntAttribute(ui::AX_ATTR_TEXT_SEL_END, src.selectionEnd());

      WebVector<int> src_line_breaks;
      src.lineBreaks(src_line_breaks);
      if (src_line_breaks.size()) {
        std::vector<int32_t> line_breaks;
        line_breaks.reserve(src_line_breaks.size());
        for (size_t i = 0; i < src_line_breaks.size(); ++i)
          line_breaks.push_back(src_line_breaks[i]);
        dst->AddIntListAttribute(ui::AX_ATTR_LINE_BREAKS, line_breaks);
      }
    }

    // ARIA role.
    if (element.hasAttribute("role")) {
      dst->AddStringAttribute(
          ui::AX_ATTR_ROLE,
          element.getAttribute("role").utf8());
    } else {
      std::string role = GetEquivalentAriaRoleString(dst->role);
      if (!role.empty())
        dst->AddStringAttribute(ui::AX_ATTR_ROLE, role);
      else if (dst->role == ui::AX_ROLE_TIME)
        dst->AddStringAttribute(ui::AX_ATTR_ROLE, "time");
    }

    // Browser plugin (used in a <webview>).
    BrowserPlugin* browser_plugin = BrowserPlugin::GetFromNode(element);
    if (browser_plugin) {
      dst->AddContentIntAttribute(
          AX_CONTENT_ATTR_CHILD_BROWSER_PLUGIN_INSTANCE_ID,
          browser_plugin->browser_plugin_instance_id());
    }

    // Iframe.
    if (is_iframe) {
      WebFrame* frame = WebFrame::fromFrameOwnerElement(element);
      if (frame) {
        dst->AddContentIntAttribute(
            AX_CONTENT_ATTR_CHILD_ROUTING_ID,
            GetRoutingIdForFrameOrProxy(frame));
      }
    }
  }

  if (src.isInLiveRegion()) {
    dst->AddBoolAttribute(ui::AX_ATTR_LIVE_ATOMIC, src.liveRegionAtomic());
    dst->AddBoolAttribute(ui::AX_ATTR_LIVE_BUSY, src.liveRegionBusy());
    if (src.liveRegionBusy())
      dst->state |= (1 << ui::AX_STATE_BUSY);
    if (!src.liveRegionStatus().isEmpty()) {
      dst->AddStringAttribute(
          ui::AX_ATTR_LIVE_STATUS,
          src.liveRegionStatus().utf8());
    }
    dst->AddStringAttribute(
        ui::AX_ATTR_LIVE_RELEVANT,
        src.liveRegionRelevant().utf8());
    // If we are not at the root of an atomic live region.
    if (src.containerLiveRegionAtomic() && !src.liveRegionRoot().isDetached() &&
        !src.liveRegionAtomic()) {
      dst->AddIntAttribute(ui::AX_ATTR_MEMBER_OF_ID,
                           src.liveRegionRoot().axID());
    }
    dst->AddBoolAttribute(ui::AX_ATTR_CONTAINER_LIVE_ATOMIC,
                          src.containerLiveRegionAtomic());
    dst->AddBoolAttribute(ui::AX_ATTR_CONTAINER_LIVE_BUSY,
                          src.containerLiveRegionBusy());
    dst->AddStringAttribute(
        ui::AX_ATTR_CONTAINER_LIVE_STATUS,
        src.containerLiveRegionStatus().utf8());
    dst->AddStringAttribute(
        ui::AX_ATTR_CONTAINER_LIVE_RELEVANT,
        src.containerLiveRegionRelevant().utf8());
  }

  if (dst->role == ui::AX_ROLE_PROGRESS_INDICATOR ||
      dst->role == ui::AX_ROLE_METER ||
      dst->role == ui::AX_ROLE_SCROLL_BAR ||
      dst->role == ui::AX_ROLE_SLIDER ||
      dst->role == ui::AX_ROLE_SPIN_BUTTON) {
    dst->AddFloatAttribute(ui::AX_ATTR_VALUE_FOR_RANGE, src.valueForRange());
    dst->AddFloatAttribute(ui::AX_ATTR_MAX_VALUE_FOR_RANGE,
                           src.maxValueForRange());
    dst->AddFloatAttribute(ui::AX_ATTR_MIN_VALUE_FOR_RANGE,
                           src.minValueForRange());
  }

  if (dst->role == ui::AX_ROLE_ROOT_WEB_AREA) {
    dst->AddStringAttribute(ui::AX_ATTR_HTML_TAG, "#document");
    dst->transform.reset(
        new gfx::Transform(src.transformFromLocalParentFrame()));
  }

  if (dst->role == ui::AX_ROLE_TABLE) {
    int column_count = src.columnCount();
    int row_count = src.rowCount();
    if (column_count > 0 && row_count > 0) {
      std::set<int32_t> unique_cell_id_set;
      std::vector<int32_t> cell_ids;
      std::vector<int32_t> unique_cell_ids;
      dst->AddIntAttribute(ui::AX_ATTR_TABLE_COLUMN_COUNT, column_count);
      dst->AddIntAttribute(ui::AX_ATTR_TABLE_ROW_COUNT, row_count);
      WebAXObject header = src.headerContainerObject();
      if (!header.isDetached())
        dst->AddIntAttribute(ui::AX_ATTR_TABLE_HEADER_ID, header.axID());
      for (int i = 0; i < column_count * row_count; ++i) {
        WebAXObject cell = src.cellForColumnAndRow(
            i % column_count, i / column_count);
        int cell_id = -1;
        if (!cell.isDetached()) {
          cell_id = cell.axID();
          if (unique_cell_id_set.find(cell_id) == unique_cell_id_set.end()) {
            unique_cell_id_set.insert(cell_id);
            unique_cell_ids.push_back(cell_id);
          }
        }
        cell_ids.push_back(cell_id);
      }
      dst->AddIntListAttribute(ui::AX_ATTR_CELL_IDS, cell_ids);
      dst->AddIntListAttribute(ui::AX_ATTR_UNIQUE_CELL_IDS, unique_cell_ids);
    }
  }

  if (dst->role == ui::AX_ROLE_ROW) {
    dst->AddIntAttribute(ui::AX_ATTR_TABLE_ROW_INDEX, src.rowIndex());
    WebAXObject header = src.rowHeader();
    if (!header.isDetached())
      dst->AddIntAttribute(ui::AX_ATTR_TABLE_ROW_HEADER_ID, header.axID());
  }

  if (dst->role == ui::AX_ROLE_COLUMN) {
    dst->AddIntAttribute(ui::AX_ATTR_TABLE_COLUMN_INDEX, src.columnIndex());
    WebAXObject header = src.columnHeader();
    if (!header.isDetached())
      dst->AddIntAttribute(ui::AX_ATTR_TABLE_COLUMN_HEADER_ID, header.axID());
  }

  if (dst->role == ui::AX_ROLE_CELL ||
      dst->role == ui::AX_ROLE_ROW_HEADER ||
      dst->role == ui::AX_ROLE_COLUMN_HEADER) {
    dst->AddIntAttribute(ui::AX_ATTR_TABLE_CELL_COLUMN_INDEX,
                         src.cellColumnIndex());
    dst->AddIntAttribute(ui::AX_ATTR_TABLE_CELL_COLUMN_SPAN,
                         src.cellColumnSpan());
    dst->AddIntAttribute(ui::AX_ATTR_TABLE_CELL_ROW_INDEX, src.cellRowIndex());
    dst->AddIntAttribute(ui::AX_ATTR_TABLE_CELL_ROW_SPAN, src.cellRowSpan());
  }

  if ((dst->role == ui::AX_ROLE_ROW_HEADER ||
      dst->role == ui::AX_ROLE_COLUMN_HEADER) && src.sortDirection()) {
    dst->AddIntAttribute(ui::AX_ATTR_SORT_DIRECTION,
                         AXSortDirectionFromBlink(src.sortDirection()));
  }

  // Add the ids of *indirect* children - those who are children of this node,
  // but whose parent is *not* this node. One example is a table
  // cell, which is a child of both a row and a column. Because the cell's
  // parent is the row, the row adds it as a child, and the column adds it
  // as an indirect child.
  int child_count = src.childCount();
  for (int i = 0; i < child_count; ++i) {
    WebAXObject child = src.childAt(i);
    std::vector<int32_t> indirect_child_ids;
    if (!is_iframe && !child.isDetached() && !IsParentUnignoredOf(src, child))
      indirect_child_ids.push_back(child.axID());
    if (indirect_child_ids.size() > 0) {
      dst->AddIntListAttribute(
          ui::AX_ATTR_INDIRECT_CHILD_IDS, indirect_child_ids);
    }
  }

  WebVector<WebAXObject> controls;
  if (src.ariaControls(controls))
    AddIntListAttributeFromWebObjects(ui::AX_ATTR_CONTROLS_IDS, controls, dst);

  WebVector<WebAXObject> flowTo;
  if (src.ariaFlowTo(flowTo))
    AddIntListAttributeFromWebObjects(ui::AX_ATTR_FLOWTO_IDS, flowTo, dst);

  if (src.isScrollableContainer()) {
    const gfx::Point& scrollOffset = src.scrollOffset();
    dst->AddIntAttribute(ui::AX_ATTR_SCROLL_X, scrollOffset.x());
    dst->AddIntAttribute(ui::AX_ATTR_SCROLL_Y, scrollOffset.y());

    const gfx::Point& minScrollOffset = src.minimumScrollOffset();
    dst->AddIntAttribute(ui::AX_ATTR_SCROLL_X_MIN, minScrollOffset.x());
    dst->AddIntAttribute(ui::AX_ATTR_SCROLL_Y_MIN, minScrollOffset.y());

    const gfx::Point& maxScrollOffset = src.maximumScrollOffset();
    dst->AddIntAttribute(ui::AX_ATTR_SCROLL_X_MAX, maxScrollOffset.x());
    dst->AddIntAttribute(ui::AX_ATTR_SCROLL_Y_MAX, maxScrollOffset.y());
  }
}

blink::WebDocument BlinkAXTreeSource::GetMainDocument() const {
  if (render_frame_ && render_frame_->GetWebFrame())
    return render_frame_->GetWebFrame()->document();
  return WebDocument();
}

}  // namespace content
