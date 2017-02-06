// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_base.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

void AXPlatformNodeBase::Init(AXPlatformNodeDelegate* delegate) {
  delegate_ = delegate;
}

const AXNodeData& AXPlatformNodeBase::GetData() const {
  CHECK(delegate_);
  return delegate_->GetData();
}

gfx::Rect AXPlatformNodeBase::GetBoundsInScreen() const {
  CHECK(delegate_);
  gfx::Rect bounds = GetData().location;
  bounds.Offset(delegate_->GetGlobalCoordinateOffset());
  return bounds;
}

gfx::NativeViewAccessible AXPlatformNodeBase::GetParent() {
  CHECK(delegate_);
  return delegate_->GetParent();
}

int AXPlatformNodeBase::GetChildCount() {
  CHECK(delegate_);
  return delegate_->GetChildCount();
}

gfx::NativeViewAccessible AXPlatformNodeBase::ChildAtIndex(int index) {
  CHECK(delegate_);
  return delegate_->ChildAtIndex(index);
}

// AXPlatformNode overrides.

void AXPlatformNodeBase::Destroy() {
  AXPlatformNode::Destroy();
  delegate_ = nullptr;
  Dispose();
}

void AXPlatformNodeBase::Dispose() {
  delete this;
}

gfx::NativeViewAccessible AXPlatformNodeBase::GetNativeViewAccessible() {
  return nullptr;
}

AXPlatformNodeDelegate* AXPlatformNodeBase::GetDelegate() const {
  return delegate_;
}

// Helpers.

AXPlatformNodeBase* AXPlatformNodeBase::GetPreviousSibling() {
  CHECK(delegate_);
  gfx::NativeViewAccessible parent_accessible = GetParent();
  AXPlatformNodeBase* parent = FromNativeViewAccessible(parent_accessible);
  if (!parent)
    return nullptr;

  int previous_index = GetIndexInParent() - 1;
  if (previous_index >= 0 &&
      previous_index < parent->GetChildCount()) {
    return FromNativeViewAccessible(parent->ChildAtIndex(previous_index));
  }
  return nullptr;
}

AXPlatformNodeBase* AXPlatformNodeBase::GetNextSibling() {
  CHECK(delegate_);
  gfx::NativeViewAccessible parent_accessible = GetParent();
  AXPlatformNodeBase* parent = FromNativeViewAccessible(parent_accessible);
  if (!parent)
    return nullptr;

  int next_index = GetIndexInParent() + 1;
  if (next_index >= 0 && next_index < parent->GetChildCount())
    return FromNativeViewAccessible(parent->ChildAtIndex(next_index));
  return nullptr;
}

bool AXPlatformNodeBase::IsDescendant(AXPlatformNodeBase* node) {
  CHECK(delegate_);
  if (!node)
    return false;
  if (node == this)
    return true;
  gfx::NativeViewAccessible native_parent = node->GetParent();
  if (!native_parent)
    return false;
  AXPlatformNodeBase* parent = FromNativeViewAccessible(native_parent);
  return IsDescendant(parent);
}

bool AXPlatformNodeBase::HasBoolAttribute(
    ui::AXBoolAttribute attribute) const {
  CHECK(delegate_);
  return GetData().HasBoolAttribute(attribute);
}

bool AXPlatformNodeBase::GetBoolAttribute(
    ui::AXBoolAttribute attribute) const {
  CHECK(delegate_);
  return GetData().GetBoolAttribute(attribute);
}

bool AXPlatformNodeBase::GetBoolAttribute(
    ui::AXBoolAttribute attribute, bool* value) const {
  CHECK(delegate_);
  return GetData().GetBoolAttribute(attribute, value);
}

bool AXPlatformNodeBase::HasFloatAttribute(
    ui::AXFloatAttribute attribute) const {
  CHECK(delegate_);
  return GetData().HasFloatAttribute(attribute);
}

float AXPlatformNodeBase::GetFloatAttribute(
    ui::AXFloatAttribute attribute) const {
  CHECK(delegate_);
  return GetData().GetFloatAttribute(attribute);
}

bool AXPlatformNodeBase::GetFloatAttribute(
    ui::AXFloatAttribute attribute, float* value) const {
  CHECK(delegate_);
  return GetData().GetFloatAttribute(attribute, value);
}

bool AXPlatformNodeBase::HasIntAttribute(
    ui::AXIntAttribute attribute) const {
  CHECK(delegate_);
  return GetData().HasIntAttribute(attribute);
}

int AXPlatformNodeBase::GetIntAttribute(
    ui::AXIntAttribute attribute) const {
  CHECK(delegate_);
  return GetData().GetIntAttribute(attribute);
}

bool AXPlatformNodeBase::GetIntAttribute(
    ui::AXIntAttribute attribute, int* value) const {
  CHECK(delegate_);
  return GetData().GetIntAttribute(attribute, value);
}

bool AXPlatformNodeBase::HasStringAttribute(
    ui::AXStringAttribute attribute) const {
  CHECK(delegate_);
  return GetData().HasStringAttribute(attribute);
}

const std::string& AXPlatformNodeBase::GetStringAttribute(
    ui::AXStringAttribute attribute) const {
  CHECK(delegate_);
  return GetData().GetStringAttribute(attribute);
}

bool AXPlatformNodeBase::GetStringAttribute(
    ui::AXStringAttribute attribute, std::string* value) const {
  CHECK(delegate_);
  return GetData().GetStringAttribute(attribute, value);
}

base::string16 AXPlatformNodeBase::GetString16Attribute(
    ui::AXStringAttribute attribute) const {
  CHECK(delegate_);
  return GetData().GetString16Attribute(attribute);
}

bool AXPlatformNodeBase::GetString16Attribute(
    ui::AXStringAttribute attribute,
    base::string16* value) const {
  CHECK(delegate_);
  return GetData().GetString16Attribute(attribute, value);
}

AXPlatformNodeBase::AXPlatformNodeBase() {
}

AXPlatformNodeBase::~AXPlatformNodeBase() {
  CHECK(!delegate_);
}

// static
AXPlatformNodeBase* AXPlatformNodeBase::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  return static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(accessible));
}

}  // namespace ui
