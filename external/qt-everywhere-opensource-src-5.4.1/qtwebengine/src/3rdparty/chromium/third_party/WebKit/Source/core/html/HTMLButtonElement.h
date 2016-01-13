/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef HTMLButtonElement_h
#define HTMLButtonElement_h

#include "core/html/HTMLFormControlElement.h"

namespace WebCore {

class HTMLButtonElement FINAL : public HTMLFormControlElement {
public:
    static PassRefPtrWillBeRawPtr<HTMLButtonElement> create(Document&, HTMLFormElement*);

    void setType(const AtomicString&);

    const AtomicString& value() const;

    virtual bool willRespondToMouseClickEvents() OVERRIDE;

private:
    HTMLButtonElement(Document&, HTMLFormElement*);

    enum Type { SUBMIT, RESET, BUTTON };

    virtual const AtomicString& formControlType() const OVERRIDE;

    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;

    // HTMLFormControlElement always creates one, but buttons don't need it.
    virtual bool alwaysCreateUserAgentShadowRoot() const OVERRIDE { return false; }

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void defaultEventHandler(Event*) OVERRIDE;

    virtual bool appendFormData(FormDataList&, bool) OVERRIDE;

    virtual bool isEnumeratable() const OVERRIDE { return true; }
    virtual bool supportLabels() const OVERRIDE { return true; }
    virtual bool isInteractiveContent() const OVERRIDE;
    virtual bool supportsAutofocus() const OVERRIDE;

    virtual bool canBeSuccessfulSubmitButton() const OVERRIDE;
    virtual bool isActivatedSubmit() const OVERRIDE;
    virtual void setActivatedSubmit(bool flag) OVERRIDE;

    virtual void accessKeyAction(bool sendMouseEvents) OVERRIDE;
    virtual bool isURLAttribute(const Attribute&) const OVERRIDE;

    virtual bool canStartSelection() const OVERRIDE { return false; }

    virtual bool isOptionalFormControl() const OVERRIDE { return true; }
    virtual bool recalcWillValidate() const OVERRIDE;

    Type m_type;
    bool m_isActivatedSubmit;
};

} // namespace

#endif
