/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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

#include <Qt3DQuickExtras/qt3dquickwindow.h>
#include <Qt3DQuick/QQmlAspectEngine>
#include <Qt3DRender/qcamera.h>
#include <Qt3DRender/qrenderaspect.h>
#include <Qt3DRender/qrendersurfaceselector.h>
#include <Qt3DRender/private/qrendersurfaceselector_p.h>
#include <Qt3DInput/qinputaspect.h>
#include <Qt3DInput/qinputsettings.h>
#include <Qt3DLogic/qlogicaspect.h>

#include <QQmlContext>
#include <qqmlincubator.h>
#include <QGuiApplication>
#include <QScreen>

#include <QtGui/qopenglcontext.h>

QT_BEGIN_NAMESPACE

namespace Qt3DExtras {

namespace Quick {

namespace {

class Qt3DQuickWindowIncubationController : public QObject, public QQmlIncubationController
{
    Q_OBJECT
public:
    explicit Qt3DQuickWindowIncubationController(QObject *parent = nullptr)
        : QObject(parent)
        , m_incubationTime(std::max(1, int(1000 / QGuiApplication::primaryScreen()->refreshRate()) / 3))
    {
        startTimer(QGuiApplication::primaryScreen()->refreshRate());
    }

    void timerEvent(QTimerEvent *) Q_DECL_OVERRIDE
    {
        incubateFor(m_incubationTime);
    }

private:
    const int m_incubationTime;
};

} // anonymous

Qt3DQuickWindow::Qt3DQuickWindow(QWindow *parent)
    : QWindow(parent)
    , m_engine(nullptr)
    , m_renderAspect(nullptr)
    , m_inputAspect(nullptr)
    , m_logicAspect(nullptr)
    , m_initialized(false)
    , m_cameraAspectRatioMode(AutomaticAspectRatio)
    , m_incubationController(nullptr)
{
    setSurfaceType(QSurface::OpenGLSurface);

    resize(1024, 768);

    QSurfaceFormat format;
#ifdef QT_OPENGL_ES_2
    format.setRenderableType(QSurfaceFormat::OpenGLES);
#else
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) {
        format.setVersion(4, 3);
        format.setProfile(QSurfaceFormat::CoreProfile);
    }
#endif
    format.setDepthBufferSize(24);
    format.setSamples(4);
    format.setStencilBufferSize(8);
    setFormat(format);
    QSurfaceFormat::setDefaultFormat(format);
    create();

    m_engine.reset(new Qt3DCore::Quick::QQmlAspectEngine);
    m_renderAspect = new Qt3DRender::QRenderAspect;
    m_inputAspect = new Qt3DInput::QInputAspect;
    m_logicAspect = new Qt3DLogic::QLogicAspect;

    m_engine->aspectEngine()->registerAspect(m_renderAspect);
    m_engine->aspectEngine()->registerAspect(m_inputAspect);
    m_engine->aspectEngine()->registerAspect(m_logicAspect);
}

Qt3DQuickWindow::~Qt3DQuickWindow()
{
}

void Qt3DQuickWindow::registerAspect(Qt3DCore::QAbstractAspect *aspect)
{
    Q_ASSERT(!isVisible());
    m_engine->aspectEngine()->registerAspect(aspect);
}

void Qt3DQuickWindow::registerAspect(const QString &name)
{
    Q_ASSERT(!isVisible());
    m_engine->aspectEngine()->registerAspect(name);
}

void Qt3DQuickWindow::setSource(const QUrl &source)
{
    m_source = source;
}

Qt3DCore::Quick::QQmlAspectEngine *Qt3DQuickWindow::engine() const
{
    return m_engine.data();
}

void Qt3DQuickWindow::setCameraAspectRatioMode(CameraAspectRatioMode mode)
{
    if (m_cameraAspectRatioMode == mode)
        return;

    m_cameraAspectRatioMode = mode;
    setCameraAspectModeHelper();
    emit cameraAspectRatioModeChanged(mode);
}

Qt3DQuickWindow::CameraAspectRatioMode Qt3DQuickWindow::cameraAspectRatioMode() const
{
    return m_cameraAspectRatioMode;
}

void Qt3DQuickWindow::showEvent(QShowEvent *e)
{
    if (!m_initialized) {

        // Connect to the QQmlAspectEngine's statusChanged signal so that when the QML is loaded
        // and th eobjects hav ebeen instantiated, but before we set them on the QAspectEngine we
        // can swoop in and set the window surface and camera on the framegraph and ensure the camera
        // respects the window's aspect ratio
        connect(m_engine.data(), &Qt3DCore::Quick::QQmlAspectEngine::sceneCreated,
                this, &Qt3DQuickWindow::onSceneCreated);

        m_engine->setSource(m_source);

        // Set the QQmlIncubationController on the window
        // to benefit from asynchronous incubation
        if (!m_incubationController)
            m_incubationController = new Qt3DQuickWindowIncubationController(this);

        m_engine->qmlEngine()->setIncubationController(m_incubationController);

        m_initialized = true;
    }
    QWindow::showEvent(e);
}

void Qt3DQuickWindow::onSceneCreated(QObject *rootObject)
{
    Q_ASSERT(rootObject);

    setWindowSurface(rootObject);

    if (m_cameraAspectRatioMode == AutomaticAspectRatio) {
        // Set aspect ratio of first camera to match the window
        QList<Qt3DRender::QCamera *> cameras
                = rootObject->findChildren<Qt3DRender::QCamera *>();
        if (cameras.isEmpty()) {
            qWarning() << "No camera found";
        } else {
            m_camera = cameras.first();
            setCameraAspectModeHelper();
        }
    }

    // Set ourselves up as a source of input events for the input aspect
    Qt3DInput::QInputSettings *inputSettings = rootObject->findChild<Qt3DInput::QInputSettings *>();
    if (inputSettings) {
        inputSettings->setEventSource(this);
    } else {
        qWarning() << "No Input Settings found, keyboard and mouse events won't be handled";
    }
}

void Qt3DQuickWindow::setWindowSurface(QObject *rootObject)
{
    Qt3DRender::QRenderSurfaceSelector *surfaceSelector = Qt3DRender::QRenderSurfaceSelectorPrivate::find(rootObject);
    if (surfaceSelector)
        surfaceSelector->setSurface(this);
}

void Qt3DQuickWindow::setCameraAspectModeHelper()
{
    switch (m_cameraAspectRatioMode) {
    case AutomaticAspectRatio:
        connect(this, &QWindow::widthChanged, this, &Qt3DQuickWindow::updateCameraAspectRatio);
        connect(this, &QWindow::heightChanged, this, &Qt3DQuickWindow::updateCameraAspectRatio);
        // Update the aspect ratio the first time the surface is set
        updateCameraAspectRatio();
        break;
    case UserAspectRatio:
        disconnect(this, &QWindow::widthChanged, this, &Qt3DQuickWindow::updateCameraAspectRatio);
        disconnect(this, &QWindow::heightChanged, this, &Qt3DQuickWindow::updateCameraAspectRatio);
        break;
    }
}

void Qt3DQuickWindow::updateCameraAspectRatio()
{
    if (m_camera) {
        m_camera->setAspectRatio(static_cast<float>(width()) /
                                 static_cast<float>(height()));
    }
}

} // Quick

} // Qt3DExtras

QT_END_NAMESPACE

#include "qt3dquickwindow.moc"
