// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/PasswordCredential.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/URLSearchParams.h"
#include "core/html/FormAssociatedElement.h"
#include "core/html/FormData.h"
#include "core/html/HTMLFormElement.h"
#include "modules/credentialmanager/FormDataOptions.h"
#include "modules/credentialmanager/PasswordCredentialData.h"
#include "platform/credentialmanager/PlatformPasswordCredential.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebCredential.h"
#include "public/platform/WebPasswordCredential.h"

namespace blink {

PasswordCredential* PasswordCredential::create(WebPasswordCredential* webPasswordCredential)
{
    return new PasswordCredential(webPasswordCredential);
}

PasswordCredential* PasswordCredential::create(const PasswordCredentialData& data, ExceptionState& exceptionState)
{
    if (data.id().isEmpty()) {
        exceptionState.throwTypeError("'id' must not be empty.");
        return nullptr;
    }
    if (data.password().isEmpty()) {
        exceptionState.throwTypeError("'password' must not be empty.");
        return nullptr;
    }

    KURL iconURL = parseStringAsURL(data.iconURL(), exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    return new PasswordCredential(data.id(), data.password(), data.name(), iconURL);
}

// https://w3c.github.io/webappsec-credential-management/#passwordcredential-form-constructor
PasswordCredential* PasswordCredential::create(HTMLFormElement* form, ExceptionState& exceptionState)
{
    // Extract data from the form, then use the extracted |formData| object's
    // value to populate |data|.
    FormData* formData = FormData::create(form);
    PasswordCredentialData data;

    AtomicString idName;
    AtomicString passwordName;
    for (FormAssociatedElement* element : form->associatedElements()) {
        // If |element| isn't a "submittable element" with string data, then it
        // won't have a matching value in |formData|, and we can safely skip it.
        FileOrUSVString result;
        formData->get(element->name(), result);
        if (!result.isUSVString())
            continue;

        AtomicString autocomplete = toHTMLElement(element)->fastGetAttribute(HTMLNames::autocompleteAttr);
        if (equalIgnoringCase(autocomplete, "current-password") || equalIgnoringCase(autocomplete, "new-password")) {
            data.setPassword(result.getAsUSVString());
            passwordName = element->name();
        } else if (equalIgnoringCase(autocomplete, "photo")) {
            data.setIconURL(result.getAsUSVString());
        } else if (equalIgnoringCase(autocomplete, "name") || equalIgnoringCase(autocomplete, "nickname")) {
            data.setName(result.getAsUSVString());
        } else if (equalIgnoringCase(autocomplete, "username")) {
            data.setId(result.getAsUSVString());
            idName = element->name();
        }
    }

    // Create a PasswordCredential using the data gathered above.
    PasswordCredential* credential = PasswordCredential::create(data, exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    ASSERT(credential);

    // After creating the Credential, populate its 'additionalData', 'idName', and 'passwordName' attributes.
    // If the form's 'enctype' is anything other than multipart, generate a URLSearchParams using the
    // data in |formData|.
    credential->setIdName(idName);
    credential->setPasswordName(passwordName);

    FormDataOrURLSearchParams additionalData;
    if (form->enctype() == "multipart/form-data") {
        additionalData.setFormData(formData);
    } else {
        URLSearchParams* params = URLSearchParams::create(URLSearchParamsInit());
        for (const FormData::Entry* entry : formData->entries()) {
            if (entry->isString())
                params->append(entry->name().data(), entry->value().data());
        }
        additionalData.setURLSearchParams(params);
    }

    credential->setAdditionalData(additionalData);
    return credential;
}

PasswordCredential::PasswordCredential(WebPasswordCredential* webPasswordCredential)
    : SiteBoundCredential(webPasswordCredential->getPlatformCredential())
    , m_idName("username")
    , m_passwordName("password")
{
}

PasswordCredential::PasswordCredential(const String& id, const String& password, const String& name, const KURL& icon)
    : SiteBoundCredential(PlatformPasswordCredential::create(id, password, name, icon))
    , m_idName("username")
    , m_passwordName("password")
{
}

PassRefPtr<EncodedFormData> PasswordCredential::encodeFormData(String& contentType) const
{
    if (m_additionalData.isURLSearchParams()) {
        // If |additionalData| is a 'URLSearchParams' object, build a urlencoded response.
        URLSearchParams* params = URLSearchParams::create(URLSearchParamsInit());
        URLSearchParams* additionalData = m_additionalData.getAsURLSearchParams();
        for (const auto& param : additionalData->params()) {
            const String& name = param.first;
            if (name != idName() && name != passwordName())
                params->append(name, param.second);
        }
        params->append(idName(), id());
        params->append(passwordName(), password());

        contentType = AtomicString("application/x-www-form-urlencoded;charset=UTF-8");

        return params->toEncodedFormData();
    }

    // Otherwise, we'll build a multipart response.
    FormData* formData = FormData::create(nullptr);
    if (m_additionalData.isFormData()) {
        FormData* additionalData = m_additionalData.getAsFormData();
        for (const FormData::Entry* entry : additionalData->entries()) {
            const String& name = formData->decode(entry->name());
            if (name == idName() || name == passwordName())
                continue;

            if (entry->blob())
                formData->append(name, entry->blob(), entry->filename());
            else
                formData->append(name, formData->decode(entry->value()));
        }
    }
    formData->append(idName(), id());
    formData->append(passwordName(), password());

    RefPtr<EncodedFormData> encodedData = formData->encodeMultiPartFormData();
    contentType = AtomicString("multipart/form-data; boundary=") + encodedData->boundary().data();
    return encodedData.release();
}

const String& PasswordCredential::password() const
{
    return static_cast<PlatformPasswordCredential*>(m_platformCredential.get())->password();
}

DEFINE_TRACE(PasswordCredential)
{
    SiteBoundCredential::trace(visitor);
    visitor->trace(m_additionalData);
}

} // namespace blink
