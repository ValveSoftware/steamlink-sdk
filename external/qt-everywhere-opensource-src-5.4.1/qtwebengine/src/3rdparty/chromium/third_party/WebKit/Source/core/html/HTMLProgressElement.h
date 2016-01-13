/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef HTMLProgressElement_h
#define HTMLProgressElement_h

#include "core/html/LabelableElement.h"

namespace WebCore {

class ExceptionState;
class ProgressValueElement;
class RenderProgress;

class HTMLProgressElement FINAL : public LabelableElement {
public:
    static const double IndeterminatePosition;
    static const double InvalidPosition;

    static PassRefPtrWillBeRawPtr<HTMLProgressElement> create(Document&);

    double value() const;
    void setValue(double);

    double max() const;
    void setMax(double);

    double position() const;

    virtual bool canContainRangeEndPoint() const OVERRIDE { return false; }

    virtual void trace(Visitor*) OVERRIDE;

private:
    explicit HTMLProgressElement(Document&);
    virtual ~HTMLProgressElement();

    virtual bool areAuthorShadowsAllowed() const OVERRIDE { return false; }
    virtual bool shouldAppearIndeterminate() const OVERRIDE;
    virtual bool supportLabels() const OVERRIDE { return true; }

    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;
    RenderProgress* renderProgress() const;

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;

    virtual void attach(const AttachContext& = AttachContext()) OVERRIDE;

    void didElementStateChange();
    virtual void didAddUserAgentShadowRoot(ShadowRoot&) OVERRIDE;
    bool isDeterminate() const;

    RawPtrWillBeMember<ProgressValueElement> m_value;
};

} // namespace

#endif
