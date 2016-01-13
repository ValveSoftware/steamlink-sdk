// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PermissionCallbacks_h
#define PermissionCallbacks_h

#include "platform/PlatformExport.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

class PLATFORM_EXPORT PermissionCallbacks {
    WTF_MAKE_NONCOPYABLE(PermissionCallbacks);
public:
    static PassOwnPtr<PermissionCallbacks> create(const Closure& allowed, const Closure& denied);
    virtual ~PermissionCallbacks() { }

    void onAllowed() { m_allowed(); }
    void onDenied() { m_denied(); }

private:
    PermissionCallbacks(const Closure& allowed, const Closure& denied);

    Closure m_allowed;
    Closure m_denied;
};

} // namespace WebCore

#endif // PermissionCallbacks_h
