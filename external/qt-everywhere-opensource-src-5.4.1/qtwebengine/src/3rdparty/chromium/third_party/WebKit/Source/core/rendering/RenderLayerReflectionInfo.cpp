/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "config.h"
#include "core/rendering/RenderLayerReflectionInfo.h"

#include "core/frame/UseCounter.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderReplica.h"
#include "core/rendering/style/RenderStyle.h"
#include "platform/transforms/ScaleTransformOperation.h"
#include "platform/transforms/TranslateTransformOperation.h"

#include "wtf/RefPtr.h"

namespace WebCore {

RenderLayerReflectionInfo::RenderLayerReflectionInfo(RenderBox& renderer)
    : m_box(renderer)
    , m_isPaintingInsideReflection(false)
{
    UseCounter::count(m_box.document(), UseCounter::Reflection);

    m_reflection = RenderReplica::createAnonymous(&(m_box.document()));
    m_reflection->setParent(&m_box); // We create a 1-way connection.
}

RenderLayerReflectionInfo::~RenderLayerReflectionInfo()
{
    if (!m_reflection->documentBeingDestroyed())
        m_reflection->removeLayers(m_box.layer());

    m_reflection->setParent(0);
    m_reflection->destroy();
    m_reflection = 0;
}


RenderLayer* RenderLayerReflectionInfo::reflectionLayer() const
{
    return m_reflection->layer();
}

void RenderLayerReflectionInfo::updateAfterStyleChange(const RenderStyle* oldStyle)
{
    RefPtr<RenderStyle> newStyle = RenderStyle::create();
    newStyle->inheritFrom(m_box.style());

    // Map in our transform.
    TransformOperations transform;
    switch (m_box.style()->boxReflect()->direction()) {
    case ReflectionBelow:
        transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed),
            Length(100., Percent), TransformOperation::Translate));
        transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed),
            m_box.style()->boxReflect()->offset(), TransformOperation::Translate));
        transform.operations().append(ScaleTransformOperation::create(1.0, -1.0, ScaleTransformOperation::Scale));
        break;

    case ReflectionAbove:
        transform.operations().append(ScaleTransformOperation::create(1.0, -1.0, ScaleTransformOperation::Scale));
        transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed),
            Length(100., Percent), TransformOperation::Translate));
        transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed),
            m_box.style()->boxReflect()->offset(), TransformOperation::Translate));
        break;

    case ReflectionRight:
        transform.operations().append(TranslateTransformOperation::create(Length(100., Percent),
            Length(0, Fixed), TransformOperation::Translate));
        transform.operations().append(TranslateTransformOperation::create(
            m_box.style()->boxReflect()->offset(), Length(0, Fixed), TransformOperation::Translate));
        transform.operations().append(ScaleTransformOperation::create(-1.0, 1.0, ScaleTransformOperation::Scale));
        break;

    case ReflectionLeft:
        transform.operations().append(ScaleTransformOperation::create(-1.0, 1.0, ScaleTransformOperation::Scale));
        transform.operations().append(TranslateTransformOperation::create(Length(100., Percent),
            Length(0, Fixed), TransformOperation::Translate));
        transform.operations().append(TranslateTransformOperation::create(
            m_box.style()->boxReflect()->offset(), Length(0, Fixed), TransformOperation::Translate));
        break;
    }
    newStyle->setTransform(transform);

    // Map in our mask.
    newStyle->setMaskBoxImage(m_box.style()->boxReflect()->mask());

    m_reflection->setStyle(newStyle.release());
}

void RenderLayerReflectionInfo::paint(GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags flags)
{
    if (m_isPaintingInsideReflection)
        return;

    // Mark that we are now inside replica painting.
    m_isPaintingInsideReflection = true;
    reflectionLayer()->paintLayer(context, paintingInfo, flags);
    m_isPaintingInsideReflection = false;
}

String RenderLayerReflectionInfo::debugName() const
{
    return m_box.debugName() + " (reflection)";
}

} // namespace WebCore
