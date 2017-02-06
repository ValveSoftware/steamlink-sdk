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
****************************************************************************/

#include "qaxis.h"
#include "qaxis_p.h"
#include <Qt3DInput/qabstractaxisinput.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include <Qt3DCore/qnodecreatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

/*!
    Constructs a new QAxis instance with \a parent.
    \class Qt3DInput::QAxis
    \inmodule Qt3DInput
    \inherits Qt3DCore::QNode
    \brief QAxis stores QAbstractAxisInputs used to trigger an input event.
    \since 5.7
*/

/*!
    \qmltype Axis
    \inqmlmodule Qt3D.Input
    \instantiates Qt3DInput::QAxis
    \brief QML frontend for the Qt3DInput::QAxis C++ class.

    Links a set of AbstractAxisInputs that trigger the same event.
    \since 5.7
*/

/*!
    \qmlproperty int QAxis::value
    \readonly

    Holds the value of the axis.

    Note this property is not updated when the axis is disabled.
*/

/*!
    Constructs a new QAxis instance with parent \a parent.
 */
QAxis::QAxis(Qt3DCore::QNode *parent)
    : Qt3DCore::QNode(*new QAxisPrivate(), parent)
{
}

/*! \internal */
QAxis::~QAxis()
{
}

/*!
  \qmlproperty list<AbstractAxisInput>  Qt3D.Input::Axis::inputs

  the list of AbstractAxisInput that can trigger this Axis.
*/

/*!
    QAxis::addInput
    Adds an \a input for the axis.

    \sa Qt3DInput::QAbstractAxisInput
 */
void QAxis::addInput(QAbstractAxisInput *input)
{
    Q_D(QAxis);
    if (!d->m_inputs.contains(input)) {
        d->m_inputs.push_back(input);

        if (!input->parent())
            input->setParent(this);

        // Ensures proper bookkeeping
        d->registerDestructionHelper(input, &QAxis::removeInput, d->m_inputs);

        if (d->m_changeArbiter != nullptr) {
            const auto change = Qt3DCore::QPropertyNodeAddedChangePtr::create(id(), input);
            change->setPropertyName("input");
            d->notifyObservers(change);
        }
    }
}

/*!
    \qmlproperty QVariantList Qt3D.Input::Axis::buttons

    The Buttons that can trigger this Action
*/

/*!
    QAxis::removeInput

    Removes an \a input from the axis.

    \sa Qt3DInput::QAbstractAxisInput
 */
void QAxis::removeInput(QAbstractAxisInput *input)
{
    Q_D(QAxis);
    if (d->m_inputs.contains(input)) {

        if (d->m_changeArbiter != nullptr) {
            const auto change = Qt3DCore::QPropertyNodeRemovedChangePtr::create(id(), input);
            change->setPropertyName("input");
            d->notifyObservers(change);
        }

        d->m_inputs.removeOne(input);

        // Remove bookkeeping connection
        d->unregisterDestructionHelper(input);
    }
}

/*!
    QAxis::inputs

    \return vector of all inputs added to the axis.
 */
QVector<QAbstractAxisInput *> QAxis::inputs() const
{
    Q_D(const QAxis);
    return d->m_inputs;
}

/*!
  \property QAxis::value

  The value of the axis.
 */
float QAxis::value() const
{
    Q_D(const QAxis);
    return d->m_value;
}

/*! \internal */
void QAxis::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &change)
{
    Q_D(QAxis);
    Qt3DCore::QPropertyUpdatedChangePtr e = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(change);
    if (e->type() == Qt3DCore::PropertyUpdated && e->propertyName() == QByteArrayLiteral("value")) {
        d->setValue(e->value().toFloat());
    }
}

Qt3DCore::QNodeCreatedChangeBasePtr QAxis::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QAxisData>::create(this);
    auto &data = creationChange->data;
    data.inputIds = qIdsForNodes(inputs());
    return creationChange;
}

} // Qt3DInput

QT_END_NAMESPACE
