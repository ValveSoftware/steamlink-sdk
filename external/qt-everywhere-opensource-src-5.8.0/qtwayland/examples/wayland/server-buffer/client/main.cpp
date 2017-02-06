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

#include "serverbufferrenderer.h"

#include <QtGui/QWindow>
#include <QtGui/QGuiApplication>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLVertexArrayObject>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLTexture>
#include <QtCore/QTimer>

#include "qwayland-share-buffer.h"
#include <QtWaylandClient/private/qwayland-wayland.h>
#include <QtWaylandClient/private/qwaylandserverbufferintegration_p.h>
#include <QtWaylandClient/private/qwaylanddisplay_p.h>
#include <QtWaylandClient/private/qwaylandintegration_p.h>


class Window
    : public QWindow
    , public QtWayland::wl_registry
    , public QtWayland::qt_share_buffer
{
    Q_OBJECT
public:
    Window(QWindow *parent = 0)
        : QWindow(parent)
        , m_context(0)
    {
        setSurfaceType(QSurface::OpenGLSurface);
        QSurfaceFormat sformat = format();
        sformat.setAlphaBufferSize(8);
        sformat.setRedBufferSize(8);
        sformat.setGreenBufferSize(8);
        sformat.setBlueBufferSize(8);
        setFormat(sformat);
        create();

        if (!QGuiApplication::platformNativeInterface() || !QGuiApplication::platformNativeInterface()->nativeResourceForIntegration("wl_display")) {
            qDebug() << "This application requires a wayland plugin";
            QCoreApplication::quit();
            return;
        }

        QtWaylandClient::QWaylandIntegration *wayland_integration = static_cast<QtWaylandClient::QWaylandIntegration *>(QGuiApplicationPrivate::platformIntegration());

        m_server_buffer_integration = wayland_integration->serverBufferIntegration();
        if (!m_server_buffer_integration) {
            qDebug() << "This application requires a working serverBufferIntegration";
            QCoreApplication::quit();
            return;
        }


        QtWaylandClient::QWaylandDisplay *wayland_display = wayland_integration->display();
        struct ::wl_registry *registry = wl_display_get_registry(wayland_display->wl_display());
        QtWayland::wl_registry::init(registry);
    }

public slots:
    void render()
    {
        if (m_server_buffer_list.isEmpty())
            return;

        if (!m_context) {
            m_context = new QOpenGLContext(this);
            m_context->setFormat(format());
            m_context->create();
        }

        m_context->makeCurrent(this);
        QOpenGLFunctions *funcs = m_context->functions();
        funcs->glBindFramebuffer(GL_FRAMEBUFFER, m_context->defaultFramebufferObject());

        glViewport(0, 0, width() * devicePixelRatio(), height() * devicePixelRatio());
        glClearColor(0.f, 0.f, 0.0f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        qreal x = 0;
        for (int i = 0; i < m_server_buffer_list.size(); i++) {
            ServerBufferRenderer *renderer = static_cast<ServerBufferRenderer *>(m_server_buffer_list[i]->userData());
            if (!renderer) {
                renderer = new ServerBufferRenderer(m_server_buffer_list.at(i));
            }

            const QSizeF buffer_size = m_server_buffer_list.at(i)->size();
            qreal scale_x = buffer_size.width() / width();
            qreal scale_y = buffer_size.height() / height() * - 1;
            qreal translate_left = (((buffer_size.width() / 2) / width()) * 2) - 1;
            qreal translate_top = -(((buffer_size.height() / 2) / height()) * 2) + 1;
            qreal translate_x = translate_left + ((x / width())*2);

            QMatrix4x4 transform;
            transform.translate(translate_x, translate_top);
            transform.scale(scale_x, scale_y);
            renderer->render(transform);

            x += buffer_size.width();
        }

        m_context->swapBuffers(this);
    }

protected:
    void registry_global(uint32_t name, const QString &interface, uint32_t version) Q_DECL_OVERRIDE
    {
        Q_UNUSED(version);
        if (interface == QStringLiteral("qt_share_buffer")) {
            QtWayland::qt_share_buffer::init(QtWayland::wl_registry::object(), name, 1);
        }
    }

    void share_buffer_cross_buffer(struct ::qt_server_buffer *buffer) Q_DECL_OVERRIDE
    {
        QtWaylandClient::QWaylandServerBuffer *serverBuffer = m_server_buffer_integration->serverBuffer(buffer);
        if (m_server_buffer_list.isEmpty()) {
            setWidth(serverBuffer->size().width());
            setHeight(serverBuffer->size().height());
        } else {
            setWidth(width() + serverBuffer->size().width());
            setHeight(std::max(serverBuffer->size().height(), height()));
        }
        m_server_buffer_list.append(serverBuffer);
        render();
    }

private:
    QtWaylandClient::QWaylandServerBufferIntegration *m_server_buffer_integration;
    QList<QtWaylandClient::QWaylandServerBuffer *>m_server_buffer_list;
    GLuint m_server_buffer_texture;
    QOpenGLContext *m_context;
    QOpenGLVertexArrayObject *m_vao;
    GLuint m_vertexbuffer;
    GLuint m_texture_coords;
    QOpenGLShaderProgram *m_shader_program;
    QOpenGLTexture *m_image_texture;
};

int main (int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    Window window;
    window.show();
    return app.exec();
}

#include "main.moc"
