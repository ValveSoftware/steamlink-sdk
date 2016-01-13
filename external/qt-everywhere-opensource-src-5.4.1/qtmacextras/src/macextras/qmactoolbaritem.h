/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtMacExtras module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QMACTOOLBARITEM_H
#define QMACTOOLBARITEM_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtGui/QIcon>

#include "qmacextrasglobal.h"

Q_FORWARD_DECLARE_OBJC_CLASS(NSToolbarItem);

QT_BEGIN_NAMESPACE

class QMacToolBarItemPrivate;
class Q_MACEXTRAS_EXPORT QMacToolBarItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool selectable READ selectable WRITE setSelectable)
    Q_PROPERTY(StandardItem standardItem READ standardItem WRITE setStandardItem)
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(QIcon icon READ icon WRITE setIcon)
    Q_ENUMS(StandardItem)

public:
    enum StandardItem
    {
        NoStandardItem,
        Space,
        FlexibleSpace
    };

    QMacToolBarItem(QObject *parent = 0);
    virtual ~QMacToolBarItem();

    bool selectable() const;
    void setSelectable(bool selectable);

    StandardItem standardItem() const;
    void setStandardItem(StandardItem standardItem);

    QString text() const;
    void setText(const QString &text);

    QIcon icon() const;
    void setIcon(const QIcon &icon);

    NSToolbarItem *nativeToolBarItem() const;

Q_SIGNALS:
    void activated();
private:
    friend class QMacToolBarPrivate;
    Q_DECLARE_PRIVATE(QMacToolBarItem)
};

QT_END_NAMESPACE

#endif
