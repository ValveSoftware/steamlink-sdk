// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_autofill_util.h"

#include <map>
#include <set>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebElementCollection.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLabelElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebOptionElement.h"
#include "third_party/WebKit/public/web/WebSelectElement.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebLabelElement;
using blink::WebNode;
using blink::WebOptionElement;
using blink::WebSelectElement;
using blink::WebString;
using blink::WebVector;

namespace autofill {
namespace form_util {

const size_t kMaxParseableFields = 200;

namespace {

// A bit field mask for FillForm functions to not fill some fields.
enum FieldFilterMask {
  FILTER_NONE                      = 0,
  FILTER_DISABLED_ELEMENTS         = 1 << 0,
  FILTER_READONLY_ELEMENTS         = 1 << 1,
  // Filters non-focusable elements with the exception of select elements, which
  // are sometimes made non-focusable because they are present for accessibility
  // while a prettier, non-<select> dropdown is shown. We still want to autofill
  // the non-focusable <select>.
  FILTER_NON_FOCUSABLE_ELEMENTS    = 1 << 2,
  FILTER_ALL_NON_EDITABLE_ELEMENTS = FILTER_DISABLED_ELEMENTS |
                                     FILTER_READONLY_ELEMENTS |
                                     FILTER_NON_FOCUSABLE_ELEMENTS,
};

// If true, operations causing layout computation should be avoided. Set by
// ScopedLayoutPreventer.
bool g_prevent_layout = false;

void TruncateString(base::string16* str, size_t max_length) {
  if (str->length() > max_length)
    str->resize(max_length);
}

bool IsOptionElement(const WebElement& element) {
  CR_DEFINE_STATIC_LOCAL(WebString, kOption, ("option"));
  return element.hasHTMLTagName(kOption);
}

bool IsScriptElement(const WebElement& element) {
  CR_DEFINE_STATIC_LOCAL(WebString, kScript, ("script"));
  return element.hasHTMLTagName(kScript);
}

bool IsNoScriptElement(const WebElement& element) {
  CR_DEFINE_STATIC_LOCAL(WebString, kNoScript, ("noscript"));
  return element.hasHTMLTagName(kNoScript);
}

bool HasTagName(const WebNode& node, const blink::WebString& tag) {
  return node.isElementNode() && node.toConst<WebElement>().hasHTMLTagName(tag);
}

bool IsAutofillableElement(const WebFormControlElement& element) {
  const WebInputElement* input_element = toWebInputElement(&element);
  return IsAutofillableInputElement(input_element) ||
         IsSelectElement(element) ||
         IsTextAreaElement(element);
}

bool IsElementInControlElementSet(
    const WebElement& element,
    const std::vector<WebFormControlElement>& control_elements) {
  if (!element.isFormControlElement())
    return false;
  const WebFormControlElement form_control_element =
      element.toConst<WebFormControlElement>();
  return std::find(control_elements.begin(),
                   control_elements.end(),
                   form_control_element) != control_elements.end();
}

bool IsElementInsideFormOrFieldSet(const WebElement& element) {
  for (WebNode parent_node = element.parentNode();
       !parent_node.isNull();
       parent_node = parent_node.parentNode()) {
    if (!parent_node.isElementNode())
      continue;

    WebElement cur_element = parent_node.to<WebElement>();
    if (cur_element.hasHTMLTagName("form") ||
        cur_element.hasHTMLTagName("fieldset")) {
      return true;
    }
  }
  return false;
}

// Returns true if |node| is an element and it is a container type that
// InferLabelForElement() can traverse.
bool IsTraversableContainerElement(const WebNode& node) {
  if (!node.isElementNode())
    return false;

  const WebElement element = node.toConst<WebElement>();
  return element.hasHTMLTagName("dd") ||
          element.hasHTMLTagName("div") ||
          element.hasHTMLTagName("fieldset") ||
          element.hasHTMLTagName("li") ||
          element.hasHTMLTagName("td") ||
          element.hasHTMLTagName("table");
}

// Returns the colspan for a <td> / <th>. Defaults to 1.
size_t CalculateTableCellColumnSpan(const WebElement& element) {
  DCHECK(element.hasHTMLTagName("td") || element.hasHTMLTagName("th"));

  size_t span = 1;
  if (element.hasAttribute("colspan")) {
    base::string16 colspan = element.getAttribute("colspan");
    // Do not check return value to accept imperfect conversions.
    base::StringToSizeT(colspan, &span);
    // Handle overflow.
    if (span == std::numeric_limits<size_t>::max())
      span = 1;
    span = std::max(span, static_cast<size_t>(1));
  }

  return span;
}

// Appends |suffix| to |prefix| so that any intermediary whitespace is collapsed
// to a single space.  If |force_whitespace| is true, then the resulting string
// is guaranteed to have a space between |prefix| and |suffix|.  Otherwise, the
// result includes a space only if |prefix| has trailing whitespace or |suffix|
// has leading whitespace.
// A few examples:
//  * CombineAndCollapseWhitespace("foo", "bar", false)       -> "foobar"
//  * CombineAndCollapseWhitespace("foo", "bar", true)        -> "foo bar"
//  * CombineAndCollapseWhitespace("foo ", "bar", false)      -> "foo bar"
//  * CombineAndCollapseWhitespace("foo", " bar", false)      -> "foo bar"
//  * CombineAndCollapseWhitespace("foo", " bar", true)       -> "foo bar"
//  * CombineAndCollapseWhitespace("foo   ", "   bar", false) -> "foo bar"
//  * CombineAndCollapseWhitespace(" foo", "bar ", false)     -> " foobar "
//  * CombineAndCollapseWhitespace(" foo", "bar ", true)      -> " foo bar "
const base::string16 CombineAndCollapseWhitespace(
    const base::string16& prefix,
    const base::string16& suffix,
    bool force_whitespace) {
  base::string16 prefix_trimmed;
  base::TrimPositions prefix_trailing_whitespace =
      base::TrimWhitespace(prefix, base::TRIM_TRAILING, &prefix_trimmed);

  // Recursively compute the children's text.
  base::string16 suffix_trimmed;
  base::TrimPositions suffix_leading_whitespace =
      base::TrimWhitespace(suffix, base::TRIM_LEADING, &suffix_trimmed);

  if (prefix_trailing_whitespace || suffix_leading_whitespace ||
      force_whitespace) {
    return prefix_trimmed + base::ASCIIToUTF16(" ") + suffix_trimmed;
  } else {
    return prefix_trimmed + suffix_trimmed;
  }
}

// This is a helper function for the FindChildText() function (see below).
// Search depth is limited with the |depth| parameter.
// |divs_to_skip| is a list of <div> tags to ignore if encountered.
base::string16 FindChildTextInner(const WebNode& node,
                                  int depth,
                                  const std::set<WebNode>& divs_to_skip) {
  if (depth <= 0 || node.isNull())
    return base::string16();

  // Skip over comments.
  if (node.isCommentNode())
    return FindChildTextInner(node.nextSibling(), depth - 1, divs_to_skip);

  if (!node.isElementNode() && !node.isTextNode())
    return base::string16();

  // Ignore elements known not to contain inferable labels.
  if (node.isElementNode()) {
    const WebElement element = node.toConst<WebElement>();
    if (IsOptionElement(element) ||
        IsScriptElement(element) ||
        IsNoScriptElement(element) ||
        (element.isFormControlElement() &&
         IsAutofillableElement(element.toConst<WebFormControlElement>()))) {
      return base::string16();
    }

    if (element.hasHTMLTagName("div") && ContainsKey(divs_to_skip, node))
      return base::string16();
  }

  // Extract the text exactly at this node.
  base::string16 node_text = node.nodeValue();

  // Recursively compute the children's text.
  // Preserve inter-element whitespace separation.
  base::string16 child_text =
      FindChildTextInner(node.firstChild(), depth - 1, divs_to_skip);
  bool add_space = node.isTextNode() && node_text.empty();
  node_text = CombineAndCollapseWhitespace(node_text, child_text, add_space);

  // Recursively compute the siblings' text.
  // Again, preserve inter-element whitespace separation.
  base::string16 sibling_text =
      FindChildTextInner(node.nextSibling(), depth - 1, divs_to_skip);
  add_space = node.isTextNode() && node_text.empty();
  node_text = CombineAndCollapseWhitespace(node_text, sibling_text, add_space);

  return node_text;
}

// Same as FindChildText() below, but with a list of div nodes to skip.
// TODO(thestig): See if other FindChildText() callers can benefit from this.
base::string16 FindChildTextWithIgnoreList(
    const WebNode& node,
    const std::set<WebNode>& divs_to_skip) {
  if (node.isTextNode())
    return node.nodeValue();

  WebNode child = node.firstChild();

  const int kChildSearchDepth = 10;
  base::string16 node_text =
      FindChildTextInner(child, kChildSearchDepth, divs_to_skip);
  base::TrimWhitespace(node_text, base::TRIM_ALL, &node_text);
  return node_text;
}

// Returns the aggregated values of the descendants of |element| that are
// non-empty text nodes.  This is a faster alternative to |innerText()| for
// performance critical operations.  It does a full depth-first search so can be
// used when the structure is not directly known.  However, unlike with
// |innerText()|, the search depth and breadth are limited to a fixed threshold.
// Whitespace is trimmed from text accumulated at descendant nodes.
base::string16 FindChildText(const WebNode& node) {
  return FindChildTextWithIgnoreList(node, std::set<WebNode>());
}

// Shared function for InferLabelFromPrevious() and InferLabelFromNext().
base::string16 InferLabelFromSibling(const WebFormControlElement& element,
                                     bool forward) {
  base::string16 inferred_label;
  WebNode sibling = element;
  while (true) {
    sibling = forward ? sibling.nextSibling() : sibling.previousSibling();
    if (sibling.isNull())
      break;

    // Skip over comments.
    if (sibling.isCommentNode())
      continue;

    // Otherwise, only consider normal HTML elements and their contents.
    if (!sibling.isElementNode() && !sibling.isTextNode())
      break;

    // A label might be split across multiple "lightweight" nodes.
    // Coalesce any text contained in multiple consecutive
    //  (a) plain text nodes or
    //  (b) inline HTML elements that are essentially equivalent to text nodes.
    CR_DEFINE_STATIC_LOCAL(WebString, kBold, ("b"));
    CR_DEFINE_STATIC_LOCAL(WebString, kStrong, ("strong"));
    CR_DEFINE_STATIC_LOCAL(WebString, kSpan, ("span"));
    CR_DEFINE_STATIC_LOCAL(WebString, kFont, ("font"));
    if (sibling.isTextNode() ||
        HasTagName(sibling, kBold) || HasTagName(sibling, kStrong) ||
        HasTagName(sibling, kSpan) || HasTagName(sibling, kFont)) {
      base::string16 value = FindChildText(sibling);
      // A text node's value will be empty if it is for a line break.
      bool add_space = sibling.isTextNode() && value.empty();
      inferred_label =
          CombineAndCollapseWhitespace(value, inferred_label, add_space);
      continue;
    }

    // If we have identified a partial label and have reached a non-lightweight
    // element, consider the label to be complete.
    base::string16 trimmed_label;
    base::TrimWhitespace(inferred_label, base::TRIM_ALL, &trimmed_label);
    if (!trimmed_label.empty())
      break;

    // <img> and <br> tags often appear between the input element and its
    // label text, so skip over them.
    CR_DEFINE_STATIC_LOCAL(WebString, kImage, ("img"));
    CR_DEFINE_STATIC_LOCAL(WebString, kBreak, ("br"));
    if (HasTagName(sibling, kImage) || HasTagName(sibling, kBreak))
      continue;

    // We only expect <p> and <label> tags to contain the full label text.
    CR_DEFINE_STATIC_LOCAL(WebString, kPage, ("p"));
    CR_DEFINE_STATIC_LOCAL(WebString, kLabel, ("label"));
    if (HasTagName(sibling, kPage) || HasTagName(sibling, kLabel))
      inferred_label = FindChildText(sibling);

    break;
  }

  base::TrimWhitespace(inferred_label, base::TRIM_ALL, &inferred_label);
  return inferred_label;
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// a previous sibling of |element|,
// e.g. Some Text <input ...>
// or   Some <span>Text</span> <input ...>
// or   <p>Some Text</p><input ...>
// or   <label>Some Text</label> <input ...>
// or   Some Text <img><input ...>
// or   <b>Some Text</b><br/> <input ...>.
base::string16 InferLabelFromPrevious(const WebFormControlElement& element) {
  return InferLabelFromSibling(element, false /* forward? */);
}

// Same as InferLabelFromPrevious(), but in the other direction.
// Useful for cases like: <span><input type="checkbox">Label For Checkbox</span>
base::string16 InferLabelFromNext(const WebFormControlElement& element) {
  return InferLabelFromSibling(element, true /* forward? */);
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// the placeholder text. e.g. <input placeholder="foo">
base::string16 InferLabelFromPlaceholder(const WebFormControlElement& element) {
  CR_DEFINE_STATIC_LOCAL(WebString, kPlaceholder, ("placeholder"));
  if (element.hasAttribute(kPlaceholder))
    return element.getAttribute(kPlaceholder);

  return base::string16();
}

// Helper for |InferLabelForElement()| that infers a label, from
// the value attribute when it is present and user has not typed in (if
// element's value attribute is same as the element's value).
base::string16 InferLabelFromValueAttr(const WebFormControlElement& element) {
  CR_DEFINE_STATIC_LOCAL(WebString, kValue, ("value"));
  if (element.hasAttribute(kValue) && element.getAttribute(kValue) ==
      element.value()) {
    return element.getAttribute(kValue);
  }

  return base::string16();
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// enclosing list item,
// e.g. <li>Some Text<input ...><input ...><input ...></li>
base::string16 InferLabelFromListItem(const WebFormControlElement& element) {
  WebNode parent = element.parentNode();
  CR_DEFINE_STATIC_LOCAL(WebString, kListItem, ("li"));
  while (!parent.isNull() && parent.isElementNode() &&
         !parent.to<WebElement>().hasHTMLTagName(kListItem)) {
    parent = parent.parentNode();
  }

  if (!parent.isNull() && HasTagName(parent, kListItem))
    return FindChildText(parent);

  return base::string16();
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// enclosing label,
// e.g. <label>Some Text<input ...><input ...><input ...></label>
base::string16 InferLabelFromEnclosingLabel(
    const WebFormControlElement& element) {
  WebNode parent = element.parentNode();
  CR_DEFINE_STATIC_LOCAL(WebString, kLabel, ("label"));
  while (!parent.isNull() && parent.isElementNode() &&
         !parent.to<WebElement>().hasHTMLTagName(kLabel)) {
    parent = parent.parentNode();
  }

  if (!parent.isNull() && HasTagName(parent, kLabel))
    return FindChildText(parent);

  return base::string16();
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// surrounding table structure,
// e.g. <tr><td>Some Text</td><td><input ...></td></tr>
// or   <tr><th>Some Text</th><td><input ...></td></tr>
// or   <tr><td><b>Some Text</b></td><td><b><input ...></b></td></tr>
// or   <tr><th><b>Some Text</b></th><td><b><input ...></b></td></tr>
base::string16 InferLabelFromTableColumn(const WebFormControlElement& element) {
  CR_DEFINE_STATIC_LOCAL(WebString, kTableCell, ("td"));
  WebNode parent = element.parentNode();
  while (!parent.isNull() && parent.isElementNode() &&
         !parent.to<WebElement>().hasHTMLTagName(kTableCell)) {
    parent = parent.parentNode();
  }

  if (parent.isNull())
    return base::string16();

  // Check all previous siblings, skipping non-element nodes, until we find a
  // non-empty text block.
  base::string16 inferred_label;
  WebNode previous = parent.previousSibling();
  CR_DEFINE_STATIC_LOCAL(WebString, kTableHeader, ("th"));
  while (inferred_label.empty() && !previous.isNull()) {
    if (HasTagName(previous, kTableCell) || HasTagName(previous, kTableHeader))
      inferred_label = FindChildText(previous);

    previous = previous.previousSibling();
  }

  return inferred_label;
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// surrounding table structure,
//
// If there are multiple cells and the row with the input matches up with the
// previous row, then look for a specific cell within the previous row.
// e.g. <tr><td>Input 1 label</td><td>Input 2 label</td></tr>
//      <tr><td><input name="input 1"></td><td><input name="input2"></td></tr>
//
// Otherwise, just look in the entire previous row.
// e.g. <tr><td>Some Text</td></tr><tr><td><input ...></td></tr>
base::string16 InferLabelFromTableRow(const WebFormControlElement& element) {
  CR_DEFINE_STATIC_LOCAL(WebString, kTableCell, ("td"));
  base::string16 inferred_label;

  // First find the <td> that contains |element|.
  WebNode cell = element.parentNode();
  while (!cell.isNull()) {
    if (cell.isElementNode() &&
        cell.to<WebElement>().hasHTMLTagName(kTableCell)) {
      break;
    }
    cell = cell.parentNode();
  }

  // Not in a cell - bail out.
  if (cell.isNull())
    return inferred_label;

  // Count the cell holding |element|.
  size_t cell_count = CalculateTableCellColumnSpan(cell.to<WebElement>());
  size_t cell_position = 0;
  size_t cell_position_end = cell_count - 1;

  // Count cells to the left to figure out |element|'s cell's position.
  for (WebNode cell_it = cell.previousSibling();
       !cell_it.isNull();
       cell_it = cell_it.previousSibling()) {
    if (cell_it.isElementNode() &&
        cell_it.to<WebElement>().hasHTMLTagName(kTableCell)) {
      cell_position += CalculateTableCellColumnSpan(cell_it.to<WebElement>());
    }
  }

  // Count cells to the right.
  for (WebNode cell_it = cell.nextSibling();
       !cell_it.isNull();
       cell_it = cell_it.nextSibling()) {
    if (cell_it.isElementNode() &&
        cell_it.to<WebElement>().hasHTMLTagName(kTableCell)) {
      cell_count += CalculateTableCellColumnSpan(cell_it.to<WebElement>());
    }
  }

  // Combine left + right.
  cell_count += cell_position;
  cell_position_end += cell_position;

  // Find the current row.
  CR_DEFINE_STATIC_LOCAL(WebString, kTableRow, ("tr"));
  WebNode parent = element.parentNode();
  while (!parent.isNull() && parent.isElementNode() &&
         !parent.to<WebElement>().hasHTMLTagName(kTableRow)) {
    parent = parent.parentNode();
  }

  if (parent.isNull())
    return inferred_label;

  // Now find the previous row.
  WebNode row_it = parent.previousSibling();
  while (!row_it.isNull()) {
    if (row_it.isElementNode() &&
        row_it.to<WebElement>().hasHTMLTagName(kTableRow)) {
      break;
    }
    row_it = row_it.previousSibling();
  }

  // If there exists a previous row, check its cells and size. If they align
  // with the current row, infer the label from the cell above.
  if (!row_it.isNull()) {
    WebNode matching_cell;
    size_t prev_row_count = 0;
    WebNode prev_row_it = row_it.firstChild();
    CR_DEFINE_STATIC_LOCAL(WebString, kTableHeader, ("th"));
    while (!prev_row_it.isNull()) {
      if (prev_row_it.isElementNode()) {
        WebElement prev_row_element = prev_row_it.to<WebElement>();
        if (prev_row_element.hasHTMLTagName(kTableCell) ||
            prev_row_element.hasHTMLTagName(kTableHeader)) {
          size_t span = CalculateTableCellColumnSpan(prev_row_element);
          size_t prev_row_count_end = prev_row_count + span - 1;
          if (prev_row_count == cell_position &&
              prev_row_count_end == cell_position_end) {
            matching_cell = prev_row_it;
          }
          prev_row_count += span;
        }
      }
      prev_row_it = prev_row_it.nextSibling();
    }
    if ((cell_count == prev_row_count) && !matching_cell.isNull()) {
      inferred_label = FindChildText(matching_cell);
      if (!inferred_label.empty())
        return inferred_label;
    }
  }

  // If there is no previous row, or if the previous row and current row do not
  // align, check all previous siblings, skipping non-element nodes, until we
  // find a non-empty text block.
  WebNode previous = parent.previousSibling();
  while (inferred_label.empty() && !previous.isNull()) {
    if (HasTagName(previous, kTableRow))
      inferred_label = FindChildText(previous);

    previous = previous.previousSibling();
  }

  return inferred_label;
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// a surrounding div table,
// e.g. <div>Some Text<span><input ...></span></div>
// e.g. <div>Some Text</div><div><input ...></div>
//
// Because this is already traversing the <div> structure, if it finds a <label>
// sibling along the way, infer from that <label>.
base::string16 InferLabelFromDivTable(const WebFormControlElement& element) {
  WebNode node = element.parentNode();
  bool looking_for_parent = true;
  std::set<WebNode> divs_to_skip;

  // Search the sibling and parent <div>s until we find a candidate label.
  base::string16 inferred_label;
  CR_DEFINE_STATIC_LOCAL(WebString, kDiv, ("div"));
  CR_DEFINE_STATIC_LOCAL(WebString, kLabel, ("label"));
  while (inferred_label.empty() && !node.isNull()) {
    if (HasTagName(node, kDiv)) {
      if (looking_for_parent)
        inferred_label = FindChildTextWithIgnoreList(node, divs_to_skip);
      else
        inferred_label = FindChildText(node);

      // Avoid sibling DIVs that contain autofillable fields.
      if (!looking_for_parent && !inferred_label.empty()) {
        CR_DEFINE_STATIC_LOCAL(WebString, kSelector,
                               ("input, select, textarea"));
        WebElement result_element = node.querySelector(kSelector);
        if (!result_element.isNull()) {
          inferred_label.clear();
          divs_to_skip.insert(node);
        }
      }

      looking_for_parent = false;
    } else if (!looking_for_parent && HasTagName(node, kLabel)) {
      WebLabelElement label_element = node.to<WebLabelElement>();
      if (label_element.correspondingControl().isNull())
        inferred_label = FindChildText(node);
    } else if (looking_for_parent && IsTraversableContainerElement(node)) {
      // If the element is in a non-div container, its label most likely is too.
      break;
    }

    if (node.previousSibling().isNull()) {
      // If there are no more siblings, continue walking up the tree.
      looking_for_parent = true;
    }

    node = looking_for_parent ? node.parentNode() : node.previousSibling();
  }

  return inferred_label;
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// a surrounding definition list,
// e.g. <dl><dt>Some Text</dt><dd><input ...></dd></dl>
// e.g. <dl><dt><b>Some Text</b></dt><dd><b><input ...></b></dd></dl>
base::string16 InferLabelFromDefinitionList(
    const WebFormControlElement& element) {
  CR_DEFINE_STATIC_LOCAL(WebString, kDefinitionData, ("dd"));
  WebNode parent = element.parentNode();
  while (!parent.isNull() && parent.isElementNode() &&
         !parent.to<WebElement>().hasHTMLTagName(kDefinitionData))
    parent = parent.parentNode();

  if (parent.isNull() || !HasTagName(parent, kDefinitionData))
    return base::string16();

  // Skip by any intervening text nodes.
  WebNode previous = parent.previousSibling();
  while (!previous.isNull() && previous.isTextNode())
    previous = previous.previousSibling();

  CR_DEFINE_STATIC_LOCAL(WebString, kDefinitionTag, ("dt"));
  if (previous.isNull() || !HasTagName(previous, kDefinitionTag))
    return base::string16();

  return FindChildText(previous);
}

// Returns the element type for all ancestor nodes in CAPS, starting with the
// parent node.
std::vector<std::string> AncestorTagNames(
    const WebFormControlElement& element) {
  std::vector<std::string> tag_names;
  for (WebNode parent_node = element.parentNode();
       !parent_node.isNull();
       parent_node = parent_node.parentNode()) {
    if (!parent_node.isElementNode())
      continue;

    tag_names.push_back(parent_node.to<WebElement>().tagName().utf8());
  }
  return tag_names;
}

bool IsLabelValid(base::StringPiece16 inferred_label,
    const std::vector<base::char16>& stop_words) {
  // If |inferred_label| has any character other than those in |stop_words|.
  auto first_non_stop_word = std::find_if(inferred_label.begin(),
      inferred_label.end(), [&stop_words](base::char16 c) {
          return !ContainsValue(stop_words, c);
      });
  return first_non_stop_word != inferred_label.end();
}

// Infers corresponding label for |element| from surrounding context in the DOM,
// e.g. the contents of the preceding <p> tag or text element.
base::string16 InferLabelForElement(const WebFormControlElement& element,
    const std::vector<base::char16>& stop_words) {
  base::string16 inferred_label;

  if (IsCheckableElement(toWebInputElement(&element))) {
    inferred_label = InferLabelFromNext(element);
    if (IsLabelValid(inferred_label, stop_words))
      return inferred_label;
  }

  inferred_label = InferLabelFromPrevious(element);
  if (IsLabelValid(inferred_label, stop_words))
    return inferred_label;

  // If we didn't find a label, check for placeholder text.
  inferred_label = InferLabelFromPlaceholder(element);
  if (IsLabelValid(inferred_label, stop_words))
    return inferred_label;

  // For all other searches that involve traversing up the tree, the search
  // order is based on which tag is the closest ancestor to |element|.
  std::vector<std::string> tag_names = AncestorTagNames(element);
  std::set<std::string> seen_tag_names;
  for (const std::string& tag_name : tag_names) {
    if (ContainsKey(seen_tag_names, tag_name))
      continue;

    seen_tag_names.insert(tag_name);
    if (tag_name == "LABEL") {
      inferred_label = InferLabelFromEnclosingLabel(element);
    } else if (tag_name == "DIV") {
      inferred_label = InferLabelFromDivTable(element);
    } else if (tag_name == "TD") {
      inferred_label = InferLabelFromTableColumn(element);
      if (!IsLabelValid(inferred_label, stop_words))
        inferred_label = InferLabelFromTableRow(element);
    } else if (tag_name == "DD") {
      inferred_label = InferLabelFromDefinitionList(element);
    } else if (tag_name == "LI") {
      inferred_label = InferLabelFromListItem(element);
    } else if (tag_name == "FIELDSET") {
      break;
    }

    if (IsLabelValid(inferred_label, stop_words))
      return inferred_label;
  }

  // If we didn't find a label, check the value attr used as the placeholder.
  inferred_label = InferLabelFromValueAttr(element);
  if (IsLabelValid(inferred_label, stop_words))
    return inferred_label;
  else
    return base::string16();
}

// Fills |option_strings| with the values of the <option> elements present in
// |select_element|.
void GetOptionStringsFromElement(const WebSelectElement& select_element,
                                 std::vector<base::string16>* option_values,
                                 std::vector<base::string16>* option_contents) {
  DCHECK(!select_element.isNull());

  option_values->clear();
  option_contents->clear();
  WebVector<WebElement> list_items = select_element.listItems();

  // Constrain the maximum list length to prevent a malicious site from DOS'ing
  // the browser, without entirely breaking autocomplete for some extreme
  // legitimate sites: http://crbug.com/49332 and http://crbug.com/363094
  if (list_items.size() > kMaxListSize)
    return;

  option_values->reserve(list_items.size());
  option_contents->reserve(list_items.size());
  for (size_t i = 0; i < list_items.size(); ++i) {
    if (IsOptionElement(list_items[i])) {
      const WebOptionElement option = list_items[i].toConst<WebOptionElement>();
      option_values->push_back(option.value());
      option_contents->push_back(option.text());
    }
  }
}

// The callback type used by |ForEachMatchingFormField()|.
typedef void (*Callback)(const FormFieldData&,
                         bool, /* is_initiating_element */
                         blink::WebFormControlElement*);

void ForEachMatchingFormFieldCommon(
    std::vector<WebFormControlElement>* control_elements,
    const WebElement& initiating_element,
    const FormData& data,
    FieldFilterMask filters,
    bool force_override,
    const Callback& callback) {
  DCHECK(control_elements);
  if (control_elements->size() != data.fields.size()) {
    // This case should be reachable only for pathological websites and tests,
    // which add or remove form fields while the user is interacting with the
    // Autofill popup.
    return;
  }

  // It's possible that the site has injected fields into the form after the
  // page has loaded, so we can't assert that the size of the cached control
  // elements is equal to the size of the fields in |form|.  Fortunately, the
  // one case in the wild where this happens, paypal.com signup form, the fields
  // are appended to the end of the form and are not visible.
  for (size_t i = 0; i < control_elements->size(); ++i) {
    WebFormControlElement* element = &(*control_elements)[i];

    if (base::string16(element->nameForAutofill()) != data.fields[i].name) {
      // This case should be reachable only for pathological websites, which
      // rename form fields while the user is interacting with the Autofill
      // popup.  I (isherman) am not aware of any such websites, and so am
      // optimistically including a NOTREACHED().  If you ever trip this check,
      // please file a bug against me.
      NOTREACHED();
      continue;
    }

    bool is_initiating_element = (*element == initiating_element);

    // Only autofill empty fields (or those with the field's default value
    // attribute) and the field that initiated the filling, i.e. the field the
    // user is currently editing and interacting with.
    const WebInputElement* input_element = toWebInputElement(element);
    CR_DEFINE_STATIC_LOCAL(WebString, kValue, ("value"));
    CR_DEFINE_STATIC_LOCAL(WebString, kPlaceholder, ("placeholder"));
    if (!force_override && !is_initiating_element &&
        // A text field, with a non-empty value that is NOT the value of the
        // input field's "value" or "placeholder" attribute, is skipped.
        (IsAutofillableInputElement(input_element) ||
         IsTextAreaElement(*element)) &&
        !element->value().isEmpty() &&
        (!element->hasAttribute(kValue) ||
         element->getAttribute(kValue) != element->value()) &&
        (!element->hasAttribute(kPlaceholder) ||
         element->getAttribute(kPlaceholder) != element->value()))
      continue;

    DCHECK(!g_prevent_layout || !(filters & FILTER_NON_FOCUSABLE_ELEMENTS))
        << "The callsite of this code wanted to both prevent layout and check "
           "isFocusable. Pick one.";
    if (((filters & FILTER_DISABLED_ELEMENTS) && !element->isEnabled()) ||
        ((filters & FILTER_READONLY_ELEMENTS) && element->isReadOnly()) ||
        // See description for FILTER_NON_FOCUSABLE_ELEMENTS.
        ((filters & FILTER_NON_FOCUSABLE_ELEMENTS) && !element->isFocusable() &&
         !IsSelectElement(*element)))
      continue;

    callback(data.fields[i], is_initiating_element, element);
  }
}

// For each autofillable field in |data| that matches a field in the |form|,
// the |callback| is invoked with the corresponding |form| field data.
void ForEachMatchingFormField(const WebFormElement& form_element,
                              const WebElement& initiating_element,
                              const FormData& data,
                              FieldFilterMask filters,
                              bool force_override,
                              const Callback& callback) {
  std::vector<WebFormControlElement> control_elements =
      ExtractAutofillableElementsInForm(form_element);
  ForEachMatchingFormFieldCommon(&control_elements, initiating_element, data,
                                 filters, force_override, callback);
}

// For each autofillable field in |data| that matches a field in the set of
// unowned autofillable form fields, the |callback| is invoked with the
// corresponding |data| field.
void ForEachMatchingUnownedFormField(const WebElement& initiating_element,
                                     const FormData& data,
                                     FieldFilterMask filters,
                                     bool force_override,
                                     const Callback& callback) {
  if (initiating_element.isNull())
    return;

  std::vector<WebFormControlElement> control_elements =
      GetUnownedAutofillableFormFieldElements(
          initiating_element.document().all(), nullptr);
  if (!IsElementInControlElementSet(initiating_element, control_elements))
    return;

  ForEachMatchingFormFieldCommon(&control_elements, initiating_element, data,
                                 filters, force_override, callback);
}

// Sets the |field|'s value to the value in |data|.
// Also sets the "autofilled" attribute, causing the background to be yellow.
void FillFormField(const FormFieldData& data,
                   bool is_initiating_node,
                   blink::WebFormControlElement* field) {
  // Nothing to fill.
  if (data.value.empty())
    return;

  if (!data.is_autofilled)
    return;

  WebInputElement* input_element = toWebInputElement(field);
  if (IsCheckableElement(input_element)) {
    input_element->setChecked(IsChecked(data.check_status), true);
  } else {
    base::string16 value = data.value;
    if (IsTextInput(input_element) || IsMonthInput(input_element)) {
      // If the maxlength attribute contains a negative value, maxLength()
      // returns the default maxlength value.
      TruncateString(&value, input_element->maxLength());
    }
    field->setAutofillValue(value);
  }
  // Setting the form might trigger JavaScript, which is capable of
  // destroying the frame.
  if (!field->document().frame())
    return;

  field->setAutofilled(true);

  if (is_initiating_node &&
      ((IsTextInput(input_element) || IsMonthInput(input_element)) ||
       IsTextAreaElement(*field))) {
    int length = field->value().length();
    field->setSelectionRange(length, length);
    // Clear the current IME composition (the underline), if there is one.
    field->document().frame()->unmarkText();
  }
}

// Sets the |field|'s "suggested" (non JS visible) value to the value in |data|.
// Also sets the "autofilled" attribute, causing the background to be yellow.
void PreviewFormField(const FormFieldData& data,
                      bool is_initiating_node,
                      blink::WebFormControlElement* field) {
  // Nothing to preview.
  if (data.value.empty())
    return;

  if (!data.is_autofilled)
    return;

  // Preview input, textarea and select fields. For input fields, excludes
  // checkboxes and radio buttons, as there is no provision for
  // setSuggestedCheckedValue in WebInputElement.
  WebInputElement* input_element = toWebInputElement(field);
  if (IsTextInput(input_element) || IsMonthInput(input_element)) {
    // If the maxlength attribute contains a negative value, maxLength()
    // returns the default maxlength value.
    input_element->setSuggestedValue(
      data.value.substr(0, input_element->maxLength()));
    input_element->setAutofilled(true);
  } else if (IsTextAreaElement(*field) || IsSelectElement(*field)) {
    field->setSuggestedValue(data.value);
    field->setAutofilled(true);
  }

  if (is_initiating_node &&
      (IsTextInput(input_element) || IsTextAreaElement(*field))) {
    // Select the part of the text that the user didn't type.
    PreviewSuggestion(field->suggestedValue(), field->value(), field);
  }
}

// Extracts the fields from |control_elements| with |extract_mask| to
// |form_fields|. The extracted fields are also placed in |element_map|.
// |form_fields| and |element_map| should start out empty.
// |fields_extracted| should have as many elements as |control_elements|,
// initialized to false.
// Returns true if the number of fields extracted is within
// [1, kMaxParseableFields].
bool ExtractFieldsFromControlElements(
    const WebVector<WebFormControlElement>& control_elements,
    ExtractMask extract_mask,
    ScopedVector<FormFieldData>* form_fields,
    std::vector<bool>* fields_extracted,
    std::map<WebFormControlElement, FormFieldData*>* element_map) {
  DCHECK(form_fields->empty());
  DCHECK(element_map->empty());
  DCHECK_EQ(control_elements.size(), fields_extracted->size());

  for (size_t i = 0; i < control_elements.size(); ++i) {
    const WebFormControlElement& control_element = control_elements[i];

    if (!IsAutofillableElement(control_element))
      continue;

    // Create a new FormFieldData, fill it out and map it to the field's name.
    FormFieldData* form_field = new FormFieldData;
    WebFormControlElementToFormField(control_element, extract_mask, form_field);
    form_fields->push_back(form_field);
    (*element_map)[control_element] = form_field;
    (*fields_extracted)[i] = true;

    // To avoid overly expensive computation, we impose a maximum number of
    // allowable fields.
    if (form_fields->size() > kMaxParseableFields)
      return false;
  }

  // Succeeded if fields were extracted.
  return !form_fields->empty();
}

// For each label element, get the corresponding form control element, use the
// form control element's name as a key into the
// <WebFormControlElement, FormFieldData> map to find the previously created
// FormFieldData and set the FormFieldData's label to the
// label.firstChild().nodeValue() of the label element.
void MatchLabelsAndFields(
    const WebElementCollection& labels,
    std::map<WebFormControlElement, FormFieldData*>* element_map) {
  CR_DEFINE_STATIC_LOCAL(WebString, kFor, ("for"));
  CR_DEFINE_STATIC_LOCAL(WebString, kHidden, ("hidden"));

  for (WebElement item = labels.firstItem(); !item.isNull();
       item = labels.nextItem()) {
    WebLabelElement label = item.to<WebLabelElement>();
    WebElement control = label.correspondingControl();
    FormFieldData* field_data = nullptr;

    if (control.isNull()) {
      // Sometimes site authors will incorrectly specify the corresponding
      // field element's name rather than its id, so we compensate here.
      base::string16 element_name = label.getAttribute(kFor);
      if (element_name.empty())
        continue;
      // Look through the list for elements with this name. There can actually
      // be more than one. In this case, the label may not be particularly
      // useful, so just discard it.
      for (const auto& iter : *element_map) {
        if (iter.second->name == element_name) {
          if (field_data) {
            field_data = nullptr;
            break;
          } else {
            field_data = iter.second;
          }
        }
      }
    } else if (control.isFormControlElement()) {
      WebFormControlElement form_control = control.to<WebFormControlElement>();
      if (form_control.formControlType() == kHidden)
        continue;
      // Typical case: look up |field_data| in |element_map|.
      auto iter = element_map->find(form_control);
      if (iter == element_map->end())
        continue;
      field_data = iter->second;
    }

    if (!field_data)
      continue;

    base::string16 label_text = FindChildText(label);

    // Concatenate labels because some sites might have multiple label
    // candidates.
    if (!field_data->label.empty() && !label_text.empty())
      field_data->label += base::ASCIIToUTF16(" ");
    field_data->label += label_text;
  }
}

// Common function shared by WebFormElementToFormData() and
// UnownedFormElementsAndFieldSetsToFormData(). Either pass in:
// 1) |form_element| and an empty |fieldsets|.
// or
// 2) a NULL |form_element|.
//
// If |field| is not NULL, then |form_control_element| should be not NULL.
bool FormOrFieldsetsToFormData(
    const blink::WebFormElement* form_element,
    const blink::WebFormControlElement* form_control_element,
    const std::vector<blink::WebElement>& fieldsets,
    const WebVector<WebFormControlElement>& control_elements,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field) {
  CR_DEFINE_STATIC_LOCAL(WebString, kLabel, ("label"));

  if (form_element)
    DCHECK(fieldsets.empty());
  if (field)
    DCHECK(form_control_element);

  // A map from a FormFieldData's name to the FormFieldData itself.
  std::map<WebFormControlElement, FormFieldData*> element_map;

  // The extracted FormFields. We use pointers so we can store them in
  // |element_map|.
  ScopedVector<FormFieldData> form_fields;

  // A vector of bools that indicate whether each field in the form meets the
  // requirements and thus will be in the resulting |form|.
  std::vector<bool> fields_extracted(control_elements.size(), false);

  if (!ExtractFieldsFromControlElements(control_elements, extract_mask,
                                        &form_fields, &fields_extracted,
                                        &element_map)) {
    return false;
  }

  if (form_element) {
    // Loop through the label elements inside the form element.  For each label
    // element, get the corresponding form control element, use the form control
    // element's name as a key into the <name, FormFieldData> map to find the
    // previously created FormFieldData and set the FormFieldData's label to the
    // label.firstChild().nodeValue() of the label element.
    WebElementCollection labels =
        form_element->getElementsByHTMLTagName(kLabel);
    DCHECK(!labels.isNull());
    MatchLabelsAndFields(labels, &element_map);
  } else {
    // Same as the if block, but for all the labels in fieldsets.
    for (size_t i = 0; i < fieldsets.size(); ++i) {
      WebElementCollection labels =
          fieldsets[i].getElementsByHTMLTagName(kLabel);
      DCHECK(!labels.isNull());
      MatchLabelsAndFields(labels, &element_map);
    }
  }

  // List of characters a label can't be entirely made of (this list can grow).
  // Since the term |stop_words| is a known text processing concept we use here
  // it to refer to such characters. They are not to be confused with words.
  std::vector<base::char16> stop_words;
  stop_words.push_back(static_cast<base::char16>(' '));
  stop_words.push_back(static_cast<base::char16>('*'));
  stop_words.push_back(static_cast<base::char16>(':'));
  stop_words.push_back(static_cast<base::char16>('-'));
  stop_words.push_back(static_cast<base::char16>(L'\u2013'));
  stop_words.push_back(static_cast<base::char16>('('));
  stop_words.push_back(static_cast<base::char16>(')'));

  // Loop through the form control elements, extracting the label text from
  // the DOM.  We use the |fields_extracted| vector to make sure we assign the
  // extracted label to the correct field, as it's possible |form_fields| will
  // not contain all of the elements in |control_elements|.
  for (size_t i = 0, field_idx = 0;
       i < control_elements.size() && field_idx < form_fields.size(); ++i) {
    // This field didn't meet the requirements, so don't try to find a label
    // for it.
    if (!fields_extracted[i])
      continue;

    const WebFormControlElement& control_element = control_elements[i];
    if (form_fields[field_idx]->label.empty()) {
      form_fields[field_idx]->label = InferLabelForElement(control_element,
                                                           stop_words);
    }
    TruncateString(&form_fields[field_idx]->label, kMaxDataLength);

    if (field && *form_control_element == control_element)
      *field = *form_fields[field_idx];

    ++field_idx;
  }

  // Copy the created FormFields into the resulting FormData object.
  for (const auto& iter : form_fields)
    form->fields.push_back(*iter);
  return true;
}

bool UnownedFormElementsAndFieldSetsToFormData(
    const std::vector<blink::WebElement>& fieldsets,
    const std::vector<blink::WebFormControlElement>& control_elements,
    const blink::WebFormControlElement* element,
    const blink::WebDocument& document,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field) {
  form->origin = GetCanonicalOriginForDocument(document);
  form->is_form_tag = false;

  return FormOrFieldsetsToFormData(nullptr, element, fieldsets,
                                   control_elements, extract_mask, form, field);
}

GURL StripAuthAndParams(const GURL& gurl) {
  // We want to keep the path but strip any authentication data, as well as
  // query and ref portions of URL, for the form action and form origin.
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  return gurl.ReplaceComponents(rep);
}

}  // namespace

ScopedLayoutPreventer::ScopedLayoutPreventer() {
  DCHECK(!g_prevent_layout) << "Is any other instance of ScopedLayoutPreventer "
                               "alive in the same process?";
  g_prevent_layout = true;
}

ScopedLayoutPreventer::~ScopedLayoutPreventer() {
  DCHECK(g_prevent_layout) << "Is any other instance of ScopedLayoutPreventer "
                              "alive in the same process?";
  g_prevent_layout = false;
}

bool ExtractFormData(const WebFormElement& form_element, FormData* data) {
  return WebFormElementToFormData(
      form_element, WebFormControlElement(),
      static_cast<form_util::ExtractMask>(form_util::EXTRACT_VALUE |
                                          form_util::EXTRACT_OPTION_TEXT |
                                          form_util::EXTRACT_OPTIONS),
      data, NULL);
}

bool IsFormVisible(blink::WebFrame* frame,
                   const GURL& canonical_action,
                   const GURL& canonical_origin,
                   const FormData& form_data) {
  const GURL frame_origin = GetCanonicalOriginForDocument(frame->document());
  blink::WebVector<WebFormElement> forms;
  frame->document().forms(forms);

#if !defined(OS_ANDROID)
  // Omitting the action attribute would result in |canonical_origin| for
  // hierarchical schemes like http:, and in an empty URL for non-hierarchical
  // schemes like about: or data: etc.
  const bool action_is_empty = canonical_action.is_empty()
                               || canonical_action == canonical_origin;
#endif

  // Since empty or unspecified action fields are automatically set to page URL,
  // action field for forms cannot be used for comparing (all forms with
  // empty/unspecified actions have the same value). If an action field is set
  // to the page URL, this method checks ALL fields of the form instead (using
  // FormData.SameFormAs). This is also true if the action was set to the page
  // URL on purpose.
  for (const WebFormElement& form : forms) {
    if (!AreFormContentsVisible(form))
      continue;

    GURL iter_canonical_action = GetCanonicalActionForForm(form);
#if !defined(OS_ANDROID)
    bool form_action_is_empty = iter_canonical_action.is_empty() ||
                                iter_canonical_action == frame_origin;
    if (action_is_empty != form_action_is_empty)
      continue;

    if (action_is_empty) {  // Both actions are empty, compare all fields.
      FormData extracted_form_data;
      WebFormElementToFormData(form, WebFormControlElement(), EXTRACT_NONE,
                               &extracted_form_data, nullptr);
      if (form_data.SameFormAs(extracted_form_data)) {
        return true;  // Form still exists.
      }
    } else {  // Both actions are non-empty, compare actions only.
      if (canonical_action == iter_canonical_action) {
        return true;  // Form still exists.
      }
    }
#else  // OS_ANDROID
    if (canonical_action == iter_canonical_action) {
      return true;  // Form still exists.
    }
#endif
  }

  return false;
}

bool IsSomeControlElementVisible(
    const WebVector<WebFormControlElement>& control_elements) {
  for (const WebFormControlElement& control_element : control_elements) {
    if (IsWebNodeVisible(control_element))
      return true;
  }
  return false;
}

bool AreFormContentsVisible(const WebFormElement& form) {
  WebVector<WebFormControlElement> control_elements;
  form.getFormControlElements(control_elements);
  return IsSomeControlElementVisible(control_elements);
}

GURL GetCanonicalActionForForm(const WebFormElement& form) {
  WebString action = form.action();
  if (action.isNull())
    action = WebString("");  // missing 'action' attribute implies current URL.
  GURL full_action(form.document().completeURL(action));
  return StripAuthAndParams(full_action);
}

GURL GetCanonicalOriginForDocument(const WebDocument& document) {
  GURL full_origin(document.url());
  return StripAuthAndParams(full_origin);
}

bool IsMonthInput(const WebInputElement* element) {
  CR_DEFINE_STATIC_LOCAL(WebString, kMonth, ("month"));
  return element && !element->isNull() && element->formControlType() == kMonth;
}

// All text fields, including password fields, should be extracted.
bool IsTextInput(const WebInputElement* element) {
  return element && !element->isNull() && element->isTextField();
}

bool IsSelectElement(const WebFormControlElement& element) {
  // Static for improved performance.
  CR_DEFINE_STATIC_LOCAL(WebString, kSelectOne, ("select-one"));
  return !element.isNull() && element.formControlType() == kSelectOne;
}

bool IsTextAreaElement(const WebFormControlElement& element) {
  // Static for improved performance.
  CR_DEFINE_STATIC_LOCAL(WebString, kTextArea, ("textarea"));
  return !element.isNull() && element.formControlType() == kTextArea;
}

bool IsCheckableElement(const WebInputElement* element) {
  if (!element || element->isNull())
    return false;

  return element->isCheckbox() || element->isRadioButton();
}

bool IsAutofillableInputElement(const WebInputElement* element) {
  return IsTextInput(element) ||
         IsMonthInput(element) ||
         IsCheckableElement(element);
}

const base::string16 GetFormIdentifier(const WebFormElement& form) {
  base::string16 identifier = form.name();
  CR_DEFINE_STATIC_LOCAL(WebString, kId, ("id"));
  if (identifier.empty())
    identifier = form.getAttribute(kId);

  return identifier;
}

bool IsWebNodeVisible(const blink::WebNode& node) {
  // TODO(esprehn): This code doesn't really check if the node is visible, just
  // if the node takes up space in the layout. Does it want to check opacity,
  // transform, and visibility too?
  if (!node.isElementNode())
    return false;
  const WebElement element = node.toConst<WebElement>();
  return element.hasNonEmptyLayoutSize();
}

std::vector<blink::WebFormControlElement> ExtractAutofillableElementsFromSet(
    const WebVector<WebFormControlElement>& control_elements) {
  std::vector<blink::WebFormControlElement> autofillable_elements;
  for (size_t i = 0; i < control_elements.size(); ++i) {
    WebFormControlElement element = control_elements[i];
    if (!IsAutofillableElement(element))
      continue;

    autofillable_elements.push_back(element);
  }
  return autofillable_elements;
}

std::vector<WebFormControlElement> ExtractAutofillableElementsInForm(
    const WebFormElement& form_element) {
  WebVector<WebFormControlElement> control_elements;
  form_element.getFormControlElements(control_elements);

  return ExtractAutofillableElementsFromSet(control_elements);
}

void WebFormControlElementToFormField(const WebFormControlElement& element,
                                      ExtractMask extract_mask,
                                      FormFieldData* field) {
  DCHECK(field);
  DCHECK(!element.isNull());
  CR_DEFINE_STATIC_LOCAL(WebString, kAutocomplete, ("autocomplete"));
  CR_DEFINE_STATIC_LOCAL(WebString, kRole, ("role"));
  CR_DEFINE_STATIC_LOCAL(WebString, kPlaceholder, ("placeholder"));
  CR_DEFINE_STATIC_LOCAL(WebString, kClass, ("class"));

  // The label is not officially part of a WebFormControlElement; however, the
  // labels for all form control elements are scraped from the DOM and set in
  // WebFormElementToFormData.
  field->name = element.nameForAutofill();
  field->form_control_type = element.formControlType().utf8();
  field->autocomplete_attribute = element.getAttribute(kAutocomplete).utf8();
  if (field->autocomplete_attribute.size() > kMaxDataLength) {
    // Discard overly long attribute values to avoid DOS-ing the browser
    // process.  However, send over a default string to indicate that the
    // attribute was present.
    field->autocomplete_attribute = "x-max-data-length-exceeded";
  }
  if (base::LowerCaseEqualsASCII(
          base::StringPiece16(element.getAttribute(kRole)), "presentation"))
    field->role = FormFieldData::ROLE_ATTRIBUTE_PRESENTATION;

  field->placeholder = element.getAttribute(kPlaceholder);
  if (element.hasAttribute(kClass))
    field->css_classes = element.getAttribute(kClass);

  if (!IsAutofillableElement(element))
    return;

  const WebInputElement* input_element = toWebInputElement(&element);
  if (IsAutofillableInputElement(input_element) ||
      IsTextAreaElement(element) ||
      IsSelectElement(element)) {
    field->is_autofilled = element.isAutofilled();
    if (!g_prevent_layout)
      field->is_focusable = element.isFocusable();
    field->should_autocomplete = element.autoComplete();
    field->text_direction = element.directionForFormData() ==
        "rtl" ? base::i18n::RIGHT_TO_LEFT : base::i18n::LEFT_TO_RIGHT;
  }

  if (IsAutofillableInputElement(input_element)) {
    if (IsTextInput(input_element))
      field->max_length = input_element->maxLength();

    SetCheckStatus(field, IsCheckableElement(input_element),
                   input_element->isChecked());
  } else if (IsTextAreaElement(element)) {
    // Nothing more to do in this case.
  } else if (extract_mask & EXTRACT_OPTIONS) {
    // Set option strings on the field if available.
    DCHECK(IsSelectElement(element));
    const WebSelectElement select_element = element.toConst<WebSelectElement>();
    GetOptionStringsFromElement(select_element,
                                &field->option_values,
                                &field->option_contents);
  }

  if (!(extract_mask & EXTRACT_VALUE))
    return;

  base::string16 value = element.value();

  if (IsSelectElement(element) && (extract_mask & EXTRACT_OPTION_TEXT)) {
    const WebSelectElement select_element = element.toConst<WebSelectElement>();
    // Convert the |select_element| value to text if requested.
    WebVector<WebElement> list_items = select_element.listItems();
    for (size_t i = 0; i < list_items.size(); ++i) {
      if (IsOptionElement(list_items[i])) {
        const WebOptionElement option_element =
            list_items[i].toConst<WebOptionElement>();
        if (option_element.value() == value) {
          value = option_element.text();
          break;
        }
      }
    }
  }

  // Constrain the maximum data length to prevent a malicious site from DOS'ing
  // the browser: http://crbug.com/49332
  TruncateString(&value, kMaxDataLength);

  field->value = value;
}

bool WebFormElementToFormData(
    const blink::WebFormElement& form_element,
    const blink::WebFormControlElement& form_control_element,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field) {
  const WebFrame* frame = form_element.document().frame();
  if (!frame)
    return false;

  form->name = GetFormIdentifier(form_element);
  form->origin = GetCanonicalOriginForDocument(frame->document());
  form->action = frame->document().completeURL(form_element.action());

  // If the completed URL is not valid, just use the action we get from
  // WebKit.
  if (!form->action.is_valid())
    form->action = GURL(blink::WebStringToGURL(form_element.action()));

  WebVector<WebFormControlElement> control_elements;
  form_element.getFormControlElements(control_elements);

  std::vector<blink::WebElement> dummy_fieldset;
  return FormOrFieldsetsToFormData(&form_element, &form_control_element,
                                   dummy_fieldset, control_elements,
                                   extract_mask, form, field);
}

std::vector<WebFormControlElement> GetUnownedFormFieldElements(
    const WebElementCollection& elements,
    std::vector<WebElement>* fieldsets) {
  std::vector<WebFormControlElement> unowned_fieldset_children;
  for (WebElement element = elements.firstItem();
       !element.isNull();
       element = elements.nextItem()) {
    if (element.isFormControlElement()) {
      WebFormControlElement control = element.to<WebFormControlElement>();
      if (control.form().isNull())
        unowned_fieldset_children.push_back(control);
    }

    if (fieldsets && element.hasHTMLTagName("fieldset") &&
        !IsElementInsideFormOrFieldSet(element)) {
      fieldsets->push_back(element);
    }
  }
  return unowned_fieldset_children;
}

std::vector<WebFormControlElement> GetUnownedAutofillableFormFieldElements(
    const WebElementCollection& elements,
    std::vector<WebElement>* fieldsets) {
  return ExtractAutofillableElementsFromSet(
      GetUnownedFormFieldElements(elements, fieldsets));
}

bool UnownedCheckoutFormElementsAndFieldSetsToFormData(
    const std::vector<blink::WebElement>& fieldsets,
    const std::vector<blink::WebFormControlElement>& control_elements,
    const blink::WebFormControlElement* element,
    const blink::WebDocument& document,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field) {
  // Only attempt formless Autofill on checkout flows. This avoids the many
  // false positives found on the non-checkout web. See
  // http://crbug.com/462375.
  WebElement html_element = document.documentElement();

  // For now this restriction only applies to English-language pages, because
  // the keywords are not translated. Note that an empty "lang" attribute
  // counts as English.
  std::string lang;
  if (!html_element.isNull())
    lang = html_element.getAttribute("lang").utf8();
  if (!lang.empty() &&
      !base::StartsWith(lang, "en", base::CompareCase::INSENSITIVE_ASCII)) {
    return UnownedFormElementsAndFieldSetsToFormData(
        fieldsets, control_elements, element, document, extract_mask, form,
        field);
  }

  // A potential problem is that this only checks document.title(), but should
  // actually check the main frame's title. Thus it may make bad decisions for
  // iframes.
  base::string16 title(base::ToLowerASCII(base::string16(document.title())));

  // Don't check the path for url's without a standard format path component,
  // such as data:.
  std::string path;
  GURL url(document.url());
  if (url.IsStandard())
    path = base::ToLowerASCII(url.path());

  const char* const kKeywords[] = {
    "payment",
    "checkout",
    "address",
    "delivery",
    "shipping",
    "wallet"
  };

  for (const auto& keyword : kKeywords) {
    // Compare char16 elements of |title| with char elements of |keyword| using
    // operator==.
    auto title_pos = std::search(title.begin(), title.end(),
                                 keyword, keyword + strlen(keyword));
    if (title_pos != title.end() ||
        path.find(keyword) != std::string::npos) {
      form->is_formless_checkout = true;
      // Found a keyword: treat this as an unowned form.
      return UnownedFormElementsAndFieldSetsToFormData(
          fieldsets, control_elements, element, document, extract_mask, form,
          field);
    }
  }

  // Since it's not a checkout flow, only add fields that have a non-"off"
  // autocomplete attribute to the formless autofill.
  CR_DEFINE_STATIC_LOCAL(WebString, kOffAttribute, ("off"));
  std::vector<WebFormControlElement> elements_with_autocomplete;
  for (const WebFormControlElement& element : control_elements) {
    blink::WebString autocomplete = element.getAttribute("autocomplete");
    if (autocomplete.length() && autocomplete != kOffAttribute)
      elements_with_autocomplete.push_back(element);
  }

  if (elements_with_autocomplete.empty())
    return false;

  return UnownedFormElementsAndFieldSetsToFormData(
      fieldsets, elements_with_autocomplete, element, document, extract_mask,
      form, field);
}

bool UnownedPasswordFormElementsAndFieldSetsToFormData(
    const std::vector<blink::WebElement>& fieldsets,
    const std::vector<blink::WebFormControlElement>& control_elements,
    const blink::WebFormControlElement* element,
    const blink::WebDocument& document,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field) {
  return UnownedFormElementsAndFieldSetsToFormData(
      fieldsets, control_elements, element, document, extract_mask, form,
      field);
}


bool FindFormAndFieldForFormControlElement(const WebFormControlElement& element,
                                           FormData* form,
                                           FormFieldData* field) {
  if (!IsAutofillableElement(element))
    return false;

  ExtractMask extract_mask =
      static_cast<ExtractMask>(EXTRACT_VALUE | EXTRACT_OPTIONS);
  const WebFormElement form_element = element.form();
  if (form_element.isNull()) {
    // No associated form, try the synthetic form for unowned form elements.
    WebDocument document = element.document();
    std::vector<WebElement> fieldsets;
    std::vector<WebFormControlElement> control_elements =
        GetUnownedAutofillableFormFieldElements(document.all(), &fieldsets);
    return UnownedCheckoutFormElementsAndFieldSetsToFormData(
        fieldsets, control_elements, &element, document, extract_mask,
        form, field);
  }

  return WebFormElementToFormData(form_element,
                                  element,
                                  extract_mask,
                                  form,
                                  field);
}

void FillForm(const FormData& form, const WebFormControlElement& element) {
  WebFormElement form_element = element.form();
  if (form_element.isNull()) {
    ForEachMatchingUnownedFormField(element,
                                    form,
                                    FILTER_ALL_NON_EDITABLE_ELEMENTS,
                                    false, /* dont force override */
                                    &FillFormField);
    return;
  }

  ForEachMatchingFormField(form_element,
                           element,
                           form,
                           FILTER_ALL_NON_EDITABLE_ELEMENTS,
                           false, /* dont force override */
                           &FillFormField);
}

void FillFormIncludingNonFocusableElements(const FormData& form_data,
                                           const WebFormElement& form_element) {
  if (form_element.isNull()) {
    NOTREACHED();
    return;
  }

  FieldFilterMask filter_mask = static_cast<FieldFilterMask>(
      FILTER_DISABLED_ELEMENTS | FILTER_READONLY_ELEMENTS);
  ForEachMatchingFormField(form_element,
                           WebInputElement(),
                           form_data,
                           filter_mask,
                           true, /* force override */
                           &FillFormField);
}

void PreviewForm(const FormData& form, const WebFormControlElement& element) {
  WebFormElement form_element = element.form();
  if (form_element.isNull()) {
    ForEachMatchingUnownedFormField(element,
                                    form,
                                    FILTER_ALL_NON_EDITABLE_ELEMENTS,
                                    false, /* dont force override */
                                    &PreviewFormField);
    return;
  }

  ForEachMatchingFormField(form_element,
                           element,
                           form,
                           FILTER_ALL_NON_EDITABLE_ELEMENTS,
                           false, /* dont force override */
                           &PreviewFormField);
}

bool ClearPreviewedFormWithElement(const WebFormControlElement& element,
                                   bool was_autofilled) {
  WebFormElement form_element = element.form();
  std::vector<WebFormControlElement> control_elements;
  if (form_element.isNull()) {
    control_elements = GetUnownedAutofillableFormFieldElements(
        element.document().all(), nullptr);
    if (!IsElementInControlElementSet(element, control_elements))
      return false;
  } else {
    control_elements = ExtractAutofillableElementsInForm(form_element);
  }

  for (size_t i = 0; i < control_elements.size(); ++i) {
    // There might be unrelated elements in this form which have already been
    // auto-filled.  For example, the user might have already filled the address
    // part of a form and now be dealing with the credit card section.  We only
    // want to reset the auto-filled status for fields that were previewed.
    WebFormControlElement control_element = control_elements[i];

    // Only text input, textarea and select elements can be previewed.
    WebInputElement* input_element = toWebInputElement(&control_element);
    if (!IsTextInput(input_element) &&
        !IsMonthInput(input_element) &&
        !IsTextAreaElement(control_element) &&
        !IsSelectElement(control_element))
      continue;

    // If the element is not auto-filled, we did not preview it,
    // so there is nothing to reset.
    if (!control_element.isAutofilled())
      continue;

    if ((IsTextInput(input_element) ||
         IsMonthInput(input_element) ||
         IsTextAreaElement(control_element) ||
         IsSelectElement(control_element)) &&
        control_element.suggestedValue().isEmpty())
      continue;

    // Clear the suggested value. For the initiating node, also restore the
    // original value.
    if (IsTextInput(input_element) || IsMonthInput(input_element) ||
        IsTextAreaElement(control_element)) {
      control_element.setSuggestedValue(WebString());
      bool is_initiating_node = (element == control_element);
      if (is_initiating_node) {
        control_element.setAutofilled(was_autofilled);
        // Clearing the suggested value in the focused node (above) can cause
        // selection to be lost. We force selection range to restore the text
        // cursor.
        int length = control_element.value().length();
        control_element.setSelectionRange(length, length);
      } else {
        control_element.setAutofilled(false);
      }
    } else if (IsSelectElement(control_element)) {
      control_element.setSuggestedValue(WebString());
      control_element.setAutofilled(false);
    }
  }

  return true;
}

bool IsWebpageEmpty(const blink::WebFrame* frame) {
  blink::WebDocument document = frame->document();

  return IsWebElementEmpty(document.head()) &&
         IsWebElementEmpty(document.body());
}

bool IsWebElementEmpty(const blink::WebElement& root) {
  CR_DEFINE_STATIC_LOCAL(WebString, kScript, ("script"));
  CR_DEFINE_STATIC_LOCAL(WebString, kMeta, ("meta"));
  CR_DEFINE_STATIC_LOCAL(WebString, kTitle, ("title"));

  if (root.isNull())
    return true;

  for (WebNode child = root.firstChild();
      !child.isNull();
      child = child.nextSibling()) {
    if (child.isTextNode() &&
        !base::ContainsOnlyChars(child.nodeValue().utf8(),
                                 base::kWhitespaceASCII))
      return false;

    if (!child.isElementNode())
      continue;

    WebElement element = child.to<WebElement>();
    if (!element.hasHTMLTagName(kScript) &&
        !element.hasHTMLTagName(kMeta) &&
        !element.hasHTMLTagName(kTitle))
      return false;
  }
  return true;
}

void PreviewSuggestion(const base::string16& suggestion,
                       const base::string16& user_input,
                       blink::WebFormControlElement* input_element) {
  size_t selection_start = user_input.length();
  if (IsFeatureSubstringMatchEnabled()) {
    size_t offset = GetTextSelectionStart(suggestion, user_input, false);
    // Zero selection start is for password manager, which can show usernames
    // that do not begin with the user input value.
    selection_start = (offset == base::string16::npos) ? 0 : offset;
  }

  input_element->setSelectionRange(selection_start, suggestion.length());
}

}  // namespace form_util
}  // namespace autofill
