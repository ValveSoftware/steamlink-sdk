// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/blimp_contents_impl.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/supports_user_data.h"
#include "blimp/client/core/contents/tab_control_feature.h"
#include "blimp/client/public/contents/blimp_contents_observer.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "blimp/client/core/contents/android/blimp_contents_impl_android.h"
#endif  // OS_ANDROID

namespace blimp {
namespace client {

namespace {

#if defined(OS_ANDROID)
const char kBlimpContentsImplAndroidKey[] = "blimp_contents_impl_android";
#endif  // OS_ANDROID
}

BlimpContentsImpl::BlimpContentsImpl(
    int id,
    gfx::NativeWindow window,
    BlimpCompositorDependencies* compositor_deps,
    ImeFeature* ime_feature,
    NavigationFeature* navigation_feature,
    RenderWidgetFeature* render_widget_feature,
    TabControlFeature* tab_control_feature)
    : navigation_controller_(id, this, navigation_feature),
      document_manager_(id, render_widget_feature, compositor_deps),
      id_(id),
      ime_feature_(ime_feature),
      window_(window),
      tab_control_feature_(tab_control_feature) {
  blimp_contents_view_ =
      BlimpContentsViewImpl::Create(this, document_manager_.layer());
  ime_feature_->set_delegate(blimp_contents_view_->GetImeDelegate());
}

BlimpContentsImpl::~BlimpContentsImpl() {
  for (auto& observer : observers_)
    observer.BlimpContentsDying();
  ime_feature_->set_delegate(nullptr);
}

#if defined(OS_ANDROID)

base::android::ScopedJavaLocalRef<jobject> BlimpContentsImpl::GetJavaObject() {
  return GetBlimpContentsImplAndroid()->GetJavaObject();
}

BlimpContentsImplAndroid* BlimpContentsImpl::GetBlimpContentsImplAndroid() {
  BlimpContentsImplAndroid* blimp_contents_impl_android =
      static_cast<BlimpContentsImplAndroid*>(
          GetUserData(kBlimpContentsImplAndroidKey));
  if (!blimp_contents_impl_android) {
    blimp_contents_impl_android = new BlimpContentsImplAndroid(this);
    SetUserData(kBlimpContentsImplAndroidKey, blimp_contents_impl_android);
  }
  return blimp_contents_impl_android;
}

#endif  // defined(OS_ANDROID)

BlimpNavigationControllerImpl& BlimpContentsImpl::GetNavigationController() {
  return navigation_controller_;
}

gfx::NativeWindow BlimpContentsImpl::GetNativeWindow() {
  return window_;
}

void BlimpContentsImpl::AddObserver(BlimpContentsObserver* observer) {
  observers_.AddObserver(observer);
}

void BlimpContentsImpl::RemoveObserver(BlimpContentsObserver* observer) {
  observers_.RemoveObserver(observer);
}

BlimpContentsViewImpl* BlimpContentsImpl::GetView() {
  return blimp_contents_view_.get();
}

void BlimpContentsImpl::Show() {
  document_manager_.SetVisible(true);
  UMA_HISTOGRAM_BOOLEAN("Blimp.Tab.Visible", true);
}

void BlimpContentsImpl::Hide() {
  document_manager_.SetVisible(false);
  UMA_HISTOGRAM_BOOLEAN("Blimp.Tab.Visible", false);
}

bool BlimpContentsImpl::HasObserver(BlimpContentsObserver* observer) {
  return observers_.HasObserver(observer);
}

void BlimpContentsImpl::OnNavigationStateChanged() {
  for (auto& observer : observers_)
    observer.OnNavigationStateChanged();
}

void BlimpContentsImpl::OnLoadingStateChanged(bool loading) {
  for (auto& observer : observers_)
    observer.OnLoadingStateChanged(loading);
}

void BlimpContentsImpl::OnPageLoadingStateChanged(bool loading) {
  for (auto& observer : observers_)
    observer.OnPageLoadingStateChanged(loading);
}

void BlimpContentsImpl::SetSizeAndScale(const gfx::Size& size,
                                        float device_pixel_ratio) {
  tab_control_feature_->SetSizeAndScale(size, device_pixel_ratio);
}

}  // namespace client
}  // namespace blimp
