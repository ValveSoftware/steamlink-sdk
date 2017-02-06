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

#include "qsgsoftwarerenderer_p.h"

#include "qsgsoftwarerenderablenodeupdater_p.h"
#include "qsgsoftwarerenderlistbuilder_p.h"
#include "qsgsoftwarecontext_p.h"
#include "qsgsoftwarerenderablenode_p.h"

#include <QtGui/QPaintDevice>
#include <QtGui/QBackingStore>
#include <QElapsedTimer>

Q_LOGGING_CATEGORY(lcRenderer, "qt.scenegraph.softwarecontext.renderer")

QT_BEGIN_NAMESPACE

QSGSoftwareRenderer::QSGSoftwareRenderer(QSGRenderContext *context)
    : QSGAbstractSoftwareRenderer(context)
    , m_paintDevice(nullptr)
    , m_backingStore(nullptr)
{
}

QSGSoftwareRenderer::~QSGSoftwareRenderer()
{
}

void QSGSoftwareRenderer::setCurrentPaintDevice(QPaintDevice *device)
{
    m_paintDevice = device;
    m_backingStore = nullptr;
}

QPaintDevice *QSGSoftwareRenderer::currentPaintDevice() const
{
    return m_paintDevice;
}

void QSGSoftwareRenderer::setBackingStore(QBackingStore *backingStore)
{
    m_backingStore = backingStore;
    m_paintDevice = nullptr;
}

QRegion QSGSoftwareRenderer::flushRegion() const
{
    return m_flushRegion;
}

void QSGSoftwareRenderer::renderScene(uint)
{
    class B : public QSGBindable
    {
    public:
        void bind() const { }
    } bindable;
    QSGRenderer::renderScene(bindable);
}

void QSGSoftwareRenderer::render()
{
    if (!m_paintDevice && !m_backingStore)
        return;

    // If there is a backingstore, set the current paint device
    if (m_backingStore) {
        // For HiDPI QBackingStores, the paint device is not valid
        // until begin() has been called. See: QTBUG-55875
        m_backingStore->beginPaint(QRegion());
        m_paintDevice = m_backingStore->paintDevice();
        m_backingStore->endPaint();
    }

    QElapsedTimer renderTimer;

    setBackgroundColor(clearColor());
    setBackgroundSize(QSize(m_paintDevice->width() / m_paintDevice->devicePixelRatio(),
                            m_paintDevice->height() / m_paintDevice->devicePixelRatio()));

    // Build Renderlist
    // The renderlist is created by visiting each node in the tree and when a
    // renderable node is reach, we find the coorosponding RenderableNode object
    // and append it to the renderlist.  At this point the RenderableNode object
    // should not need any further updating so it is just a matter of appending
    // RenderableNodes
    renderTimer.start();
    buildRenderList();
    qint64 buildRenderListTime = renderTimer.restart();

    // Optimize Renderlist
    // This is a pass through the renderlist to determine what actually needs to
    // be painted.  Without this pass the renderlist will simply render each item
    // from back to front, with a high potential for overdraw.  It would also lead
    // to the entire window being flushed every frame.  The objective of the
    // optimization pass is to only paint dirty nodes that are not occuluded.  A
    // side effect of this is that additional nodes may need to be marked dirty to
    // force a repaint.  It is also important that any item that needs to be
    // repainted only paints what is needed, via the use of clip regions.
    const QRegion updateRegion = optimizeRenderList();
    qint64 optimizeRenderListTime = renderTimer.restart();

    // If Rendering to a backingstore, prepare it to be updated
    if (m_backingStore != nullptr) {
        m_backingStore->beginPaint(updateRegion);
        // It is possible that a QBackingStore's paintDevice() will change
        // when begin() is called.
        m_paintDevice = m_backingStore->paintDevice();
    }

    QPainter painter(m_paintDevice);
    painter.setRenderHint(QPainter::Antialiasing);
    auto rc = static_cast<QSGSoftwareRenderContext *>(context());
    QPainter *prevPainter = rc->m_activePainter;
    rc->m_activePainter = &painter;

    // Render the contents Renderlist
    m_flushRegion = renderNodes(&painter);
    qint64 renderTime = renderTimer.elapsed();

    if (m_backingStore != nullptr)
        m_backingStore->endPaint();

    rc->m_activePainter = prevPainter;
    qCDebug(lcRenderer) << "render" << m_flushRegion << buildRenderListTime << optimizeRenderListTime << renderTime;
}

QT_END_NAMESPACE
