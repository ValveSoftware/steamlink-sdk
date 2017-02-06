// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSDstyle license that can be
// found in the LICENSE file.

#ifndef HTMLIFrameElementPermissions_h
#define HTMLIFrameElementPermissions_h

#include "core/CoreExport.h"
#include "core/dom/DOMTokenList.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/permissions/WebPermissionType.h"
#include "wtf/Vector.h"

namespace blink {

class HTMLIFrameElement;

class CORE_EXPORT HTMLIFrameElementPermissions final : public DOMTokenList, public DOMTokenListObserver {
    USING_GARBAGE_COLLECTED_MIXIN(HTMLIFrameElementPermissions);
public:
    static HTMLIFrameElementPermissions* create(HTMLIFrameElement* element)
    {
        return new HTMLIFrameElementPermissions(element);
    }

    ~HTMLIFrameElementPermissions() override;

    Vector<WebPermissionType> parseDelegatedPermissions(String& invalidTokensErrorMessage) const;

    DECLARE_VIRTUAL_TRACE();

private:
    explicit HTMLIFrameElementPermissions(HTMLIFrameElement*);
    bool validateTokenValue(const AtomicString& tokenValue, ExceptionState&) const override;

    // DOMTokenListObserver.
    void valueWasSet() override;

    Member<HTMLIFrameElement> m_element;
};

} // namespace blink

#endif
