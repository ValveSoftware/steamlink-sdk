// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_BROWSER_CONTEXTUAL_SEARCH_JS_API_SERVICE_IMPL_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_BROWSER_CONTEXTUAL_SEARCH_JS_API_SERVICE_IMPL_H_

#include "base/macros.h"
#include "components/contextual_search/browser/contextual_search_js_api_handler.h"
#include "components/contextual_search/common/contextual_search_js_api_service.mojom.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace contextual_search {

// This is the receiving end of Contextual Search JavaScript API calls.
class ContextualSearchJsApiServiceImpl
    : public mojom::ContextualSearchJsApiService {
 public:
  ContextualSearchJsApiServiceImpl(
      ContextualSearchJsApiHandler* contextual_search_js_api_handler,
      mojo::InterfaceRequest<mojom::ContextualSearchJsApiService> request);
  ~ContextualSearchJsApiServiceImpl() override;

  // Mojo ContextualSearchApiService implementation.
  void HandleSetCaption(const mojo::String& message, bool does_answer) override;

 private:
  mojo::StrongBinding<mojom::ContextualSearchJsApiService> binding_;

  // The UI handler for calls through the JavaScript API.
  ContextualSearchJsApiHandler* contextual_search_js_api_handler_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchJsApiServiceImpl);
};

// static
void CreateContextualSearchJsApiService(
    ContextualSearchJsApiHandler* contextual_search_js_api_handler,
    mojo::InterfaceRequest<mojom::ContextualSearchJsApiService> request);

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_BROWSER_CONTEXTUAL_SEARCH_JS_API_SERVICE_IMPL_H_
