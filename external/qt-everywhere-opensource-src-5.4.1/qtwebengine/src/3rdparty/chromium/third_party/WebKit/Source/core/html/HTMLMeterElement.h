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

#ifndef HTMLMeterElement_h
#define HTMLMeterElement_h

#include "core/html/LabelableElement.h"

namespace WebCore {

class ExceptionState;
class MeterValueElement;
class RenderMeter;

class HTMLMeterElement FINAL : public LabelableElement {
public:
    static PassRefPtrWillBeRawPtr<HTMLMeterElement> create(Document&);

    enum GaugeRegion {
        GaugeRegionOptimum,
        GaugeRegionSuboptimal,
        GaugeRegionEvenLessGood
    };

    double value() const;
    void setValue(double);

    double min() const;
    void setMin(double);

    double max() const;
    void setMax(double);

    double low() const;
    void setLow(double);

    double high() const;
    void setHigh(double);

    double optimum() const;
    void setOptimum(double);

    double valueRatio() const;
    GaugeRegion gaugeRegion() const;

    virtual bool canContainRangeEndPoint() const OVERRIDE { return false; }

    virtual void trace(Visitor*) OVERRIDE;

private:
    explicit HTMLMeterElement(Document&);
    virtual ~HTMLMeterElement();

    virtual bool areAuthorShadowsAllowed() const OVERRIDE { return false; }
    RenderMeter* renderMeter() const;

    virtual bool supportLabels() const OVERRIDE { return true; }

    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;

    void didElementStateChange();
    virtual void didAddUserAgentShadowRoot(ShadowRoot&) OVERRIDE;

    RefPtrWillBeMember<MeterValueElement> m_value;
};

} // namespace

#endif
