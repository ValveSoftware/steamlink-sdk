/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qinputchord.h"
#include "qinputchord_p.h"
#include <Qt3DCore/qnodecreatedchange.h>
#include <Qt3DInput/qabstractphysicaldevice.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

/*!
    \class Qt3DInput::QInputChord
    \inmodule Qt3DInput
    \inherits QAbstractActionInput
    \brief QInputChord represents a set of QAbstractActionInput's that must be triggerd at once.

    \since 5.7
*/

/*!
    \qmltype InputChord
    \inqmlmodule Qt3D.Input
    \inherits QAbstractActionInput
    \instantiates Qt3DInput::QInputChord
    \brief QML frontend for the Qt3DInput::QInputChord C++ class.

    Represents a set of QAbstractActionInput's that must be triggerd at once.

    The following example shows an sequence that will be triggered by pressing the G, D, and J keys in that order with a maximum time between key presses of 1 second  and overall maximum input time of 3 seconds.
    \qml
    InputChord {
        interval: 1000
        timeout: 3000
        chords: [
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
    \endqml
    \since 5.7
*/

/*!
  \qmlproperty list<AbstractActionInput> Qt3D.Input::InputChord::chords

  The list of AbstractActionInput that must be triggered to trigger this aggregate input.
*/

/*!
  \qmlproperty int Qt3D.Input::InputChord::timeout
*/

/*!
    Constructs a new QInputChord with parent \a parent.
 */
QInputChord::QInputChord(Qt3DCore::QNode *parent)
    : Qt3DInput::QAbstractActionInput(*new QInputChordPrivate(), parent)
{

}

/*! \internal */
QInputChord::~QInputChord()
{
}

/*!
    \property QInputChord::timeout

    The time in which all QAbstractActionInput's in the input chord must triggered within.
    The time is in milliseconds.
 */
int QInputChord::timeout() const
{
    Q_D(const QInputChord);
    return d->m_timeout;
}

/*!
    Sets the time in which all QAbstractActionInput's in the input chord must triggered within to \a timeout.
    The time is in milliseconds
 */
void QInputChord::setTimeout(int timeout)
{
    Q_D(QInputChord);
    if (d->m_timeout != timeout) {
        d->m_timeout = timeout;
        emit timeoutChanged(timeout);
    }
}

/*!
    Append the QAbstractActionInput \a input to the end of this QInputChord's chord vector.

    \sa removeChord
 */
void QInputChord::addChord(QAbstractActionInput *input)
{
    Q_D(QInputChord);
    if (!d->m_chords.contains(input)) {
        d->m_chords.push_back(input);

        // Ensures proper bookkeeping
        d->registerDestructionHelper(input, &QInputChord::removeChord, d->m_chords);

        if (!input->parent())
            input->setParent(this);

        if (d->m_changeArbiter != nullptr) {
            const auto change = Qt3DCore::QPropertyNodeAddedChangePtr::create(id(), input);
            change->setPropertyName("chord");
            d->notifyObservers(change);
        }
    }
}

/*!
    Remove the QAbstractActionInput \a input from this QInputChord's chord vector.

    \sa addChord
 */
void QInputChord::removeChord(QAbstractActionInput *input)
{
    Q_D(QInputChord);
    if (d->m_chords.contains(input)) {

        if (d->m_changeArbiter != nullptr) {
            const auto change = Qt3DCore::QPropertyNodeRemovedChangePtr::create(id(), input);
            change->setPropertyName("chord");
            d->notifyObservers(change);
        }

        d->m_chords.removeOne(input);

        // Remove bookkeeping connection
        d->unregisterDestructionHelper(input);
    }
}

/*!
    Returns the QInputChord's chord vector.
 */
QVector<QAbstractActionInput *> QInputChord::chords() const
{
    Q_D(const QInputChord);
    return d->m_chords;
}

Qt3DCore::QNodeCreatedChangeBasePtr QInputChord::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QInputChordData>::create(this);
    QInputChordData &data = creationChange->data;

    Q_D(const QInputChord);
    data.chordIds = qIdsForNodes(chords());
    data.timeout = d->m_timeout;

    return creationChange;
}

QInputChordPrivate::QInputChordPrivate()
    : QAbstractActionInputPrivate(),
      m_timeout(0)
{
}

} // Qt3DInput

QT_END_NAMESPACE
