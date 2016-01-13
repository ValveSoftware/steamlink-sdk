/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AXMockObject_h
#define AXMockObject_h

#include "core/accessibility/AXObject.h"

namespace WebCore {

class AXMockObject : public AXObject {

protected:
    AXMockObject();
public:
    virtual ~AXMockObject();

    virtual void setParent(AXObject* parent) { m_parent = parent; }

    // AXObject overrides.
    virtual AXObject* parentObject() const OVERRIDE { return m_parent; }
    virtual bool isEnabled() const OVERRIDE { return true; }

protected:
    AXObject* m_parent;

    // Must be called when the parent object clears its children.
    virtual void detachFromParent() OVERRIDE { m_parent = 0; }

private:
    virtual bool isMockObject() const OVERRIDE FINAL { return true; }

    virtual bool computeAccessibilityIsIgnored() const OVERRIDE;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXMockObject, isMockObject());

} // namespace WebCore

#endif // AXMockObject_h
