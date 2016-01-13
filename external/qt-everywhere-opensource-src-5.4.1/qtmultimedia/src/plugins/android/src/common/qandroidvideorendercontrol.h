/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#ifndef QANDROIDVIDEORENDERCONTROL_H
#define QANDROIDVIDEORENDERCONTROL_H

#include <qvideorenderercontrol.h>
#include <qmutex.h>
#include "qandroidvideooutput.h"

QT_BEGIN_NAMESPACE

class QOpenGLTexture;
class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;
class AndroidSurfaceTexture;

class OpenGLResourcesDeleter : public QObject
{
    Q_OBJECT
public:
    OpenGLResourcesDeleter()
        : m_textureID(0)
        , m_fbo(0)
        , m_program(0)
    { }

    ~OpenGLResourcesDeleter();

    void setTexture(quint32 id) { m_textureID = id; }
    void setFbo(QOpenGLFramebufferObject *fbo) { m_fbo = fbo; }
    void setShaderProgram(QOpenGLShaderProgram *prog) { m_program = prog; }

private:
    quint32 m_textureID;
    QOpenGLFramebufferObject *m_fbo;
    QOpenGLShaderProgram *m_program;
};

class QAndroidVideoRendererControl : public QVideoRendererControl, public QAndroidVideoOutput
{
    Q_OBJECT
    Q_INTERFACES(QAndroidVideoOutput)
public:
    explicit QAndroidVideoRendererControl(QObject *parent = 0);
    ~QAndroidVideoRendererControl() Q_DECL_OVERRIDE;

    QAbstractVideoSurface *surface() const Q_DECL_OVERRIDE;
    void setSurface(QAbstractVideoSurface *surface) Q_DECL_OVERRIDE;

    AndroidSurfaceTexture *surfaceTexture() Q_DECL_OVERRIDE;
    bool isReady() Q_DECL_OVERRIDE;
    void setVideoSize(const QSize &size) Q_DECL_OVERRIDE;
    void stop() Q_DECL_OVERRIDE;
    void reset() Q_DECL_OVERRIDE;

    void customEvent(QEvent *) Q_DECL_OVERRIDE;

Q_SIGNALS:
    void readyChanged(bool);

private Q_SLOTS:
    void onFrameAvailable();

private:
    bool initSurfaceTexture();
    void renderFrameToFbo();
    void createGLResources();

    QMutex m_mutex;
    void clearSurfaceTexture();

    QAbstractVideoSurface *m_surface;
    QSize m_nativeSize;

    AndroidSurfaceTexture *m_surfaceTexture;

    quint32 m_externalTex;
    QOpenGLFramebufferObject *m_fbo;
    QOpenGLShaderProgram *m_program;
    OpenGLResourcesDeleter *m_glDeleter;

    friend class AndroidTextureVideoBuffer;
};

QT_END_NAMESPACE

#endif // QANDROIDVIDEORENDERCONTROL_H
