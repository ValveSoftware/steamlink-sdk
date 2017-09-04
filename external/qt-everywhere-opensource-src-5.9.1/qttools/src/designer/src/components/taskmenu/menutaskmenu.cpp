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
        connect(m_removeAction, &QAction::triggered, this, &MenuTaskMenu::removeMenu);
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

