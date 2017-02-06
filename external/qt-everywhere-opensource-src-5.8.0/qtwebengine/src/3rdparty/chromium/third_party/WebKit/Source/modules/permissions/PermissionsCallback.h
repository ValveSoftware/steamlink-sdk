// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PermissionsCallback_h
#define PermissionsCallback_h

#include "platform/heap/Handle.h"
#include "public/platform/WebCallbacks.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/permissions/WebPermissionStatus.h"
#include "public/platform/modules/permissions/WebPermissionType.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class ScriptPromiseResolver;

// PermissionQueryCallback is an implementation of WebPermissionCallbacks
// that will resolve the underlying promise depending on the result passed to
// the callback. It takes a WebPermissionType in its constructor and will pass
// it to the PermissionStatus.
class PermissionsCallback final
    : public WebCallbacks<std::unique_ptr<WebVector<WebPermissionStatus>>, void> {
public:
    PermissionsCallback(ScriptPromiseResolver*, std::unique_ptr<Vector<WebPermissionType>>, std::unique_ptr<Vector<int>>);
    ~PermissionsCallback() = default;

    void onSuccess(std::unique_ptr<WebVector<WebPermissionStatus>>) override;
    void onError() override;

private:
    Persistent<ScriptPromiseResolver> m_resolver;

    // The permission types which were passed to the client to be requested.
    std::unique_ptr<Vector<WebPermissionType>> m_internalPermissions;

    // Maps each index in the caller vector to the corresponding index in the
    // internal vector (i.e. the vector passsed to the client) such that both
    // indices have the same WebPermissionType.
    std::unique_ptr<Vector<int>> m_callerIndexToInternalIndex;

    WTF_MAKE_NONCOPYABLE(PermissionsCallback);
};

} // namespace blink

#endif // PermissionsCallback_h
