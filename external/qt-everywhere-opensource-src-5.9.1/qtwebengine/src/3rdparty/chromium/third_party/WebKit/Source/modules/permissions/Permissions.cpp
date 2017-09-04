// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/permissions/Permissions.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/Nullable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/modules/v8/V8MidiPermissionDescriptor.h"
#include "bindings/modules/v8/V8PermissionDescriptor.h"
#include "bindings/modules/v8/V8PushPermissionDescriptor.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "modules/permissions/PermissionDescriptor.h"
#include "modules/permissions/PermissionStatus.h"
#include "modules/permissions/PermissionUtils.h"
#include "platform/UserGestureIndicator.h"
#include "public/platform/Platform.h"
#include "wtf/Functional.h"
#include "wtf/NotFound.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;
using mojom::blink::PermissionService;

namespace {

// Parses the raw permission dictionary and returns the Mojo
// PermissionDescriptor if parsing was successful. If an exception occurs, it
// will be stored in |exceptionState| and null will be returned. Therefore, the
// |exceptionState| should be checked before attempting to use the returned
// permission as the non-null assert will be fired otherwise.
//
// Websites will be able to run code when `name()` is called, changing the
// current context. The caller should make sure that no assumption is made
// after this has been called.
PermissionDescriptorPtr parsePermission(ScriptState* scriptState,
                                        const Dictionary rawPermission,
                                        ExceptionState& exceptionState) {
  PermissionDescriptor permission =
      NativeValueTraits<PermissionDescriptor>::nativeValue(
          scriptState->isolate(), rawPermission.v8Value(), exceptionState);

  if (exceptionState.hadException()) {
    exceptionState.throwTypeError(exceptionState.message());
    return nullptr;
  }

  const String& name = permission.name();
  if (name == "geolocation")
    return createPermissionDescriptor(PermissionName::GEOLOCATION);
  if (name == "notifications")
    return createPermissionDescriptor(PermissionName::NOTIFICATIONS);
  if (name == "push") {
    PushPermissionDescriptor pushPermission =
        NativeValueTraits<PushPermissionDescriptor>::nativeValue(
            scriptState->isolate(), rawPermission.v8Value(), exceptionState);
    if (exceptionState.hadException()) {
      exceptionState.throwTypeError(exceptionState.message());
      return nullptr;
    }

    // Only "userVisibleOnly" push is supported for now.
    if (!pushPermission.userVisibleOnly()) {
      exceptionState.throwDOMException(
          NotSupportedError,
          "Push Permission without userVisibleOnly:true isn't supported yet.");
      return nullptr;
    }

    return createPermissionDescriptor(PermissionName::PUSH_NOTIFICATIONS);
  }
  if (name == "midi") {
    MidiPermissionDescriptor midiPermission =
        NativeValueTraits<MidiPermissionDescriptor>::nativeValue(
            scriptState->isolate(), rawPermission.v8Value(), exceptionState);
    return createMidiPermissionDescriptor(midiPermission.sysex());
  }
  if (name == "background-sync")
    return createPermissionDescriptor(PermissionName::BACKGROUND_SYNC);

  return nullptr;
}

}  // anonymous namespace

ScriptPromise Permissions::query(ScriptState* scriptState,
                                 const Dictionary& rawPermission) {
  ExceptionState exceptionState(ExceptionState::GetterContext, "query",
                                "Permissions", scriptState->context()->Global(),
                                scriptState->isolate());
  PermissionDescriptorPtr descriptor =
      parsePermission(scriptState, rawPermission, exceptionState);
  if (exceptionState.hadException())
    return exceptionState.reject(scriptState);

  // This must be called after `parsePermission` because the website might
  // be able to run code.
  PermissionService* service = getService(scriptState->getExecutionContext());
  if (!service)
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(
            InvalidStateError,
            "In its current state, the global scope can't query permissions."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  // If the current origin is a file scheme, it will unlikely return a
  // meaningful value because most APIs are broken on file scheme and no
  // permission prompt will be shown even if the returned permission will most
  // likely be "prompt".
  PermissionDescriptorPtr descriptorCopy = descriptor->Clone();
  service->HasPermission(
      std::move(descriptor),
      scriptState->getExecutionContext()->getSecurityOrigin(),
      convertToBaseCallback(WTF::bind(
          &Permissions::taskComplete, wrapPersistent(this),
          wrapPersistent(resolver), passed(std::move(descriptorCopy)))));
  return promise;
}

ScriptPromise Permissions::request(ScriptState* scriptState,
                                   const Dictionary& rawPermission) {
  ExceptionState exceptionState(ExceptionState::GetterContext, "request",
                                "Permissions", scriptState->context()->Global(),
                                scriptState->isolate());
  PermissionDescriptorPtr descriptor =
      parsePermission(scriptState, rawPermission, exceptionState);
  if (exceptionState.hadException())
    return exceptionState.reject(scriptState);

  // This must be called after `parsePermission` because the website might
  // be able to run code.
  PermissionService* service = getService(scriptState->getExecutionContext());
  if (!service)
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError,
                                          "In its current state, the global "
                                          "scope can't request permissions."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  PermissionDescriptorPtr descriptorCopy = descriptor->Clone();
  service->RequestPermission(
      std::move(descriptor),
      scriptState->getExecutionContext()->getSecurityOrigin(),
      UserGestureIndicator::processingUserGesture(),
      convertToBaseCallback(WTF::bind(
          &Permissions::taskComplete, wrapPersistent(this),
          wrapPersistent(resolver), passed(std::move(descriptorCopy)))));
  return promise;
}

ScriptPromise Permissions::revoke(ScriptState* scriptState,
                                  const Dictionary& rawPermission) {
  ExceptionState exceptionState(ExceptionState::GetterContext, "revoke",
                                "Permissions", scriptState->context()->Global(),
                                scriptState->isolate());
  PermissionDescriptorPtr descriptor =
      parsePermission(scriptState, rawPermission, exceptionState);
  if (exceptionState.hadException())
    return exceptionState.reject(scriptState);

  // This must be called after `parsePermission` because the website might
  // be able to run code.
  PermissionService* service = getService(scriptState->getExecutionContext());
  if (!service)
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError,
                                          "In its current state, the global "
                                          "scope can't revoke permissions."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  PermissionDescriptorPtr descriptorCopy = descriptor->Clone();
  service->RevokePermission(
      std::move(descriptor),
      scriptState->getExecutionContext()->getSecurityOrigin(),
      convertToBaseCallback(WTF::bind(
          &Permissions::taskComplete, wrapPersistent(this),
          wrapPersistent(resolver), passed(std::move(descriptorCopy)))));
  return promise;
}

ScriptPromise Permissions::requestAll(
    ScriptState* scriptState,
    const Vector<Dictionary>& rawPermissions) {
  ExceptionState exceptionState(ExceptionState::GetterContext, "request",
                                "Permissions", scriptState->context()->Global(),
                                scriptState->isolate());
  Vector<PermissionDescriptorPtr> internalPermissions;
  Vector<int> callerIndexToInternalIndex;
  callerIndexToInternalIndex.resize(rawPermissions.size());
  for (size_t i = 0; i < rawPermissions.size(); ++i) {
    const Dictionary& rawPermission = rawPermissions[i];

    auto descriptor =
        parsePermission(scriptState, rawPermission, exceptionState);
    if (exceptionState.hadException())
      return exceptionState.reject(scriptState);

    // Only append permissions types that are not already present in the vector.
    size_t internalIndex = kNotFound;
    for (size_t j = 0; j < internalPermissions.size(); ++j) {
      if (internalPermissions[j]->name == descriptor->name) {
        internalIndex = j;
        break;
      }
    }
    if (internalIndex == kNotFound) {
      internalIndex = internalPermissions.size();
      internalPermissions.append(std::move(descriptor));
    }
    callerIndexToInternalIndex[i] = internalIndex;
  }

  // This must be called after `parsePermission` because the website might
  // be able to run code.
  PermissionService* service = getService(scriptState->getExecutionContext());
  if (!service)
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError,
                                          "In its current state, the global "
                                          "scope can't request permissions."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  Vector<PermissionDescriptorPtr> internalPermissionsCopy;
  internalPermissionsCopy.reserveCapacity(internalPermissions.size());
  for (const auto& descriptor : internalPermissions)
    internalPermissionsCopy.append(descriptor->Clone());

  service->RequestPermissions(
      std::move(internalPermissions),
      scriptState->getExecutionContext()->getSecurityOrigin(),
      UserGestureIndicator::processingUserGesture(),
      convertToBaseCallback(WTF::bind(
          &Permissions::batchTaskComplete, wrapPersistent(this),
          wrapPersistent(resolver), passed(std::move(internalPermissionsCopy)),
          passed(std::move(callerIndexToInternalIndex)))));
  return promise;
}

PermissionService* Permissions::getService(ExecutionContext* executionContext) {
  if (!m_service &&
      connectToPermissionService(executionContext, mojo::GetProxy(&m_service)))
    m_service.set_connection_error_handler(convertToBaseCallback(WTF::bind(
        &Permissions::serviceConnectionError, wrapWeakPersistent(this))));
  return m_service.get();
}

void Permissions::serviceConnectionError() {
  if (!Platform::current()) {
    // TODO(rockot): Remove this hack once renderer shutdown sequence is fixed.
    // Note that reaching this code indicates that the MessageLoop has already
    // been torn down, so it's impossible for any pending reply callbacks on
    // |m_service| to fire beyond this point anyway.
    return;
  }
  m_service.reset();
}

void Permissions::taskComplete(ScriptPromiseResolver* resolver,
                               mojom::blink::PermissionDescriptorPtr descriptor,
                               mojom::blink::PermissionStatus result) {
  if (!resolver->getExecutionContext() ||
      resolver->getExecutionContext()->activeDOMObjectsAreStopped())
    return;
  resolver->resolve(
      PermissionStatus::take(resolver, result, std::move(descriptor)));
}

void Permissions::batchTaskComplete(
    ScriptPromiseResolver* resolver,
    Vector<mojom::blink::PermissionDescriptorPtr> descriptors,
    Vector<int> callerIndexToInternalIndex,
    const Vector<mojom::blink::PermissionStatus>& results) {
  if (!resolver->getExecutionContext() ||
      resolver->getExecutionContext()->activeDOMObjectsAreStopped())
    return;

  // Create the response vector by finding the status for each index by
  // using the caller to internal index mapping and looking up the status
  // using the internal index obtained.
  HeapVector<Member<PermissionStatus>> result;
  result.reserveInitialCapacity(callerIndexToInternalIndex.size());
  for (int internalIndex : callerIndexToInternalIndex) {
    result.append(PermissionStatus::createAndListen(
        resolver->getExecutionContext(), results[internalIndex],
        descriptors[internalIndex]->Clone()));
  }
  resolver->resolve(result);
}

}  // namespace blink
