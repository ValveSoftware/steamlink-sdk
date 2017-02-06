/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

#include "spinner.h"

#include <QtQuick/QQuickWindow>
#include <QtGui/QScreen>
#include <QtQuick/QSGSimpleTextureNode>

#include <QtGui/QConicalGradient>
#include <QtGui/QPainter>

class SpinnerNode : public QObject, public QSGTransformNode
{
    Q_OBJECT
public:
    SpinnerNode(QQuickWindow *window)
        : m_rotation(0)
        , m_spinning(false)
        , m_window(window)
    {
        connect(window, &QQuickWindow::beforeRendering, this, &SpinnerNode::maybeRotate);
        connect(window, &QQuickWindow::frameSwapped, this, &SpinnerNode::maybeUpdate);

        QImage image(":/scenegraph/threadedanimation/spinner.png");
        m_texture = window->createTextureFromImage(image);
        QSGSimpleTextureNode *textureNode = new QSGSimpleTextureNode();
        textureNode->setTexture(m_texture);
        textureNode->setRect(0, 0, image.width(), image.height());
        textureNode->setFiltering(QSGTexture::Linear);
        appendChildNode(textureNode);
    }

    ~SpinnerNode() {
        delete m_texture;
    }

    void setSpinning(bool spinning)
    {
        m_spinning = spinning;
    }

public slots:
    void maybeRotate() {
        if (m_spinning) {
            m_rotation += (360 / m_window->screen()->refreshRate());
            QMatrix4x4 matrix;
            matrix.translate(32, 32);
            matrix.rotate(m_rotation, 0, 0, 1);
            matrix.translate(-32, -32);
            setMatrix(matrix);
        }
    }

    void maybeUpdate() {
        if (m_spinning) {
            m_window->update();
        }
    }

private:
    qreal m_rotation;
    bool m_spinning;
    QSGTexture *m_texture;
    QQuickWindow *m_window;
};

Spinner::Spinner()
    : m_spinning(false)
{
    setSize(QSize(64, 64));
    setFlag(ItemHasContents);
}

void Spinner::setSpinning(bool spinning)
{
    if (spinning == m_spinning)
        return;
    m_spinning = spinning;
    emit spinningChanged();
    update();
}

QSGNode *Spinner::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    SpinnerNode *n = static_cast<SpinnerNode *>(old);
    if (!n)
        n = new SpinnerNode(window());

    n->setSpinning(m_spinning);

    return n;
}

#include "spinner.moc"
