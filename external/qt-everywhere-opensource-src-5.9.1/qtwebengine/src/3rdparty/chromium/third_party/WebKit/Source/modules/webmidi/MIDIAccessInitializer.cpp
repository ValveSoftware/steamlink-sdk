// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webmidi/MIDIAccessInitializer.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/permissions/PermissionUtils.h"
#include "modules/webmidi/MIDIAccess.h"
#include "modules/webmidi/MIDIOptions.h"
#include "modules/webmidi/MIDIPort.h"
#include "platform/UserGestureIndicator.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/modules/permissions/permission.mojom-blink.h"

namespace blink {

using midi::mojom::PortState;
using midi::mojom::Result;
using mojom::blink::PermissionStatus;

MIDIAccessInitializer::MIDIAccessInitializer(ScriptState* scriptState,
                                             const MIDIOptions& options)
    : ScriptPromiseResolver(scriptState), m_options(options) {}

void MIDIAccessInitializer::contextDestroyed() {
  m_permissionService.reset();
  LifecycleObserver::contextDestroyed();
}

ScriptPromise MIDIAccessInitializer::start() {
  ScriptPromise promise = this->promise();
  m_accessor = MIDIAccessor::create(this);

  connectToPermissionService(getExecutionContext(),
                             mojo::GetProxy(&m_permissionService));
  m_permissionService->RequestPermission(
      createMidiPermissionDescriptor(m_options.hasSysex() && m_options.sysex()),
      getExecutionContext()->getSecurityOrigin(),
      UserGestureIndicator::processingUserGesture(),
      convertToBaseCallback(WTF::bind(
          &MIDIAccessInitializer::onPermissionsUpdated, wrapPersistent(this))));

  return promise;
}

void MIDIAccessInitializer::didAddInputPort(const String& id,
                                            const String& manufacturer,
                                            const String& name,
                                            const String& version,
                                            PortState state) {
  DCHECK(m_accessor);
  m_portDescriptors.append(PortDescriptor(id, manufacturer, name,
                                          MIDIPort::TypeInput, version, state));
}

void MIDIAccessInitializer::didAddOutputPort(const String& id,
                                             const String& manufacturer,
                                             const String& name,
                                             const String& version,
                                             PortState state) {
  DCHECK(m_accessor);
  m_portDescriptors.append(PortDescriptor(
      id, manufacturer, name, MIDIPort::TypeOutput, version, state));
}

void MIDIAccessInitializer::didSetInputPortState(unsigned portIndex,
                                                 PortState state) {
  // didSetInputPortState() is not allowed to call before didStartSession()
  // is called. Once didStartSession() is called, MIDIAccessorClient methods
  // are delegated to MIDIAccess. See constructor of MIDIAccess.
  NOTREACHED();
}

void MIDIAccessInitializer::didSetOutputPortState(unsigned portIndex,
                                                  PortState state) {
  // See comments on didSetInputPortState().
  NOTREACHED();
}

void MIDIAccessInitializer::didStartSession(Result result) {
  DCHECK(m_accessor);
  // We would also have AbortError and SecurityError according to the spec.
  // SecurityError is handled in onPermission(s)Updated().
  switch (result) {
    case Result::NOT_INITIALIZED:
      break;
    case Result::OK:
      return resolve(MIDIAccess::create(
          std::move(m_accessor), m_options.hasSysex() && m_options.sysex(),
          m_portDescriptors, getExecutionContext()));
    case Result::NOT_SUPPORTED:
      return reject(DOMException::create(NotSupportedError));
    case Result::INITIALIZATION_ERROR:
      return reject(DOMException::create(
          InvalidStateError, "Platform dependent initialization failed."));
  }
  NOTREACHED();
  reject(DOMException::create(InvalidStateError,
                              "Unknown internal error occurred."));
}

ExecutionContext* MIDIAccessInitializer::getExecutionContext() const {
  return getScriptState()->getExecutionContext();
}

void MIDIAccessInitializer::onPermissionsUpdated(PermissionStatus status) {
  m_permissionService.reset();
  if (status == PermissionStatus::GRANTED)
    m_accessor->startSession();
  else
    reject(DOMException::create(SecurityError));
}

void MIDIAccessInitializer::onPermissionUpdated(PermissionStatus status) {
  m_permissionService.reset();
  if (status == PermissionStatus::GRANTED)
    m_accessor->startSession();
  else
    reject(DOMException::create(SecurityError));
}

}  // namespace blink
