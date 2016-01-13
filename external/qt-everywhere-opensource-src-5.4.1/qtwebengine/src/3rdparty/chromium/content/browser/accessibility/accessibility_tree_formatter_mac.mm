// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter.h"

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

using base::StringPrintf;
using base::SysNSStringToUTF8;
using base::SysNSStringToUTF16;
using std::string;

namespace content {

namespace {

const char* kPositionDictAttr = "position";
const char* kXCoordDictAttr = "x";
const char* kYCoordDictAttr = "y";
const char* kSizeDictAttr = "size";
const char* kWidthDictAttr = "width";
const char* kHeightDictAttr = "height";
const char* kRangeLocDictAttr = "loc";
const char* kRangeLenDictAttr = "len";

scoped_ptr<base::DictionaryValue> PopulatePosition(
    const BrowserAccessibility& node) {
  scoped_ptr<base::DictionaryValue> position(new base::DictionaryValue);
  // The NSAccessibility position of an object is in global coordinates and
  // based on the lower-left corner of the object. To make this easier and less
  // confusing, convert it to local window coordinates using the top-left
  // corner when dumping the position.
  BrowserAccessibility* root = node.manager()->GetRoot();
  BrowserAccessibilityCocoa* cocoa_root = root->ToBrowserAccessibilityCocoa();
  NSPoint root_position = [[cocoa_root position] pointValue];
  NSSize root_size = [[cocoa_root size] sizeValue];
  int root_top = -static_cast<int>(root_position.y + root_size.height);
  int root_left = static_cast<int>(root_position.x);

  BrowserAccessibilityCocoa* cocoa_node =
      const_cast<BrowserAccessibility*>(&node)->ToBrowserAccessibilityCocoa();
  NSPoint node_position = [[cocoa_node position] pointValue];
  NSSize node_size = [[cocoa_node size] sizeValue];

  position->SetInteger(kXCoordDictAttr,
                       static_cast<int>(node_position.x - root_left));
  position->SetInteger(kYCoordDictAttr,
      static_cast<int>(-node_position.y - node_size.height - root_top));
  return position.Pass();
}

scoped_ptr<base::DictionaryValue>
PopulateSize(const BrowserAccessibilityCocoa* cocoa_node) {
  scoped_ptr<base::DictionaryValue> size(new base::DictionaryValue);
  NSSize node_size = [[cocoa_node size] sizeValue];
  size->SetInteger(kHeightDictAttr, static_cast<int>(node_size.height));
  size->SetInteger(kWidthDictAttr, static_cast<int>(node_size.width));
  return size.Pass();
}

scoped_ptr<base::DictionaryValue> PopulateRange(NSRange range) {
  scoped_ptr<base::DictionaryValue> rangeDict(new base::DictionaryValue);
  rangeDict->SetInteger(kRangeLocDictAttr, static_cast<int>(range.location));
  rangeDict->SetInteger(kRangeLenDictAttr, static_cast<int>(range.length));
  return rangeDict.Pass();
}

// Returns true if |value| is an NSValue containing a NSRange.
bool IsRangeValue(id value) {
  if (![value isKindOfClass:[NSValue class]])
    return false;
  return 0 == strcmp([value objCType], @encode(NSRange));
}

scoped_ptr<base::Value> PopulateObject(id value);

scoped_ptr<base::ListValue> PopulateArray(NSArray* array) {
  scoped_ptr<base::ListValue> list(new base::ListValue);
  for (NSUInteger i = 0; i < [array count]; i++)
    list->Append(PopulateObject([array objectAtIndex:i]).release());
  return list.Pass();
}

scoped_ptr<base::StringValue> StringForBrowserAccessibility(
    BrowserAccessibilityCocoa* obj) {
  NSString* description = [obj role];
  id value = [obj value];
  id roleDescription = [obj roleDescription];
  if (value && ![value isEqual:@""]) {
    description = [NSString stringWithFormat:@"%@ %@", description, value];
  } else if ([description isEqualToString:NSAccessibilityGroupRole] &&
           roleDescription != nil &&
           ![roleDescription isEqualToString:@""]) {
    description = [NSString stringWithFormat:@"%@ %@",
                            description, roleDescription];
  }
  return scoped_ptr<base::StringValue>(
      new base::StringValue(SysNSStringToUTF16(description))).Pass();
}

scoped_ptr<base::Value> PopulateObject(id value) {
  if ([value isKindOfClass:[NSArray class]])
    return scoped_ptr<base::Value>(PopulateArray((NSArray*) value));
  if (IsRangeValue(value))
    return scoped_ptr<base::Value>(PopulateRange([value rangeValue]));
  if ([value isKindOfClass:[BrowserAccessibilityCocoa class]]) {
    std::string str;
    StringForBrowserAccessibility(value)->GetAsString(&str);
    return scoped_ptr<base::Value>(StringForBrowserAccessibility(
        (BrowserAccessibilityCocoa*) value));
  }

  return scoped_ptr<base::Value>(
      new base::StringValue(
          SysNSStringToUTF16([NSString stringWithFormat:@"%@", value]))).Pass();
}

NSArray* BuildAllAttributesArray() {
  NSArray* array = [NSArray arrayWithObjects:
      NSAccessibilityRoleDescriptionAttribute,
      NSAccessibilityTitleAttribute,
      NSAccessibilityValueAttribute,
      NSAccessibilityMinValueAttribute,
      NSAccessibilityMaxValueAttribute,
      NSAccessibilityValueDescriptionAttribute,
      NSAccessibilityDescriptionAttribute,
      NSAccessibilityHelpAttribute,
      @"AXInvalid",
      NSAccessibilityDisclosingAttribute,
      NSAccessibilityDisclosureLevelAttribute,
      @"AXAccessKey",
      @"AXARIAAtomic",
      @"AXARIABusy",
      @"AXARIALive",
      @"AXARIARelevant",
      NSAccessibilityColumnIndexRangeAttribute,
      NSAccessibilityEnabledAttribute,
      NSAccessibilityFocusedAttribute,
      NSAccessibilityIndexAttribute,
      @"AXLoaded",
      @"AXLoadingProcess",
      NSAccessibilityNumberOfCharactersAttribute,
      NSAccessibilityOrientationAttribute,
      @"AXRequired",
      NSAccessibilityRowIndexRangeAttribute,
      NSAccessibilityTitleUIElementAttribute,
      NSAccessibilityURLAttribute,
      NSAccessibilityVisibleCharacterRangeAttribute,
      @"AXVisited",
      @"AXLinkedUIElements",
      nil];
  return [array retain];
}

}  // namespace

void AccessibilityTreeFormatter::Initialize() {
}


void AccessibilityTreeFormatter::AddProperties(const BrowserAccessibility& node,
                                               base::DictionaryValue* dict) {
  BrowserAccessibilityCocoa* cocoa_node =
      const_cast<BrowserAccessibility*>(&node)->ToBrowserAccessibilityCocoa();
  NSArray* supportedAttributes = [cocoa_node accessibilityAttributeNames];

  string role = SysNSStringToUTF8(
      [cocoa_node accessibilityAttributeValue:NSAccessibilityRoleAttribute]);
  dict->SetString(SysNSStringToUTF8(NSAccessibilityRoleAttribute), role);

  NSString* subrole =
      [cocoa_node accessibilityAttributeValue:NSAccessibilitySubroleAttribute];
  if (subrole != nil) {
    dict->SetString(SysNSStringToUTF8(NSAccessibilitySubroleAttribute),
                    SysNSStringToUTF8(subrole));
  }

  CR_DEFINE_STATIC_LOCAL(NSArray*, all_attributes, (BuildAllAttributesArray()));
  for (NSString* requestedAttribute in all_attributes) {
    if (![supportedAttributes containsObject:requestedAttribute])
      continue;
    id value = [cocoa_node accessibilityAttributeValue:requestedAttribute];
    if (value != nil) {
      dict->Set(
          SysNSStringToUTF8(requestedAttribute),
          PopulateObject(value).release());
    }
  }
  dict->Set(kPositionDictAttr, PopulatePosition(node).release());
  dict->Set(kSizeDictAttr, PopulateSize(cocoa_node).release());
}

base::string16 AccessibilityTreeFormatter::ToString(
    const base::DictionaryValue& dict,
    const base::string16& indent) {
  base::string16 line;
  NSArray* defaultAttributes =
      [NSArray arrayWithObjects:NSAccessibilityTitleAttribute,
                                NSAccessibilityValueAttribute,
                                nil];
  string s_value;
  dict.GetString(SysNSStringToUTF8(NSAccessibilityRoleAttribute), &s_value);
  WriteAttribute(true, base::UTF8ToUTF16(s_value), &line);

  string subroleAttribute = SysNSStringToUTF8(NSAccessibilitySubroleAttribute);
  if (dict.GetString(subroleAttribute, &s_value)) {
    WriteAttribute(false,
                   StringPrintf("%s=%s",
                                subroleAttribute.c_str(), s_value.c_str()),
                   &line);
  }

  CR_DEFINE_STATIC_LOCAL(NSArray*, all_attributes, (BuildAllAttributesArray()));
  for (NSString* requestedAttribute in all_attributes) {
    string requestedAttributeUTF8 = SysNSStringToUTF8(requestedAttribute);
    if (dict.GetString(requestedAttributeUTF8, &s_value)) {
      WriteAttribute([defaultAttributes containsObject:requestedAttribute],
                     StringPrintf("%s='%s'",
                                  requestedAttributeUTF8.c_str(),
                                  s_value.c_str()),
                     &line);
      continue;
    }
    const base::Value* value;
    if (dict.Get(requestedAttributeUTF8, &value)) {
      std::string json_value;
      base::JSONWriter::Write(value, &json_value);
      WriteAttribute(
          [defaultAttributes containsObject:requestedAttribute],
          StringPrintf("%s=%s",
                       requestedAttributeUTF8.c_str(),
                       json_value.c_str()),
          &line);
    }
  }
  const base::DictionaryValue* d_value = NULL;
  if (dict.GetDictionary(kPositionDictAttr, &d_value)) {
    WriteAttribute(false,
                   FormatCoordinates(kPositionDictAttr,
                                     kXCoordDictAttr, kYCoordDictAttr,
                                     *d_value),
                   &line);
  }
  if (dict.GetDictionary(kSizeDictAttr, &d_value)) {
    WriteAttribute(false,
                   FormatCoordinates(kSizeDictAttr,
                                     kWidthDictAttr, kHeightDictAttr, *d_value),
                   &line);
  }

  return indent + line + base::ASCIIToUTF16("\n");
}

// static
const base::FilePath::StringType
AccessibilityTreeFormatter::GetActualFileSuffix() {
  return FILE_PATH_LITERAL("-actual-mac.txt");
}

// static
const base::FilePath::StringType
AccessibilityTreeFormatter::GetExpectedFileSuffix() {
  return FILE_PATH_LITERAL("-expected-mac.txt");
}

// static
const string AccessibilityTreeFormatter::GetAllowEmptyString() {
  return "@MAC-ALLOW-EMPTY:";
}

// static
const string AccessibilityTreeFormatter::GetAllowString() {
  return "@MAC-ALLOW:";
}

// static
const string AccessibilityTreeFormatter::GetDenyString() {
  return "@MAC-DENY:";
}

}  // namespace content
