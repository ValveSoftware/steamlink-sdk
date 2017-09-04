/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qscissortest.h"
#include "qscissortest_p.h"
#include <Qt3DRender/private/qrenderstatecreatedchange_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DRender::QScissorTest
    \brief The QScissorTest class discards fragments that fall outside of a
    certain rectangular portion of the screen.
    \since 5.7
    \ingroup renderstates
    \inmodule Qt3DRender

    A QScissorTest class enables scissor test, which discards fragments outside
    the rectangular area of the screen specified by the left, bottom, width and
    height properties.
 */

/*!
    \qmltype ScissorTest
    \brief The ScissorTest type discards fragments that fall outside of a
    certain rectangular portion of the screen.
    \since 5.7
    \ingroup renderstates
    \inqmlmodule Qt3D.Render
    \instantiates Qt3DRender::QScissorTest
    \inherits RenderState

    A ScissorTest type enables scissor test, which discards fragments outside
    the rectangular area of the screen specified by the left, bottom, width and
    height properties.
 */

/*!
    \qmlproperty int ScissorTest::left
    Holds the left coordinate of the scissor box.
*/

/*!
    \qmlproperty int ScissorTest::bottom
    Holds the bottom coordinate of the scissor box.
*/

/*!
    \qmlproperty int ScissorTest::width
    Holds the width of the scissor box.
*/

/*!
    \qmlproperty int ScissorTest::height
    Holds the height of the scissor box.
*/

/*!
    \property QScissorTest::left
    Holds the left coordinate of the scissor box.
*/

/*!
    \property QScissorTest::bottom
    Holds the bottom coordinate of the scissor box.
*/

/*!
    \property QScissorTest::width
    Holds the width of the scissor box.
*/

/*!
    \property QScissorTest::height
    Holds the height of the scissor box.
*/

/*!
    The constructor creates a new QScissorTest::QScissorTest instance with the
    specified \a parent
 */
QScissorTest::QScissorTest(QNode *parent)
    : QRenderState(*new QScissorTestPrivate, parent)
{
}

/*! \internal */
QScissorTest::~QScissorTest()
{
}

int QScissorTest::left() const
{
    Q_D(const QScissorTest);
    return d->m_left;
}

void QScissorTest::setLeft(int left)
{
    Q_D(QScissorTest);
    if (d->m_left != left) {
        d->m_left = left;
        emit leftChanged(left);
    }
}

int QScissorTest::bottom() const
{
    Q_D(const QScissorTest);
    return d->m_bottom;
}

void QScissorTest::setBottom(int bottom)
{
    Q_D(QScissorTest);
    if (d->m_bottom != bottom) {
        d->m_bottom = bottom;
        emit bottomChanged(bottom);
    }
}

int QScissorTest::width() const
{
    Q_D(const QScissorTest);
    return d->m_width;
}

void QScissorTest::setWidth(int width)
{
    Q_D(QScissorTest);
    if (d->m_width != width) {
        d->m_width = width;
        emit widthChanged(width);
    }
}

int QScissorTest::height() const
{
    Q_D(const QScissorTest);
    return d->m_height;
}

void QScissorTest::setHeight(int height)
{
    Q_D(QScissorTest);
    if (d->m_height != height) {
        d->m_height = height;
        emit heightChanged(height);
    }
}

Qt3DCore::QNodeCreatedChangeBasePtr QScissorTest::createNodeCreationChange() const
{
    auto creationChange = QRenderStateCreatedChangePtr<QScissorTestData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QScissorTest);
    data.left = d->m_left;
    data.bottom = d->m_bottom;
    data.width = d->m_width;
    data.height = d->m_height;
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
