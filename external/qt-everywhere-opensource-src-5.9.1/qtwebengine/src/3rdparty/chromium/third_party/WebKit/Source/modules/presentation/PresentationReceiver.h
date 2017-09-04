// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationReceiver_h
#define PresentationReceiver_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseProperty.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMException.h"
#include "core/frame/DOMWindowProperty.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "public/platform/modules/presentation/WebPresentationReceiver.h"

namespace blink {

class PresentationConnection;
class PresentationConnectionList;
class WebPresentationClient;
class WebPresentationConnectionClient;

// Implements the PresentationReceiver interface from the Presentation API from
// which websites can implement the receiving side of a presentation session.
class MODULES_EXPORT PresentationReceiver final
    : public GarbageCollectedFinalized<PresentationReceiver>,
      public ScriptWrappable,
      public DOMWindowProperty,
      public WebPresentationReceiver {
  USING_GARBAGE_COLLECTED_MIXIN(PresentationReceiver);
  DEFINE_WRAPPERTYPEINFO();
  using ConnectionListProperty =
      ScriptPromiseProperty<Member<PresentationReceiver>,
                            Member<PresentationConnectionList>,
                            Member<DOMException>>;

 public:
  explicit PresentationReceiver(LocalFrame*, WebPresentationClient*);
  ~PresentationReceiver() = default;

  // PresentationReceiver.idl implementation
  ScriptPromise connectionList(ScriptState*);

  // Implementation of WebPresentationController.
  void onReceiverConnectionAvailable(WebPresentationConnectionClient*) override;
  void registerConnection(PresentationConnection*);

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class PresentationReceiverTest;

  Member<ConnectionListProperty> m_connectionListProperty;
  Member<PresentationConnectionList> m_connectionList;
};

}  // namespace blink

#endif  // PresentationReceiver_h
