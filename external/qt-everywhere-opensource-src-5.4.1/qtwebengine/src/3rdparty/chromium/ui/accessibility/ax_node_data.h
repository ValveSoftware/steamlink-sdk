// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_NODE_DATA_H_
#define UI_ACCESSIBILITY_AX_NODE_DATA_H_

#include <map>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_export.h"
#include "ui/gfx/rect.h"

namespace ui {

// A compact representation of the accessibility information for a
// single web object, in a form that can be serialized and sent from
// one process to another.
struct AX_EXPORT AXNodeData {
  AXNodeData();
  virtual ~AXNodeData();

  void AddStringAttribute(AXStringAttribute attribute,
                          const std::string& value);
  void AddIntAttribute(AXIntAttribute attribute, int value);
  void AddFloatAttribute(AXFloatAttribute attribute, float value);
  void AddBoolAttribute(AXBoolAttribute attribute, bool value);
  void AddIntListAttribute(AXIntListAttribute attribute,
                           const std::vector<int32>& value);

  // Convenience functions, mainly for writing unit tests.
  // Equivalent to AddStringAttribute(ATTR_NAME, name).
  void SetName(std::string name);
  // Equivalent to AddStringAttribute(ATTR_VALUE, value).
  void SetValue(std::string value);

  // Return a string representation of this data, for debugging.
  std::string ToString() const;

  // This is a simple serializable struct. All member variables should be
  // public and copyable.
  int32 id;
  AXRole role;
  uint32 state;
  gfx::Rect location;
  std::vector<std::pair<AXStringAttribute, std::string> > string_attributes;
  std::vector<std::pair<AXIntAttribute, int32> > int_attributes;
  std::vector<std::pair<AXFloatAttribute, float> > float_attributes;
  std::vector<std::pair<AXBoolAttribute, bool> > bool_attributes;
  std::vector<std::pair<AXIntListAttribute, std::vector<int32> > >
      intlist_attributes;
  std::vector<std::pair<std::string, std::string> > html_attributes;
  std::vector<int32> child_ids;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_NODE_DATA_H_
