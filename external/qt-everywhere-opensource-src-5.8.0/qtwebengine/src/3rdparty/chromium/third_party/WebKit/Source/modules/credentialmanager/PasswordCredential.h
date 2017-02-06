// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PasswordCredential_h
#define PasswordCredential_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/modules/v8/FormDataOrURLSearchParams.h"
#include "modules/ModulesExport.h"
#include "modules/credentialmanager/SiteBoundCredential.h"
#include "platform/heap/Handle.h"
#include "platform/network/EncodedFormData.h"
#include "platform/weborigin/KURL.h"

namespace blink {

class FormData;
class FormDataOptions;
class HTMLFormElement;
class PasswordCredentialData;
class WebPasswordCredential;

using CredentialPostBodyType = FormDataOrURLSearchParams;

class MODULES_EXPORT PasswordCredential final : public SiteBoundCredential {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PasswordCredential* create(const PasswordCredentialData&, ExceptionState&);
    static PasswordCredential* create(HTMLFormElement*, ExceptionState&);
    static PasswordCredential* create(WebPasswordCredential*);

    // PasswordCredential.idl
    void setIdName(const String& name) { m_idName = name; }
    const String& idName() const { return m_idName; }

    void setPasswordName(const String& name) { m_passwordName = name; }
    const String& passwordName() const { return m_passwordName; }

    void setAdditionalData(const CredentialPostBodyType& data) { m_additionalData = data; }
    void additionalData(CredentialPostBodyType& out) const { out = m_additionalData; }

    // Internal methods
    PassRefPtr<EncodedFormData> encodeFormData(String& contentType) const;
    const String& password() const;
    DECLARE_VIRTUAL_TRACE();

private:
    PasswordCredential(WebPasswordCredential*);
    PasswordCredential(const String& id, const String& password, const String& name, const KURL& icon);

    String m_idName;
    String m_passwordName;
    CredentialPostBodyType m_additionalData;
};

} // namespace blink

#endif // PasswordCredential_h
