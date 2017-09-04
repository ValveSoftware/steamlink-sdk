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
**
****************************************************************************/

#include <Qt3DQuickInput/private/quick3dinputsequence_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {
namespace Input {
namespace Quick {

Quick3DInputSequence::Quick3DInputSequence(QObject *parent)
    : QObject(parent)
{
}

QQmlListProperty<QAbstractActionInput> Quick3DInputSequence::qmlActionInputs()
{
    return QQmlListProperty<QAbstractActionInput>(this, 0,
                                        &Quick3DInputSequence::appendActionInput,
                                        &Quick3DInputSequence::actionInputCount,
                                        &Quick3DInputSequence::actionInputAt,
                                        &Quick3DInputSequence::clearActionInputs);
}

void Quick3DInputSequence::appendActionInput(QQmlListProperty<QAbstractActionInput> *list, QAbstractActionInput *input)
{
    Quick3DInputSequence *action = qobject_cast<Quick3DInputSequence *>(list->object);
    action->parentSequence()->addSequence(input);
}

QAbstractActionInput *Quick3DInputSequence::actionInputAt(QQmlListProperty<QAbstractActionInput> *list, int index)
{
    Quick3DInputSequence *action = qobject_cast<Quick3DInputSequence *>(list->object);
    return action->parentSequence()->sequences().at(index);
}

int Quick3DInputSequence::actionInputCount(QQmlListProperty<QAbstractActionInput> *list)
{
    Quick3DInputSequence *action = qobject_cast<Quick3DInputSequence *>(list->object);
    return action->parentSequence()->sequences().count();
}

void Quick3DInputSequence::clearActionInputs(QQmlListProperty<QAbstractActionInput> *list)
{
    Quick3DInputSequence *action = qobject_cast<Quick3DInputSequence *>(list->object);
    const auto sequences = action->parentSequence()->sequences();
    for (QAbstractActionInput *input : sequences)
        action->parentSequence()->removeSequence(input);
}


} // namespace Quick
} // namespace Input
} // namespace Qt3DInput

QT_END_NAMESPACE
