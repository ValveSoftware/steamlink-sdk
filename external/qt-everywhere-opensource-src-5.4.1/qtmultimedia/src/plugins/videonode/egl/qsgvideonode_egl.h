/****************************************************************************
**
** Copyright (C) 2014 Jolla Ltd.
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

#ifndef EGLVIDEONODE_H
#define EGLVIDEONODE_H

#include <private/qsgvideonode_p.h>

#include <QSGOpaqueTextureMaterial>
#include <QSGTexture>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifdef Bool
#  undef Bool
#endif

QT_BEGIN_NAMESPACE

class QSGVideoMaterial_EGL : public QSGMaterial
{
public:
    QSGVideoMaterial_EGL();
    ~QSGVideoMaterial_EGL();

    QSGMaterialShader *createShader() const;
    QSGMaterialType *type() const;
    int compare(const QSGMaterial *other) const;

    void setImage(EGLImageKHR image);

private:
    friend class QSGVideoMaterial_EGLShader;

    QRectF m_subrect;
    EGLImageKHR m_image;
    GLuint m_textureId;
};

class QSGVideoNode_EGL : public QSGVideoNode
{
public:
    QSGVideoNode_EGL(const QVideoSurfaceFormat &format);
    ~QSGVideoNode_EGL();

    void setCurrentFrame(const QVideoFrame &frame);
    QVideoFrame::PixelFormat pixelFormat() const;

private:
    QSGVideoMaterial_EGL m_material;
    QVideoFrame::PixelFormat m_pixelFormat;
};

class QSGVideoNodeFactory_EGL : public QSGVideoNodeFactoryPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.qt.sgvideonodefactory/5.2" FILE "egl.json")
public:
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
            QAbstractVideoBuffer::HandleType handleType) const;
    QSGVideoNode *createNode(const QVideoSurfaceFormat &format);
};

QT_END_NAMESPACE

#endif

