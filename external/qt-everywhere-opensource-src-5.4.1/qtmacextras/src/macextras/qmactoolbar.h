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

#ifndef QMACTOOLBAR_H
#define QMACTOOLBAR_H

#include "qmacextrasglobal.h"
#include "qmactoolbaritem.h"

#include <QtCore/QString>
#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtGui/QIcon>

Q_FORWARD_DECLARE_OBJC_CLASS(NSToolbar);

QT_BEGIN_NAMESPACE

class QWindow;
class QMacToolBarPrivate;

class Q_MACEXTRAS_EXPORT QMacToolBar : public QObject
{
    Q_OBJECT
public:
    explicit QMacToolBar(QObject *parent = 0);
    QMacToolBar(const QString &identifier, QObject *parent = 0);
    ~QMacToolBar();

    QMacToolBarItem *addItem(const QIcon &icon, const QString &text);
    QMacToolBarItem *addAllowedItem(const QIcon &icon, const QString &text);
    QMacToolBarItem *addStandardItem(QMacToolBarItem::StandardItem standardItem);
    QMacToolBarItem *addAllowedStandardItem(QMacToolBarItem::StandardItem standardItem);
    void addSeparator();

    void setItems(QList<QMacToolBarItem *> &items);
    QList<QMacToolBarItem *> items();
    void setAllowedItems(QList<QMacToolBarItem *> &allowedItems);
    QList<QMacToolBarItem *> allowedItems();

    void attachToWindow(QWindow *window);
    void detachFromWindow();

    NSToolbar* nativeToolbar() const;
private Q_SLOTS:
    void showInWindow_impl();
private:
    Q_DECLARE_PRIVATE(QMacToolBar)
};

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QMacToolBar*)

#endif

