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

#include "qactioninput.h"
#include "qactioninput_p.h"
#include <Qt3DInput/qabstractphysicaldevice.h>
#include <Qt3DInput/QAbstractActionInput>
#include <Qt3DCore/qnodecreatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

QActionInputPrivate::QActionInputPrivate()
    : QAbstractActionInputPrivate(),
      m_sourceDevice(nullptr)
{
}

/*!
    \class Qt3DInput::QActionInput
    \inmodule Qt3DInput
    \inherits Qt3DInput::QAbstractActionInput
    \brief QActionInput stores Device and Buttons used to trigger an input event.

    \since 5.7
*/

/*!
    \qmltype ActionInput
    \inqmlmodule Qt3D.Input
    \inherits QAbstractActionInput
    \instantiates Qt3DInput::QActionInput
    \brief QML frontend for the Qt3DInput::QActionInput C++ class.

    Links a physical device and selected buttons on it which can trigger this action.

    Each Action input can be triggered by one or many buttons on a source device
    \qml
    ActionInput {
      sourceDevice: keyboardSourceDevice
      buttons: [Qt.Key_A]
    }
    \endqml
   \qml
    ActionInput {
      sourceDevice: keyboardSourceDevice
      buttons: [Qt.Key_A,Qt.Key_B]
    }
    \endqml
    \since 5.7
*/

/*!
    Constructs a new QActionInput instance with \a parent.
 */
QActionInput::QActionInput(Qt3DCore::QNode *parent)
    : Qt3DInput::QAbstractActionInput(*new QActionInputPrivate(), parent)
{

}

/*! \internal */
QActionInput::~QActionInput()
{
}

/*!
    \property Qt3DInput::QActionInput::buttons

    Holds the buttons that can trigger this Action.
 */
QVector<int> QActionInput::buttons() const
{
    Q_D(const QActionInput);
    return d->m_buttons;
}

/*!
    \property Qt3DInput::QActionInput::sourceDevice

    The current source device of the QActionInput.
*/
/*!
  \fn QActionInput::sourceDeviceChanged(QAbstractPhysicalDevice *sourceDevice)

  This signal is emitted when the source device associated with the action input is changed to \a sourceDevice.
*/

/*!
  \qmlproperty AbstractPhysicalDevice Qt3D.Input::ActionInput::sourceDevice

  The current source device of the ActionInput
*/

/*!
    \qmlsignal Qt3D.Input::ActionInput::sourceDeviceChanged()

    This signal is emitted when the source device associated with the action input is changed.

    The corresponding handler is \c onSourceDeviceChanged
*/
void QActionInput::setSourceDevice(QAbstractPhysicalDevice *sourceDevice)
{
    Q_D(QActionInput);
    if (d->m_sourceDevice != sourceDevice) {

        if (d->m_sourceDevice)
            d->unregisterDestructionHelper(d->m_sourceDevice);

        // Check and set parent if needed
        // to force creation in the backend
        if (sourceDevice && !sourceDevice->parent())
            sourceDevice->setParent(this);

        d->m_sourceDevice = sourceDevice;

        // Ensures proper bookkeeping
        if (d->m_sourceDevice)
            d->registerDestructionHelper(sourceDevice, &QActionInput::setSourceDevice, d->m_sourceDevice);

        emit sourceDeviceChanged(sourceDevice);
    }
}

QAbstractPhysicalDevice *QActionInput::sourceDevice() const
{
    Q_D(const QActionInput);
    return d->m_sourceDevice;
}

/*!
  \fn QActionInput::buttonsChanged(const QVector<int> &buttons)

  This signal is emitted when the buttons associated with the action input is changed.
  The buttons changed are \a buttons
*/

/*!
    \qmlproperty list<int> Qt3D.Input::ActionInput::buttons

    The Buttons that can trigger this Action.
*/

/*!
    \qmlsignal Qt3D.Input::ActionInput::buttonsChanged(const QVector<int> &buttons)

    This signal is emitted when the buttons associated with the action input is changed.

    The corresponding handler is \c onbuttonsChanged
*/

/*!
    Set the buttons to trigger the QActionInput instance to \a buttons.
 */
void QActionInput::setButtons(const QVector<int> &buttons)
{
    Q_D(QActionInput);
    if (buttons != d->m_buttons) {
        d->m_buttons = buttons;
        emit buttonsChanged(buttons);
    }
}

Qt3DCore::QNodeCreatedChangeBasePtr QActionInput::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QActionInputData>::create(this);
    auto &data = creationChange->data;

    Q_D(const QActionInput);
    data.sourceDeviceId = qIdForNode(d->m_sourceDevice);
    data.buttons = d->m_buttons;

    return creationChange;
}

} // Qt3DInput

QT_END_NAMESPACE
