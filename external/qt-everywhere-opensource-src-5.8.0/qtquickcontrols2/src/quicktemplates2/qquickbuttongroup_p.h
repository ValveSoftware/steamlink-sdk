/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Templates 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQUICKBUTTONGROUP_P_H
#define QQUICKBUTTONGROUP_P_H

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

#include <QtCore/qobject.h>
#include <QtQuickTemplates2/private/qtquicktemplates2global_p.h>
#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

class QQuickAbstractButton;
class QQuickButtonGroupPrivate;
class QQuickButtonGroupAttached;
class QQuickButtonGroupAttachedPrivate;

class Q_QUICKTEMPLATES2_PRIVATE_EXPORT QQuickButtonGroup : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQuickAbstractButton *checkedButton READ checkedButton WRITE setCheckedButton NOTIFY checkedButtonChanged)
    Q_PROPERTY(QQmlListProperty<QQuickAbstractButton> buttons READ buttons NOTIFY buttonsChanged FINAL)

public:
    explicit QQuickButtonGroup(QObject *parent = nullptr);
    ~QQuickButtonGroup();

    static QQuickButtonGroupAttached *qmlAttachedProperties(QObject *object);

    QQuickAbstractButton *checkedButton() const;
    void setCheckedButton(QQuickAbstractButton *checkedButton);

    QQmlListProperty<QQuickAbstractButton> buttons();

public Q_SLOTS:
    void addButton(QQuickAbstractButton *button);
    void removeButton(QQuickAbstractButton *button);

Q_SIGNALS:
    void checkedButtonChanged();
    void buttonsChanged();
    Q_REVISION(1) void clicked(QQuickAbstractButton *button);

private:
    Q_DISABLE_COPY(QQuickButtonGroup)
    Q_DECLARE_PRIVATE(QQuickButtonGroup)

    Q_PRIVATE_SLOT(d_func(), void _q_updateCurrent())
};

class Q_QUICKTEMPLATES2_PRIVATE_EXPORT QQuickButtonGroupAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQuickButtonGroup *group READ group WRITE setGroup NOTIFY groupChanged FINAL)

public:
    explicit QQuickButtonGroupAttached(QObject *parent = nullptr);

    QQuickButtonGroup *group() const;
    void setGroup(QQuickButtonGroup *group);

Q_SIGNALS:
    void groupChanged();

private:
    Q_DISABLE_COPY(QQuickButtonGroupAttached)
    Q_DECLARE_PRIVATE(QQuickButtonGroupAttached)
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickButtonGroup)
QML_DECLARE_TYPEINFO(QQuickButtonGroup, QML_HAS_ATTACHED_PROPERTIES)

#endif // QQUICKBUTTONGROUP_P_H
