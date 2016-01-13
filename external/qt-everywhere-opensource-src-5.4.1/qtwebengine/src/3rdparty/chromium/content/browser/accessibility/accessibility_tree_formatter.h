// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/common/content_export.h"

namespace content {

class RenderViewHost;

// A utility class for formatting platform-specific accessibility information,
// for use in testing, debugging, and developer tools.
// This is extended by a subclass for each platform where accessibility is
// implemented.
class CONTENT_EXPORT AccessibilityTreeFormatter {
 public:
  explicit AccessibilityTreeFormatter(BrowserAccessibility* root);
  virtual ~AccessibilityTreeFormatter();

  static AccessibilityTreeFormatter* Create(RenderViewHost* rvh);

  // Populates the given DictionaryValue with the accessibility tree.
  // The dictionary contains a key/value pair for each attribute of the node,
  // plus a "children" attribute containing a list of all child nodes.
  // {
  //   "AXName": "node",  /* actual attributes will vary by platform */
  //   "position": {  /* some attributes may be dictionaries */
  //     "x": 0,
  //     "y": 0
  //   },
  //   /* ... more attributes of |node| */
  //   "children": [ {  /* list of children created recursively */
  //     "AXName": "child node 1",
  //     /* ... more attributes */
  //     "children": [ ]
  //   }, {
  //     "AXName": "child name 2",
  //     /* ... more attributes */
  //     "children": [ ]
  //   } ]
  // }
  scoped_ptr<base::DictionaryValue> BuildAccessibilityTree();

  // Dumps a BrowserAccessibility tree into a string.
  void FormatAccessibilityTree(base::string16* contents);

  // A single filter specification. See GetAllowString() and GetDenyString()
  // for more information.
  struct Filter {
    enum Type {
      ALLOW,
      ALLOW_EMPTY,
      DENY
    };
    base::string16 match_str;
    Type type;

    Filter(base::string16 match_str, Type type)
        : match_str(match_str), type(type) {}
  };

  // Set regular expression filters that apply to each component of every
  // line before it's output.
  void SetFilters(const std::vector<Filter>& filters);

  // Suffix of the expectation file corresponding to html file.
  // Example:
  // HTML test:      test-file.html
  // Expected:       test-file-expected-mac.txt.
  // Auto-generated: test-file-actual-mac.txt
  static const base::FilePath::StringType GetActualFileSuffix();
  static const base::FilePath::StringType GetExpectedFileSuffix();

  // A platform-specific string that indicates a given line in a file
  // is an allow-empty, allow or deny filter. Example:
  // Mac values:
  //   GetAllowEmptyString() -> "@MAC-ALLOW-EMPTY:"
  //   GetAllowString() -> "@MAC-ALLOW:"
  //   GetDenyString() -> "@MAC-DENY:"
  // Example html:
  // <!--
  // @MAC-ALLOW-EMPTY:description*
  // @MAC-ALLOW:roleDescription*
  // @MAC-DENY:subrole*
  // -->
  // <p>Text</p>
  static const std::string GetAllowEmptyString();
  static const std::string GetAllowString();
  static const std::string GetDenyString();

 protected:
  void RecursiveFormatAccessibilityTree(const BrowserAccessibility& node,
                                        base::string16* contents,
                                        int indent);
  void RecursiveBuildAccessibilityTree(const BrowserAccessibility& node,
                                       base::DictionaryValue* tree_node);
  void RecursiveFormatAccessibilityTree(const base::DictionaryValue& tree_node,
                                        base::string16* contents,
                                        int depth = 0);

  // Overridden by each platform to add the required attributes for each node
  // into the given dict.
  void AddProperties(const BrowserAccessibility& node,
                     base::DictionaryValue* dict);

  base::string16 FormatCoordinates(const char* name,
                                   const char* x_name,
                                   const char* y_name,
                                   const base::DictionaryValue& value);

  // Returns a platform specific representation of a BrowserAccessibility.
  // Should be zero or more complete lines, each with |prefix| prepended
  // (to indent each line).
  base::string16 ToString(const base::DictionaryValue& node,
                          const base::string16& indent);

  void Initialize();

  bool MatchesFilters(const base::string16& text, bool default_result) const;

  // Writes the given attribute string out to |line| if it matches the filters.
  void WriteAttribute(bool include_by_default,
                      const base::string16& attr,
                      base::string16* line);
  void WriteAttribute(bool include_by_default,
                      const std::string& attr,
                      base::string16* line);

  BrowserAccessibility* root_;

  // Filters used when formatting the accessibility tree as text.
  std::vector<Filter> filters_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityTreeFormatter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_H_
