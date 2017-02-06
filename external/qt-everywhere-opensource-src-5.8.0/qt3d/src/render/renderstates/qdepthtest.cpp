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

#include "qdepthtest.h"
#include "qdepthtest_p.h"
#include <Qt3DRender/private/qrenderstatecreatedchange_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DRender::QDepthTest
    \brief The QDepthTest class tests the fragment shader's depth value against
    the depth of a sample being written to.
    \since 5.7
    \inmodule Qt3DRender
    \ingroup renderstates

    A QDepthTest class is used to enable depth testing with a given depth test function.
    The depth test enables writing fragment color values when the depth test passes, and
    reject fragments which fail the test. The depth test uses the depth function to
    test the fragments depth value to the value against z-buffer. If the underlying surface
    does not have z-buffer, then QDepthTest does nothing.

    \sa QAlphaTest, QStencilTest
 */

/*!
    \qmltype DepthTest
    \brief The DepthTest type tests the fragment shader's depth value against
    the depth of a sample being written to.
    \since 5.7
    \inqmlmodule Qt3D.Render
    \inherits RenderState
    \instantiates Qt3DRender::QDepthTest
    \ingroup renderstates

    A DepthTest type is used to enable depth testing with a given depth test function.
    The depth test enables writing fragment color values when the depth test passes, and
    reject fragments which fail the test. The depth test uses the depth function to
    test the fragments depth value to the value against z-buffer. If the underlying surface
    does not have z-buffer, the DepthTest does nothing.

    \sa AlphaTest, StencilTest
 */

/*!
    \enum Qt3DRender::QDepthTest::DepthFunction

    Enumeration for the depth function values
    \value Never Never pass depth test
    \value Always Always pass depth test
    \value Less Pass depth test if fragment depth is less than z-buffer value
    \value LessOrEqual Pass depth test if fragment depth is less than or equal to z-buffer value
    \value Equal Pass depth test if fragment depth is equal to z-buffer value
    \value GreaterOrEqual Pass depth test if fragment depth is greater than or equal to z-buffer value
    \value Greater Pass depth test if fragment depth is greater than z-buffer value
    \value NotEqual Pass depth test if fragment depth is not equal to z-buffer value
*/

/*!
    \qmlproperty enumeration DepthTest::depthFunction
    Holds the current function used by depth test. The default is DepthTest.Never.
    \list
    \li DepthTest.Never
    \li DepthTest.Always
    \li DepthTest.Less
    \li DepthTest.LessOrEqual
    \li DepthTest.Equal
    \li DepthTest.GreaterOrEqual
    \li DepthTest.Greater
    \li DepthTest.NotEqual
    \endlist
    \sa Qt3DRender::QDepthTest::DepthFunction
*/

/*!
    \property QDepthTest::depthFunction
    Holds the current function used by depth test. The default is Never.
*/


/*!
    The constructor creates a new QDepthTest::QDepthTest instance with the specified \a parent.
 */
QDepthTest::QDepthTest(QNode *parent)
    : QRenderState(*new QDepthTestPrivate, parent)
{
}

/*! \internal */
QDepthTest::~QDepthTest()
{
}

QDepthTest::DepthFunction QDepthTest::depthFunction() const
{
    Q_D(const QDepthTest);
    return d->m_depthFunction;
}

void QDepthTest::setDepthFunction(QDepthTest::DepthFunction depthFunction)
{
    Q_D(QDepthTest);
    if (d->m_depthFunction != depthFunction) {
        d->m_depthFunction = depthFunction;
        emit depthFunctionChanged(depthFunction);
    }
}

Qt3DCore::QNodeCreatedChangeBasePtr QDepthTest::createNodeCreationChange() const
{
    auto creationChange = QRenderStateCreatedChangePtr<QDepthTestData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QDepthTest);
    data.depthFunction = d->m_depthFunction;
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
