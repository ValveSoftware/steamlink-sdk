// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/app_banner/AppBannerController.h"

#include "core/EventTypeNames.h"
#include "core/dom/Document.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "modules/app_banner/BeforeInstallPromptEvent.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/Referrer.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/AtomicString.h"
#include <memory>
#include <utility>

namespace blink {

AppBannerController::AppBannerController(LocalFrame& frame) : m_frame(frame) {}

void AppBannerController::bindMojoRequest(
    LocalFrame* frame,
    mojom::blink::AppBannerControllerRequest request) {
  DCHECK(frame);

  mojo::MakeStrongBinding(wrapUnique(new AppBannerController(*frame)),
                          std::move(request));
}

void AppBannerController::BannerPromptRequest(
    mojom::blink::AppBannerServicePtr servicePtr,
    mojom::blink::AppBannerEventRequest eventRequest,
    const Vector<String>& platforms,
    const BannerPromptRequestCallback& callback) {
  if (!m_frame || !m_frame->document()) {
    callback.Run(mojom::blink::AppBannerPromptReply::NONE, "");
    return;
  }

  mojom::AppBannerPromptReply reply =
      m_frame->domWindow()->dispatchEvent(BeforeInstallPromptEvent::create(
          EventTypeNames::beforeinstallprompt, *m_frame, std::move(servicePtr),
          std::move(eventRequest), platforms)) ==
              DispatchEventResult::NotCanceled
          ? mojom::AppBannerPromptReply::NONE
          : mojom::AppBannerPromptReply::CANCEL;

  AtomicString referrer = SecurityPolicy::generateReferrer(
                              m_frame->document()->getReferrerPolicy(), KURL(),
                              m_frame->document()->outgoingReferrer())
                              .referrer;

  callback.Run(reply, referrer.isNull() ? emptyString() : referrer);
}

}  // namespace blink
