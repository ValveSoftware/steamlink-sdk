/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#ifndef QQUICKSTACK_P_H
#define QQUICKSTACK_P_H

#include <QtQuick/qquickitem.h>

QT_BEGIN_NAMESPACE

class QQuickStack1 : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int index READ index NOTIFY indexChanged)
    Q_PROPERTY(int __index READ index WRITE setIndex NOTIFY indexChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(Status __status READ status WRITE setStatus NOTIFY statusChanged)
    Q_PROPERTY(QQuickItem* view READ view NOTIFY viewChanged)
    Q_PROPERTY(QQuickItem* __view READ view WRITE setView NOTIFY viewChanged)
    Q_ENUMS(Status)

public:
    QQuickStack1(QObject *object = 0);

    static QQuickStack1 *qmlAttachedProperties(QObject *object);

    int index() const;
    void setIndex(int index);

    enum Status {
        Inactive = 0,
        Deactivating = 1,
        Activating = 2,
        Active = 3
    };

    Status status() const;
    void setStatus(Status status);

    QQuickItem *view() const;
    void setView(QQuickItem *view);

signals:
    void statusChanged();
    void viewChanged();
    void indexChanged();

private:
    int m_index;
    Status m_status;
    QQuickItem *m_view;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickStack1)
QML_DECLARE_TYPEINFO(QQuickStack1, QML_HAS_ATTACHED_PROPERTIES)

#endif // QQUICKSTACK_P_H
