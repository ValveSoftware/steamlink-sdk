// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/ios/distiller_page_ios.h"

#import <UIKit/UIKit.h>

#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "ios/public/provider/web/web_controller_provider.h"
#include "ios/web/public/browser_state.h"

namespace dom_distiller {

// Helper class for observing the loading of URLs to distill.
class DistillerWebStateObserver : public web::WebStateObserver {
 public:
  DistillerWebStateObserver(web::WebState* web_state,
                            DistillerPageIOS* distiller_page);

  // WebStateObserver implementation:
  void PageLoaded(
      web::PageLoadCompletionStatus load_completion_status) override;

 private:
  DistillerPageIOS* distiller_page_;  // weak, owns this object.
};

DistillerWebStateObserver::DistillerWebStateObserver(
    web::WebState* web_state,
    DistillerPageIOS* distiller_page)
    : web::WebStateObserver(web_state), distiller_page_(distiller_page) {
  DCHECK(web_state);
  DCHECK(distiller_page_);
}

void DistillerWebStateObserver::PageLoaded(
    web::PageLoadCompletionStatus load_completion_status) {
  distiller_page_->OnLoadURLDone(load_completion_status);
}

#pragma mark -

DistillerPageIOS::DistillerPageIOS(web::BrowserState* browser_state)
    : browser_state_(browser_state), weak_ptr_factory_(this) {
}

DistillerPageIOS::~DistillerPageIOS() {
}

bool DistillerPageIOS::StringifyOutput() {
 // UIWebView requires JavaScript to return a single string value.
 return true;
}

void DistillerPageIOS::DistillPageImpl(const GURL& url,
                                       const std::string& script) {
  if (!url.is_valid() || !script.length())
    return;
  url_ = url;
  script_ = script;

  // Lazily create provider.
  if (!provider_) {
    if (ios::GetWebControllerProviderFactory()) {
      provider_ =
          ios::GetWebControllerProviderFactory()->CreateWebControllerProvider(
              browser_state_);
      web_state_observer_.reset(
          new DistillerWebStateObserver(provider_->GetWebState(), this));
    }
  }

  // Load page using provider.
  if (provider_)
    provider_->LoadURL(url_);
  else
    OnLoadURLDone(web::PageLoadCompletionStatus::FAILURE);
}

void DistillerPageIOS::OnLoadURLDone(
    web::PageLoadCompletionStatus load_completion_status) {
  // Don't attempt to distill if the page load failed or if there is no
  // provider.
  if (load_completion_status == web::PageLoadCompletionStatus::FAILURE ||
      !provider_) {
    HandleJavaScriptResultString(@"");
    return;
  }

  // Inject the script.
  base::WeakPtr<DistillerPageIOS> weak_this = weak_ptr_factory_.GetWeakPtr();
  provider_->InjectScript(script_, ^(NSString* string, NSError* error) {
    DistillerPageIOS* distiller_page = weak_this.get();
    if (distiller_page)
      distiller_page->HandleJavaScriptResultString(string);
  });
}

void DistillerPageIOS::HandleJavaScriptResultString(NSString* result) {
  std::unique_ptr<base::Value> resultValue = base::Value::CreateNullValue();
  if (result.length) {
    std::unique_ptr<base::Value> dictionaryValue =
        base::JSONReader::Read(base::SysNSStringToUTF8(result));
    if (dictionaryValue &&
        dictionaryValue->IsType(base::Value::TYPE_DICTIONARY)) {
      resultValue = std::move(dictionaryValue);
    }
  }
  OnDistillationDone(url_, resultValue.get());
}

}  // namespace dom_distiller
