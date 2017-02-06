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

#ifndef MENUTASKMENU_H
#define MENUTASKMENU_H

#include <QtDesigner/QDesignerTaskMenuExtension>

#include <qdesigner_menu_p.h>
#include <qdesigner_menubar_p.h>
#include <extensionfactory_p.h>

QT_BEGIN_NAMESPACE

class QDesignerFormWindowInterface;

namespace qdesigner_internal {

    class PromotionTaskMenu;

// The QMenu task menu provides promotion and a remove option. The actual
// menu context options are not forwarded since they make only sense
// when a menu is being edited/visible.

class MenuTaskMenu : public QObject, public QDesignerTaskMenuExtension
{
    Q_OBJECT
    Q_INTERFACES(QDesignerTaskMenuExtension)
public:
    explicit MenuTaskMenu(QDesignerMenu *menu, QObject *parent = 0);

    QAction *preferredEditAction() const Q_DECL_OVERRIDE;
    QList<QAction*> taskActions() const Q_DECL_OVERRIDE;

private slots:
    void removeMenu();

private:
    QDesignerMenu *m_menu;
    QAction *m_removeAction;
    PromotionTaskMenu *m_promotionTaskMenu;
};

// The QMenuBar task menu forwards the actions of QDesignerMenuBar,
// making them available in the object inspector.

class MenuBarTaskMenu : public QObject, public QDesignerTaskMenuExtension
{
    Q_OBJECT
    Q_INTERFACES(QDesignerTaskMenuExtension)
public:
    explicit MenuBarTaskMenu(QDesignerMenuBar *bar, QObject *parent = 0);

    QAction *preferredEditAction() const Q_DECL_OVERRIDE;
    QList<QAction*> taskActions() const Q_DECL_OVERRIDE;

private:
    QDesignerMenuBar *m_bar;
};

typedef ExtensionFactory<QDesignerTaskMenuExtension, QDesignerMenu, MenuTaskMenu> MenuTaskMenuFactory;
typedef ExtensionFactory<QDesignerTaskMenuExtension, QDesignerMenuBar, MenuBarTaskMenu> MenuBarTaskMenuFactory;

}  // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // MENUTASKMENU_H
