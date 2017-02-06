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

#include "tabordereditor_tool.h"
#include "tabordereditor.h"

#include <QtDesigner/QDesignerFormWindowInterface>

#include <QtCore/QEvent>
#include <QtWidgets/QAction>

QT_BEGIN_NAMESPACE

using namespace qdesigner_internal;

TabOrderEditorTool::TabOrderEditorTool(QDesignerFormWindowInterface *formWindow, QObject *parent)
    : QDesignerFormWindowToolInterface(parent),
      m_formWindow(formWindow),
      m_action(new QAction(tr("Edit Tab Order"), this))
{
}

TabOrderEditorTool::~TabOrderEditorTool()
{
}

QDesignerFormEditorInterface *TabOrderEditorTool::core() const
{
    return m_formWindow->core();
}

QDesignerFormWindowInterface *TabOrderEditorTool::formWindow() const
{
    return m_formWindow;
}

bool TabOrderEditorTool::handleEvent(QWidget *widget, QWidget *managedWidget, QEvent *event)
{
    Q_UNUSED(widget);
    Q_UNUSED(managedWidget);

    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
        return true;

    return false;
}

QWidget *TabOrderEditorTool::editor() const
{
    if (!m_editor) {
        Q_ASSERT(formWindow() != 0);
        m_editor = new TabOrderEditor(formWindow(), 0);
        connect(formWindow(), &QDesignerFormWindowInterface::mainContainerChanged,
                m_editor.data(), &TabOrderEditor::setBackground);
    }

    return m_editor;
}

void TabOrderEditorTool::activated()
{
    connect(formWindow(), &QDesignerFormWindowInterface::changed,
                m_editor.data(), &TabOrderEditor::updateBackground);
}

void TabOrderEditorTool::deactivated()
{
    disconnect(formWindow(), &QDesignerFormWindowInterface::changed,
                m_editor.data(), &TabOrderEditor::updateBackground);
}

QAction *TabOrderEditorTool::action() const
{
    return m_action;
}

QT_END_NAMESPACE
