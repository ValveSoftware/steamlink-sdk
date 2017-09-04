/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Graphical Effects module of the Qt Toolkit.
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

#include "qgfxsourceproxy_p.h"

#include <private/qquickshadereffectsource_p.h>
#include <private/qquickitem_p.h>
#include <private/qquickimage_p.h>

QT_BEGIN_NAMESPACE

QGfxSourceProxy::QGfxSourceProxy(QQuickItem *parentItem)
    : QQuickItem(parentItem)
    , m_input(0)
    , m_output(0)
    , m_proxy(0)
    , m_interpolation(AnyInterpolation)
{
}

QGfxSourceProxy::~QGfxSourceProxy()
{
    delete m_proxy;
}

void QGfxSourceProxy::setInput(QQuickItem *input)
{
    if (m_input == input)
        return;
    m_input = input;
    polish();
    emit inputChanged();
}

void QGfxSourceProxy::setOutput(QQuickItem *output)
{
    if (m_output == output)
        return;
    m_output = output;
    emit activeChanged();
    emit outputChanged();
}

void QGfxSourceProxy::setSourceRect(const QRectF &sourceRect)
{
    if (m_sourceRect == sourceRect)
        return;
    m_sourceRect = sourceRect;
    polish();
    emit sourceRectChanged();
}

void QGfxSourceProxy::setInterpolation(Interpolation i)
{
    if (m_interpolation == i)
        return;
    m_interpolation = i;
    polish();
    emit interpolationChanged();
}


void QGfxSourceProxy::useProxy()
{
    if (!m_proxy)
        m_proxy = new QQuickShaderEffectSource(this);
    m_proxy->setSourceRect(m_sourceRect);
    m_proxy->setSourceItem(m_input);
    m_proxy->setSmooth(m_interpolation != NearestInterpolation);
    setOutput(m_proxy);
}

QObject *QGfxSourceProxy::findLayer(QQuickItem *item)
{
    if (!item)
        return 0;
    QQuickItemPrivate *d = QQuickItemPrivate::get(item);
    if (d->extra.isAllocated() && d->extra->layer) {
        QObject *layer = qvariant_cast<QObject *>(item->property("layer"));
        if (layer && layer->property("enabled").toBool())
            return layer;
    }
    return 0;
}

void QGfxSourceProxy::updatePolish()
{
    if (m_input == 0) {
        setOutput(0);
        return;
    }

    QQuickImage *image = qobject_cast<QQuickImage *>(m_input);
    QQuickShaderEffectSource *shaderSource = qobject_cast<QQuickShaderEffectSource *>(m_input);
    bool childless = m_input->childItems().size() == 0;
    bool interpOk = m_interpolation == AnyInterpolation
                    || (m_interpolation == LinearInterpolation && m_input->smooth() == true)
                    || (m_interpolation == NearestInterpolation && m_input->smooth() == false);

    // Layers can be used in two different ways. Option 1 is when the item is
    // used as input to a separate ShaderEffect component. In this case,
    // m_input will be the item itself.
    QObject *layer = findLayer(m_input);
    if (!layer && shaderSource) {
        // Alternatively, the effect is applied via layer.effect, and the
        // input to the effect will be the layer's internal ShaderEffectSource
        // item. In this case, we need to backtrack and find the item that has
        // the layer and configure it accordingly.
        layer = findLayer(shaderSource->sourceItem());
    }

    // A bit crude test, but we're only using source rect for
    // blurring+transparent edge, so this is good enough.
    bool padded = m_sourceRect.x() < 0 || m_sourceRect.y() < 0;

    bool direct = false;

    if (layer) {
        // Auto-configure the layer so interpolation and padding works as
        // expected without allocating additional FBOs. In edgecases, where
        // this feature is undesiered, the user can simply use
        // ShaderEffectSource rather than layer.
        layer->setProperty("sourceRect", m_sourceRect);
        layer->setProperty("smooth", m_interpolation != NearestInterpolation);
        direct = true;

    } else if (childless && interpOk) {

        if (shaderSource) {
            if (shaderSource->sourceRect() == m_sourceRect || m_sourceRect.isEmpty())
                direct = true;

        } else if (!padded && ((image && image->fillMode() == QQuickImage::Stretch)
                                || (!image && m_input->isTextureProvider())
                              )
                  ) {
            direct = true;
        }
    }

    if (direct) {
        setOutput(m_input);
    } else {
        useProxy();
    }

    // Remove the proxy if it is not in use..
    if (m_proxy && m_output == m_input) {
        delete m_proxy;
        m_proxy = 0;
    }
}

QT_END_NAMESPACE
