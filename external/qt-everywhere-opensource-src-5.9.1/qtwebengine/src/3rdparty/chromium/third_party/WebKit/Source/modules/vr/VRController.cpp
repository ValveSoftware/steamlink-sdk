// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRController.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "modules/vr/NavigatorVR.h"
#include "modules/vr/VRGetDevicesCallback.h"
#include "public/platform/InterfaceProvider.h"

#include "wtf/Assertions.h"

namespace blink {

VRController::VRController(NavigatorVR* navigatorVR)
    : ContextLifecycleObserver(navigatorVR->document()),
      m_navigatorVR(navigatorVR),
      m_displaySynced(false),
      m_binding(this) {
  navigatorVR->document()->frame()->interfaceProvider()->getInterface(
      mojo::GetProxy(&m_service));
  m_service.set_connection_error_handler(convertToBaseCallback(
      WTF::bind(&VRController::dispose, wrapWeakPersistent(this))));
  m_service->SetClient(
      m_binding.CreateInterfacePtrAndBind(),
      convertToBaseCallback(
          WTF::bind(&VRController::onDisplaysSynced, wrapPersistent(this))));
  ThreadState::current()->registerPreFinalizer(this);
}

VRController::~VRController() {}

void VRController::getDisplays(ScriptPromiseResolver* resolver) {
  // If we've previously synced the VRDisplays or no longer have a valid service
  // connection just return the current list. In the case of the service being
  // disconnected this will be an empty array.
  if (!m_service || m_displaySynced) {
    resolver->resolve(m_displays);
    return;
  }

  // Otherwise we're still waiting for the full list of displays to be populated
  // so queue up the promise for resolution when onDisplaysSynced is called.
  m_pendingGetDevicesCallbacks.append(
      makeUnique<VRGetDevicesCallback>(resolver));
}

void VRController::setListeningForActivate(bool listening) {
  if (m_service)
    m_service->SetListeningForActivate(listening);
}

// Each time a new VRDisplay is connected we'll recieve a VRDisplayPtr for it
// here. Upon calling SetClient in the constructor we should receive one call
// for each VRDisplay that was already connected at the time.
void VRController::OnDisplayConnected(
    device::mojom::blink::VRDisplayPtr display,
    device::mojom::blink::VRDisplayClientRequest request,
    device::mojom::blink::VRDisplayInfoPtr displayInfo) {
  VRDisplay* vrDisplay =
      new VRDisplay(m_navigatorVR, std::move(display), std::move(request));
  vrDisplay->update(displayInfo);
  vrDisplay->onConnected();
  m_displays.append(vrDisplay);

  if (m_displays.size() == m_numberOfSyncedDisplays) {
    m_displaySynced = true;
    onGetDisplays();
  }
}

// Called when the VRService has called OnDisplayConnected for all active
// VRDisplays.
void VRController::onDisplaysSynced(unsigned numberOfDisplays) {
  m_numberOfSyncedDisplays = numberOfDisplays;
  if (m_numberOfSyncedDisplays == m_displays.size()) {
    m_displaySynced = true;
    onGetDisplays();
  }
}

void VRController::onGetDisplays() {
  while (!m_pendingGetDevicesCallbacks.isEmpty()) {
    std::unique_ptr<VRGetDevicesCallback> callback =
        m_pendingGetDevicesCallbacks.takeFirst();
    callback->onSuccess(m_displays);
  }
}

void VRController::contextDestroyed() {
  dispose();
  // The context is not automatically cleared, so do it manually.
  ContextLifecycleObserver::clearContext();
}

void VRController::dispose() {
  // If the document context was destroyed, shut down the client connection
  // and never call the mojo service again.
  m_service.reset();
  m_binding.Close();

  // Shutdown all displays' message pipe
  for (size_t i = 0; i < m_displays.size(); ++i)
    m_displays[i]->dispose();

  m_displays.clear();

  // Ensure that any outstanding getDisplays promises are resolved.
  onGetDisplays();
}

DEFINE_TRACE(VRController) {
  visitor->trace(m_navigatorVR);
  visitor->trace(m_displays);

  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
