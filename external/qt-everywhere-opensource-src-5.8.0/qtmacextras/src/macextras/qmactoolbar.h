/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtMacExtras module of the Qt Toolkit.
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

#ifndef QMACTOOLBAR_H
#define QMACTOOLBAR_H

#include <QtMacExtras/qmacextrasglobal.h>
#include <QtMacExtras/qmactoolbaritem.h>

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
    explicit QMacToolBar(QObject *parent = Q_NULLPTR);
    explicit QMacToolBar(const QString &identifier, QObject *parent = Q_NULLPTR);
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

