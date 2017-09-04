// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Permissions_h
#define Permissions_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/permissions/permission.mojom-blink.h"

namespace blink {

class Dictionary;
class ScriptPromiseResolver;
class ScriptState;

class Permissions final : public GarbageCollectedFinalized<Permissions>,
                          public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DEFINE_INLINE_TRACE() {}

  ScriptPromise query(ScriptState*, const Dictionary&);
  ScriptPromise request(ScriptState*, const Dictionary&);
  ScriptPromise revoke(ScriptState*, const Dictionary&);
  ScriptPromise requestAll(ScriptState*, const Vector<Dictionary>&);

 private:
  mojom::blink::PermissionService* getService(ExecutionContext*);
  void serviceConnectionError();
  void taskComplete(ScriptPromiseResolver*,
                    mojom::blink::PermissionDescriptorPtr,
                    mojom::blink::PermissionStatus);
  void batchTaskComplete(ScriptPromiseResolver*,
                         Vector<mojom::blink::PermissionDescriptorPtr>,
                         Vector<int>,
                         const Vector<mojom::blink::PermissionStatus>&);

  mojom::blink::PermissionServicePtr m_service;
};

}  // namespace blink

#endif  // Permissions_h
