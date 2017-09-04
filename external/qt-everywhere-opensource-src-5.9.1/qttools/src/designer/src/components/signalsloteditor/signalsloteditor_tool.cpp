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

#include "signalsloteditor_tool.h"
#include "signalsloteditor.h"

#include <QtDesigner/private/ui4_p.h>
#include <QtDesigner/QDesignerFormWindowInterface>

#include <QtWidgets/QAction>
#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

using namespace qdesigner_internal;

SignalSlotEditorTool::SignalSlotEditorTool(QDesignerFormWindowInterface *formWindow, QObject *parent)
    : QDesignerFormWindowToolInterface(parent),
      m_formWindow(formWindow),
      m_action(new QAction(tr("Edit Signals/Slots"), this))
{
}

SignalSlotEditorTool::~SignalSlotEditorTool()
{
}

QDesignerFormEditorInterface *SignalSlotEditorTool::core() const
{
    return m_formWindow->core();
}

QDesignerFormWindowInterface *SignalSlotEditorTool::formWindow() const
{
    return m_formWindow;
}

bool SignalSlotEditorTool::handleEvent(QWidget *widget, QWidget *managedWidget, QEvent *event)
{
    Q_UNUSED(widget);
    Q_UNUSED(managedWidget);
    Q_UNUSED(event);

    return false;
}

QWidget *SignalSlotEditorTool::editor() const
{
    if (!m_editor) {
        Q_ASSERT(formWindow() != 0);
        m_editor = new qdesigner_internal::SignalSlotEditor(formWindow(), 0);
        connect(formWindow(), &QDesignerFormWindowInterface::mainContainerChanged,
                m_editor.data(), &SignalSlotEditor::setBackground);
        connect(formWindow(), &QDesignerFormWindowInterface::changed,
                m_editor.data(), &SignalSlotEditor::updateBackground);
    }

    return m_editor;
}

QAction *SignalSlotEditorTool::action() const
{
    return m_action;
}

void SignalSlotEditorTool::activated()
{
    m_editor->enableUpdateBackground(true);
}

void SignalSlotEditorTool::deactivated()
{
    m_editor->enableUpdateBackground(false);
}

void SignalSlotEditorTool::saveToDom(DomUI *ui, QWidget*)
{
    ui->setElementConnections(m_editor->toUi());
}

void SignalSlotEditorTool::loadFromDom(DomUI *ui, QWidget *mainContainer)
{
    m_editor->fromUi(ui->elementConnections(), mainContainer);
}

QT_END_NAMESPACE
