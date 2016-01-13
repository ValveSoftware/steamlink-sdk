/*
 * Copyright (c) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTMLOutputElement_h
#define HTMLOutputElement_h

#include "core/dom/DOMSettableTokenList.h"
#include "core/html/HTMLFormControlElement.h"

namespace WebCore {

class HTMLOutputElement FINAL : public HTMLFormControlElement {
public:
    static PassRefPtrWillBeRawPtr<HTMLOutputElement> create(Document&, HTMLFormElement*);

    virtual bool willValidate() const OVERRIDE { return false; }

    String value() const;
    void setValue(const String&);
    String defaultValue() const;
    void setDefaultValue(const String&);
    void setFor(const AtomicString&);
    DOMSettableTokenList* htmlFor() const;

    virtual bool canContainRangeEndPoint() const OVERRIDE { return false; }

    virtual void trace(Visitor*) OVERRIDE;

private:
    HTMLOutputElement(Document&, HTMLFormElement*);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual const AtomicString& formControlType() const OVERRIDE;
    virtual bool isEnumeratable() const OVERRIDE { return true; }
    virtual bool supportLabels() const OVERRIDE { return true; }
    virtual bool supportsFocus() const OVERRIDE;
    virtual void childrenChanged(bool createdByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0) OVERRIDE;
    virtual void resetImpl() OVERRIDE;

    bool m_isDefaultValueMode;
    String m_defaultValue;
    RefPtrWillBeMember<DOMSettableTokenList> m_tokens;
};

} // namespace

#endif
