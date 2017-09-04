// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/view_android.h"

#include <algorithm>

#include "base/android/jni_android.h"
#include "cc/layers/layer.h"
#include "jni/ViewAndroidDelegate_jni.h"
#include "ui/android/window_android.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ui {

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

ViewAndroid::ScopedAnchorView::ScopedAnchorView(
    JNIEnv* env,
    const JavaRef<jobject>& jview,
    const JavaRef<jobject>& jdelegate)
    : view_(env, jview.obj()), delegate_(env, jdelegate.obj()) {
  // If there's a view, then we need a delegate to remove it.
  DCHECK(!jdelegate.is_null() || jview.is_null());
}

ViewAndroid::ScopedAnchorView::ScopedAnchorView() { }

ViewAndroid::ScopedAnchorView::ScopedAnchorView(ScopedAnchorView&& other) {
  view_ = other.view_;
  other.view_.reset();
  delegate_ = other.delegate_;
  other.delegate_.reset();
}

ViewAndroid::ScopedAnchorView&
ViewAndroid::ScopedAnchorView::operator=(ScopedAnchorView&& other) {
  if (this != &other) {
    view_ = other.view_;
    other.view_.reset();
    delegate_ = other.delegate_;
    other.delegate_.reset();
  }
  return *this;
}

ViewAndroid::ScopedAnchorView::~ScopedAnchorView() {
  Reset();
}

void ViewAndroid::ScopedAnchorView::Reset() {
  JNIEnv* env = base::android::AttachCurrentThread();
  const ScopedJavaLocalRef<jobject> view = view_.get(env);
  const ScopedJavaLocalRef<jobject> delegate = delegate_.get(env);
  if (!view.is_null() && !delegate.is_null()) {
    Java_ViewAndroidDelegate_removeView(env, delegate, view);
  }
  view_.reset();
  delegate_.reset();
}

const base::android::ScopedJavaLocalRef<jobject>
ViewAndroid::ScopedAnchorView::view() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return view_.get(env);
}

ViewAndroid::ViewAndroid(const JavaRef<jobject>& delegate)
    : parent_(nullptr)
    , delegate_(base::android::AttachCurrentThread(),
                delegate.obj()) {}

ViewAndroid::ViewAndroid() : parent_(nullptr) {}

ViewAndroid::~ViewAndroid() {
  RemoveFromParent();

  for (std::list<ViewAndroid*>::iterator it = children_.begin();
       it != children_.end(); it++) {
    DCHECK_EQ((*it)->parent_, this);
    (*it)->parent_ = nullptr;
  }
}

void ViewAndroid::SetDelegate(const JavaRef<jobject>& delegate) {
  JNIEnv* env = base::android::AttachCurrentThread();
  delegate_ = JavaObjectWeakGlobalRef(env, delegate);
}

void ViewAndroid::AddChild(ViewAndroid* child) {
  DCHECK(child);
  DCHECK(std::find(children_.begin(), children_.end(), child) ==
         children_.end());

  children_.push_back(child);
  if (child->parent_)
    child->RemoveFromParent();
  child->parent_ = this;
}

void ViewAndroid::RemoveFromParent() {
  if (parent_)
    parent_->RemoveChild(this);
}

ViewAndroid::ScopedAnchorView ViewAndroid::AcquireAnchorView() {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return ViewAndroid::ScopedAnchorView();

  JNIEnv* env = base::android::AttachCurrentThread();
  return ViewAndroid::ScopedAnchorView(
      env, Java_ViewAndroidDelegate_acquireView(env, delegate), delegate);
}

void ViewAndroid::SetAnchorRect(const JavaRef<jobject>& anchor,
                                const gfx::RectF& bounds) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return;

  float scale = display::Screen::GetScreen()
      ->GetDisplayNearestWindow(this)
      .device_scale_factor();
  int left_margin = std::round(bounds.x() * scale);
  // TODO(jinsukkim): Move content_offset() to ViewAndroid, since it's
  //                  specific to a given web contents/render widget.
  int top_margin = std::round(
      (GetWindowAndroid()->content_offset().y() + bounds.y()) * scale);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ViewAndroidDelegate_setViewPosition(
      env, delegate, anchor, bounds.x(), bounds.y(), bounds.width(),
      bounds.height(), scale, left_margin, top_margin);
}

void ViewAndroid::RemoveChild(ViewAndroid* child) {
  DCHECK(child);
  DCHECK_EQ(child->parent_, this);

  std::list<ViewAndroid*>::iterator it =
      std::find(children_.begin(), children_.end(), child);
  DCHECK(it != children_.end());
  children_.erase(it);
  child->parent_ = nullptr;
}

WindowAndroid* ViewAndroid::GetWindowAndroid() const {
  return parent_ ? parent_->GetWindowAndroid() : nullptr;
}

const ScopedJavaLocalRef<jobject> ViewAndroid::GetViewAndroidDelegate()
    const {
  JNIEnv* env = base::android::AttachCurrentThread();
  const ScopedJavaLocalRef<jobject> delegate = delegate_.get(env);
  if (!delegate.is_null())
    return delegate;

  return parent_ ? parent_->GetViewAndroidDelegate() : delegate;
}

cc::Layer* ViewAndroid::GetLayer() const {
  return layer_.get();
}

void ViewAndroid::SetLayer(scoped_refptr<cc::Layer> layer) {
  layer_ = layer;
}

bool ViewAndroid::StartDragAndDrop(const JavaRef<jstring>& jtext,
                                   const JavaRef<jobject>& jimage) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return false;
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ViewAndroidDelegate_startDragAndDrop(env, delegate, jtext,
                                                   jimage);
}

}  // namespace ui
