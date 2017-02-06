// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node.h"

#include <atlbase.h>
#include <atlcom.h>
#include <oleacc.h>

#include <memory>

#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"
#include "ui/base/win/atl_module.h"

using base::win::ScopedBstr;
using base::win::ScopedComPtr;
using base::win::ScopedVariant;

namespace ui {

namespace {

// Most IAccessible functions require a VARIANT set to CHILDID_SELF as
// the first argument.
ScopedVariant SELF(CHILDID_SELF);

}  // namespace

class AXPlatformNodeWinTest : public testing::Test {
 public:
  AXPlatformNodeWinTest() {}
  ~AXPlatformNodeWinTest() override {}

  void SetUp() override {
    win::CreateATLModuleIfNeeded();
  }

  // Initialize given an AXTreeUpdate.
  void Init(const AXTreeUpdate& initial_state) {
    tree_.reset(new AXTree(initial_state));
  }

  // Convenience functions to initialize directly from a few AXNodeDatas.
  void Init(const AXNodeData& node1) {
    AXTreeUpdate update;
    update.root_id = node1.id;
    update.nodes.push_back(node1);
    Init(update);
  }

  void Init(const AXNodeData& node1,
            const AXNodeData& node2) {
    AXTreeUpdate update;
    update.root_id = node1.id;
    update.nodes.push_back(node1);
    update.nodes.push_back(node2);
    Init(update);
  }

  void Init(const AXNodeData& node1,
            const AXNodeData& node2,
            const AXNodeData& node3) {
    AXTreeUpdate update;
    update.root_id = node1.id;
    update.nodes.push_back(node1);
    update.nodes.push_back(node2);
    update.nodes.push_back(node3);
    Init(update);
  }

 protected:
  AXNode* GetRootNode() {
    return tree_->root();
  }

  ScopedComPtr<IAccessible> IAccessibleFromNode(AXNode* node) {
    TestAXNodeWrapper* wrapper =
        TestAXNodeWrapper::GetOrCreate(tree_.get(), node);
    AXPlatformNode* ax_platform_node = wrapper->ax_platform_node();
    IAccessible* iaccessible = ax_platform_node->GetNativeViewAccessible();
    iaccessible->AddRef();
    return ScopedComPtr<IAccessible>(iaccessible);
  }

  ScopedComPtr<IAccessible> GetRootIAccessible() {
    return IAccessibleFromNode(GetRootNode());
  }

  ScopedComPtr<IAccessible2> ToIAccessible2(
      ScopedComPtr<IAccessible> accessible) {
    ScopedComPtr<IServiceProvider> service_provider;
    service_provider.QueryFrom(accessible.get());
    ScopedComPtr<IAccessible2> result;
    CHECK(SUCCEEDED(
        service_provider->QueryService(IID_IAccessible2, result.Receive())));
    return result;
  }

  std::unique_ptr<AXTree> tree_;
};

TEST_F(AXPlatformNodeWinTest, TestIAccessibleDetachedObject) {
  AXNodeData root;
  root.id = 1;
  root.AddStringAttribute(AX_ATTR_NAME, "Name");
  Init(root);

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());
  ScopedBstr name;
  ASSERT_EQ(S_OK, root_obj->get_accName(SELF, name.Receive()));
  EXPECT_EQ(L"Name", base::string16(name));

  tree_.reset(new AXTree());
  ScopedBstr name2;
  ASSERT_EQ(E_FAIL, root_obj->get_accName(SELF, name2.Receive()));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleName) {
  AXNodeData root;
  root.id = 1;
  root.AddStringAttribute(AX_ATTR_NAME, "Name");
  Init(root);

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());
  ScopedBstr name;
  ASSERT_EQ(S_OK, root_obj->get_accName(SELF, name.Receive()));
  EXPECT_EQ(L"Name", base::string16(name));

  ASSERT_EQ(E_INVALIDARG, root_obj->get_accName(SELF, nullptr));
  ScopedVariant bad_id(999);
  ScopedBstr name2;
  ASSERT_EQ(E_INVALIDARG, root_obj->get_accName(bad_id, name2.Receive()));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleDescription) {
  AXNodeData root;
  root.id = 1;
  root.AddStringAttribute(AX_ATTR_DESCRIPTION, "Description");
  Init(root);

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());
  ScopedBstr description;
  ASSERT_EQ(S_OK, root_obj->get_accDescription(SELF, description.Receive()));
  EXPECT_EQ(L"Description", base::string16(description));

  ASSERT_EQ(E_INVALIDARG, root_obj->get_accDescription(SELF, nullptr));
  ScopedVariant bad_id(999);
  ScopedBstr d2;
  ASSERT_EQ(E_INVALIDARG, root_obj->get_accDescription(bad_id, d2.Receive()));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleValue) {
  AXNodeData root;
  root.id = 1;
  root.AddStringAttribute(AX_ATTR_VALUE, "Value");
  Init(root);

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());
  ScopedBstr value;
  ASSERT_EQ(S_OK, root_obj->get_accValue(SELF, value.Receive()));
  EXPECT_EQ(L"Value", base::string16(value));

  ASSERT_EQ(E_INVALIDARG, root_obj->get_accValue(SELF, nullptr));
  ScopedVariant bad_id(999);
  ScopedBstr v2;
  ASSERT_EQ(E_INVALIDARG, root_obj->get_accValue(bad_id, v2.Receive()));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleShortcut) {
  AXNodeData root;
  root.id = 1;
  root.AddStringAttribute(AX_ATTR_SHORTCUT, "Shortcut");
  Init(root);

  ScopedComPtr<IAccessible> root_obj(GetRootIAccessible());
  ScopedBstr shortcut;
  ASSERT_EQ(S_OK, root_obj->get_accKeyboardShortcut(
      SELF, shortcut.Receive()));
  EXPECT_EQ(L"Shortcut", base::string16(shortcut));

  ASSERT_EQ(E_INVALIDARG, root_obj->get_accKeyboardShortcut(SELF, nullptr));
  ScopedVariant bad_id(999);
  ScopedBstr k2;
  ASSERT_EQ(E_INVALIDARG, root_obj->get_accKeyboardShortcut(
      bad_id, k2.Receive()));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleRole) {
  AXNodeData root;
  root.id = 1;
  root.child_ids.push_back(2);

  AXNodeData child;
  child.id = 2;

  Init(root, child);
  AXNode* child_node = GetRootNode()->children()[0];
  ScopedComPtr<IAccessible> child_iaccessible(
      IAccessibleFromNode(child_node));

  ScopedVariant role;

  child.role = AX_ROLE_ALERT;
  child_node->SetData(child);
  ASSERT_EQ(S_OK, child_iaccessible->get_accRole(SELF, role.Receive()));
  EXPECT_EQ(ROLE_SYSTEM_ALERT, V_I4(role.ptr()));

  child.role = AX_ROLE_BUTTON;
  child_node->SetData(child);
  ASSERT_EQ(S_OK, child_iaccessible->get_accRole(SELF, role.Receive()));
  EXPECT_EQ(ROLE_SYSTEM_PUSHBUTTON, V_I4(role.ptr()));

  child.role = AX_ROLE_POP_UP_BUTTON;
  child_node->SetData(child);
  ASSERT_EQ(S_OK, child_iaccessible->get_accRole(SELF, role.Receive()));
  EXPECT_EQ(ROLE_SYSTEM_BUTTONMENU, V_I4(role.ptr()));

  ASSERT_EQ(E_INVALIDARG, child_iaccessible->get_accRole(SELF, nullptr));
  ScopedVariant bad_id(999);
  ASSERT_EQ(E_INVALIDARG, child_iaccessible->get_accRole(
      bad_id, role.Receive()));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleLocation) {
  AXNodeData root;
  root.id = 1;
  root.location = gfx::Rect(10, 40, 800, 600);
  Init(root);

  TestAXNodeWrapper::SetGlobalCoordinateOffset(gfx::Vector2d(100, 200));

  LONG x_left, y_top, width, height;
  ASSERT_EQ(S_OK, GetRootIAccessible()->accLocation(
      &x_left, &y_top, &width, &height, SELF));
  EXPECT_EQ(110, x_left);
  EXPECT_EQ(240, y_top);
  EXPECT_EQ(800, width);
  EXPECT_EQ(600, height);

  ASSERT_EQ(E_INVALIDARG, GetRootIAccessible()->accLocation(
      nullptr, &y_top, &width, &height, SELF));
  ASSERT_EQ(E_INVALIDARG, GetRootIAccessible()->accLocation(
      &x_left, nullptr, &width, &height, SELF));
  ASSERT_EQ(E_INVALIDARG, GetRootIAccessible()->accLocation(
      &x_left, &y_top, nullptr, &height, SELF));
  ASSERT_EQ(E_INVALIDARG, GetRootIAccessible()->accLocation(
      &x_left, &y_top, &width, nullptr, SELF));
  ScopedVariant bad_id(999);
  ASSERT_EQ(E_INVALIDARG, GetRootIAccessible()->accLocation(
      &x_left, &y_top, &width, &height, bad_id));
}

TEST_F(AXPlatformNodeWinTest, TestIAccessibleChildAndParent) {
  AXNodeData root;
  root.id = 1;
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  AXNodeData button;
  button.role = AX_ROLE_BUTTON;
  button.id = 2;

  AXNodeData checkbox;
  checkbox.role = AX_ROLE_CHECK_BOX;
  checkbox.id = 3;

  Init(root, button, checkbox);
  AXNode* button_node = GetRootNode()->children()[0];
  AXNode* checkbox_node = GetRootNode()->children()[1];
  ScopedComPtr<IAccessible> root_iaccessible(GetRootIAccessible());
  ScopedComPtr<IAccessible> button_iaccessible(
      IAccessibleFromNode(button_node));
  ScopedComPtr<IAccessible> checkbox_iaccessible(
      IAccessibleFromNode(checkbox_node));

  LONG child_count;
  ASSERT_EQ(S_OK, root_iaccessible->get_accChildCount(&child_count));
  ASSERT_EQ(2L, child_count);
  ASSERT_EQ(S_OK, button_iaccessible->get_accChildCount(&child_count));
  ASSERT_EQ(0L, child_count);
  ASSERT_EQ(S_OK, checkbox_iaccessible->get_accChildCount(&child_count));
  ASSERT_EQ(0L, child_count);

  {
    ScopedComPtr<IDispatch> result;
    ASSERT_EQ(S_OK, root_iaccessible->get_accChild(SELF, result.Receive()));
    ASSERT_EQ(result.get(), root_iaccessible);
  }

  {
    ScopedComPtr<IDispatch> result;
    ScopedVariant child1(1);
    ASSERT_EQ(S_OK, root_iaccessible->get_accChild(child1, result.Receive()));
    ASSERT_EQ(result.get(), button_iaccessible);
  }

  {
    ScopedComPtr<IDispatch> result;
    ScopedVariant child2(2);
    ASSERT_EQ(S_OK, root_iaccessible->get_accChild(child2, result.Receive()));
    ASSERT_EQ(result.get(), checkbox_iaccessible);
  }

  {
    // Asking for child id 3 should fail.
    ScopedComPtr<IDispatch> result;
    ScopedVariant child3(3);
    ASSERT_EQ(E_INVALIDARG,
              root_iaccessible->get_accChild(child3, result.Receive()));
  }

  // We should be able to ask for the button by its unique id too.
  LONG button_unique_id;
  ScopedComPtr<IAccessible2> button_iaccessible2 =
      ToIAccessible2(button_iaccessible);
  button_iaccessible2->get_uniqueID(&button_unique_id);
  ASSERT_LT(button_unique_id, 0);
  {
    ScopedComPtr<IDispatch> result;
    ScopedVariant button_id_variant(button_unique_id);
    ASSERT_EQ(S_OK, root_iaccessible->get_accChild(button_id_variant,
                                                   result.Receive()));
    ASSERT_EQ(result.get(), button_iaccessible);
  }

  // We shouldn't be able to ask for the root node by its unique ID
  // from one of its children, though.
  LONG root_unique_id;
  ScopedComPtr<IAccessible2> root_iaccessible2 =
      ToIAccessible2(root_iaccessible);
  root_iaccessible2->get_uniqueID(&root_unique_id);
  ASSERT_LT(root_unique_id, 0);
  {
    ScopedComPtr<IDispatch> result;
    ScopedVariant root_id_variant(root_unique_id);
    ASSERT_EQ(E_INVALIDARG, button_iaccessible->get_accChild(root_id_variant,
                                                             result.Receive()));
  }

  // Now check parents.
  {
    ScopedComPtr<IDispatch> result;
    ASSERT_EQ(S_OK, button_iaccessible->get_accParent(result.Receive()));
    ASSERT_EQ(result.get(), root_iaccessible);
  }

  {
    ScopedComPtr<IDispatch> result;
    ASSERT_EQ(S_OK, checkbox_iaccessible->get_accParent(result.Receive()));
    ASSERT_EQ(result.get(), root_iaccessible);
  }

  {
    ScopedComPtr<IDispatch> result;
    ASSERT_EQ(S_FALSE, root_iaccessible->get_accParent(result.Receive()));
  }
}

TEST_F(AXPlatformNodeWinTest, TestIAccessible2IndexInParent) {
  AXNodeData root;
  root.id = 1;
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  AXNodeData left;
  left.id = 2;

  AXNodeData right;
  right.id = 3;

  Init(root, left, right);
  ScopedComPtr<IAccessible> root_iaccessible(GetRootIAccessible());
  ScopedComPtr<IAccessible2> root_iaccessible2 =
      ToIAccessible2(root_iaccessible);
  ScopedComPtr<IAccessible> left_iaccessible(
      IAccessibleFromNode(GetRootNode()->children()[0]));
  ScopedComPtr<IAccessible2> left_iaccessible2 =
      ToIAccessible2(left_iaccessible);
  ScopedComPtr<IAccessible> right_iaccessible(
      IAccessibleFromNode(GetRootNode()->children()[1]));
  ScopedComPtr<IAccessible2> right_iaccessible2 =
      ToIAccessible2(right_iaccessible);

  LONG index;
  ASSERT_EQ(E_FAIL, root_iaccessible2->get_indexInParent(&index));

  ASSERT_EQ(S_OK, left_iaccessible2->get_indexInParent(&index));
  EXPECT_EQ(0, index);

  ASSERT_EQ(S_OK, right_iaccessible2->get_indexInParent(&index));
  EXPECT_EQ(1, index);
}

}  // namespace ui
