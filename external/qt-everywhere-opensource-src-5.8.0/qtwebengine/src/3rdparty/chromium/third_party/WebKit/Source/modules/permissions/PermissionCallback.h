// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PermissionCallback_h
#define PermissionCallback_h

#include "platform/heap/Handle.h"
#include "public/platform/modules/permissions/WebPermissionClient.h"
#include "public/platform/modules/permissions/WebPermissionStatus.h"
#include "public/platform/modules/permissions/WebPermissionType.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class ScriptPromiseResolver;

// PermissionQueryCallback is an implementation of WebPermissionCallbacks
// that will resolve the underlying promise depending on the result passed to
// the callback. It takes a WebPermissionType in its constructor and will pass
// it to the PermissionStatus.
class PermissionCallback final : public WebPermissionCallback {
    USING_FAST_MALLOC(PermissionCallback);
    WTF_MAKE_NONCOPYABLE(PermissionCallback);
public:
    PermissionCallback(ScriptPromiseResolver*, WebPermissionType);
    ~PermissionCallback() override;

    void onSuccess(WebPermissionStatus) override;
    void onError() override;

private:
    Persistent<ScriptPromiseResolver> m_resolver;
    WebPermissionType m_permissionType;
};

} // namespace blink

#endif // PermissionCallback_h
