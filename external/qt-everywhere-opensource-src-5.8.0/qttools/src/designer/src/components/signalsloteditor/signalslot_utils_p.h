/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef SIGNALSLOTUTILS_P_H
#define SIGNALSLOTUTILS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtCore/QObject>

QT_BEGIN_NAMESPACE

class QDesignerFormWindowInterface;
class QDesignerFormEditorInterface;

namespace qdesigner_internal {

enum MemberType { SignalMember, SlotMember };

// member to class name
QMap<QString, QString> getSignals(QDesignerFormEditorInterface *core, QObject *object, bool showAll);
QMap<QString, QString> getMatchingSlots(QDesignerFormEditorInterface *core, QObject *object,
            const QString &signalSignature, bool showAll);

bool memberFunctionListContains(QDesignerFormEditorInterface *core, QObject *object, MemberType type, const QString &signature);

// Members functions listed by class they were inherited from
struct ClassMemberFunctions
{
    ClassMemberFunctions() {}
    ClassMemberFunctions(const QString &_class_name);

    QString m_className;
    QStringList m_memberList;
};

typedef QList<ClassMemberFunctions> ClassesMemberFunctions;

// Return classes and members in reverse class order to
// populate of the combo of the ToolWindow.

ClassesMemberFunctions reverseClassesMemberFunctions(const QString &obj_name, MemberType member_type,
                                                     const QString &peer, QDesignerFormWindowInterface *form);

bool signalMatchesSlot(QDesignerFormEditorInterface *core, const QString &signal, const QString &slot);

QString realObjectName(QDesignerFormEditorInterface *core, QObject *object);

} // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // SIGNALSLOTUTILS_P_H
