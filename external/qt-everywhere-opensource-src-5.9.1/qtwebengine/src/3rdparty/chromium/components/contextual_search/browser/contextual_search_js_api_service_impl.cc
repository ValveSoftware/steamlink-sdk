// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/browser/contextual_search_js_api_service_impl.h"

#include <utility>

#include "components/contextual_search/browser/contextual_search_js_api_handler.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace contextual_search {

ContextualSearchJsApiServiceImpl::ContextualSearchJsApiServiceImpl(
    ContextualSearchJsApiHandler* contextual_search_js_api_handler)
    : contextual_search_js_api_handler_(contextual_search_js_api_handler) {}

ContextualSearchJsApiServiceImpl::~ContextualSearchJsApiServiceImpl() {}

void ContextualSearchJsApiServiceImpl::HandleSetCaption(
    const std::string& caption,
    bool does_answer) {
  contextual_search_js_api_handler_->SetCaption(caption, does_answer);
}

// static
void CreateContextualSearchJsApiService(
    ContextualSearchJsApiHandler* contextual_search_js_api_handler,
    mojo::InterfaceRequest<mojom::ContextualSearchJsApiService> request) {
  mojo::MakeStrongBinding(base::MakeUnique<ContextualSearchJsApiServiceImpl>(
                              contextual_search_js_api_handler),
                          std::move(request));
}

}  // namespace contextual_search
