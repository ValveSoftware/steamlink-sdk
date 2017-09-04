// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BeforeInstallPromptEvent_h
#define BeforeInstallPromptEvent_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseProperty.h"
#include "core/frame/LocalFrame.h"
#include "modules/EventModules.h"
#include "modules/app_banner/AppBannerPromptResult.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "public/platform/modules/app_banner/app_banner.mojom-blink.h"
#include <utility>

namespace blink {

class BeforeInstallPromptEvent;
class BeforeInstallPromptEventInit;

using UserChoiceProperty =
    ScriptPromiseProperty<Member<BeforeInstallPromptEvent>,
                          Member<AppBannerPromptResult>,
                          ToV8UndefinedGenerator>;

class BeforeInstallPromptEvent final : public Event,
                                       public mojom::blink::AppBannerEvent {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(BeforeInstallPromptEvent, dispose);

 public:
  ~BeforeInstallPromptEvent() override;

  static BeforeInstallPromptEvent* create(
      const AtomicString& name,
      LocalFrame& frame,
      mojom::blink::AppBannerServicePtr servicePtr,
      mojom::blink::AppBannerEventRequest eventRequest,
      const Vector<String>& platforms) {
    return new BeforeInstallPromptEvent(name, frame, std::move(servicePtr),
                                        std::move(eventRequest), platforms);
  }

  static BeforeInstallPromptEvent* create(
      const AtomicString& name,
      const BeforeInstallPromptEventInit& init) {
    return new BeforeInstallPromptEvent(name, init);
  }

  void dispose();

  Vector<String> platforms() const;
  ScriptPromise userChoice(ScriptState*);
  ScriptPromise prompt(ScriptState*);

  const AtomicString& interfaceName() const override;
  void preventDefault() override;

  DECLARE_VIRTUAL_TRACE();

 private:
  BeforeInstallPromptEvent(const AtomicString& name,
                           LocalFrame&,
                           mojom::blink::AppBannerServicePtr,
                           mojom::blink::AppBannerEventRequest,
                           const Vector<String>& platforms);
  BeforeInstallPromptEvent(const AtomicString& name,
                           const BeforeInstallPromptEventInit&);

  // mojom::blink::AppBannerEvent methods:
  void BannerAccepted(const String& platform) override;
  void BannerDismissed() override;

  mojom::blink::AppBannerServicePtr m_bannerService;
  mojo::Binding<mojom::blink::AppBannerEvent> m_binding;
  Vector<String> m_platforms;
  Member<UserChoiceProperty> m_userChoice;
  bool m_promptCalled;
};

DEFINE_TYPE_CASTS(BeforeInstallPromptEvent,
                  Event,
                  event,
                  event->interfaceName() ==
                      EventNames::BeforeInstallPromptEvent,
                  event.interfaceName() ==
                      EventNames::BeforeInstallPromptEvent);

}  // namespace blink

#endif  // BeforeInstallPromptEvent_h
