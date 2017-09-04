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

#include "toolbar_taskmenu.h"
#include "qdesigner_toolbar_p.h"

#include <QtDesigner/QDesignerFormWindowInterface>

#include <promotiontaskmenu_p.h>
#include <qdesigner_command_p.h>

#include <QtWidgets/QAction>
#include <QtWidgets/QUndoStack>

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {
    // ------------ ToolBarTaskMenu
    ToolBarTaskMenu::ToolBarTaskMenu(QToolBar *tb, QObject *parent) :
        QObject(parent),
        m_toolBar(tb)
    {
    }

    QAction *ToolBarTaskMenu::preferredEditAction() const
    {
        return 0;
    }

    QList<QAction*> ToolBarTaskMenu::taskActions() const
    {
        if (ToolBarEventFilter *ef = ToolBarEventFilter::eventFilterOf(m_toolBar))
            return ef->contextMenuActions();
        return QList<QAction*>();
    }

    // ------------ StatusBarTaskMenu
    StatusBarTaskMenu::StatusBarTaskMenu(QStatusBar *sb, QObject *parent) :
        QObject(parent),
        m_statusBar(sb),
        m_removeAction(new QAction(tr("Remove"), this)),
        m_promotionTaskMenu(new PromotionTaskMenu(sb, PromotionTaskMenu::ModeSingleWidget, this))
    {
        connect(m_removeAction, &QAction::triggered, this, &StatusBarTaskMenu::removeStatusBar);
    }

    QAction *StatusBarTaskMenu::preferredEditAction() const
    {
        return 0;
    }

    QList<QAction*> StatusBarTaskMenu::taskActions() const
    {
        QList<QAction*> rc;
        rc.push_back(m_removeAction);
        m_promotionTaskMenu->addActions(PromotionTaskMenu::LeadingSeparator, rc);
        return rc;
    }

    void StatusBarTaskMenu::removeStatusBar()
    {
        if (QDesignerFormWindowInterface *fw = QDesignerFormWindowInterface::findFormWindow(m_statusBar)) {
            DeleteStatusBarCommand *cmd = new DeleteStatusBarCommand(fw);
            cmd->init(m_statusBar);
            fw->commandHistory()->push(cmd);
        }
    }
}

QT_END_NAMESPACE

