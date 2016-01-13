/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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
 */

#include "config.h"
#include "core/svg/SVGZoomEvent.h"

#include "core/svg/SVGPointTearOff.h"
#include "core/svg/SVGRectTearOff.h"

namespace WebCore {

SVGZoomEvent::SVGZoomEvent()
    : m_newScale(0.0f)
    , m_previousScale(0.0f)
{
    ScriptWrappable::init(this);
}

PassRefPtr<SVGRectTearOff> SVGZoomEvent::zoomRectScreen() const
{
    RefPtr<SVGRectTearOff> rectTearOff = SVGRectTearOff::create(SVGRect::create(), 0, PropertyIsNotAnimVal);
    rectTearOff->setIsReadOnlyProperty();
    return rectTearOff.release();
}

float SVGZoomEvent::previousScale() const
{
    return m_previousScale;
}

PassRefPtr<SVGPointTearOff> SVGZoomEvent::previousTranslate() const
{
    RefPtr<SVGPointTearOff> pointTearOff = SVGPointTearOff::create(SVGPoint::create(m_previousTranslate), 0, PropertyIsNotAnimVal);
    pointTearOff->setIsReadOnlyProperty();
    return pointTearOff.release();
}

float SVGZoomEvent::newScale() const
{
    return m_newScale;
}

PassRefPtr<SVGPointTearOff> SVGZoomEvent::newTranslate() const
{
    RefPtr<SVGPointTearOff> pointTearOff = SVGPointTearOff::create(SVGPoint::create(m_newTranslate), 0, PropertyIsNotAnimVal);
    pointTearOff->setIsReadOnlyProperty();
    return pointTearOff.release();
}

const AtomicString& SVGZoomEvent::interfaceName() const
{
    return EventNames::SVGZoomEvent;
}

void SVGZoomEvent::trace(Visitor* visitor)
{
    UIEvent::trace(visitor);
}

} // namespace WebCore
