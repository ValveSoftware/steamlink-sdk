// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FederatedCredential_h
#define FederatedCredential_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "modules/ModulesExport.h"
#include "modules/credentialmanager/SiteBoundCredential.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"

namespace blink {

class FederatedCredentialData;
class WebFederatedCredential;

class MODULES_EXPORT FederatedCredential final : public SiteBoundCredential {
    DEFINE_WRAPPERTYPEINFO();
public:
    static FederatedCredential* create(const FederatedCredentialData&, ExceptionState&);
    static FederatedCredential* create(WebFederatedCredential*);

    // FederatedCredential.idl
    const String provider() const;

    // TODO(mkwst): This is a stub, as we don't yet have any support on the Chromium-side.
    const String& protocol() const { return emptyString(); }

private:
    FederatedCredential(WebFederatedCredential*);
    FederatedCredential(const String& id, const KURL& provider, const String& name, const KURL& icon);
};

} // namespace blink

#endif // FederatedCredential_h
