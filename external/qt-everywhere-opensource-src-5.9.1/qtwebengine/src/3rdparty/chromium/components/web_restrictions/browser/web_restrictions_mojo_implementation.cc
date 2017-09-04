// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_restrictions/browser/web_restrictions_mojo_implementation.h"

#include <utility>

#include "base/bind.h"
#include "components/web_restrictions/browser/web_restrictions_client.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace web_restrictions {

namespace {

void ClientRequestPermissionCallback(
    const mojom::WebRestrictions::RequestPermissionCallback& callback,
    bool result) {
  callback.Run(result);
}

}  // namespace

WebRestrictionsMojoImplementation::WebRestrictionsMojoImplementation(
    WebRestrictionsClient* client)
    : web_restrictions_client_(client) {}

WebRestrictionsMojoImplementation::~WebRestrictionsMojoImplementation() {}

void WebRestrictionsMojoImplementation::Create(
    WebRestrictionsClient* client,
    mojo::InterfaceRequest<mojom::WebRestrictions> request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<WebRestrictionsMojoImplementation>(client),
      std::move(request));
}

void WebRestrictionsMojoImplementation::GetResult(
    const std::string& url,
    const GetResultCallback& callback) {
  std::unique_ptr<const WebRestrictionsClientResult> web_restrictions_result(
      web_restrictions_client_->GetCachedWebRestrictionsResult(url));
  mojom::ClientResultPtr result = mojom::ClientResult::New();
  if (!web_restrictions_result) {
    callback.Run(std::move(result));
    return;
  }
  int columnCount = web_restrictions_result->GetColumnCount();
  for (int i = 0; i < columnCount; i++) {
    if (web_restrictions_result->IsString(i)) {
      result->stringParams[web_restrictions_result->GetColumnName(i)] =
          web_restrictions_result->GetString(i);
    } else {
      result->intParams[web_restrictions_result->GetColumnName(i)] =
          web_restrictions_result->GetInt(i);
    }
  }
  callback.Run(std::move(result));
}

void WebRestrictionsMojoImplementation::RequestPermission(
    const std::string& url,
    const mojom::WebRestrictions::RequestPermissionCallback& callback) {
  web_restrictions_client_->RequestPermission(
      url, base::Bind(&ClientRequestPermissionCallback, callback));
}

}  // namespace web_restrictions
