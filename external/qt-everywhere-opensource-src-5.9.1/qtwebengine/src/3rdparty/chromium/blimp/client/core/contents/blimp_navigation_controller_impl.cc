// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/blimp_navigation_controller_impl.h"

#include "blimp/client/core/contents/blimp_navigation_controller_delegate.h"

namespace blimp {
namespace client {

BlimpNavigationControllerImpl::BlimpNavigationControllerImpl(
    int blimp_contents_id,
    BlimpNavigationControllerDelegate* delegate,
    NavigationFeature* feature)
    : blimp_contents_id_(blimp_contents_id),
      navigation_feature_(feature),
      delegate_(delegate) {
  if (navigation_feature_)
    navigation_feature_->SetDelegate(blimp_contents_id_, this);
}

BlimpNavigationControllerImpl::~BlimpNavigationControllerImpl() {
  if (navigation_feature_)
    navigation_feature_->RemoveDelegate(blimp_contents_id_);
}

void BlimpNavigationControllerImpl::LoadURL(const GURL& url) {
  current_url_ = url;
  navigation_feature_->NavigateToUrlText(blimp_contents_id_,
                                         current_url_.spec());
}

void BlimpNavigationControllerImpl::Reload() {
  navigation_feature_->Reload(blimp_contents_id_);
}

bool BlimpNavigationControllerImpl::CanGoBack() const {
  NOTIMPLEMENTED();
  return true;
}

bool BlimpNavigationControllerImpl::CanGoForward() const {
  NOTIMPLEMENTED();
  return true;
}

void BlimpNavigationControllerImpl::GoBack() {
  navigation_feature_->GoBack(blimp_contents_id_);
}

void BlimpNavigationControllerImpl::GoForward() {
  navigation_feature_->GoForward(blimp_contents_id_);
}

const GURL& BlimpNavigationControllerImpl::GetURL() {
  return current_url_;
}

const std::string& BlimpNavigationControllerImpl::GetTitle() {
  return current_title_;
}

void BlimpNavigationControllerImpl::OnUrlChanged(int tab_id, const GURL& url) {
  current_url_ = url;
  delegate_->OnNavigationStateChanged();
}

void BlimpNavigationControllerImpl::OnFaviconChanged(int tab_id,
                                                     const SkBitmap& favicon) {
  delegate_->OnNavigationStateChanged();
}

void BlimpNavigationControllerImpl::OnTitleChanged(int tab_id,
                                                   const std::string& title) {
  current_title_ = title;
  delegate_->OnNavigationStateChanged();
}

void BlimpNavigationControllerImpl::OnLoadingChanged(int tab_id, bool loading) {
  delegate_->OnNavigationStateChanged();
  delegate_->OnLoadingStateChanged(loading);
}

void BlimpNavigationControllerImpl::OnPageLoadStatusUpdate(int tab_id,
                                                           bool completed) {
  delegate_->OnNavigationStateChanged();
  delegate_->OnPageLoadingStateChanged(!completed);
}

}  // namespace client
}  // namespace blimp
