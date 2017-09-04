/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "contextattributes_p.h"

#include <QVariantMap>
#include <QDebug>

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

/*!
 * \qmltype Canvas3DContextAttributes
 * \since QtCanvas3D 1.0
 * \inqmlmodule QtCanvas3D
 * \brief Attribute set for Context3D
 *
 * Canvas3DContextAttributes is an attribute set that can be given as parameter on first call to
 * Canvas3D object's \l{Canvas3D::getContext}{getContext(string type, Canvas3DContextAttributes options)}
 * method call.
 * It can also be requested from the Context3D using Context3D::getContextAttributes() later on
 * to verify what exact attributes are in fact enabled/disabled in the created context.
 *
 * \sa Context3D, Canvas3D
 */

CanvasContextAttributes::CanvasContextAttributes(QObject *parent) :
    CanvasAbstractObject(0, parent),
    m_alpha(true),
    m_depth(true),
    m_stencil(false),
    m_antialias(true),
    m_premultipliedAlpha(true),
    m_preserveDrawingBuffer(false),
    m_preferLowPowerToHighPerformance(false),
    m_failIfMajorPerformanceCaveat(false)
{
}

CanvasContextAttributes::~CanvasContextAttributes()
{
}

void CanvasContextAttributes::setFrom(const QVariantMap &options)
{
    for (QVariantMap::const_iterator iter = options.begin(); iter != options.end(); ++iter) {
        if (iter.key() == "alpha")
            setAlpha(iter.value().toBool());
        else if (iter.key() == "depth")
            setDepth(iter.value().toBool());
        else if (iter.key() == "stencil")
            setStencil(iter.value().toBool());
        else if (iter.key() == "antialias")
            setAntialias(iter.value().toBool());
        else if (iter.key() == "premultipliedAlpha")
            setPremultipliedAlpha(iter.value().toBool());
        else if (iter.key() == "preserveDrawingBuffer")
            setPreserveDrawingBuffer(iter.value().toBool());
        else if (iter.key() == "preferLowPowerToHighPerformance")
            setPreferLowPowerToHighPerformance(iter.value().toBool());
        else if (iter.key() == "failIfMajorPerformanceCaveat")
            setFailIfMajorPerformanceCaveat(iter.value().toBool());
    }
}

void CanvasContextAttributes::setFrom(const CanvasContextAttributes &source)
{
    m_alpha = source.alpha();
    m_depth = source.depth();
    m_stencil = source.stencil();
    m_antialias = source.antialias();
    m_premultipliedAlpha = source.premultipliedAlpha();
    m_preserveDrawingBuffer = source.preserveDrawingBuffer();
    m_preferLowPowerToHighPerformance = source.preferLowPowerToHighPerformance();
    m_failIfMajorPerformanceCaveat = source.failIfMajorPerformanceCaveat();
}

/*!
 * \qmlproperty bool Canvas3DContextAttributes::alpha
 * Specifies whether the default render target of the Context3D has an alpha channel for the
 * purposes of blending its contents with overlapping Qt Quick items.
 * Defaults to \c{true}.
 */
bool CanvasContextAttributes::alpha()  const
{
    return m_alpha;
}

void CanvasContextAttributes::setAlpha(bool value)
{
    if (m_alpha == value)
        return;

    m_alpha = value;
    emit alphaChanged(value);
}

/*!
 * \qmlproperty bool Canvas3DContextAttributes::depth
 * Specifies whether a depth attachment is to be created and attached to the default render target
 * of the Context3D.
 * Defaults to \c{true}.
 */
bool CanvasContextAttributes::depth() const
{
    return m_depth;
}

void CanvasContextAttributes::setDepth(bool value)
{
    if (m_depth == value)
        return;

    m_depth = value;
    emit depthChanged(value);
}

/*!
 * \qmlproperty bool Canvas3DContextAttributes::stencil
 * Specifies whether a stencil attachment is to be created and attached to the default render
 * target of the Context3D.
 * Defaults to \c{false}.
 */
bool CanvasContextAttributes::stencil() const
{
    return m_stencil;
}

void CanvasContextAttributes::setStencil(bool value)
{
    if (m_stencil == value)
        return;

    m_stencil = value;
    emit stencilChanged(value);
}

/*!
 * \qmlproperty bool Canvas3DContextAttributes::antialias
 * Specifies whether antialiasing buffer is created for the default render target of the Context3D.
 * Defaults to \c{true}.
 */
bool CanvasContextAttributes::antialias() const
{
    return m_antialias;
}

void CanvasContextAttributes::setAntialias(bool value)
{
    if (m_antialias == value)
        return;

    m_antialias = value;
    emit antialiasChanged(value);
}

/*!
 * \qmlproperty bool Canvas3DContextAttributes::premultipliedAlpha
 * Qt Quick always expects premultiplied alpha values when blending Qt Quick items together,
 * so keeping this property \c{true} is recommended. Setting it to \c{false} can cause
 * a minor performance impact, as an additional render pass is needed.
 * Defaults to \c{true}.
 */
bool CanvasContextAttributes::premultipliedAlpha() const
{
    return m_premultipliedAlpha;
}

void CanvasContextAttributes::setPremultipliedAlpha(bool value)
{
    if (m_premultipliedAlpha == value)
        return;

    m_premultipliedAlpha = value;
    emit premultipliedAlphaChanged(value);
}

/*!
 * \qmlproperty bool Canvas3DContextAttributes::preserveDrawingBuffer
 * Specifies whether or not the drawing buffer contents are preserved from frame to frame.
 * Ignored when drawing to the background or the foreground of the Qt Quick scene.
 * Defaults to \c{false}.
 */
bool CanvasContextAttributes::preserveDrawingBuffer() const
{
    return m_preserveDrawingBuffer;
}

void CanvasContextAttributes::setPreserveDrawingBuffer(bool value)
{
    if (m_preserveDrawingBuffer == value)
        return;

    m_preserveDrawingBuffer = value;
    emit preserveDrawingBufferChanged(value);
}

/*!
 * \qmlproperty bool Canvas3DContextAttributes::preferLowPowerToHighPerformance
 * Ignored. Defaults to \c{false}.
 */
bool CanvasContextAttributes::preferLowPowerToHighPerformance() const
{
    return m_preferLowPowerToHighPerformance;
}

void CanvasContextAttributes::setPreferLowPowerToHighPerformance(bool value)
{
    if (m_preferLowPowerToHighPerformance == value)
        return;

    m_preferLowPowerToHighPerformance = value;
    emit preferLowPowerToHighPerformanceChanged(value);
}

/*!
 * \qmlproperty bool Canvas3DContextAttributes::failIfMajorPerformanceCaveat
 * Ignored. Defaults to \c{false}.
 */
bool CanvasContextAttributes::failIfMajorPerformanceCaveat() const
{
    return m_failIfMajorPerformanceCaveat;
}

void CanvasContextAttributes::setFailIfMajorPerformanceCaveat(bool value)
{
    if (m_failIfMajorPerformanceCaveat == value)
        return;

    m_failIfMajorPerformanceCaveat = value;
    emit failIfMajorPerformanceCaveatChanged(value);
}

QDebug operator<<(QDebug dbg, const CanvasContextAttributes &attribs)
{
    dbg.nospace() << "Canvas3DContextAttributes(\n    alpha:"<< attribs.m_alpha  <<
                     "\n    depth:" << attribs.m_depth <<
                     "\n    m_stencil:" << attribs.m_stencil <<
                     "\n    antialias:"<< attribs.m_antialias <<
                     "\n    premultipliedAlpha:" << attribs.m_premultipliedAlpha <<
                     "\n    preserveDrawingBuffer:" << attribs.m_preserveDrawingBuffer <<
                     "\n    preferLowPowerToHighPerformance:" << attribs.m_preferLowPowerToHighPerformance <<
                     "\n    failIfMajorPerformanceCaveat:" << attribs.m_failIfMajorPerformanceCaveat << ")";
    return dbg.maybeSpace();
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
