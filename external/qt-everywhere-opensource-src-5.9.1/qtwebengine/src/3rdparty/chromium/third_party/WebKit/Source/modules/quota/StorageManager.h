// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StorageManager_h
#define StorageManager_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Heap.h"
#include "public/platform/modules/permissions/permission.mojom-blink.h"

namespace blink {

class ExecutionContext;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

class StorageManager final : public GarbageCollectedFinalized<StorageManager>,
                             public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ScriptPromise persisted(ScriptState*);
  ScriptPromise persist(ScriptState*);

  ScriptPromise estimate(ScriptState*);
  DECLARE_TRACE();

 private:
  mojom::blink::PermissionService* getPermissionService(ExecutionContext*);
  void permissionServiceConnectionError();
  void permissionRequestComplete(ScriptPromiseResolver*,
                                 mojom::blink::PermissionStatus);

  mojom::blink::PermissionServicePtr m_permissionService;
};

}  // namespace blink

#endif  // StorageManager_h
