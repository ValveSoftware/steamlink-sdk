// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/linux/blimp_client_session_linux.h"

#include <string>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "blimp/client/app/linux/blimp_display_manager.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace blimp {
namespace client {
namespace {

const int kDummyTabId = 0;
const char kDefaultAssignerUrl[] =
    "https://blimp-pa.googleapis.com/v1/assignment";

class FakeNavigationFeatureDelegate
    : public NavigationFeature::NavigationFeatureDelegate {
 public:
  FakeNavigationFeatureDelegate();
  ~FakeNavigationFeatureDelegate() override;

  // NavigationFeatureDelegate implementation.
  void OnUrlChanged(int tab_id, const GURL& url) override;
  void OnFaviconChanged(int tab_id, const SkBitmap& favicon) override;
  void OnTitleChanged(int tab_id, const std::string& title) override;
  void OnLoadingChanged(int tab_id, bool loading) override;
  void OnPageLoadStatusUpdate(int tab_id, bool completed) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeNavigationFeatureDelegate);
};

FakeNavigationFeatureDelegate::FakeNavigationFeatureDelegate() {}

FakeNavigationFeatureDelegate::~FakeNavigationFeatureDelegate() {}

void FakeNavigationFeatureDelegate::OnUrlChanged(int tab_id, const GURL& url) {
  DVLOG(1) << "URL changed to " << url << " in tab " << tab_id;
}

void FakeNavigationFeatureDelegate::OnFaviconChanged(int tab_id,
                                                     const SkBitmap& favicon) {
  DVLOG(1) << "Favicon changed in tab " << tab_id;
}

void FakeNavigationFeatureDelegate::OnTitleChanged(int tab_id,
                                                   const std::string& title) {
  DVLOG(1) << "Title changed to " << title << " in tab " << tab_id;
}

void FakeNavigationFeatureDelegate::OnLoadingChanged(int tab_id, bool loading) {
  DVLOG(1) << "Loading status changed to " << loading << " in tab " << tab_id;
}

void FakeNavigationFeatureDelegate::OnPageLoadStatusUpdate(int tab_id,
                                                           bool completed) {
  DVLOG(1) << "Page Load Status changed to completed = " << completed <<
      " in tab " << tab_id;
}

class FakeImeFeatureDelegate : public ImeFeature::Delegate {
 public:
  FakeImeFeatureDelegate();
  ~FakeImeFeatureDelegate() override;

  // ImeFeature::Delegate implementation.
  void OnShowImeRequested(ui::TextInputType input_type,
                          const std::string& text) override;
  void OnHideImeRequested() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeImeFeatureDelegate);
};

FakeImeFeatureDelegate::FakeImeFeatureDelegate() {}

FakeImeFeatureDelegate::~FakeImeFeatureDelegate() {}

void FakeImeFeatureDelegate::OnShowImeRequested(ui::TextInputType input_type,
                                                const std::string& text) {
  DVLOG(1) << "Show IME requested (input_type=" << input_type << ")";
}

void FakeImeFeatureDelegate::OnHideImeRequested() {
  DVLOG(1) << "Hide IME requested";
}

}  // namespace

BlimpClientSessionLinux::BlimpClientSessionLinux()
    : BlimpClientSession(GURL(kDefaultAssignerUrl)),
      event_source_(ui::PlatformEventSource::CreateDefault()),
      navigation_feature_delegate_(new FakeNavigationFeatureDelegate),
      ime_feature_delegate_(new FakeImeFeatureDelegate) {
  blimp_display_manager_.reset(new BlimpDisplayManager(gfx::Size(800, 600),
                                                       this,
                                                       GetRenderWidgetFeature(),
                                                       GetTabControlFeature()));
  GetNavigationFeature()->SetDelegate(kDummyTabId,
                                      navigation_feature_delegate_.get());
  GetImeFeature()->set_delegate(ime_feature_delegate_.get());
}

BlimpClientSessionLinux::~BlimpClientSessionLinux() {}

void BlimpClientSessionLinux::OnClosed() {
  base::MessageLoop::current()->QuitNow();
}

}  // namespace client
}  // namespace blimp
