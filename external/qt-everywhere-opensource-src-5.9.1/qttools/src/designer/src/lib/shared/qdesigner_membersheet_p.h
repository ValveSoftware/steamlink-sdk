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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of Qt Designer.  This header
// file may change from version to version without notice, or even be removed.
//
// We mean it.
//

#ifndef QDESIGNER_MEMBERSHEET_H
#define QDESIGNER_MEMBERSHEET_H

#include "shared_global_p.h"

#include <QtDesigner/membersheet.h>
#include <QtDesigner/default_extensionfactory.h>
#include <QtCore/QStringList>

QT_BEGIN_NAMESPACE

class QDesignerMemberSheetPrivate;

class QDESIGNER_SHARED_EXPORT QDesignerMemberSheet: public QObject, public QDesignerMemberSheetExtension
{
    Q_OBJECT
    Q_INTERFACES(QDesignerMemberSheetExtension)

public:
    explicit QDesignerMemberSheet(QObject *object, QObject *parent = 0);
    virtual ~QDesignerMemberSheet();

    int indexOf(const QString &name) const Q_DECL_OVERRIDE;

    int count() const Q_DECL_OVERRIDE;
    QString memberName(int index) const Q_DECL_OVERRIDE;

    QString memberGroup(int index) const Q_DECL_OVERRIDE;
    void setMemberGroup(int index, const QString &group) Q_DECL_OVERRIDE;

    bool isVisible(int index) const Q_DECL_OVERRIDE;
    void setVisible(int index, bool b) Q_DECL_OVERRIDE;

    bool isSignal(int index) const Q_DECL_OVERRIDE;
    bool isSlot(int index) const Q_DECL_OVERRIDE;

    bool inheritedFromWidget(int index) const Q_DECL_OVERRIDE;

    static bool signalMatchesSlot(const QString &signal, const QString &slot);

    QString declaredInClass(int index) const Q_DECL_OVERRIDE;

    QString signature(int index) const Q_DECL_OVERRIDE;
    QList<QByteArray> parameterTypes(int index) const Q_DECL_OVERRIDE;
    QList<QByteArray> parameterNames(int index) const Q_DECL_OVERRIDE;

private:
    QDesignerMemberSheetPrivate *d;
};

class QDESIGNER_SHARED_EXPORT QDesignerMemberSheetFactory: public QExtensionFactory
{
    Q_OBJECT
    Q_INTERFACES(QAbstractExtensionFactory)

public:
    QDesignerMemberSheetFactory(QExtensionManager *parent = 0);

protected:
    QObject *createExtension(QObject *object, const QString &iid, QObject *parent) const Q_DECL_OVERRIDE;
};

QT_END_NAMESPACE

#endif // QDESIGNER_MEMBERSHEET_H
