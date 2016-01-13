// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace content {
namespace {
const int kIndentSpaces = 4;
const char* kSkipString = "@NO_DUMP";
const char* kChildrenDictAttr = "children";
}

AccessibilityTreeFormatter::AccessibilityTreeFormatter(
    BrowserAccessibility* root)
    : root_(root) {
  Initialize();
}

// static
AccessibilityTreeFormatter* AccessibilityTreeFormatter::Create(
    RenderViewHost* rvh) {
  RenderWidgetHostViewBase* host_view = static_cast<RenderWidgetHostViewBase*>(
      WebContents::FromRenderViewHost(rvh)->GetRenderWidgetHostView());

  BrowserAccessibilityManager* manager =
      host_view->GetBrowserAccessibilityManager();
  if (!manager)
    return NULL;

  BrowserAccessibility* root = manager->GetRoot();
  return new AccessibilityTreeFormatter(root);
}


AccessibilityTreeFormatter::~AccessibilityTreeFormatter() {
}

scoped_ptr<base::DictionaryValue>
AccessibilityTreeFormatter::BuildAccessibilityTree() {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  RecursiveBuildAccessibilityTree(*root_, dict.get());
  return dict.Pass();
}

void AccessibilityTreeFormatter::FormatAccessibilityTree(
    base::string16* contents) {
  scoped_ptr<base::DictionaryValue> dict = BuildAccessibilityTree();
  RecursiveFormatAccessibilityTree(*(dict.get()), contents);
}

void AccessibilityTreeFormatter::RecursiveBuildAccessibilityTree(
    const BrowserAccessibility& node, base::DictionaryValue* dict) {
  AddProperties(node, dict);

  base::ListValue* children = new base::ListValue;
  dict->Set(kChildrenDictAttr, children);

  for (size_t i = 0; i < node.PlatformChildCount(); ++i) {
    BrowserAccessibility* child_node = node.InternalGetChild(i);
    base::DictionaryValue* child_dict = new base::DictionaryValue;
    children->Append(child_dict);
    RecursiveBuildAccessibilityTree(*child_node, child_dict);
  }
}

void AccessibilityTreeFormatter::RecursiveFormatAccessibilityTree(
    const base::DictionaryValue& dict, base::string16* contents, int depth) {
  base::string16 line =
      ToString(dict, base::string16(depth * kIndentSpaces, ' '));
  if (line.find(base::ASCIIToUTF16(kSkipString)) != base::string16::npos)
    return;

  *contents += line;
  const base::ListValue* children;
  dict.GetList(kChildrenDictAttr, &children);
  const base::DictionaryValue* child_dict;
  for (size_t i = 0; i < children->GetSize(); i++) {
    children->GetDictionary(i, &child_dict);
    RecursiveFormatAccessibilityTree(*child_dict, contents, depth + 1);
  }
}

#if (!defined(OS_WIN) && !defined(OS_MACOSX) && !defined(OS_ANDROID))
void AccessibilityTreeFormatter::AddProperties(const BrowserAccessibility& node,
                                               base::DictionaryValue* dict) {
  dict->SetInteger("id", node.GetId());
}

base::string16 AccessibilityTreeFormatter::ToString(
    const base::DictionaryValue& node,
    const base::string16& indent) {
  int id_value;
  node.GetInteger("id", &id_value);
  return indent + base::IntToString16(id_value) +
       base::ASCIIToUTF16("\n");
}

void AccessibilityTreeFormatter::Initialize() {}

// static
const base::FilePath::StringType
AccessibilityTreeFormatter::GetActualFileSuffix() {
  return base::FilePath::StringType();
}

// static
const base::FilePath::StringType
AccessibilityTreeFormatter::GetExpectedFileSuffix() {
  return base::FilePath::StringType();
}

// static
const std::string AccessibilityTreeFormatter::GetAllowEmptyString() {
  return std::string();
}

// static
const std::string AccessibilityTreeFormatter::GetAllowString() {
  return std::string();
}

// static
const std::string AccessibilityTreeFormatter::GetDenyString() {
  return std::string();
}
#endif

void AccessibilityTreeFormatter::SetFilters(
    const std::vector<Filter>& filters) {
  filters_ = filters;
}

bool AccessibilityTreeFormatter::MatchesFilters(
    const base::string16& text, bool default_result) const {
  std::vector<Filter>::const_iterator iter = filters_.begin();
  bool allow = default_result;
  for (iter = filters_.begin(); iter != filters_.end(); ++iter) {
    if (MatchPattern(text, iter->match_str)) {
      if (iter->type == Filter::ALLOW_EMPTY)
        allow = true;
      else if (iter->type == Filter::ALLOW)
        allow = (!MatchPattern(text, base::UTF8ToUTF16("*=''")));
      else
        allow = false;
    }
  }
  return allow;
}

base::string16 AccessibilityTreeFormatter::FormatCoordinates(
    const char* name, const char* x_name, const char* y_name,
    const base::DictionaryValue& value) {
  int x, y;
  value.GetInteger(x_name, &x);
  value.GetInteger(y_name, &y);
  std::string xy_str(base::StringPrintf("%s=(%d, %d)", name, x, y));

  return base::UTF8ToUTF16(xy_str);
}

void AccessibilityTreeFormatter::WriteAttribute(
    bool include_by_default, const std::string& attr, base::string16* line) {
  WriteAttribute(include_by_default, base::UTF8ToUTF16(attr), line);
}

void AccessibilityTreeFormatter::WriteAttribute(
    bool include_by_default, const base::string16& attr, base::string16* line) {
  if (attr.empty())
    return;
  if (!MatchesFilters(attr, include_by_default))
    return;
  if (!line->empty())
    *line += base::ASCIIToUTF16(" ");
  *line += attr;
}

}  // namespace content
