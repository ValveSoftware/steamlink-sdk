// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchEvent_h
#define FetchEvent_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseProperty.h"
#include "modules/EventModules.h"
#include "modules/ModulesExport.h"
#include "modules/fetch/Request.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "modules/serviceworkers/FetchEventInit.h"
#include "modules/serviceworkers/RespondWithObserver.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;
class Request;
class Response;
class RespondWithObserver;
class ScriptState;
class WebDataConsumerHandle;
struct WebServiceWorkerError;
class WebServiceWorkerResponse;

// A fetch event is dispatched by the client to a service worker's script
// context. RespondWithObserver can be used to notify the client about the
// service worker's response.
class MODULES_EXPORT FetchEvent final : public ExtendableEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using PreloadResponseProperty = ScriptPromiseProperty<Member<FetchEvent>,
                                                        Member<Response>,
                                                        Member<DOMException>>;
  static FetchEvent* create(ScriptState*,
                            const AtomicString& type,
                            const FetchEventInit&);
  static FetchEvent* create(ScriptState*,
                            const AtomicString& type,
                            const FetchEventInit&,
                            RespondWithObserver*,
                            WaitUntilObserver*,
                            bool navigationPreloadSent);

  Request* request() const;
  String clientId() const;
  bool isReload() const;

  void respondWith(ScriptState*, ScriptPromise, ExceptionState&);
  ScriptPromise preloadResponse(ScriptState*);

  void onNavigationPreloadResponse(std::unique_ptr<WebServiceWorkerResponse>,
                                   std::unique_ptr<WebDataConsumerHandle>);
  void onNavigationPreloadError(std::unique_ptr<WebServiceWorkerError>);

  const AtomicString& interfaceName() const override;

  DECLARE_VIRTUAL_TRACE();

 protected:
  FetchEvent(ScriptState*,
             const AtomicString& type,
             const FetchEventInit&,
             RespondWithObserver*,
             WaitUntilObserver*,
             bool navigationPreloadSent);

 private:
  RefPtr<ScriptState> m_scriptState;
  Member<RespondWithObserver> m_observer;
  Member<Request> m_request;
  Member<PreloadResponseProperty> m_preloadResponseProperty;
  String m_clientId;
  bool m_isReload;
};

}  // namespace blink

#endif  // FetchEvent_h
