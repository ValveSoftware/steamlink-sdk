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

#include "layouttaskmenu.h"
#include <formlayoutmenu_p.h>
#include <morphmenu_p.h>

#include <QtDesigner/QDesignerFormWindowInterface>

#include <QtWidgets/QAction>
#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

// ------------ LayoutWidgetTaskMenu
LayoutWidgetTaskMenu::LayoutWidgetTaskMenu(QLayoutWidget *lw, QObject *parent) :
   QObject(parent),
   m_widget(lw),
   m_morphMenu(new qdesigner_internal::MorphMenu(this)),
   m_formLayoutMenu(new qdesigner_internal::FormLayoutMenu(this))
{
}

QAction *LayoutWidgetTaskMenu::preferredEditAction() const
{
    return m_formLayoutMenu->preferredEditAction(m_widget, m_widget->formWindow());
}

QList<QAction*> LayoutWidgetTaskMenu::taskActions() const
{
    QList<QAction*> rc;
    QDesignerFormWindowInterface *fw = m_widget->formWindow();
    m_morphMenu->populate(m_widget, fw, rc);
    m_formLayoutMenu->populate(m_widget, fw, rc);
    return rc;
}

// ------------- SpacerTaskMenu
SpacerTaskMenu::SpacerTaskMenu(Spacer *, QObject *parent) :
    QObject(parent)
{
}

QAction *SpacerTaskMenu::preferredEditAction() const
{
    return 0;
}

QList<QAction*> SpacerTaskMenu::taskActions() const
{
    return QList<QAction*>();
}

QT_END_NAMESPACE

