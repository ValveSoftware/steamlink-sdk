/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QWAYLANDSERVERBUFFERINTEGRATION_H
#define QWAYLANDSERVERBUFFERINTEGRATION_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qglobal.h>
#include <QtCore/QSize>
#include <QtGui/qopengl.h>

#include <QtWaylandCompositor/qtwaylandcompositorglobal.h>

struct wl_client;
struct wl_resource;

QT_BEGIN_NAMESPACE

class QWaylandCompositor;
class QOpenGLContext;

namespace QtWayland {
class Display;

class Q_WAYLAND_COMPOSITOR_EXPORT ServerBuffer
{
public:
    enum Format {
        RGBA32,
        A8
    };

    ServerBuffer(const QSize &size, ServerBuffer::Format format);
    virtual ~ServerBuffer();

    virtual struct ::wl_resource *resourceForClient(struct ::wl_client *) = 0;

    virtual void bindTextureToBuffer() = 0;

    virtual bool isYInverted() const;

    QSize size() const;
    Format format() const;
protected:
    QSize m_size;
    Format m_format;
};

class Q_WAYLAND_COMPOSITOR_EXPORT ServerBufferIntegration
{
public:
    ServerBufferIntegration();
    virtual ~ServerBufferIntegration();

    virtual void initializeHardware(QWaylandCompositor *);

    virtual bool supportsFormat(ServerBuffer::Format format) const = 0;
    virtual ServerBuffer *createServerBuffer(const QSize &size, ServerBuffer::Format format) = 0;
};

}

QT_END_NAMESPACE

#endif //QWAYLANDSERVERBUFFERINTEGRATION_H
