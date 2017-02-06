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

#include "window.h"

#include <QOpenGLContext>
#include <QSGAbstractRenderer>
#include <QSGEngine>
#include <QSGSimpleTextureNode>
#include <QSGTransformNode>
#include <QScreen>
#include <QVariantAnimation>
#include <QOpenGLFunctions>

class Item {
public:
    Item(QSGNode *parentNode, QSGTexture *texture, const QPointF &fromPos, const QPointF &toPos) {
        textureNode = new QSGSimpleTextureNode;
        textureNode->setRect(QRect(QPoint(), texture->textureSize()));
        textureNode->setTexture(texture);

        transformNode = new QSGTransformNode;
        transformNode->setFlag(QSGNode::OwnedByParent, false);
        transformNode->appendChildNode(textureNode);
        parentNode->appendChildNode(transformNode);

        int duration = qrand() / float(RAND_MAX) * 400 + 800;
        rotAnimation.setStartValue(qrand() / float(RAND_MAX) * 720 - 180);
        rotAnimation.setEndValue(qrand() / float(RAND_MAX) * 720 - 180);
        rotAnimation.setDuration(duration);
        rotAnimation.start();

        posAnimation.setStartValue(fromPos);
        posAnimation.setEndValue(toPos);
        posAnimation.setDuration(duration);
        posAnimation.start();
    }

    ~Item() {
        delete transformNode;
    }

    bool isDone() const { return posAnimation.state() != QAbstractAnimation::Running; }

    void sync() {
        QPointF currentPos = posAnimation.currentValue().toPointF();
        QPointF center = textureNode->rect().center();
        QMatrix4x4 m;
        m.translate(currentPos.x(), currentPos.y());
        m.translate(center.x(), center.y());
        m.rotate(rotAnimation.currentValue().toFloat(), 0, 0, 1);
        m.translate(-center.x(), -center.y());
        transformNode->setMatrix(m);
    }

private:
    QSGTransformNode *transformNode;
    QSGSimpleTextureNode *textureNode;
    QVariantAnimation posAnimation;
    QVariantAnimation rotAnimation;
};

Window::Window()
    : m_initialized(false)
    , m_context(new QOpenGLContext)
    , m_sgEngine(new QSGEngine)
    , m_sgRootNode(new QSGRootNode)
{
    setSurfaceType(QWindow::OpenGLSurface);
    QRect g(0, 0, 640, 480);
    g.moveCenter(screen()->geometry().center());
    setGeometry(g);
    setTitle(QStringLiteral("Click me!"));

    QSurfaceFormat format;
    format.setDepthBufferSize(16);
    setFormat(format);
    m_context->setFormat(format);
    m_context->create();

    m_animationDriver.install();
    connect(&m_animationDriver, &QAnimationDriver::started, this, &Window::update);
}

Window::~Window()
{
}

void Window::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == m_updateTimer.timerId()) {
        m_updateTimer.stop();

        if (!m_context->makeCurrent(this))
            return;

        if (!m_initialized)
            initialize();

        sync();
        render();

        if (m_animationDriver.isRunning()) {
            m_animationDriver.advance();
            update();
        }
    } else
        QWindow::timerEvent(e);
}

void Window::exposeEvent(QExposeEvent *)
{
    if (isExposed())
        update();
    else
        invalidate();
}

void Window::mousePressEvent(QMouseEvent *)
{
    addItems();
}

void Window::keyPressEvent(QKeyEvent *)
{
    addItems();
}

void Window::addItems()
{
    if (!m_initialized)
        return;

    QSGTexture *textures[] = { m_smileTexture.data(), m_qtTexture.data() };
    for (int i = 0; i < 50; ++i) {
        QSGTexture *tex = textures[i%2];
        QPointF fromPos(-tex->textureSize().width(), qrand() / float(RAND_MAX) * (height() - tex->textureSize().height()));
        QPointF toPos(width(), qrand() / float(RAND_MAX) * height() * 1.5 - height() * 0.25);
        m_items.append(QSharedPointer<Item>(new Item(m_sgRootNode.data(), tex, fromPos, toPos)));
    }
    update();
}

void Window::update()
{
    if (!m_updateTimer.isActive())
        m_updateTimer.start(0, this);
}

void Window::sync()
{
    QList<QSharedPointer<Item> > validItems;
    foreach (QSharedPointer<Item> item, m_items) {
        if (!item->isDone()) {
            validItems.append(item);
            item->sync();
        }
    }
    m_items.swap(validItems);
}

void Window::render()
{
    m_sgRenderer->setDeviceRect(size());
    m_sgRenderer->setViewportRect(size());
    m_sgRenderer->setProjectionMatrixToRect(QRectF(QPointF(), size()));
    m_sgRenderer->renderScene();

    m_context->swapBuffers(this);
}

void Window::initialize()
{
    m_sgEngine->initialize(m_context.data());
    m_sgRenderer.reset(m_sgEngine->createRenderer());
    m_sgRenderer->setRootNode(m_sgRootNode.data());
    m_sgRenderer->setClearColor(QColor(32, 32, 32));

    // With QSGEngine::createTextureFromId
    QOpenGLFunctions glFuncs(m_context.data());
    GLuint glTexture;
    glFuncs.glGenTextures(1, &glTexture);
    glFuncs.glBindTexture(GL_TEXTURE_2D, glTexture);
    QImage smile = QImage(":/scenegraph/sgengine/face-smile.png").scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    smile = smile.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    Q_ASSERT(!smile.isNull());
    glFuncs.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, smile.width(), smile.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, smile.constBits());
    m_smileTexture.reset(m_sgEngine->createTextureFromId(glTexture, smile.size(), QFlag(QSGEngine::TextureOwnsGLTexture | QSGEngine::TextureHasAlphaChannel)));

    // With QSGEngine::createTextureFromImage
    QImage qtLogo = QImage(":/shared/images/qt-logo.png").scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    Q_ASSERT(!qtLogo.isNull());
    m_qtTexture.reset(m_sgEngine->createTextureFromImage(qtLogo));
    m_initialized = true;
}

void Window::invalidate()
{
    m_updateTimer.stop();
    m_items.clear();
    m_smileTexture.reset();
    m_qtTexture.reset();
    m_sgRenderer.reset();
    m_sgEngine->invalidate();
    m_initialized = false;
}
