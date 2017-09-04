// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#import <Cocoa/Cocoa.h>

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "testing/gtest_mac.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_node_data.h"
#import "ui/accessibility/platform/ax_platform_node_mac.h"
#include "ui/base/ime/text_input_type.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

// Expose some methods from AXPlatformNodeCocoa for testing purposes only.
@interface AXPlatformNodeCocoa (Testing)
- (NSString*)AXRole;
@end

namespace views {

namespace {

NSString* const kTestPlaceholderText = @"Test placeholder text";
NSString* const kTestStringValue = @"Test string value";
NSString* const kTestTitle = @"Test textfield";

class FlexibleRoleTestView : public View {
 public:
  explicit FlexibleRoleTestView(ui::AXRole role) : role_(role) {}
  void set_role(ui::AXRole role) { role_ = role; }

  // Add a child view and resize to fit the child.
  void FitBoundsToNewChild(View* view) {
    View::AddChildView(view);
    // Fit the parent widget to the size of the child for accurate hit tests.
    SetBoundsRect(view->bounds());
  }

  // View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    View::GetAccessibleNodeData(node_data);
    node_data->role = role_;
  }

 private:
  ui::AXRole role_;

  DISALLOW_COPY_AND_ASSIGN(FlexibleRoleTestView);
};

class NativeWidgetMacAccessibilityTest : public test::WidgetTest {
 public:
  NativeWidgetMacAccessibilityTest() {}

  void SetUp() override {
    test::WidgetTest::SetUp();
    widget_ = CreateTopLevelPlatformWidget();
    widget_->SetBounds(gfx::Rect(50, 50, 100, 100));
    widget()->Show();
  }

  void TearDown() override {
    widget_->CloseNow();
    test::WidgetTest::TearDown();
  }

  id AttributeValueAtMidpoint(NSString* attribute) {
    // Accessibility hit tests come in Cocoa screen coordinates.
    NSPoint midpoint_in_screen_ = gfx::ScreenPointToNSPoint(
        widget_->GetWindowBoundsInScreen().CenterPoint());
    id hit =
        [widget_->GetNativeWindow() accessibilityHitTest:midpoint_in_screen_];
    id value = [hit accessibilityAttributeValue:attribute];
    return value;
  }

  Textfield* AddChildTextfield(const gfx::Size& size) {
    Textfield* textfield = new Textfield;
    textfield->SetText(base::SysNSStringToUTF16(kTestStringValue));
    textfield->SetAccessibleName(base::SysNSStringToUTF16(kTestTitle));
    textfield->SetSize(size);
    widget()->GetContentsView()->AddChildView(textfield);
    return textfield;
  }

  Widget* widget() { return widget_; }
  gfx::Rect GetWidgetBounds() { return widget_->GetClientAreaBoundsInScreen(); }

 private:
  Widget* widget_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetMacAccessibilityTest);
};

}  // namespace

// Test for NSAccessibilityChildrenAttribute, and ensure it excludes ignored
// children from the accessibility tree.
TEST_F(NativeWidgetMacAccessibilityTest, ChildrenAttribute) {
  // Check childless views don't have accessibility children.
  EXPECT_EQ(0u,
            [AttributeValueAtMidpoint(NSAccessibilityChildrenAttribute) count]);

  const size_t kNumChildren = 3;
  for (size_t i = 0; i < kNumChildren; ++i) {
    // Make sure the labels won't interfere with the hit test.
    AddChildTextfield(gfx::Size());
  }

  EXPECT_EQ(kNumChildren,
            [AttributeValueAtMidpoint(NSAccessibilityChildrenAttribute) count]);

  // Check ignored children don't show up in the accessibility tree.
  widget()->GetContentsView()->AddChildView(
      new FlexibleRoleTestView(ui::AX_ROLE_IGNORED));
  EXPECT_EQ(kNumChildren,
            [AttributeValueAtMidpoint(NSAccessibilityChildrenAttribute) count]);
}

// Test for NSAccessibilityParentAttribute, including for a Widget with no
// parent.
TEST_F(NativeWidgetMacAccessibilityTest, ParentAttribute) {
  Textfield* child = AddChildTextfield(widget()->GetContentsView()->size());

  // Views with Widget parents will have a NSWindow parent.
  EXPECT_NSEQ(
      NSAccessibilityWindowRole,
      [AttributeValueAtMidpoint(NSAccessibilityParentAttribute) AXRole]);

  // Views with non-Widget parents will have the role of the parent view.
  widget()->GetContentsView()->RemoveChildView(child);
  FlexibleRoleTestView* parent = new FlexibleRoleTestView(ui::AX_ROLE_GROUP);
  parent->FitBoundsToNewChild(child);
  widget()->GetContentsView()->AddChildView(parent);
  EXPECT_NSEQ(
      NSAccessibilityGroupRole,
      [AttributeValueAtMidpoint(NSAccessibilityParentAttribute) AXRole]);

  // Test an ignored role parent is skipped in favor of the grandparent.
  parent->set_role(ui::AX_ROLE_IGNORED);
  EXPECT_NSEQ(
      NSAccessibilityWindowRole,
      [AttributeValueAtMidpoint(NSAccessibilityParentAttribute) AXRole]);
}

// Test for NSAccessibilityPositionAttribute, including on Widget movement
// updates.
TEST_F(NativeWidgetMacAccessibilityTest, PositionAttribute) {
  NSValue* widget_origin =
      [NSValue valueWithPoint:gfx::ScreenPointToNSPoint(
                                  GetWidgetBounds().bottom_left())];
  EXPECT_NSEQ(widget_origin,
              AttributeValueAtMidpoint(NSAccessibilityPositionAttribute));

  // Check the attribute is updated when the Widget is moved.
  gfx::Rect new_bounds(60, 80, 100, 100);
  widget()->SetBounds(new_bounds);
  widget_origin = [NSValue
      valueWithPoint:gfx::ScreenPointToNSPoint(new_bounds.bottom_left())];
  EXPECT_NSEQ(widget_origin,
              AttributeValueAtMidpoint(NSAccessibilityPositionAttribute));
}

// Test for NSAccessibilityHelpAttribute.
TEST_F(NativeWidgetMacAccessibilityTest, HelpAttribute) {
  Label* label = new Label(base::SysNSStringToUTF16(kTestPlaceholderText));
  label->SetSize(GetWidgetBounds().size());
  EXPECT_NSEQ(@"", AttributeValueAtMidpoint(NSAccessibilityHelpAttribute));
  label->SetTooltipText(base::SysNSStringToUTF16(kTestPlaceholderText));
  widget()->GetContentsView()->AddChildView(label);
  EXPECT_NSEQ(kTestPlaceholderText,
              AttributeValueAtMidpoint(NSAccessibilityHelpAttribute));
}

// Test for NSAccessibilityWindowAttribute and
// NSAccessibilityTopLevelUIElementAttribute.
TEST_F(NativeWidgetMacAccessibilityTest, WindowAndTopLevelUIElementAttributes) {
  FlexibleRoleTestView* view = new FlexibleRoleTestView(ui::AX_ROLE_GROUP);
  view->SetSize(GetWidgetBounds().size());
  widget()->GetContentsView()->AddChildView(view);
  // Make sure it's |view| in the hit test by checking its accessibility role.
  EXPECT_EQ(NSAccessibilityGroupRole,
            AttributeValueAtMidpoint(NSAccessibilityRoleAttribute));
  EXPECT_NSEQ(widget()->GetNativeWindow(),
              AttributeValueAtMidpoint(NSAccessibilityWindowAttribute));
  EXPECT_NSEQ(
      widget()->GetNativeWindow(),
      AttributeValueAtMidpoint(NSAccessibilityTopLevelUIElementAttribute));
}

// Tests for accessibility attributes on a views::Textfield.
// TODO(patricialor): Test against Cocoa-provided attributes as well to ensure
// consistency between Cocoa and toolkit-views.
TEST_F(NativeWidgetMacAccessibilityTest, TextfieldGenericAttributes) {
  Textfield* textfield = AddChildTextfield(GetWidgetBounds().size());

  // NSAccessibilityEnabledAttribute.
  textfield->SetEnabled(false);
  EXPECT_EQ(NO, [AttributeValueAtMidpoint(NSAccessibilityEnabledAttribute)
                    boolValue]);
  textfield->SetEnabled(true);
  EXPECT_EQ(YES, [AttributeValueAtMidpoint(NSAccessibilityEnabledAttribute)
                     boolValue]);

  // NSAccessibilityFocusedAttribute.
  EXPECT_EQ(NO, [AttributeValueAtMidpoint(NSAccessibilityFocusedAttribute)
                    boolValue]);
  textfield->RequestFocus();
  EXPECT_EQ(YES, [AttributeValueAtMidpoint(NSAccessibilityFocusedAttribute)
                     boolValue]);

  // NSAccessibilityTitleAttribute.
  EXPECT_NSEQ(kTestTitle,
              AttributeValueAtMidpoint(NSAccessibilityTitleAttribute));

  // NSAccessibilityValueAttribute.
  EXPECT_NSEQ(kTestStringValue,
              AttributeValueAtMidpoint(NSAccessibilityValueAttribute));

  // NSAccessibilityRoleAttribute.
  EXPECT_NSEQ(NSAccessibilityTextFieldRole,
              AttributeValueAtMidpoint(NSAccessibilityRoleAttribute));

  // NSAccessibilitySubroleAttribute and
  // NSAccessibilityRoleDescriptionAttribute.
  EXPECT_NSEQ(nil, AttributeValueAtMidpoint(NSAccessibilitySubroleAttribute));
  NSString* role_description =
      NSAccessibilityRoleDescription(NSAccessibilityTextFieldRole, nil);
  EXPECT_NSEQ(role_description, AttributeValueAtMidpoint(
                                    NSAccessibilityRoleDescriptionAttribute));

  // Test accessibility clients can see subroles as well.
  textfield->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_NSEQ(NSAccessibilitySecureTextFieldSubrole,
              AttributeValueAtMidpoint(NSAccessibilitySubroleAttribute));
  role_description = NSAccessibilityRoleDescription(
      NSAccessibilityTextFieldRole, NSAccessibilitySecureTextFieldSubrole);
  EXPECT_NSEQ(role_description, AttributeValueAtMidpoint(
                                    NSAccessibilityRoleDescriptionAttribute));

  // Prevent the textfield from interfering with hit tests on the widget itself.
  widget()->GetContentsView()->RemoveChildView(textfield);

  // NSAccessibilitySizeAttribute.
  EXPECT_EQ(GetWidgetBounds().size(),
            gfx::Size([AttributeValueAtMidpoint(NSAccessibilitySizeAttribute)
                sizeValue]));
  // Check the attribute is updated when the Widget is resized.
  gfx::Size new_size(200, 40);
  widget()->SetSize(new_size);
  EXPECT_EQ(new_size, gfx::Size([AttributeValueAtMidpoint(
                          NSAccessibilitySizeAttribute) sizeValue]));
}

TEST_F(NativeWidgetMacAccessibilityTest, TextfieldEditableAttributes) {
  Textfield* textfield = AddChildTextfield(GetWidgetBounds().size());
  textfield->set_placeholder_text(
      base::SysNSStringToUTF16(kTestPlaceholderText));

  // NSAccessibilityInsertionPointLineNumberAttribute.
  EXPECT_EQ(0, [AttributeValueAtMidpoint(
                   NSAccessibilityInsertionPointLineNumberAttribute) intValue]);

  // NSAccessibilityNumberOfCharactersAttribute.
  EXPECT_EQ(
      kTestStringValue.length,
      [AttributeValueAtMidpoint(NSAccessibilityNumberOfCharactersAttribute)
          unsignedIntegerValue]);

  // NSAccessibilityPlaceholderAttribute.
  EXPECT_NSEQ(
      kTestPlaceholderText,
      AttributeValueAtMidpoint(NSAccessibilityPlaceholderValueAttribute));

  // NSAccessibilitySelectedTextAttribute and
  // NSAccessibilitySelectedTextRangeAttribute.
  EXPECT_NSEQ(@"",
              AttributeValueAtMidpoint(NSAccessibilitySelectedTextAttribute));
  // The cursor will be at the end of the textfield, so the selection range will
  // span 0 characters and be located at the index after the last character.
  EXPECT_EQ(gfx::Range(kTestStringValue.length, kTestStringValue.length),
            gfx::Range([AttributeValueAtMidpoint(
                NSAccessibilitySelectedTextRangeAttribute) rangeValue]));
  // Select some text in the middle of the textfield.
  gfx::Range selection_range(2, 6);
  textfield->SelectRange(selection_range);
  EXPECT_NSEQ([kTestStringValue substringWithRange:selection_range.ToNSRange()],
              AttributeValueAtMidpoint(NSAccessibilitySelectedTextAttribute));
  EXPECT_EQ(selection_range,
            gfx::Range([AttributeValueAtMidpoint(
                NSAccessibilitySelectedTextRangeAttribute) rangeValue]));

  // NSAccessibilityVisibleCharacterRangeAttribute.
  EXPECT_EQ(gfx::Range(0, kTestStringValue.length),
            gfx::Range([AttributeValueAtMidpoint(
                NSAccessibilityVisibleCharacterRangeAttribute) rangeValue]));
}

// Test writing accessibility attributes via an accessibility client for normal
// Views.
TEST_F(NativeWidgetMacAccessibilityTest, ViewWritableAttributes) {
  FlexibleRoleTestView* view = new FlexibleRoleTestView(ui::AX_ROLE_GROUP);
  view->SetSize(GetWidgetBounds().size());
  widget()->GetContentsView()->AddChildView(view);

  // Get the FlexibleRoleTestView accessibility object.
  NSPoint midpoint = gfx::ScreenPointToNSPoint(GetWidgetBounds().CenterPoint());
  id ax_node = [widget()->GetNativeWindow() accessibilityHitTest:midpoint];
  EXPECT_TRUE(ax_node);

  // Make sure it's the correct accessibility object.
  id value = [ax_node accessibilityAttributeValue:NSAccessibilityRoleAttribute];
  EXPECT_NSEQ(NSAccessibilityGroupRole, value);

  // Make sure |view| is focusable, then focus/unfocus it.
  view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  EXPECT_FALSE(view->HasFocus());
  EXPECT_FALSE(
      [AttributeValueAtMidpoint(NSAccessibilityFocusedAttribute) boolValue]);
  EXPECT_TRUE([ax_node
      accessibilityIsAttributeSettable:NSAccessibilityFocusedAttribute]);
  [ax_node accessibilitySetValue:[NSNumber numberWithBool:YES]
                    forAttribute:NSAccessibilityFocusedAttribute];
  EXPECT_TRUE(
      [AttributeValueAtMidpoint(NSAccessibilityFocusedAttribute) boolValue]);
  EXPECT_TRUE(view->HasFocus());
}

// Test writing accessibility attributes via an accessibility client for
// editable controls (in this case, views::Textfields).
TEST_F(NativeWidgetMacAccessibilityTest, TextfieldWritableAttributes) {
  Textfield* textfield = AddChildTextfield(GetWidgetBounds().size());

  // Get the Textfield accessibility object.
  NSPoint midpoint = gfx::ScreenPointToNSPoint(GetWidgetBounds().CenterPoint());
  id ax_node = [widget()->GetNativeWindow() accessibilityHitTest:midpoint];
  EXPECT_TRUE(ax_node);

  // Make sure it's the correct accessibility object.
  id value =
      [ax_node accessibilityAttributeValue:NSAccessibilityValueAttribute];
  EXPECT_NSEQ(kTestStringValue, value);

  // Write a new NSAccessibilityValueAttribute.
  EXPECT_TRUE(
      [ax_node accessibilityIsAttributeSettable:NSAccessibilityValueAttribute]);
  [ax_node accessibilitySetValue:kTestPlaceholderText
                    forAttribute:NSAccessibilityValueAttribute];
  EXPECT_NSEQ(kTestPlaceholderText,
              AttributeValueAtMidpoint(NSAccessibilityValueAttribute));
  EXPECT_EQ(base::SysNSStringToUTF16(kTestPlaceholderText), textfield->text());

  // Test a read-only textfield.
  textfield->SetReadOnly(true);
  EXPECT_FALSE(
      [ax_node accessibilityIsAttributeSettable:NSAccessibilityValueAttribute]);
  [ax_node accessibilitySetValue:kTestStringValue
                    forAttribute:NSAccessibilityValueAttribute];
  EXPECT_NSEQ(kTestPlaceholderText,
              AttributeValueAtMidpoint(NSAccessibilityValueAttribute));
  EXPECT_EQ(base::SysNSStringToUTF16(kTestPlaceholderText), textfield->text());
  textfield->SetReadOnly(false);

  // Change the selection text when there is no selected text.
  textfield->SelectRange(gfx::Range(0, 0));
  EXPECT_TRUE([ax_node
      accessibilityIsAttributeSettable:NSAccessibilitySelectedTextAttribute]);

  NSString* new_string =
      [kTestStringValue stringByAppendingString:kTestPlaceholderText];
  [ax_node accessibilitySetValue:kTestStringValue
                    forAttribute:NSAccessibilitySelectedTextAttribute];
  EXPECT_NSEQ(new_string,
              AttributeValueAtMidpoint(NSAccessibilityValueAttribute));
  EXPECT_EQ(base::SysNSStringToUTF16(new_string), textfield->text());

  // Replace entire selection.
  gfx::Range test_range(0, [new_string length]);
  textfield->SelectRange(test_range);
  [ax_node accessibilitySetValue:kTestStringValue
                    forAttribute:NSAccessibilitySelectedTextAttribute];
  EXPECT_NSEQ(kTestStringValue,
              AttributeValueAtMidpoint(NSAccessibilityValueAttribute));
  EXPECT_EQ(base::SysNSStringToUTF16(kTestStringValue), textfield->text());
  // Make sure the cursor is at the end of the Textfield.
  EXPECT_EQ(gfx::Range([kTestStringValue length]),
            textfield->GetSelectedRange());

  // Replace a middle section only (with a backwards selection range).
  base::string16 front = base::ASCIIToUTF16("Front ");
  base::string16 middle = base::ASCIIToUTF16("middle");
  base::string16 back = base::ASCIIToUTF16(" back");
  base::string16 replacement = base::ASCIIToUTF16("replaced");
  textfield->SetText(front + middle + back);
  test_range = gfx::Range(front.length() + middle.length(), front.length());
  new_string = base::SysUTF16ToNSString(front + replacement + back);
  textfield->SelectRange(test_range);
  [ax_node accessibilitySetValue:base::SysUTF16ToNSString(replacement)
                    forAttribute:NSAccessibilitySelectedTextAttribute];
  EXPECT_NSEQ(new_string,
              AttributeValueAtMidpoint(NSAccessibilityValueAttribute));
  EXPECT_EQ(base::SysNSStringToUTF16(new_string), textfield->text());
  // Make sure the cursor is at the end of the replacement.
  EXPECT_EQ(gfx::Range(front.length() + replacement.length()),
            textfield->GetSelectedRange());
}

}  // namespace views
