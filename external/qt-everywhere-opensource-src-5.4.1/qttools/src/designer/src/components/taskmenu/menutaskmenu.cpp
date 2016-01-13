/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#include "menutaskmenu.h"

#include <promotiontaskmenu_p.h>

#include <QtWidgets/QAction>
#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {
    // ------------ MenuTaskMenu
    MenuTaskMenu::MenuTaskMenu(QDesignerMenu *menu, QObject *parent) :
       QObject(parent),
       m_menu(menu),
       m_removeAction(new QAction(tr("Remove"), this)),
       m_promotionTaskMenu(new PromotionTaskMenu(menu, PromotionTaskMenu::ModeSingleWidget, this))
    {
        connect(m_removeAction, SIGNAL(triggered()), this, SLOT(removeMenu()));
    }

    QAction *MenuTaskMenu::preferredEditAction() const
    {
        return 0;
    }

    QList<QAction*> MenuTaskMenu::taskActions() const
    {
        QList<QAction*> rc;
        rc.push_back(m_removeAction);
        m_promotionTaskMenu->addActions(PromotionTaskMenu::LeadingSeparator, rc);
        return rc;
    }

    void MenuTaskMenu::removeMenu()
    {
        // Are we on a menu bar or on a menu?
        QWidget *pw = m_menu->parentWidget();
        if (QDesignerMenuBar *mb = qobject_cast<QDesignerMenuBar *>(pw)) {
            mb->deleteMenuAction(m_menu->menuAction());
            return;
        }
        if (QDesignerMenu *m = qobject_cast<QDesignerMenu *>(pw)) {
            m->deleteAction(m_menu->menuAction());
        }
    }

    // ------------- MenuBarTaskMenu
    MenuBarTaskMenu::MenuBarTaskMenu(QDesignerMenuBar *bar, QObject *parent) :
        QObject(parent),
        m_bar(bar)
    {
    }

    QAction *MenuBarTaskMenu::preferredEditAction() const
    {
        return 0;
    }

    QList<QAction*> MenuBarTaskMenu::taskActions() const
    {
        return m_bar->contextMenuActions();
    }
}

QT_END_NAMESPACE

