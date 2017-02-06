// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSDstyle license that can be
// found in the LICENSE file.

#include "core/html/HTMLIFrameElementPermissions.h"

#include "core/html/HTMLIFrameElement.h"
#include "wtf/HashMap.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {

struct SupportedPermission {
    const char* name;
    WebPermissionType type;
};

const SupportedPermission kSupportedPermissions[] = {
    { "geolocation", WebPermissionTypeGeolocation },
    { "notifications", WebPermissionTypeNotifications },
    { "midi", WebPermissionTypeMidiSysEx },
};

// Returns true if the name is valid and the type is stored in |result|.
bool getPermissionType(const AtomicString& name, WebPermissionType* result)
{
    for (const SupportedPermission& permission : kSupportedPermissions) {
        if (name == permission.name) {
            if (result)
                *result = permission.type;
            return true;
        }
    }
    return false;
}

} // namespace

HTMLIFrameElementPermissions::HTMLIFrameElementPermissions(HTMLIFrameElement* element)
    : DOMTokenList(this)
    , m_element(element)
{
}

HTMLIFrameElementPermissions::~HTMLIFrameElementPermissions()
{
}

DEFINE_TRACE(HTMLIFrameElementPermissions)
{
    visitor->trace(m_element);
    DOMTokenList::trace(visitor);
    DOMTokenListObserver::trace(visitor);
}

Vector<WebPermissionType> HTMLIFrameElementPermissions::parseDelegatedPermissions(String& invalidTokensErrorMessage) const
{
    Vector<WebPermissionType> permissions;
    unsigned numTokenErrors = 0;
    StringBuilder tokenErrors;
    const SpaceSplitString& tokens = this->tokens();

    for (size_t i = 0; i < tokens.size(); ++i) {
        WebPermissionType type;
        if (getPermissionType(tokens[i], &type)) {
            permissions.append(type);
        } else {
            if (numTokenErrors)
                tokenErrors.append(", '");
            else
                tokenErrors.append('\'');
            tokenErrors.append(tokens[i]);
            tokenErrors.append('\'');
            ++numTokenErrors;
        }
    }

    if (numTokenErrors) {
        if (numTokenErrors > 1)
            tokenErrors.append(" are invalid permissions flags.");
        else
            tokenErrors.append(" is an invalid permissions flag.");
        invalidTokensErrorMessage = tokenErrors.toString();
    }

    return permissions;
}

bool HTMLIFrameElementPermissions::validateTokenValue(const AtomicString& tokenValue, ExceptionState&) const
{
    WebPermissionType unused;
    return getPermissionType(tokenValue, &unused);
}

void HTMLIFrameElementPermissions::valueWasSet()
{
    if (m_element)
        m_element->permissionsValueWasSet();
}

} // namespace blink
