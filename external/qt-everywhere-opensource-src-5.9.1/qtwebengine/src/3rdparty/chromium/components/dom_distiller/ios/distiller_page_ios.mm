// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/ios/distiller_page_ios.h"

#import <UIKit/UIKit.h>

#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "ios/public/provider/web/web_controller_provider.h"
#include "ios/web/public/browser_state.h"

namespace {

// This is duplicated here from ios/web/web_state/ui/web_view_js_utils.mm in
// order to handle numbers. The dom distiller proto expects integers and the
// generated JSON deserializer does not accept doubles in the place of ints.
// However WKWebView only returns "numbers." However, here the proto expects
// integers and doubles, which is done by checking if the number has a fraction
// or not; since this is a hacky method it's isolated to this file so as to
// limit the risk of broken JS calls.

int const kMaximumParsingRecursionDepth = 6;
// Converts result of WKWebView script evaluation to base::Value, parsing
// |wk_result| up to a depth of |max_depth|.
std::unique_ptr<base::Value> ValueResultFromScriptResult(id wk_result,
                                                         int max_depth) {
  if (!wk_result)
    return nullptr;

  std::unique_ptr<base::Value> result;

  if (max_depth < 0) {
    DLOG(WARNING) << "JS maximum recursion depth exceeded.";
    return result;
  }

  CFTypeID result_type = CFGetTypeID(wk_result);
  if (result_type == CFStringGetTypeID()) {
    result.reset(new base::StringValue(base::SysNSStringToUTF16(wk_result)));
    DCHECK(result->IsType(base::Value::TYPE_STRING));
  } else if (result_type == CFNumberGetTypeID()) {
    // Different implementation is here.
    if ([wk_result intValue] != [wk_result doubleValue]) {
      result.reset(new base::FundamentalValue([wk_result doubleValue]));
      DCHECK(result->IsType(base::Value::TYPE_DOUBLE));
    } else {
      result.reset(new base::FundamentalValue([wk_result intValue]));
      DCHECK(result->IsType(base::Value::TYPE_INTEGER));
    }
    // End of different implementation.
  } else if (result_type == CFBooleanGetTypeID()) {
    result.reset(
        new base::FundamentalValue(static_cast<bool>([wk_result boolValue])));
    DCHECK(result->IsType(base::Value::TYPE_BOOLEAN));
  } else if (result_type == CFNullGetTypeID()) {
    result = base::Value::CreateNullValue();
    DCHECK(result->IsType(base::Value::TYPE_NULL));
  } else if (result_type == CFDictionaryGetTypeID()) {
    std::unique_ptr<base::DictionaryValue> dictionary =
        base::MakeUnique<base::DictionaryValue>();
    for (id key in wk_result) {
      NSString* obj_c_string = base::mac::ObjCCast<NSString>(key);
      const std::string path = base::SysNSStringToUTF8(obj_c_string);
      std::unique_ptr<base::Value> value =
          ValueResultFromScriptResult(wk_result[obj_c_string], max_depth - 1);
      if (value) {
        dictionary->Set(path, std::move(value));
      }
    }
    result = std::move(dictionary);
  } else if (result_type == CFArrayGetTypeID()) {
    std::unique_ptr<base::ListValue> list = base::MakeUnique<base::ListValue>();
    for (id list_item in wk_result) {
      std::unique_ptr<base::Value> value =
      ValueResultFromScriptResult(list_item, max_depth - 1);
      if (value) {
        list->Append(std::move(value));
      }
    }
    result = std::move(list);
  } else {
    NOTREACHED();  // Convert other types as needed.
  }
  return result;
}
}

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
  return false;
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
    HandleJavaScriptResult(nil);
    return;
  }

  // Inject the script.
  base::WeakPtr<DistillerPageIOS> weak_this = weak_ptr_factory_.GetWeakPtr();
  provider_->InjectScript(script_, ^(id result, NSError* error) {
    DistillerPageIOS* distiller_page = weak_this.get();
    if (distiller_page)
      distiller_page->HandleJavaScriptResult(result);
  });
}

void DistillerPageIOS::HandleJavaScriptResult(id result) {
  std::unique_ptr<base::Value> resultValue = base::Value::CreateNullValue();
  if (result) {
    resultValue = ValueResultFromScriptResult(result);
  }
  OnDistillationDone(url_, resultValue.get());
}

std::unique_ptr<base::Value> DistillerPageIOS::ValueResultFromScriptResult(
    id wk_result) {
  return ::ValueResultFromScriptResult(wk_result,
                                       kMaximumParsingRecursionDepth);
}
}  // namespace dom_distiller
