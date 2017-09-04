/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef FBITEM_H
#define FBITEM_H

#include <QQuickFramebufferObject>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QVector3D>
#include "logo.h"

struct StateBinder;

class FbItemRenderer : public QQuickFramebufferObject::Renderer
{
public:
    FbItemRenderer(bool multisample);
    void synchronize(QQuickFramebufferObject *item) Q_DECL_OVERRIDE;
    void render() Q_DECL_OVERRIDE;
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) Q_DECL_OVERRIDE;

private:
    void ensureInit();
    void initBuf();
    void setupVertexAttribs();
    void initProgram();
    void updateDirtyUniforms();

    bool m_inited;
    bool m_multisample;
    QMatrix4x4 m_proj;
    QMatrix4x4 m_camera;
    QMatrix4x4 m_baseWorld;
    QMatrix4x4 m_world;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_logoVbo;
    Logo m_logo;
    QScopedPointer<QOpenGLShaderProgram> m_program;
    int m_projMatrixLoc;
    int m_camMatrixLoc;
    int m_worldMatrixLoc;
    int m_normalMatrixLoc;
    int m_lightPosLoc;
    QVector3D m_rotation;

    enum Dirty {
        DirtyProjection = 0x01,
        DirtyCamera = 0x02,
        DirtyWorld = 0x04,
        DirtyLight = 0x08,
        DirtyAll = 0xFF
    };
    int m_dirty;

    friend struct StateBinder;
};

class FbItem : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(QVector3D eye READ eye WRITE setEye)
    Q_PROPERTY(QVector3D target READ target WRITE setTarget)
    Q_PROPERTY(QVector3D rotation READ rotation WRITE setRotation)
    Q_PROPERTY(bool multisample READ multisample WRITE setMultisample)

public:
    explicit FbItem(QQuickItem *parent = 0);

    QQuickFramebufferObject::Renderer *createRenderer() const Q_DECL_OVERRIDE;

    QVector3D eye() const { return m_eye; }
    void setEye(const QVector3D &v);
    QVector3D target() const { return m_target; }
    void setTarget(const QVector3D &v);

    QVector3D rotation() const { return m_rotation; }
    void setRotation(const QVector3D &v);

    enum SyncState {
        CameraNeedsSync = 0x01,
        RotationNeedsSync = 0x02,
        AllNeedsSync = 0xFF
    };
    int swapSyncState();

    bool multisample() const { return m_multisample; }
    void setMultisample(bool m) { m_multisample = m; }

private:
    QVector3D m_eye;
    QVector3D m_target;
    QVector3D m_rotation;
    int m_syncState;
    bool m_multisample;
};

#endif // FBITEM_H
