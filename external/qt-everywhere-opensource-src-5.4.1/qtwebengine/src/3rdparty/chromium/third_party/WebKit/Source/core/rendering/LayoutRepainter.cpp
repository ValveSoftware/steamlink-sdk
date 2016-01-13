/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/rendering/LayoutRepainter.h"

#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObject.h"

namespace WebCore {

LayoutRepainter::LayoutRepainter(RenderObject& object, bool checkForRepaint)
    : m_object(object)
    , m_repaintContainer(0)
    , m_checkForRepaint(checkForRepaint)
{
    if (RuntimeEnabledFeatures::repaintAfterLayoutEnabled())
        return;

    if (m_checkForRepaint) {
        m_repaintContainer = m_object.containerForPaintInvalidation();
        {
            // Hits in compositing/video/video-controls-layer-creation.html
            DisableCompositingQueryAsserts disabler;
            m_oldBounds = m_object.boundsRectForPaintInvalidation(m_repaintContainer);
            m_oldOffset = RenderLayer::positionFromPaintInvalidationContainer(&m_object, m_repaintContainer);
        }
    }
}

bool LayoutRepainter::repaintAfterLayout()
{
    if (RuntimeEnabledFeatures::repaintAfterLayoutEnabled())
        return false;

    // Hits in compositing/video/video-controls-layer-creation.html
    DisableCompositingQueryAsserts disabler;

    return m_checkForRepaint ? m_object.invalidatePaintAfterLayoutIfNeeded(m_repaintContainer, m_object.selfNeedsLayout(), m_oldBounds, m_oldOffset) : false;
}

} // namespace WebCore
