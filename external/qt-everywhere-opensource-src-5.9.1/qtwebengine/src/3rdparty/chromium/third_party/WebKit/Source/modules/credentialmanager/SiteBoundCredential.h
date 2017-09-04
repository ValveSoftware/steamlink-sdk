// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SiteBoundCredential_h
#define SiteBoundCredential_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "modules/credentialmanager/Credential.h"
#include "platform/heap/Handle.h"

namespace blink {

class MODULES_EXPORT SiteBoundCredential : public Credential {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // SiteBoundCredential.idl
  const String& name() const { return m_platformCredential->name(); }
  const KURL& iconURL() const { return m_platformCredential->iconURL(); }

 protected:
  SiteBoundCredential(PlatformCredential*);
};

}  // namespace blink

#endif  // SiteBoundCredential_h
