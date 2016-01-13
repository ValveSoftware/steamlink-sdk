/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the demonstration applications of the Qt Toolkit.
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
        connect(window, SIGNAL(beforeRendering()), this, SLOT(maybeRotate()));
        connect(window, SIGNAL(frameSwapped()), this, SLOT(maybeUpdate()));

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
