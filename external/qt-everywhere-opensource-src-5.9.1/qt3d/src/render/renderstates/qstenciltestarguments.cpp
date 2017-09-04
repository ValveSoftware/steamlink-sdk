/****************************************************************************
**
** Copyright (C) 2015 Paul Lemire
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

#include "qstenciltestarguments.h"
#include "qstenciltestarguments_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DRender::QStencilTestArguments
    \brief The QStencilTestArguments class specifies arguments for stencil test
    \since 5.7
    \ingroup renderstates
    \inmodule Qt3DRender

    The Qt3DRender::QStencilTestArguments class specifies the arguments for
    the stencil test.
 */

/*!
    \qmltype StencilTestArguments
    \brief The StencilTestArguments type specifies arguments for stencil test
    \since 5.7
    \ingroup renderstates
    \inqmlmodule Qt3D.Render
    \instantiates Qt3DRender::QStencilTestArguments
    \inherits QtObject

    The StencilTestArguments type specifies the arguments for
    the stencil test.
 */

/*!
    \enum Qt3DRender::QStencilTestArguments::StencilFaceMode
    This enumeration holds the values for stencil test arguments face modes
    \value Front Arguments are applied to front-facing polygons.
    \value Back Arguments are applied to back-facing polygons.
    \value FrontAndBack Arguments are applied to both front- and back-facing polygons.
*/

/*!
    \enum Qt3DRender::QStencilTestArguments::StencilFunction

    Enumeration for the stencil function values
    \value Never Never pass stencil test
    \value Always Always pass stencil test
    \value Less Pass stencil test if fragment stencil is less than reference value
    \value LessOrEqual Pass stencil test if fragment stencil is less than or equal to reference value
    \value Equal Pass stencil test if fragment stencil is equal to reference value
    \value GreaterOrEqual Pass stencil test if fragment stencil is greater than or equal to reference value
    \value Greater Pass stencil test if fragment stencil is greater than reference value
    \value NotEqual Pass stencil test if fragment stencil is not equal to reference value
*/

/*!
    \qmlproperty enumeration StencilTestArguments::faceMode
    Holds the faces the arguments are applied to.
    \list
    \li StencilTestArguments.Front
    \li StencilTestArguments.Back
    \li StencilTestArguments.FrontAndBack
    \endlist
    \sa Qt3DRender::QStencilTestArguments::StencilFaceMode
    \readonly
*/

/*!
    \qmlproperty int StencilTestArguments::comparisonMask
    Holds the stencil test comparison mask. Default is all zeroes.
*/

/*!
    \qmlproperty int StencilTestArguments::referenceValue
    Holds the stencil test reference value. Default is zero.
*/

/*!
    \qmlproperty enumeration StencilTestArguments::stencilFunction
    Holds the stencil test function. Default is StencilTestArguments.Never.
    \list
    \li StencilTestArguments.Never
    \li StencilTestArguments.Always
    \li StencilTestArguments.Less
    \li StencilTestArguments.LessOrEqual
    \li StencilTestArguments.Equal
    \li StencilTestArguments.GreaterOrEqual
    \li StencilTestArguments.Greater
    \li StencilTestArguments.NotEqual
    \endlist
*/

/*!
    \property QStencilTestArguments::faceMode
    Holds the faces the arguments are applied to.
    \readonly
*/

/*!
    \property QStencilTestArguments::comparisonMask
    Holds the stencil test comparison mask. Default is all zeroes.
*/

/*!
    \property QStencilTestArguments::referenceValue
    Holds the stencil test reference value. Default is zero.
*/

/*!
    \property QStencilTestArguments::stencilFunction
    Holds the stencil test function. Default is Never.
    \sa Qt3DRender::QStencilTestArguments::StencilFunction
*/

/*!
    The constructor creates a new QStencilTestArguments::QStencilTestArguments
    instance with the specified \a face and \a parent.
 */
QStencilTestArguments::QStencilTestArguments(QStencilTestArguments::StencilFaceMode face, QObject *parent)
    : QObject(*new QStencilTestArgumentsPrivate(face), parent)
{
}

/*! \internal */
QStencilTestArguments::~QStencilTestArguments()
{
}

uint QStencilTestArguments::comparisonMask() const
{
    Q_D(const QStencilTestArguments);
    return d->m_comparisonMask;
}

void QStencilTestArguments::setComparisonMask(uint comparisonMask)
{
    Q_D(QStencilTestArguments);
    if (d->m_comparisonMask != comparisonMask) {
        d->m_comparisonMask = comparisonMask;
        emit comparisonMaskChanged(comparisonMask);
    }
}

int QStencilTestArguments::referenceValue() const
{
    Q_D(const QStencilTestArguments);
    return d->m_referenceValue;
}

void QStencilTestArguments::setReferenceValue(int referenceValue)
{
    Q_D(QStencilTestArguments);
    if (d->m_referenceValue != referenceValue) {
        d->m_referenceValue = referenceValue;
        emit referenceValueChanged(referenceValue);
    }
}

QStencilTestArguments::StencilFunction QStencilTestArguments::stencilFunction() const
{
    Q_D(const QStencilTestArguments);
    return d->m_stencilFunction;
}

void QStencilTestArguments::setStencilFunction(QStencilTestArguments::StencilFunction stencilFunction)
{
    Q_D(QStencilTestArguments);
    if (d->m_stencilFunction != stencilFunction) {
        d->m_stencilFunction = stencilFunction;
        emit stencilFunctionChanged(stencilFunction);
    }
}

QStencilTestArguments::StencilFaceMode QStencilTestArguments::faceMode() const
{
    Q_D(const QStencilTestArguments);
    return d->m_face;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
