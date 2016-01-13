// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_manager_win.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "content/common/accessibility_messages.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/win/atl_module.h"

namespace content {
namespace {


// CountedBrowserAccessibility ------------------------------------------------

// Subclass of BrowserAccessibilityWin that counts the number of instances.
class CountedBrowserAccessibility : public BrowserAccessibilityWin {
 public:
  CountedBrowserAccessibility();
  virtual ~CountedBrowserAccessibility();

  static void reset() { num_instances_ = 0; }
  static int num_instances() { return num_instances_; }

 private:
  static int num_instances_;

  DISALLOW_COPY_AND_ASSIGN(CountedBrowserAccessibility);
};

// static
int CountedBrowserAccessibility::num_instances_ = 0;

CountedBrowserAccessibility::CountedBrowserAccessibility() {
  ++num_instances_;
}

CountedBrowserAccessibility::~CountedBrowserAccessibility() {
  --num_instances_;
}


// CountedBrowserAccessibilityFactory -----------------------------------------

// Factory that creates a CountedBrowserAccessibility.
class CountedBrowserAccessibilityFactory : public BrowserAccessibilityFactory {
 public:
  CountedBrowserAccessibilityFactory();

 private:
  virtual ~CountedBrowserAccessibilityFactory();

  virtual BrowserAccessibility* Create() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(CountedBrowserAccessibilityFactory);
};

CountedBrowserAccessibilityFactory::CountedBrowserAccessibilityFactory() {
}

CountedBrowserAccessibilityFactory::~CountedBrowserAccessibilityFactory() {
}

BrowserAccessibility* CountedBrowserAccessibilityFactory::Create() {
  CComObject<CountedBrowserAccessibility>* instance;
  HRESULT hr = CComObject<CountedBrowserAccessibility>::CreateInstance(
      &instance);
  DCHECK(SUCCEEDED(hr));
  instance->AddRef();
  return instance;
}

}  // namespace


// BrowserAccessibilityTest ---------------------------------------------------

class BrowserAccessibilityTest : public testing::Test {
 public:
  BrowserAccessibilityTest();
  virtual ~BrowserAccessibilityTest();

 private:
  virtual void SetUp() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityTest);
};

BrowserAccessibilityTest::BrowserAccessibilityTest() {
}

BrowserAccessibilityTest::~BrowserAccessibilityTest() {
}

void BrowserAccessibilityTest::SetUp() {
  ui::win::CreateATLModuleIfNeeded();
}


// Actual tests ---------------------------------------------------------------

// Test that BrowserAccessibilityManager correctly releases the tree of
// BrowserAccessibility instances upon delete.
TEST_F(BrowserAccessibilityTest, TestNoLeaks) {
  // Create ui::AXNodeData objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  ui::AXNodeData button;
  button.id = 2;
  button.SetName("Button");
  button.role = ui::AX_ROLE_BUTTON;
  button.state = 0;

  ui::AXNodeData checkbox;
  checkbox.id = 3;
  checkbox.SetName("Checkbox");
  checkbox.role = ui::AX_ROLE_CHECK_BOX;
  checkbox.state = 0;

  ui::AXNodeData root;
  root.id = 1;
  root.SetName("Document");
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.state = 0;
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  // Construct a BrowserAccessibilityManager with this
  // ui::AXNodeData tree and a factory for an instance-counting
  // BrowserAccessibility, and ensure that exactly 3 instances were
  // created. Note that the manager takes ownership of the factory.
  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, button, checkbox),
          NULL, new CountedBrowserAccessibilityFactory()));
  ASSERT_EQ(3, CountedBrowserAccessibility::num_instances());

  // Delete the manager and test that all 3 instances are deleted.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());

  // Construct a manager again, and this time use the IAccessible interface
  // to get new references to two of the three nodes in the tree.
  manager.reset(BrowserAccessibilityManager::Create(
      MakeAXTreeUpdate(root, button, checkbox),
      NULL, new CountedBrowserAccessibilityFactory()));
  ASSERT_EQ(3, CountedBrowserAccessibility::num_instances());
  IAccessible* root_accessible =
      manager->GetRoot()->ToBrowserAccessibilityWin();
  IDispatch* root_iaccessible = NULL;
  IDispatch* child1_iaccessible = NULL;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  HRESULT hr = root_accessible->get_accChild(childid_self, &root_iaccessible);
  ASSERT_EQ(S_OK, hr);
  base::win::ScopedVariant one(1);
  hr = root_accessible->get_accChild(one, &child1_iaccessible);
  ASSERT_EQ(S_OK, hr);

  // Now delete the manager, and only one of the three nodes in the tree
  // should be released.
  manager.reset();
  ASSERT_EQ(2, CountedBrowserAccessibility::num_instances());

  // Release each of our references and make sure that each one results in
  // the instance being deleted as its reference count hits zero.
  root_iaccessible->Release();
  ASSERT_EQ(1, CountedBrowserAccessibility::num_instances());
  child1_iaccessible->Release();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestChildrenChange) {
  // Create ui::AXNodeData objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  ui::AXNodeData text;
  text.id = 2;
  text.role = ui::AX_ROLE_STATIC_TEXT;
  text.SetName("old text");
  text.state = 0;

  ui::AXNodeData root;
  root.id = 1;
  root.SetName("Document");
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.state = 0;
  root.child_ids.push_back(2);

  // Construct a BrowserAccessibilityManager with this
  // ui::AXNodeData tree and a factory for an instance-counting
  // BrowserAccessibility.
  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, text),
          NULL, new CountedBrowserAccessibilityFactory()));

  // Query for the text IAccessible and verify that it returns "old text" as its
  // value.
  base::win::ScopedVariant one(1);
  base::win::ScopedComPtr<IDispatch> text_dispatch;
  HRESULT hr = manager->GetRoot()->ToBrowserAccessibilityWin()->get_accChild(
      one, text_dispatch.Receive());
  ASSERT_EQ(S_OK, hr);

  base::win::ScopedComPtr<IAccessible> text_accessible;
  hr = text_dispatch.QueryInterface(text_accessible.Receive());
  ASSERT_EQ(S_OK, hr);

  base::win::ScopedVariant childid_self(CHILDID_SELF);
  base::win::ScopedBstr name;
  hr = text_accessible->get_accName(childid_self, name.Receive());
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(L"old text", base::string16(name));
  name.Reset();

  text_dispatch.Release();
  text_accessible.Release();

  // Notify the BrowserAccessibilityManager that the text child has changed.
  ui::AXNodeData text2;
  text2.id = 2;
  text2.role = ui::AX_ROLE_STATIC_TEXT;
  text2.SetName("new text");
  text2.SetName("old text");
  AccessibilityHostMsg_EventParams param;
  param.event_type = ui::AX_EVENT_CHILDREN_CHANGED;
  param.update.nodes.push_back(text2);
  param.id = text2.id;
  std::vector<AccessibilityHostMsg_EventParams> events;
  events.push_back(param);
  manager->OnAccessibilityEvents(events);

  // Query for the text IAccessible and verify that it now returns "new text"
  // as its value.
  hr = manager->GetRoot()->ToBrowserAccessibilityWin()->get_accChild(
      one, text_dispatch.Receive());
  ASSERT_EQ(S_OK, hr);

  hr = text_dispatch.QueryInterface(text_accessible.Receive());
  ASSERT_EQ(S_OK, hr);

  hr = text_accessible->get_accName(childid_self, name.Receive());
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(L"new text", base::string16(name));

  text_dispatch.Release();
  text_accessible.Release();

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestChildrenChangeNoLeaks) {
  // Create ui::AXNodeData objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  ui::AXNodeData div;
  div.id = 2;
  div.role = ui::AX_ROLE_GROUP;
  div.state = 0;

  ui::AXNodeData text3;
  text3.id = 3;
  text3.role = ui::AX_ROLE_STATIC_TEXT;
  text3.state = 0;

  ui::AXNodeData text4;
  text4.id = 4;
  text4.role = ui::AX_ROLE_STATIC_TEXT;
  text4.state = 0;

  div.child_ids.push_back(3);
  div.child_ids.push_back(4);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.state = 0;
  root.child_ids.push_back(2);

  // Construct a BrowserAccessibilityManager with this
  // ui::AXNodeData tree and a factory for an instance-counting
  // BrowserAccessibility and ensure that exactly 4 instances were
  // created. Note that the manager takes ownership of the factory.
  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, div, text3, text4),
          NULL, new CountedBrowserAccessibilityFactory()));
  ASSERT_EQ(4, CountedBrowserAccessibility::num_instances());

  // Notify the BrowserAccessibilityManager that the div node and its children
  // were removed and ensure that only one BrowserAccessibility instance exists.
  root.child_ids.clear();
  AccessibilityHostMsg_EventParams param;
  param.event_type = ui::AX_EVENT_CHILDREN_CHANGED;
  param.update.nodes.push_back(root);
  param.id = root.id;
  std::vector<AccessibilityHostMsg_EventParams> events;
  events.push_back(param);
  manager->OnAccessibilityEvents(events);
  ASSERT_EQ(1, CountedBrowserAccessibility::num_instances());

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestTextBoundaries) {
  std::string text1_value = "One two three.\nFour five six.";

  ui::AXNodeData text1;
  text1.id = 11;
  text1.role = ui::AX_ROLE_TEXT_FIELD;
  text1.state = 0;
  text1.AddStringAttribute(ui::AX_ATTR_VALUE, text1_value);
  std::vector<int32> line_breaks;
  line_breaks.push_back(15);
  text1.AddIntListAttribute(
      ui::AX_ATTR_LINE_BREAKS, line_breaks);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.state = 0;
  root.child_ids.push_back(11);

  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, text1),
          NULL, new CountedBrowserAccessibilityFactory()));
  ASSERT_EQ(2, CountedBrowserAccessibility::num_instances());

  BrowserAccessibilityWin* root_obj =
      manager->GetRoot()->ToBrowserAccessibilityWin();
  BrowserAccessibilityWin* text1_obj =
      root_obj->PlatformGetChild(0)->ToBrowserAccessibilityWin();

  long text1_len;
  ASSERT_EQ(S_OK, text1_obj->get_nCharacters(&text1_len));

  base::win::ScopedBstr text;
  ASSERT_EQ(S_OK, text1_obj->get_text(0, text1_len, text.Receive()));
  ASSERT_EQ(text1_value, base::UTF16ToUTF8(base::string16(text)));
  text.Reset();

  ASSERT_EQ(S_OK, text1_obj->get_text(0, 4, text.Receive()));
  ASSERT_STREQ(L"One ", text);
  text.Reset();

  long start;
  long end;
  ASSERT_EQ(S_OK, text1_obj->get_textAtOffset(
      1, IA2_TEXT_BOUNDARY_CHAR, &start, &end, text.Receive()));
  ASSERT_EQ(1, start);
  ASSERT_EQ(2, end);
  ASSERT_STREQ(L"n", text);
  text.Reset();

  ASSERT_EQ(S_FALSE, text1_obj->get_textAtOffset(
      text1_len, IA2_TEXT_BOUNDARY_CHAR, &start, &end, text.Receive()));
  ASSERT_EQ(text1_len, start);
  ASSERT_EQ(text1_len, end);
  text.Reset();

  ASSERT_EQ(S_OK, text1_obj->get_textAtOffset(
      1, IA2_TEXT_BOUNDARY_WORD, &start, &end, text.Receive()));
  ASSERT_EQ(0, start);
  ASSERT_EQ(3, end);
  ASSERT_STREQ(L"One", text);
  text.Reset();

  ASSERT_EQ(S_OK, text1_obj->get_textAtOffset(
      6, IA2_TEXT_BOUNDARY_WORD, &start, &end, text.Receive()));
  ASSERT_EQ(4, start);
  ASSERT_EQ(7, end);
  ASSERT_STREQ(L"two", text);
  text.Reset();

  ASSERT_EQ(S_OK, text1_obj->get_textAtOffset(
      text1_len, IA2_TEXT_BOUNDARY_WORD, &start, &end, text.Receive()));
  ASSERT_EQ(25, start);
  ASSERT_EQ(29, end);
  ASSERT_STREQ(L"six.", text);
  text.Reset();

  ASSERT_EQ(S_OK, text1_obj->get_textAtOffset(
      1, IA2_TEXT_BOUNDARY_LINE, &start, &end, text.Receive()));
  ASSERT_EQ(0, start);
  ASSERT_EQ(15, end);
  ASSERT_STREQ(L"One two three.\n", text);
  text.Reset();

  ASSERT_EQ(S_OK,
            text1_obj->get_text(0, IA2_TEXT_OFFSET_LENGTH, text.Receive()));
  ASSERT_STREQ(L"One two three.\nFour five six.", text);

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestSimpleHypertext) {
  const std::string text1_name = "One two three.";
  const std::string text2_name = " Four five six.";

  ui::AXNodeData text1;
  text1.id = 11;
  text1.role = ui::AX_ROLE_STATIC_TEXT;
  text1.state = 1 << ui::AX_STATE_READ_ONLY;
  text1.SetName(text1_name);

  ui::AXNodeData text2;
  text2.id = 12;
  text2.role = ui::AX_ROLE_STATIC_TEXT;
  text2.state = 1 << ui::AX_STATE_READ_ONLY;
  text2.SetName(text2_name);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.state = 1 << ui::AX_STATE_READ_ONLY;
  root.child_ids.push_back(11);
  root.child_ids.push_back(12);

  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, root, text1, text2),
          NULL, new CountedBrowserAccessibilityFactory()));
  ASSERT_EQ(3, CountedBrowserAccessibility::num_instances());

  BrowserAccessibilityWin* root_obj =
      manager->GetRoot()->ToBrowserAccessibilityWin();

  long text_len;
  ASSERT_EQ(S_OK, root_obj->get_nCharacters(&text_len));

  base::win::ScopedBstr text;
  ASSERT_EQ(S_OK, root_obj->get_text(0, text_len, text.Receive()));
  EXPECT_EQ(text1_name + text2_name, base::UTF16ToUTF8(base::string16(text)));

  long hyperlink_count;
  ASSERT_EQ(S_OK, root_obj->get_nHyperlinks(&hyperlink_count));
  EXPECT_EQ(0, hyperlink_count);

  base::win::ScopedComPtr<IAccessibleHyperlink> hyperlink;
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(-1, hyperlink.Receive()));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(0, hyperlink.Receive()));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(28, hyperlink.Receive()));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(29, hyperlink.Receive()));

  long hyperlink_index;
  EXPECT_EQ(E_FAIL, root_obj->get_hyperlinkIndex(0, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  EXPECT_EQ(E_FAIL, root_obj->get_hyperlinkIndex(28, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlinkIndex(-1, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlinkIndex(29, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestComplexHypertext) {
  const std::string text1_name = "One two three.";
  const std::string text2_name = " Four five six.";
  const std::string button1_text_name = "red";
  const std::string link1_text_name = "blue";

  ui::AXNodeData text1;
  text1.id = 11;
  text1.role = ui::AX_ROLE_STATIC_TEXT;
  text1.state = 1 << ui::AX_STATE_READ_ONLY;
  text1.SetName(text1_name);

  ui::AXNodeData text2;
  text2.id = 12;
  text2.role = ui::AX_ROLE_STATIC_TEXT;
  text2.state = 1 << ui::AX_STATE_READ_ONLY;
  text2.SetName(text2_name);

  ui::AXNodeData button1, button1_text;
  button1.id = 13;
  button1_text.id = 15;
  button1_text.SetName(button1_text_name);
  button1.role = ui::AX_ROLE_BUTTON;
  button1_text.role = ui::AX_ROLE_STATIC_TEXT;
  button1.state = 1 << ui::AX_STATE_READ_ONLY;
  button1_text.state = 1 << ui::AX_STATE_READ_ONLY;
  button1.child_ids.push_back(15);

  ui::AXNodeData link1, link1_text;
  link1.id = 14;
  link1_text.id = 16;
  link1_text.SetName(link1_text_name);
  link1.role = ui::AX_ROLE_LINK;
  link1_text.role = ui::AX_ROLE_STATIC_TEXT;
  link1.state = 1 << ui::AX_STATE_READ_ONLY;
  link1_text.state = 1 << ui::AX_STATE_READ_ONLY;
  link1.child_ids.push_back(16);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.state = 1 << ui::AX_STATE_READ_ONLY;
  root.child_ids.push_back(11);
  root.child_ids.push_back(13);
  root.child_ids.push_back(12);
  root.child_ids.push_back(14);

  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root,
                           text1, button1, button1_text,
                           text2, link1, link1_text),
          NULL, new CountedBrowserAccessibilityFactory()));
  ASSERT_EQ(7, CountedBrowserAccessibility::num_instances());

  BrowserAccessibilityWin* root_obj =
      manager->GetRoot()->ToBrowserAccessibilityWin();

  long text_len;
  ASSERT_EQ(S_OK, root_obj->get_nCharacters(&text_len));

  base::win::ScopedBstr text;
  ASSERT_EQ(S_OK, root_obj->get_text(0, text_len, text.Receive()));
  const std::string embed = base::UTF16ToUTF8(
      BrowserAccessibilityWin::kEmbeddedCharacter);
  EXPECT_EQ(text1_name + embed + text2_name + embed,
            base::UTF16ToUTF8(base::string16(text)));
  text.Reset();

  long hyperlink_count;
  ASSERT_EQ(S_OK, root_obj->get_nHyperlinks(&hyperlink_count));
  EXPECT_EQ(2, hyperlink_count);

  base::win::ScopedComPtr<IAccessibleHyperlink> hyperlink;
  base::win::ScopedComPtr<IAccessibleText> hypertext;
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(-1, hyperlink.Receive()));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(2, hyperlink.Receive()));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(28, hyperlink.Receive()));

  EXPECT_EQ(S_OK, root_obj->get_hyperlink(0, hyperlink.Receive()));
  EXPECT_EQ(S_OK,
            hyperlink.QueryInterface<IAccessibleText>(hypertext.Receive()));
  EXPECT_EQ(S_OK, hypertext->get_text(0, 3, text.Receive()));
  EXPECT_STREQ(button1_text_name.c_str(),
               base::UTF16ToUTF8(base::string16(text)).c_str());
  text.Reset();
  hyperlink.Release();
  hypertext.Release();

  EXPECT_EQ(S_OK, root_obj->get_hyperlink(1, hyperlink.Receive()));
  EXPECT_EQ(S_OK,
            hyperlink.QueryInterface<IAccessibleText>(hypertext.Receive()));
  EXPECT_EQ(S_OK, hypertext->get_text(0, 4, text.Receive()));
  EXPECT_STREQ(link1_text_name.c_str(),
               base::UTF16ToUTF8(base::string16(text)).c_str());
  text.Reset();
  hyperlink.Release();
  hypertext.Release();

  long hyperlink_index;
  EXPECT_EQ(E_FAIL, root_obj->get_hyperlinkIndex(0, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  EXPECT_EQ(E_FAIL, root_obj->get_hyperlinkIndex(28, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  EXPECT_EQ(S_OK, root_obj->get_hyperlinkIndex(14, &hyperlink_index));
  EXPECT_EQ(0, hyperlink_index);
  EXPECT_EQ(S_OK, root_obj->get_hyperlinkIndex(30, &hyperlink_index));
  EXPECT_EQ(1, hyperlink_index);

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestCreateEmptyDocument) {
  // Try creating an empty document with busy state. Readonly is
  // set automatically.
  CountedBrowserAccessibility::reset();
  const int32 busy_state = 1 << ui::AX_STATE_BUSY;
  const int32 readonly_state = 1 << ui::AX_STATE_READ_ONLY;
  const int32 enabled_state = 1 << ui::AX_STATE_ENABLED;
  scoped_ptr<content::LegacyRenderWidgetHostHWND> accessible_hwnd(
      content::LegacyRenderWidgetHostHWND::Create(GetDesktopWindow()));
  scoped_ptr<BrowserAccessibilityManager> manager(
      new BrowserAccessibilityManagerWin(
          accessible_hwnd.get(),
          NULL,
          BrowserAccessibilityManagerWin::GetEmptyDocument(),
          NULL,
          new CountedBrowserAccessibilityFactory()));

  // Verify the root is as we expect by default.
  BrowserAccessibility* root = manager->GetRoot();
  EXPECT_EQ(0, root->GetId());
  EXPECT_EQ(ui::AX_ROLE_ROOT_WEB_AREA, root->GetRole());
  EXPECT_EQ(busy_state | readonly_state | enabled_state, root->GetState());

  // Tree with a child textfield.
  ui::AXNodeData tree1_1;
  tree1_1.id = 1;
  tree1_1.role = ui::AX_ROLE_ROOT_WEB_AREA;
  tree1_1.child_ids.push_back(2);

  ui::AXNodeData tree1_2;
  tree1_2.id = 2;
  tree1_2.role = ui::AX_ROLE_TEXT_FIELD;

  // Process a load complete.
  std::vector<AccessibilityHostMsg_EventParams> params;
  params.push_back(AccessibilityHostMsg_EventParams());
  AccessibilityHostMsg_EventParams* msg = &params[0];
  msg->event_type = ui::AX_EVENT_LOAD_COMPLETE;
  msg->update.nodes.push_back(tree1_1);
  msg->update.nodes.push_back(tree1_2);
  msg->id = tree1_1.id;
  manager->OnAccessibilityEvents(params);

  // Save for later comparison.
  BrowserAccessibility* acc1_2 = manager->GetFromID(2);

  // Verify the root has changed.
  EXPECT_NE(root, manager->GetRoot());

  // And the proper child remains.
  EXPECT_EQ(ui::AX_ROLE_TEXT_FIELD, acc1_2->GetRole());
  EXPECT_EQ(2, acc1_2->GetId());

  // Tree with a child button.
  ui::AXNodeData tree2_1;
  tree2_1.id = 1;
  tree2_1.role = ui::AX_ROLE_ROOT_WEB_AREA;
  tree2_1.child_ids.push_back(3);

  ui::AXNodeData tree2_2;
  tree2_2.id = 3;
  tree2_2.role = ui::AX_ROLE_BUTTON;

  msg->update.nodes.clear();
  msg->update.nodes.push_back(tree2_1);
  msg->update.nodes.push_back(tree2_2);
  msg->id = tree2_1.id;

  // Fire another load complete.
  manager->OnAccessibilityEvents(params);

  BrowserAccessibility* acc2_2 = manager->GetFromID(3);

  // Verify the root has changed.
  EXPECT_NE(root, manager->GetRoot());

  // And the new child exists.
  EXPECT_EQ(ui::AX_ROLE_BUTTON, acc2_2->GetRole());
  EXPECT_EQ(3, acc2_2->GetId());

  // Ensure we properly cleaned up.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST(BrowserAccessibilityManagerWinTest, TestAccessibleHWND) {
  HWND desktop_hwnd = GetDesktopWindow();
  base::win::ScopedComPtr<IAccessible> desktop_hwnd_iaccessible;
  ASSERT_EQ(S_OK, AccessibleObjectFromWindow(
      desktop_hwnd, OBJID_CLIENT,
      IID_IAccessible,
      reinterpret_cast<void**>(desktop_hwnd_iaccessible.Receive())));

  scoped_ptr<content::LegacyRenderWidgetHostHWND> accessible_hwnd(
      content::LegacyRenderWidgetHostHWND::Create(GetDesktopWindow()));

  scoped_ptr<BrowserAccessibilityManagerWin> manager(
      new BrowserAccessibilityManagerWin(
          accessible_hwnd.get(),
          desktop_hwnd_iaccessible,
          BrowserAccessibilityManagerWin::GetEmptyDocument(),
          NULL));
  ASSERT_EQ(desktop_hwnd, manager->parent_hwnd());

  // Enabling screen reader support and calling MaybeCallNotifyWinEvent
  // should trigger creating the AccessibleHWND, and we should now get a
  // new parent_hwnd with the right window class to fool older screen
  // readers.
  BrowserAccessibilityStateImpl::GetInstance()->OnScreenReaderDetected();
  manager->MaybeCallNotifyWinEvent(0, 0);
  HWND new_parent_hwnd = manager->parent_hwnd();
  ASSERT_NE(desktop_hwnd, new_parent_hwnd);
  WCHAR hwnd_class_name[256];
  ASSERT_NE(0, GetClassName(new_parent_hwnd, hwnd_class_name, 256));
  ASSERT_STREQ(L"Chrome_RenderWidgetHostHWND", hwnd_class_name);

  // Destroy the hwnd explicitly; that should trigger clearing parent_hwnd().
  DestroyWindow(new_parent_hwnd);
  ASSERT_EQ(NULL, manager->parent_hwnd());

  // Now create it again.
  accessible_hwnd = content::LegacyRenderWidgetHostHWND::Create(
      GetDesktopWindow());
  manager.reset(
      new BrowserAccessibilityManagerWin(
          accessible_hwnd.get(),
          desktop_hwnd_iaccessible,
          BrowserAccessibilityManagerWin::GetEmptyDocument(),
          NULL));
  manager->MaybeCallNotifyWinEvent(0, 0);
  new_parent_hwnd = manager->parent_hwnd();
  ASSERT_FALSE(NULL == new_parent_hwnd);

  // This time, destroy the manager first, make sure the AccessibleHWND doesn't
  // crash on destruction (to be caught by SyzyASAN or other tools).
  manager.reset(NULL);
}

}  // namespace content
