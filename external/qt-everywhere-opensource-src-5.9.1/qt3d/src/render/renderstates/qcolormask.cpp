/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qcolormask.h"
#include "qcolormask_p.h"
#include <Qt3DRender/private/qrenderstatecreatedchange_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DRender::QColorMask
    \inmodule Qt3DRender
    \since 5.7
    \brief Allows specifying which color components should be written to the
    currently bound frame buffer.

    By default, the property for each color component (red, green, blue, alpha)
    is set to \c true which means they will be written to the frame buffer.
    Setting any of the color component to \c false will prevent it from being
    written into the frame buffer.
 */

/*!
    \qmltype ColorMask
    \inqmlmodule Qt3D.Render
    \since 5.7
    \inherits RenderState
    \instantiates Qt3DRender::QColorMask
    \brief Allows specifying which color components should be written to the
    currently bound frame buffer.

    By default, the property for each color component (red, green, blue, alpha)
    is set to \c true which means they will be written to the frame buffer.
    Setting any of the color component to \c false will prevent it from being
    written into the frame buffer.
 */

/*!
    \qmlproperty bool ColorMask::redMasked
    Holds whether red color component should be written to the frame buffer.
*/

/*!
    \qmlproperty bool ColorMask::greenMasked
    Holds whether green color component should be written to the frame buffer.
*/

/*!
    \qmlproperty bool ColorMask::blueMasked
    Holds whether blue color component should be written to the frame buffer.
*/

/*!
    \qmlproperty bool ColorMask::alphaMasked
    Holds whether alpha component should be written to the frame buffer.
*/


/*!
    Constructs a new Qt3DCore::QColorMask instance with \a parent as parent.
 */
QColorMask::QColorMask(QNode *parent)
    : QRenderState(*new QColorMaskPrivate, parent)
{
}

/*! \internal */
QColorMask::~QColorMask()
{
}

bool QColorMask::isRedMasked() const
{
    Q_D(const QColorMask);
    return d->m_redMasked;
}

bool QColorMask::isGreenMasked() const
{
    Q_D(const QColorMask);
    return d->m_greenMasked;
}

bool QColorMask::isBlueMasked() const
{
    Q_D(const QColorMask);
    return d->m_blueMasked;
}

bool QColorMask::isAlphaMasked() const
{
    Q_D(const QColorMask);
    return d->m_alphaMasked;
}

/*!
    \property QColorMask::redMasked
    Holds whether the red color component should be written to the frame buffer.
 */
void QColorMask::setRedMasked(bool redMasked)
{
    Q_D(QColorMask);
    if (redMasked != d->m_redMasked) {
        d->m_redMasked = redMasked;
        emit redMaskedChanged(redMasked);
    }
}

/*!
    \property QColorMask::greenMasked
    Holds whether the green color component should be written to the frame buffer.
 */
void QColorMask::setGreenMasked(bool greenMasked)
{
    Q_D(QColorMask);
    if (greenMasked != d->m_greenMasked) {
        d->m_greenMasked = greenMasked;
        emit greenMaskedChanged(greenMasked);
    }
}

/*!
    \property QColorMask::blueMasked
    Holds whether the blue color component should be written to the frame buffer.
 */
void QColorMask::setBlueMasked(bool blueMasked)
{
    Q_D(QColorMask);
    if (blueMasked != d->m_blueMasked) {
        d->m_blueMasked = blueMasked;
        emit blueMaskedChanged(blueMasked);
    }
}

/*!
    \property QColorMask::alphaMasked
    Holds whether the alphaMasked component should be written to the frame buffer.
 */
void QColorMask::setAlphaMasked(bool alphaMasked)
{
    Q_D(QColorMask);
    if (alphaMasked != d->m_alphaMasked) {
        d->m_alphaMasked = alphaMasked;
        emit alphaMaskedChanged(alphaMasked);
    }
}

Qt3DCore::QNodeCreatedChangeBasePtr QColorMask::createNodeCreationChange() const
{
    auto creationChange = QRenderStateCreatedChangePtr<QColorMaskData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QColorMask);
    data.redMasked = d->m_redMasked;
    data.greenMasked = d->m_greenMasked;
    data.blueMasked = d->m_blueMasked;
    data.alphaMasked = d->m_alphaMasked;
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE

