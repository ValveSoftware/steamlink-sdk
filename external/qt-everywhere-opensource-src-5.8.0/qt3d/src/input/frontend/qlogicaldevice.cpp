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

#include "qlogicaldevice.h"
#include "qlogicaldevice_p.h"
#include <Qt3DInput/qaction.h>
#include <Qt3DInput/qaxis.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

QLogicalDevicePrivate::QLogicalDevicePrivate()
    : Qt3DCore::QComponentPrivate()
{
}

QLogicalDevicePrivate::~QLogicalDevicePrivate()
{
}

/*!
    \class Qt3DInput::QLogicalDevice
    \inmodule Qt3DInput
    \inherits Qt3DCore::QNode
    \brief QLogicalDevice allows the user to define a set of actions that they wish to use within an application.

    \since 5.6
*/

/*!
    \qmltype LogicalDevice
    \inqmlmodule Qt3D.Input
    \instantiates Qt3DInput::QLogicalDevice
    \brief QML frontend for the Qt3DInput::QLogicalDevice C++ class.

    Allows the user to define a set of actions that they wish to use within an application.

    \qml
    LogicalDevice {
        id: keyboardLogicalDevice

        actions: [
            Action {
                name: "fire"
                inputs: [
                    ActionInput {
                        sourceDevice: keyboardSourceDevice
                        keys: [Qt.Key_Space]
                    },
                    InputChord {
                        tolerance: 10
                        inputs: [
                            ActionInput {
                                sourceDevice: keyboardSourceDevice
                                keys: [Qt.Key_A]
                            },
                            ActionInput {
                                sourceDevice: keyboardSourceDevice
                                keys: [Qt.Key_S]
                            }
                        ]
                    }
                ]
            },
            Action {
                name: "reload"
                inputs: [
                    ActionInput {
                        sourceDevice: keyboardSourceDevice
                        keys: [Qt.Key_Alt]
                    }
                ]
            },
            Action {
                name: "combo"
                inputs: [
                    InputSequence {
                        interval: 1000
                        timeout: 10000
                        inputs: [
                            ActionInput {
                                sourceDevice: keyboardSourceDevice
                                keys: [Qt.Key_G]
                            },
                            ActionInput {
                                sourceDevice: keyboardSourceDevice
                                keys: [Qt.Key_D]
                            },
                            ActionInput {
                                sourceDevice: keyboardSourceDevice
                                keys: [Qt.Key_J]
                            }
                        ]
                    }
                ]
            }
        ]
    }
    \endqml

    \since 5.6
*/

/*!
    Constructs a new QLogicalDevice instance with parent \a parent.
 */
QLogicalDevice::QLogicalDevice(Qt3DCore::QNode *parent)
    : Qt3DCore::QComponent(*new QLogicalDevicePrivate(), parent)
{
}

QLogicalDevice::~QLogicalDevice()
{
}

/*!
  \qmlproperty list<Action> Qt3D.Input::LogicalDevice::actions

  The actions used by this Logical Device
*/

/*!
    Add an \a action to the list of actions.
 */
void QLogicalDevice::addAction(QAction *action)
{
    Q_D(QLogicalDevice);
    if (!d->m_actions.contains(action)) {
        d->m_actions.push_back(action);
        // Force creation in backend by setting parent
        if (!action->parent())
            action->setParent(this);

        // Ensures proper bookkeeping
        d->registerDestructionHelper(action, &QLogicalDevice::removeAction, d->m_actions);

        if (d->m_changeArbiter != nullptr) {
            const auto change = Qt3DCore::QPropertyNodeAddedChangePtr::create(id(), action);
            change->setPropertyName("action");
            d->notifyObservers(change);
        }
    }
}

/*!
    Remove an \a action from the list of actions.
 */
void QLogicalDevice::removeAction(QAction *action)
{
    Q_D(QLogicalDevice);
    if (d->m_actions.contains(action)) {

        if (d->m_changeArbiter != nullptr) {
            const auto change = Qt3DCore::QPropertyNodeRemovedChangePtr::create(id(), action);
            change->setPropertyName("action");
            d->notifyObservers(change);
        }

        d->m_actions.removeOne(action);

        // Remove bookkeeping connection
        d->unregisterDestructionHelper(action);
    }
}

/*!
    Returns the list of actions.
 */
QVector<QAction *> QLogicalDevice::actions() const
{
    Q_D(const QLogicalDevice);
    return d->m_actions;
}

/*!
  \qmlproperty list<Axis> Qt3D.Input::LogicalDevice::axis

  The axis used by this Logical Device
*/

/*!
    Add an \a axis to the list of axis.
 */
void QLogicalDevice::addAxis(QAxis *axis)
{
    Q_D(QLogicalDevice);
    if (!d->m_axes.contains(axis)) {
        d->m_axes.push_back(axis);

        // Force creation in backend by setting parent
        if (!axis->parent())
            axis->setParent(this);

        // Ensures proper bookkeeping
        d->registerDestructionHelper(axis, &QLogicalDevice::removeAxis, d->m_axes);

        if (d->m_changeArbiter != nullptr) {
            const auto change = Qt3DCore::QPropertyNodeAddedChangePtr::create(id(), axis);
            change->setPropertyName("axis");
            d->notifyObservers(change);
        }
    }
}

/*!
    Remove an \a axis drom the list of axis.
 */
void QLogicalDevice::removeAxis(QAxis *axis)
{
    Q_D(QLogicalDevice);
    if (d->m_axes.contains(axis)) {
        if (d->m_changeArbiter != nullptr) {
            const auto change = Qt3DCore::QPropertyNodeRemovedChangePtr::create(id(), axis);
            change->setPropertyName("axis");
            d->notifyObservers(change);
        }

        d->m_axes.removeOne(axis);

        // Remove bookkeeping connection
        d->unregisterDestructionHelper(axis);
    }
}

/*!
    Returns the list of axis.
 */
QVector<QAxis *> QLogicalDevice::axes() const
{
    Q_D(const QLogicalDevice);
    return d->m_axes;
}

Qt3DCore::QNodeCreatedChangeBasePtr QLogicalDevice::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QLogicalDeviceData>::create(this);
    auto &data = creationChange->data;
    data.actionIds = qIdsForNodes(actions());
    data.axisIds = qIdsForNodes(axes());
    return creationChange;
}

} // Qt3DInput

QT_END_NAMESPACE
