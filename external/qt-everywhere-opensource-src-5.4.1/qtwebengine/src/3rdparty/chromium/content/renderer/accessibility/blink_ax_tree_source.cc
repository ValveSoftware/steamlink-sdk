// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/blink_ax_tree_source.h"

#include <set>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/accessibility/blink_ax_enum_conversion.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebDocumentType.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebView.h"

using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using blink::WebAXObject;
using blink::WebDocument;
using blink::WebDocumentType;
using blink::WebElement;
using blink::WebLocalFrame;
using blink::WebNode;
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

bool IsTrue(std::string html_value) {
  return LowerCaseEqualsASCII(html_value, "true");
}

std::string GetEquivalentAriaRoleString(const ui::AXRole role) {
  switch (role) {
    case ui::AX_ROLE_ARTICLE:
      return "article";
    case ui::AX_ROLE_BANNER:
      return "banner";
    case ui::AX_ROLE_COMPLEMENTARY:
      return "complementary";
    case ui::AX_ROLE_CONTENT_INFO:
    case ui::AX_ROLE_FOOTER:
      return "contentinfo";
    case ui::AX_ROLE_MAIN:
      return "main";
    case ui::AX_ROLE_NAVIGATION:
      return "navigation";
    case ui::AX_ROLE_REGION:
      return "region";
    default:
      break;
  }

  return std::string();
}

void AddIntListAttributeFromWebObjects(ui::AXIntListAttribute attr,
                                       WebVector<WebAXObject> objects,
                                       ui::AXNodeData* dst) {
  std::vector<int32> ids;
  for(size_t i = 0; i < objects.size(); i++)
    ids.push_back(objects[i].axID());
  if (ids.size() > 0)
    dst->AddIntListAttribute(attr, ids);
}

}  // Anonymous namespace

BlinkAXTreeSource::BlinkAXTreeSource(RenderViewImpl* render_view)
    : render_view_(render_view) {
}

BlinkAXTreeSource::~BlinkAXTreeSource() {
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

blink::WebAXObject BlinkAXTreeSource::GetRoot() const {
  return GetMainDocument().accessibilityObject();
}

blink::WebAXObject BlinkAXTreeSource::GetFromId(int32 id) const {
  return GetMainDocument().accessibilityObjectFromID(id);
}

int32 BlinkAXTreeSource::GetId(blink::WebAXObject node) const {
  return node.axID();
}

void BlinkAXTreeSource::GetChildren(
    blink::WebAXObject parent,
    std::vector<blink::WebAXObject>* out_children) const {
  bool is_iframe = false;
  WebNode node = parent.node();
  if (!node.isNull() && node.isElementNode()) {
    WebElement element = node.to<WebElement>();
    is_iframe = (element.tagName() == ASCIIToUTF16("IFRAME"));
  }

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
                                      ui::AXNodeData* dst) const {
  dst->role = AXRoleFromBlink(src.role());
  dst->state = AXStateFromBlink(src);
  dst->location = src.boundingBoxRect();
  dst->id = src.axID();
  std::string name = UTF16ToUTF8(src.title());

  std::string value;
  if (src.valueDescription().length()) {
    dst->AddStringAttribute(ui::AX_ATTR_VALUE,
                            UTF16ToUTF8(src.valueDescription()));
  } else {
    dst->AddStringAttribute(ui::AX_ATTR_VALUE, UTF16ToUTF8(src.stringValue()));
  }

  if (dst->role == ui::AX_ROLE_COLOR_WELL) {
    int r, g, b;
    src.colorValue(r, g, b);
    dst->AddIntAttribute(ui::AX_ATTR_COLOR_VALUE_RED, r);
    dst->AddIntAttribute(ui::AX_ATTR_COLOR_VALUE_GREEN, g);
    dst->AddIntAttribute(ui::AX_ATTR_COLOR_VALUE_BLUE, b);
  }

  if (dst->role == ui::AX_ROLE_INLINE_TEXT_BOX) {
    dst->AddIntAttribute(ui::AX_ATTR_TEXT_DIRECTION,
                         AXTextDirectionFromBlink(src.textDirection()));

    WebVector<int> src_character_offsets;
    src.characterOffsets(src_character_offsets);
    std::vector<int32> character_offsets;
    character_offsets.reserve(src_character_offsets.size());
    for (size_t i = 0; i < src_character_offsets.size(); ++i)
      character_offsets.push_back(src_character_offsets[i]);
    dst->AddIntListAttribute(ui::AX_ATTR_CHARACTER_OFFSETS, character_offsets);

    WebVector<int> src_word_starts;
    WebVector<int> src_word_ends;
    src.wordBoundaries(src_word_starts, src_word_ends);
    std::vector<int32> word_starts;
    std::vector<int32> word_ends;
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
    dst->AddStringAttribute(ui::AX_ATTR_ACCESS_KEY,
    UTF16ToUTF8(src.accessKey()));
  }
  if (src.actionVerb().length())
    dst->AddStringAttribute(ui::AX_ATTR_ACTION, UTF16ToUTF8(src.actionVerb()));
  if (src.isAriaReadOnly())
    dst->AddBoolAttribute(ui::AX_ATTR_ARIA_READONLY, true);
  if (src.isButtonStateMixed())
    dst->AddBoolAttribute(ui::AX_ATTR_BUTTON_MIXED, true);
  if (src.canSetValueAttribute())
    dst->AddBoolAttribute(ui::AX_ATTR_CAN_SET_VALUE, true);
  if (src.accessibilityDescription().length()) {
    dst->AddStringAttribute(ui::AX_ATTR_DESCRIPTION,
                            UTF16ToUTF8(src.accessibilityDescription()));
  }
  if (src.hasComputedStyle()) {
    dst->AddStringAttribute(ui::AX_ATTR_DISPLAY,
                            UTF16ToUTF8(src.computedStyleDisplay()));
  }
  if (src.helpText().length())
    dst->AddStringAttribute(ui::AX_ATTR_HELP, UTF16ToUTF8(src.helpText()));
  if (src.keyboardShortcut().length()) {
    dst->AddStringAttribute(ui::AX_ATTR_SHORTCUT,
                            UTF16ToUTF8(src.keyboardShortcut()));
  }
  if (!src.titleUIElement().isDetached()) {
    dst->AddIntAttribute(ui::AX_ATTR_TITLE_UI_ELEMENT,
                         src.titleUIElement().axID());
  }
  if (!src.ariaActiveDescendant().isDetached()) {
    dst->AddIntAttribute(ui::AX_ATTR_ACTIVEDESCENDANT_ID,
                         src.ariaActiveDescendant().axID());
  }

  if (!src.url().isEmpty())
    dst->AddStringAttribute(ui::AX_ATTR_URL, src.url().spec());

  if (dst->role == ui::AX_ROLE_HEADING)
    dst->AddIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL, src.headingLevel());
  else if ((dst->role == ui::AX_ROLE_TREE_ITEM ||
            dst->role == ui::AX_ROLE_ROW) &&
           src.hierarchicalLevel() > 0) {
    dst->AddIntAttribute(ui::AX_ATTR_HIERARCHICAL_LEVEL,
                         src.hierarchicalLevel());
  }

  // Treat the active list box item as focused.
  if (dst->role == ui::AX_ROLE_LIST_BOX_OPTION &&
      src.isSelectedOptionActive()) {
    dst->state |= (1 << ui::AX_STATE_FOCUSED);
  }

  if (src.canvasHasFallbackContent())
    dst->AddBoolAttribute(ui::AX_ATTR_CANVAS_HAS_FALLBACK, true);

  WebNode node = src.node();
  bool is_iframe = false;
  std::string live_atomic;
  std::string live_busy;
  std::string live_status;
  std::string live_relevant;

  if (!node.isNull() && node.isElementNode()) {
    WebElement element = node.to<WebElement>();
    is_iframe = (element.tagName() == ASCIIToUTF16("IFRAME"));

    if (LowerCaseEqualsASCII(element.getAttribute("aria-expanded"), "true"))
      dst->state |= (1 << ui::AX_STATE_EXPANDED);

    // TODO(ctguil): The tagName in WebKit is lower cased but
    // HTMLElement::nodeName calls localNameUpper. Consider adding
    // a WebElement method that returns the original lower cased tagName.
    dst->AddStringAttribute(
        ui::AX_ATTR_HTML_TAG,
        StringToLowerASCII(UTF16ToUTF8(element.tagName())));
    for (unsigned i = 0; i < element.attributeCount(); ++i) {
      std::string name = StringToLowerASCII(UTF16ToUTF8(
          element.attributeLocalName(i)));
      std::string value = UTF16ToUTF8(element.attributeValue(i));
      dst->html_attributes.push_back(std::make_pair(name, value));
    }

    if (dst->role == ui::AX_ROLE_EDITABLE_TEXT ||
        dst->role == ui::AX_ROLE_TEXT_AREA ||
        dst->role == ui::AX_ROLE_TEXT_FIELD) {
      dst->AddIntAttribute(ui::AX_ATTR_TEXT_SEL_START, src.selectionStart());
      dst->AddIntAttribute(ui::AX_ATTR_TEXT_SEL_END, src.selectionEnd());

      WebVector<int> src_line_breaks;
      src.lineBreaks(src_line_breaks);
      if (src_line_breaks.size() > 0) {
        std::vector<int32> line_breaks;
        line_breaks.reserve(src_line_breaks.size());
        for (size_t i = 0; i < src_line_breaks.size(); ++i)
          line_breaks.push_back(src_line_breaks[i]);
        dst->AddIntListAttribute(ui::AX_ATTR_LINE_BREAKS, line_breaks);
      }
    }

    // ARIA role.
    if (element.hasAttribute("role")) {
      dst->AddStringAttribute(ui::AX_ATTR_ROLE,
                              UTF16ToUTF8(element.getAttribute("role")));
    } else {
      std::string role = GetEquivalentAriaRoleString(dst->role);
      if (!role.empty())
        dst->AddStringAttribute(ui::AX_ATTR_ROLE, role);
    }

    // Live region attributes
    live_atomic = UTF16ToUTF8(element.getAttribute("aria-atomic"));
    live_busy = UTF16ToUTF8(element.getAttribute("aria-busy"));
    live_status = UTF16ToUTF8(element.getAttribute("aria-live"));
    live_relevant = UTF16ToUTF8(element.getAttribute("aria-relevant"));
  }

  // Walk up the parent chain to set live region attributes of containers
  std::string container_live_atomic;
  std::string container_live_busy;
  std::string container_live_status;
  std::string container_live_relevant;
  WebAXObject container_accessible = src;
  while (!container_accessible.isDetached()) {
    WebNode container_node = container_accessible.node();
    if (!container_node.isNull() && container_node.isElementNode()) {
      WebElement container_elem = container_node.to<WebElement>();
      if (container_elem.hasAttribute("aria-atomic") &&
          container_live_atomic.empty()) {
        container_live_atomic =
            UTF16ToUTF8(container_elem.getAttribute("aria-atomic"));
      }
      if (container_elem.hasAttribute("aria-busy") &&
          container_live_busy.empty()) {
        container_live_busy =
            UTF16ToUTF8(container_elem.getAttribute("aria-busy"));
      }
      if (container_elem.hasAttribute("aria-live") &&
          container_live_status.empty()) {
        container_live_status =
            UTF16ToUTF8(container_elem.getAttribute("aria-live"));
      }
      if (container_elem.hasAttribute("aria-relevant") &&
          container_live_relevant.empty()) {
        container_live_relevant =
            UTF16ToUTF8(container_elem.getAttribute("aria-relevant"));
      }
    }
    container_accessible = container_accessible.parentObject();
  }

  if (!live_atomic.empty())
    dst->AddBoolAttribute(ui::AX_ATTR_LIVE_ATOMIC, IsTrue(live_atomic));
  if (!live_busy.empty())
    dst->AddBoolAttribute(ui::AX_ATTR_LIVE_BUSY, IsTrue(live_busy));
  if (!live_status.empty())
    dst->AddStringAttribute(ui::AX_ATTR_LIVE_STATUS, live_status);
  if (!live_relevant.empty())
    dst->AddStringAttribute(ui::AX_ATTR_LIVE_RELEVANT, live_relevant);

  if (!container_live_atomic.empty()) {
    dst->AddBoolAttribute(ui::AX_ATTR_CONTAINER_LIVE_ATOMIC,
                          IsTrue(container_live_atomic));
  }
  if (!container_live_busy.empty()) {
    dst->AddBoolAttribute(ui::AX_ATTR_CONTAINER_LIVE_BUSY,
                          IsTrue(container_live_busy));
  }
  if (!container_live_status.empty()) {
    dst->AddStringAttribute(ui::AX_ATTR_CONTAINER_LIVE_STATUS,
                            container_live_status);
  }
  if (!container_live_relevant.empty()) {
    dst->AddStringAttribute(ui::AX_ATTR_CONTAINER_LIVE_RELEVANT,
                            container_live_relevant);
  }

  if (dst->role == ui::AX_ROLE_PROGRESS_INDICATOR ||
      dst->role == ui::AX_ROLE_SCROLL_BAR ||
      dst->role == ui::AX_ROLE_SLIDER ||
      dst->role == ui::AX_ROLE_SPIN_BUTTON) {
    dst->AddFloatAttribute(ui::AX_ATTR_VALUE_FOR_RANGE, src.valueForRange());
    dst->AddFloatAttribute(ui::AX_ATTR_MAX_VALUE_FOR_RANGE,
                           src.maxValueForRange());
    dst->AddFloatAttribute(ui::AX_ATTR_MIN_VALUE_FOR_RANGE,
                           src.minValueForRange());
  }

  if (dst->role == ui::AX_ROLE_DOCUMENT ||
      dst->role == ui::AX_ROLE_WEB_AREA) {
    dst->AddStringAttribute(ui::AX_ATTR_HTML_TAG, "#document");
    const WebDocument& document = src.document();
    if (name.empty())
      name = UTF16ToUTF8(document.title());
    dst->AddStringAttribute(ui::AX_ATTR_DOC_TITLE,
                            UTF16ToUTF8(document.title()));
    dst->AddStringAttribute(ui::AX_ATTR_DOC_URL, document.url().spec());
    dst->AddStringAttribute(
        ui::AX_ATTR_DOC_MIMETYPE,
        document.isXHTMLDocument() ? "text/xhtml" : "text/html");
    dst->AddBoolAttribute(ui::AX_ATTR_DOC_LOADED, src.isLoaded());
    dst->AddFloatAttribute(ui::AX_ATTR_DOC_LOADING_PROGRESS,
                           src.estimatedLoadingProgress());

    const WebDocumentType& doctype = document.doctype();
    if (!doctype.isNull()) {
      dst->AddStringAttribute(ui::AX_ATTR_DOC_DOCTYPE,
                              UTF16ToUTF8(doctype.name()));
    }

    const gfx::Size& scroll_offset = document.frame()->scrollOffset();
    dst->AddIntAttribute(ui::AX_ATTR_SCROLL_X, scroll_offset.width());
    dst->AddIntAttribute(ui::AX_ATTR_SCROLL_Y, scroll_offset.height());

    const gfx::Size& min_offset = document.frame()->minimumScrollOffset();
    dst->AddIntAttribute(ui::AX_ATTR_SCROLL_X_MIN, min_offset.width());
    dst->AddIntAttribute(ui::AX_ATTR_SCROLL_Y_MIN, min_offset.height());

    const gfx::Size& max_offset = document.frame()->maximumScrollOffset();
    dst->AddIntAttribute(ui::AX_ATTR_SCROLL_X_MAX, max_offset.width());
    dst->AddIntAttribute(ui::AX_ATTR_SCROLL_Y_MAX, max_offset.height());
  }

  if (dst->role == ui::AX_ROLE_TABLE) {
    int column_count = src.columnCount();
    int row_count = src.rowCount();
    if (column_count > 0 && row_count > 0) {
      std::set<int32> unique_cell_id_set;
      std::vector<int32> cell_ids;
      std::vector<int32> unique_cell_ids;
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

  dst->AddStringAttribute(ui::AX_ATTR_NAME, name);

  // Add the ids of *indirect* children - those who are children of this node,
  // but whose parent is *not* this node. One example is a table
  // cell, which is a child of both a row and a column. Because the cell's
  // parent is the row, the row adds it as a child, and the column adds it
  // as an indirect child.
  int child_count = src.childCount();
  for (int i = 0; i < child_count; ++i) {
    WebAXObject child = src.childAt(i);
    std::vector<int32> indirect_child_ids;
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

  WebVector<WebAXObject> describedby;
  if (src.ariaDescribedby(describedby)) {
    AddIntListAttributeFromWebObjects(
        ui::AX_ATTR_DESCRIBEDBY_IDS, describedby, dst);
  }

  WebVector<WebAXObject> flowTo;
  if (src.ariaFlowTo(flowTo))
    AddIntListAttributeFromWebObjects(ui::AX_ATTR_FLOWTO_IDS, flowTo, dst);

  WebVector<WebAXObject> labelledby;
  if (src.ariaLabelledby(labelledby)) {
    AddIntListAttributeFromWebObjects(
        ui::AX_ATTR_LABELLEDBY_IDS, labelledby, dst);
  }

  WebVector<WebAXObject> owns;
  if (src.ariaOwns(owns))
    AddIntListAttributeFromWebObjects(ui::AX_ATTR_OWNS_IDS, owns, dst);
}

blink::WebDocument BlinkAXTreeSource::GetMainDocument() const {
  WebView* view = render_view_->GetWebView();
  WebLocalFrame* main_frame =
      view ? view->mainFrame()->toWebLocalFrame() : NULL;

  if (main_frame)
    return main_frame->document();

  return WebDocument();
}

}  // namespace content
