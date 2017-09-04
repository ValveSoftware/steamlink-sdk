// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRController_h
#define VRController_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/Document.h"
#include "device/vr/vr_service.mojom-blink.h"
#include "modules/vr/VRDisplay.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/heap/Handle.h"
#include "wtf/Deque.h"

#include <memory>

namespace blink {

class NavigatorVR;
class VRGetDevicesCallback;

class VRController final : public GarbageCollectedFinalized<VRController>,
                           public device::mojom::blink::VRServiceClient,
                           public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(VRController);
  WTF_MAKE_NONCOPYABLE(VRController);
  USING_PRE_FINALIZER(VRController, dispose);

 public:
  VRController(NavigatorVR*);
  virtual ~VRController();

  void getDisplays(ScriptPromiseResolver*);
  void setListeningForActivate(bool);

  void OnDisplayConnected(device::mojom::blink::VRDisplayPtr,
                          device::mojom::blink::VRDisplayClientRequest,
                          device::mojom::blink::VRDisplayInfoPtr) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  void onDisplaysSynced(unsigned);
  void onGetDisplays();

  // ContextLifecycleObserver.
  void contextDestroyed() override;
  void dispose();

  Member<NavigatorVR> m_navigatorVR;
  VRDisplayVector m_displays;

  bool m_displaySynced;
  unsigned m_numberOfSyncedDisplays;

  Deque<std::unique_ptr<VRGetDevicesCallback>> m_pendingGetDevicesCallbacks;
  device::mojom::blink::VRServicePtr m_service;
  mojo::Binding<device::mojom::blink::VRServiceClient> m_binding;
};

}  // namespace blink

#endif  // VRController_h
