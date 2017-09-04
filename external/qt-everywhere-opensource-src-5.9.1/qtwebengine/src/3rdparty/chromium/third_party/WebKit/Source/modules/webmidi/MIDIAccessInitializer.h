// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIAccessInitializer_h
#define MIDIAccessInitializer_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "media/midi/midi_service.mojom-blink.h"
#include "modules/ModulesExport.h"
#include "modules/webmidi/MIDIAccessor.h"
#include "modules/webmidi/MIDIAccessorClient.h"
#include "modules/webmidi/MIDIOptions.h"
#include "modules/webmidi/MIDIPort.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission.mojom-blink.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom-blink.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class ScriptState;

class MODULES_EXPORT MIDIAccessInitializer : public ScriptPromiseResolver,
                                             public MIDIAccessorClient {
 public:
  struct PortDescriptor {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    String id;
    String manufacturer;
    String name;
    MIDIPort::TypeCode type;
    String version;
    midi::mojom::PortState state;

    PortDescriptor(const String& id,
                   const String& manufacturer,
                   const String& name,
                   MIDIPort::TypeCode type,
                   const String& version,
                   midi::mojom::PortState state)
        : id(id),
          manufacturer(manufacturer),
          name(name),
          type(type),
          version(version),
          state(state) {}
  };

  static ScriptPromise start(ScriptState* scriptState,
                             const MIDIOptions& options) {
    MIDIAccessInitializer* resolver =
        new MIDIAccessInitializer(scriptState, options);
    resolver->keepAliveWhilePending();
    resolver->suspendIfNeeded();
    return resolver->start();
  }

  ~MIDIAccessInitializer() override = default;

  // Eager finalization to allow dispose() operation access
  // other (non eager) heap objects.
  EAGERLY_FINALIZE();

  // MIDIAccessorClient
  void didAddInputPort(const String& id,
                       const String& manufacturer,
                       const String& name,
                       const String& version,
                       midi::mojom::PortState) override;
  void didAddOutputPort(const String& id,
                        const String& manufacturer,
                        const String& name,
                        const String& version,
                        midi::mojom::PortState) override;
  void didSetInputPortState(unsigned portIndex,
                            midi::mojom::PortState) override;
  void didSetOutputPortState(unsigned portIndex,
                             midi::mojom::PortState) override;
  void didStartSession(midi::mojom::Result) override;
  void didReceiveMIDIData(unsigned portIndex,
                          const unsigned char* data,
                          size_t length,
                          double timeStamp) override {}

 private:
  MIDIAccessInitializer(ScriptState*, const MIDIOptions&);

  ExecutionContext* getExecutionContext() const;
  ScriptPromise start();

  void contextDestroyed() override;

  void onPermissionsUpdated(mojom::blink::PermissionStatus);
  void onPermissionUpdated(mojom::blink::PermissionStatus);

  std::unique_ptr<MIDIAccessor> m_accessor;
  Vector<PortDescriptor> m_portDescriptors;
  MIDIOptions m_options;

  mojom::blink::PermissionServicePtr m_permissionService;
};

}  // namespace blink

#endif  // MIDIAccessInitializer_h
