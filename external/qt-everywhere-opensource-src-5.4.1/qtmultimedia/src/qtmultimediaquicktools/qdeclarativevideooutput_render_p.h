/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2012 Research In Motion
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QDECLARATIVEVIDEOOUTPUT_RENDER_P_H
#define QDECLARATIVEVIDEOOUTPUT_RENDER_P_H

#include "qdeclarativevideooutput_backend_p.h"
#include "qsgvideonode_i420.h"
#include "qsgvideonode_rgb.h"
#include "qsgvideonode_texture.h"

#include <QtCore/qmutex.h>
#include <QtMultimedia/qabstractvideosurface.h>

QT_BEGIN_NAMESPACE

class QSGVideoItemSurface;
class QVideoRendererControl;
class QOpenGLContext;

class QDeclarativeVideoRendererBackend : public QDeclarativeVideoBackend
{
public:
    QDeclarativeVideoRendererBackend(QDeclarativeVideoOutput *parent);
    ~QDeclarativeVideoRendererBackend();

    bool init(QMediaService *service);
    void itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &changeData);
    void releaseSource();
    void releaseControl();
    QSize nativeSize() const;
    void updateGeometry();
    QSGNode *updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *data);
    QAbstractVideoSurface *videoSurface() const;
    QRectF adjustedViewport() const Q_DECL_OVERRIDE;
    QOpenGLContext *glContext() const;

    friend class QSGVideoItemSurface;
    void present(const QVideoFrame &frame);
    void stop();

private:
    QPointer<QVideoRendererControl> m_rendererControl;
    QList<QSGVideoNodeFactoryInterface*> m_videoNodeFactories;
    QSGVideoItemSurface *m_surface;
    QOpenGLContext *m_glContext;
    QVideoFrame m_frame;
    bool m_frameChanged;
    QSGVideoNodeFactory_I420 m_i420Factory;
    QSGVideoNodeFactory_RGB m_rgbFactory;
    QSGVideoNodeFactory_Texture m_textureFactory;
    QMutex m_frameMutex;
    QRectF m_renderedRect;         // Destination pixel coordinates, clipped
    QRectF m_sourceTextureRect;    // Source texture coordinates
};

class QSGVideoItemSurface : public QAbstractVideoSurface
{
    Q_OBJECT
public:
    explicit QSGVideoItemSurface(QDeclarativeVideoRendererBackend *backend, QObject *parent = 0);
    ~QSGVideoItemSurface();
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(QAbstractVideoBuffer::HandleType handleType) const;
    bool start(const QVideoSurfaceFormat &format);
    void stop();
    bool present(const QVideoFrame &frame);
    void scheduleOpenGLContextUpdate();

private slots:
    void updateOpenGLContext();

private:
    QDeclarativeVideoRendererBackend *m_backend;
};

QT_END_NAMESPACE

#endif
