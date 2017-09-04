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

#ifndef QQUICKMENUITEMCONTAINER_P_H
#define QQUICKMENUITEMCONTAINER_P_H

#include "qquickmenuitem_p.h"
#include <QtCore/qlist.h>

QT_BEGIN_NAMESPACE

class QQuickMenuItemContainer1 : public QQuickMenuBase1
{
    Q_OBJECT
public:
    explicit QQuickMenuItemContainer1(QObject *parent = 0)
        : QQuickMenuBase1(parent, -1)
    { }

    ~QQuickMenuItemContainer1()
    {
        clear();
        setParentMenu(0);
    }

    void setParentMenu(QQuickMenu1 *parentMenu)
    {
        QQuickMenuBase1::setParentMenu(parentMenu);
        for (QQuickMenuBase1 *item : qAsConst(m_menuItems))
            item->setParentMenu(parentMenu);
    }

    void insertItem(int index, QQuickMenuBase1 *item)
    {
        if (index == -1)
            index = m_menuItems.count();
        m_menuItems.insert(index, item);
        item->setContainer(this);
    }

    void removeItem(QQuickMenuBase1 *item)
    {
        item->setParentMenu(0);
        item->setContainer(0);
        m_menuItems.removeOne(item);
    }

    const QList<QPointer<QQuickMenuBase1> > &items()
    {
        return m_menuItems;
    }

    void clear()
    {
        while (!m_menuItems.empty()) {
            QQuickMenuBase1 *item = m_menuItems.takeFirst();
            if (item) {
                item->setParentMenu(0);
                item->setContainer(0);
            }
        }
    }

private:
    QList<QPointer<QQuickMenuBase1> > m_menuItems;
};

QT_END_NAMESPACE

#endif // QQUICKMENUITEMCONTAINER_P_H
