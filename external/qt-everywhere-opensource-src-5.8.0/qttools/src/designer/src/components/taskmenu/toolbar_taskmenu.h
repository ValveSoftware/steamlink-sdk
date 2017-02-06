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

#ifndef TOOLBAR_TASKMENU_H
#define TOOLBAR_TASKMENU_H

#include <QtDesigner/QDesignerTaskMenuExtension>

#include <extensionfactory_p.h>

#include <QtWidgets/QToolBar>
#include <QtWidgets/QStatusBar>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {
    class PromotionTaskMenu;

// ToolBarTaskMenu forwards the actions of ToolBarEventFilter
class ToolBarTaskMenu : public QObject, public QDesignerTaskMenuExtension
{
    Q_OBJECT
    Q_INTERFACES(QDesignerTaskMenuExtension)
public:
    explicit ToolBarTaskMenu(QToolBar *tb, QObject *parent = 0);

    QAction *preferredEditAction() const Q_DECL_OVERRIDE;
    QList<QAction*> taskActions() const Q_DECL_OVERRIDE;

private:
    QToolBar *m_toolBar;
};

// StatusBarTaskMenu provides promotion and deletion
class StatusBarTaskMenu : public QObject, public QDesignerTaskMenuExtension
{
    Q_OBJECT
    Q_INTERFACES(QDesignerTaskMenuExtension)
public:
    explicit StatusBarTaskMenu(QStatusBar *tb, QObject *parent = 0);

    QAction *preferredEditAction() const Q_DECL_OVERRIDE;
    QList<QAction*> taskActions() const Q_DECL_OVERRIDE;

private slots:
    void removeStatusBar();

private:
    QStatusBar  *m_statusBar;
    QAction *m_removeAction;
    PromotionTaskMenu *m_promotionTaskMenu;
};

typedef ExtensionFactory<QDesignerTaskMenuExtension, QToolBar, ToolBarTaskMenu> ToolBarTaskMenuFactory;
typedef ExtensionFactory<QDesignerTaskMenuExtension, QStatusBar, StatusBarTaskMenu> StatusBarTaskMenuFactory;

}  // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // TOOLBAR_TASKMENU_H
