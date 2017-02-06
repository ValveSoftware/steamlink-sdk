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

#ifndef QQUICKMENUBAR_P_H
#define QQUICKMENUBAR_P_H

#include "qquickmenu_p.h"

#include <QtCore/qobject.h>
#include <QtQuick/qquickitem.h>

QT_BEGIN_NAMESPACE

class QPlatformMenuBar;
class QQuickItem;

class QQuickMenuBar1: public QObject
{
    Q_OBJECT

    Q_PROPERTY(QQmlListProperty<QQuickMenu1> menus READ menus NOTIFY menusChanged)
    Q_CLASSINFO("DefaultProperty", "menus")

    Q_PROPERTY(QQuickItem *__contentItem READ contentItem WRITE setContentItem NOTIFY contentItemChanged)
    Q_PROPERTY(QQuickWindow *__parentWindow READ parentWindow WRITE setParentWindow)
    Q_PROPERTY(bool __isNative READ isNative WRITE setNative NOTIFY nativeChanged)

Q_SIGNALS:
    void menusChanged();
    void nativeChanged();
    void contentItemChanged();

public:
    QQuickMenuBar1(QObject *parent = 0);
    ~QQuickMenuBar1();

    QQmlListProperty<QQuickMenu1> menus();

    bool isNative() const;
    void setNative(bool native);

    QQuickItem *contentItem() const { return m_contentItem; }
    void setContentItem(QQuickItem *);

    QQuickWindow *parentWindow() const { return m_parentWindow; }
    void setParentWindow(QQuickWindow *);

    QPlatformMenuBar *platformMenuBar() const { return m_platformMenuBar; }

private:
    void setNativeNoNotify(bool native);
    static void append_menu(QQmlListProperty<QQuickMenu1> *list, QQuickMenu1 *menu);
    static int count_menu(QQmlListProperty<QQuickMenu1> *list);
    static QQuickMenu1 *at_menu(QQmlListProperty<QQuickMenu1> *list, int index);

private:
    QList<QQuickMenu1 *> m_menus;
    QPlatformMenuBar *m_platformMenuBar;
    QQuickItem *m_contentItem;
    QQuickWindow *m_parentWindow;
};

QT_END_NAMESPACE

#endif // QQUICKMENUBAR_P_H
