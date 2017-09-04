// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/permissions/PermissionUtils.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"

namespace blink {

using mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;

bool connectToPermissionService(
    ExecutionContext* executionContext,
    mojom::blink::PermissionServiceRequest request) {
  InterfaceProvider* interfaceProvider = nullptr;
  if (executionContext->isDocument()) {
    Document* document = toDocument(executionContext);
    if (document->frame())
      interfaceProvider = document->frame()->interfaceProvider();
  } else {
    interfaceProvider = Platform::current()->interfaceProvider();
  }

  if (interfaceProvider)
    interfaceProvider->getInterface(std::move(request));
  return interfaceProvider;
}

PermissionDescriptorPtr createPermissionDescriptor(PermissionName name) {
  auto descriptor = PermissionDescriptor::New();
  descriptor->name = name;
  return descriptor;
}

PermissionDescriptorPtr createMidiPermissionDescriptor(bool sysex) {
  auto descriptor =
      createPermissionDescriptor(mojom::blink::PermissionName::MIDI);
  auto midiExtension = mojom::blink::MidiPermissionDescriptor::New();
  midiExtension->sysex = sysex;
  descriptor->extension = mojom::blink::PermissionDescriptorExtension::New();
  descriptor->extension->set_midi(std::move(midiExtension));
  return descriptor;
}

}  // namespace blink
