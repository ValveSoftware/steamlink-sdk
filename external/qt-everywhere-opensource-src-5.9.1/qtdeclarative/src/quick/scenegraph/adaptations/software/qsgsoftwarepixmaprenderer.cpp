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

#include "qsgsoftwarepixmaprenderer_p.h"
#include "qsgsoftwarecontext_p.h"

#include <QtQuick/QSGSimpleRectNode>

#include <QElapsedTimer>

Q_LOGGING_CATEGORY(lcPixmapRenderer, "qt.scenegraph.softwarecontext.pixmapRenderer")

QT_BEGIN_NAMESPACE

QSGSoftwarePixmapRenderer::QSGSoftwarePixmapRenderer(QSGRenderContext *context)
    : QSGAbstractSoftwareRenderer(context)
{

}

QSGSoftwarePixmapRenderer::~QSGSoftwarePixmapRenderer()
{

}

void QSGSoftwarePixmapRenderer::renderScene(uint)
{
    class B : public QSGBindable
    {
    public:
        void bind() const { }
    } bindable;
    QSGRenderer::renderScene(bindable);
}

void QSGSoftwarePixmapRenderer::render()
{

}

void QSGSoftwarePixmapRenderer::render(QPaintDevice *target)
{
    QElapsedTimer renderTimer;

    // Setup background item
    setBackgroundSize(QSize(target->width(), target->height()));
    setBackgroundColor(clearColor());

    QPainter painter(target);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setWindow(m_projectionRect);
    auto rc = static_cast<QSGSoftwareRenderContext *>(context());
    QPainter *prevPainter = rc->m_activePainter;
    rc->m_activePainter = &painter;

    renderTimer.start();
    buildRenderList();
    qint64 buildRenderListTime = renderTimer.restart();

    // Optimize Renderlist
    // Right now there is an assumption that when possible the same pixmap will
    // be reused.  So we can treat it like a backing store in that we can assume
    // that when the pixmap is not being resized, the data can be reused.  What is
    // different though is that everything should be marked as dirty on a resize.
    optimizeRenderList();
    qint64 optimizeRenderListTime = renderTimer.restart();

    QRegion paintedRegion = renderNodes(&painter);
    qint64 renderTime = renderTimer.elapsed();

    rc->m_activePainter = prevPainter;
    qCDebug(lcPixmapRenderer) << "pixmapRender" << paintedRegion << buildRenderListTime << optimizeRenderListTime << renderTime;
}

void QSGSoftwarePixmapRenderer::setProjectionRect(const QRect &projectionRect)
{
    m_projectionRect = projectionRect;
}

QT_END_NAMESPACE
