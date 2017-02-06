/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickmaskextruder_p.h"
#include <QtQml/qqml.h>
#include <QtQml/qqmlinfo.h>
#include <QImage>
#include <QDebug>
QT_BEGIN_NAMESPACE
/*!
    \qmltype MaskShape
    \instantiates QQuickMaskExtruder
    \inqmlmodule QtQuick.Particles
    \inherits Shape
    \brief For representing an image as a shape to affectors and emitters
    \ingroup qtquick-particles

*/
/*!
    \qmlproperty url QtQuick.Particles::MaskShape::source

    The image to use as the mask. Areas with non-zero opacity
    will be considered inside the shape.
*/


QQuickMaskExtruder::QQuickMaskExtruder(QObject *parent) :
    QQuickParticleExtruder(parent)
  , m_lastWidth(-1)
  , m_lastHeight(-1)
{
}

void QQuickMaskExtruder::setSource(QUrl arg)
{
    if (m_source != arg) {
        m_source = arg;

        m_lastHeight = -1;//Trigger reset
        m_lastWidth = -1;
        emit sourceChanged(arg);
        startMaskLoading();
    }
}

void QQuickMaskExtruder::startMaskLoading()
{
    m_pix.clear(this);
    if (m_source.isEmpty())
        return;
    m_pix.load(qmlEngine(this), m_source);
    if (m_pix.isLoading())
        m_pix.connectFinished(this, SLOT(finishMaskLoading()));
    else
        finishMaskLoading();
}

void QQuickMaskExtruder::finishMaskLoading()
{
    if (m_pix.isError())
        qmlInfo(this) << m_pix.error();
}

QPointF QQuickMaskExtruder::extrude(const QRectF &r)
{
    ensureInitialized(r);
    if (!m_mask.count() || m_img.isNull())
        return r.topLeft();
    const QPointF p = m_mask[rand() % m_mask.count()];
    //### Should random sub-pixel positioning be added?
    return p + r.topLeft();
}

bool QQuickMaskExtruder::contains(const QRectF &bounds, const QPointF &point)
{
    ensureInitialized(bounds);//###Current usage patterns WILL lead to different bounds/r calls. Separate list?
    if (m_img.isNull())
        return false;

    QPointF pt = point - bounds.topLeft();
    QPoint p(pt.x() * m_img.width() / bounds.width(),
             pt.y() * m_img.height() / bounds.height());
    return m_img.rect().contains(p) && (m_img.pixel(p) & 0xff000000);
}

void QQuickMaskExtruder::ensureInitialized(const QRectF &rf)
{
    // Convert to integer coords to avoid comparing floats and ints which would
    // often result in rounding errors.
    QRect r = rf.toRect();
    if (m_lastWidth == r.width() && m_lastHeight == r.height())
        return;//Same as before
    if (!m_pix.isReady())
        return;
    m_lastWidth = r.width();
    m_lastHeight = r.height();

    m_mask.clear();

    m_img = m_pix.image();
    // Image will in all likelyhood be in this format already, so
    // no extra memory or conversion takes place
    if (m_img.format() != QImage::Format_ARGB32 && m_img.format() != QImage::Format_ARGB32_Premultiplied)
        m_img = m_img.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    // resample on the fly using 16-bit
    int sx = (m_img.width() << 16) / r.width();
    int sy = (m_img.height() << 16) / r.height();
    int w = r.width();
    int h = r.height();
    for (int y=0; y<h; ++y) {
        const uint *sl = (const uint *) m_img.constScanLine((y * sy) >> 16);
        for (int x=0; x<w; ++x) {
            if (sl[(x * sx) >> 16] & 0xff000000)
                m_mask << QPointF(x, y);
        }
    }
}
QT_END_NAMESPACE
