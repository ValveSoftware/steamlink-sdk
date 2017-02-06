// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/renderer/credential_manager_client.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "components/password_manager/content/public/cpp/type_converters.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/WebCredential.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerError.h"
#include "third_party/WebKit/public/platform/WebFederatedCredential.h"
#include "third_party/WebKit/public/platform/WebPasswordCredential.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace password_manager {

namespace {

blink::WebCredentialManagerError GetWebCredentialManagerErrorFromMojo(
    mojom::CredentialManagerError error) {
  switch (error) {
    case mojom::CredentialManagerError::DISABLED:
      return blink::WebCredentialManagerError::
          WebCredentialManagerDisabledError;
    case mojom::CredentialManagerError::PENDINGREQUEST:
      return blink::WebCredentialManagerError::
          WebCredentialManagerPendingRequestError;
    case mojom::CredentialManagerError::PASSWORDSTOREUNAVAILABLE:
      return blink::WebCredentialManagerError::
          WebCredentialManagerPasswordStoreUnavailableError;
    case mojom::CredentialManagerError::UNKNOWN:
      return blink::WebCredentialManagerError::WebCredentialManagerUnknownError;
    case mojom::CredentialManagerError::SUCCESS:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return blink::WebCredentialManagerError::WebCredentialManagerUnknownError;
}

// Takes ownership of blink::WebCredentialManagerClient::NotificationCallbacks
// pointer. When the wrapper is destroyed, if |callbacks| is still alive
// its onError() will get called.
class NotificationCallbacksWrapper {
 public:
  explicit NotificationCallbacksWrapper(
      blink::WebCredentialManagerClient::NotificationCallbacks* callbacks);

  ~NotificationCallbacksWrapper();

  void NotifySuccess();

 private:
  std::unique_ptr<blink::WebCredentialManagerClient::NotificationCallbacks>
      callbacks_;

  DISALLOW_COPY_AND_ASSIGN(NotificationCallbacksWrapper);
};

NotificationCallbacksWrapper::NotificationCallbacksWrapper(
    blink::WebCredentialManagerClient::NotificationCallbacks* callbacks)
    : callbacks_(callbacks) {}

NotificationCallbacksWrapper::~NotificationCallbacksWrapper() {
  if (callbacks_)
    callbacks_->onError(blink::WebCredentialManagerUnknownError);
}

void NotificationCallbacksWrapper::NotifySuccess() {
  // Call onSuccess() and reset callbacks to avoid calling onError() in
  // destructor.
  if (callbacks_) {
    callbacks_->onSuccess();
    callbacks_.reset();
  }
}

// Takes ownership of blink::WebCredentialManagerClient::RequestCallbacks
// pointer. When the wrapper is destroied, if |callbacks| is still alive
// its onError() will get called.
class RequestCallbacksWrapper {
 public:
  explicit RequestCallbacksWrapper(
      blink::WebCredentialManagerClient::RequestCallbacks* callbacks);

  ~RequestCallbacksWrapper();

  void NotifySuccess(mojom::CredentialInfoPtr info);

  void NotifyError(mojom::CredentialManagerError error);

 private:
  std::unique_ptr<blink::WebCredentialManagerClient::RequestCallbacks>
      callbacks_;

  DISALLOW_COPY_AND_ASSIGN(RequestCallbacksWrapper);
};

RequestCallbacksWrapper::RequestCallbacksWrapper(
    blink::WebCredentialManagerClient::RequestCallbacks* callbacks)
    : callbacks_(callbacks) {}

RequestCallbacksWrapper::~RequestCallbacksWrapper() {
  if (callbacks_)
    callbacks_->onError(blink::WebCredentialManagerUnknownError);
}

void RequestCallbacksWrapper::NotifySuccess(mojom::CredentialInfoPtr info) {
  // Call onSuccess() and reset callbacks to avoid calling onError() in
  // destructor.
  if (callbacks_) {
    std::unique_ptr<blink::WebCredential> credential =
        info.To<std::unique_ptr<blink::WebCredential>>();
    callbacks_->onSuccess(std::move(credential));
    callbacks_.reset();
  }
}

void RequestCallbacksWrapper::NotifyError(mojom::CredentialManagerError error) {
  if (callbacks_) {
    callbacks_->onError(GetWebCredentialManagerErrorFromMojo(error));
    callbacks_.reset();
  }
}

void RespondToNotificationCallback(
    NotificationCallbacksWrapper* callbacks_wrapper) {
  callbacks_wrapper->NotifySuccess();
}

void RespondToRequestCallback(RequestCallbacksWrapper* callbacks_wrapper,
                              mojom::CredentialManagerError error,
                              mojom::CredentialInfoPtr info) {
  if (error == mojom::CredentialManagerError::SUCCESS) {
    DCHECK(!info.is_null());
    callbacks_wrapper->NotifySuccess(std::move(info));
  } else {
    DCHECK(info.is_null());
    callbacks_wrapper->NotifyError(error);
  }
}

}  // namespace

CredentialManagerClient::CredentialManagerClient(
    content::RenderView* render_view)
    : content::RenderViewObserver(render_view) {
  render_view->GetWebView()->setCredentialManagerClient(this);
}

CredentialManagerClient::~CredentialManagerClient() {}

// -----------------------------------------------------------------------------
// Access mojo CredentialManagerService.

void CredentialManagerClient::dispatchStore(
    const blink::WebCredential& credential,
    blink::WebCredentialManagerClient::NotificationCallbacks* callbacks) {
  DCHECK(callbacks);
  ConnectToMojoCMIfNeeded();

  mojom::CredentialInfoPtr info = mojom::CredentialInfo::From(credential);
  mojo_cm_service_->Store(
      std::move(info),
      base::Bind(&RespondToNotificationCallback,
                 base::Owned(new NotificationCallbacksWrapper(callbacks))));
}

void CredentialManagerClient::dispatchRequireUserMediation(
    blink::WebCredentialManagerClient::NotificationCallbacks* callbacks) {
  DCHECK(callbacks);
  ConnectToMojoCMIfNeeded();

  mojo_cm_service_->RequireUserMediation(
      base::Bind(&RespondToNotificationCallback,
                 base::Owned(new NotificationCallbacksWrapper(callbacks))));
}

void CredentialManagerClient::dispatchGet(
    bool zero_click_only,
    bool include_passwords,
    const blink::WebVector<blink::WebURL>& federations,
    RequestCallbacks* callbacks) {
  DCHECK(callbacks);
  ConnectToMojoCMIfNeeded();

  std::vector<GURL> federation_vector;
  for (size_t i = 0; i < std::min(federations.size(), kMaxFederations); ++i)
    federation_vector.push_back(federations[i]);

  mojo_cm_service_->Get(
      zero_click_only, include_passwords, std::move(federation_vector),
      base::Bind(&RespondToRequestCallback,
                 base::Owned(new RequestCallbacksWrapper(callbacks))));
}

void CredentialManagerClient::ConnectToMojoCMIfNeeded() {
  if (mojo_cm_service_)
    return;

  content::RenderFrame* main_frame = render_view()->GetMainRenderFrame();
  main_frame->GetRemoteInterfaces()->GetInterface(&mojo_cm_service_);
}

void CredentialManagerClient::OnDestruct() {
  delete this;
}

}  // namespace password_manager
