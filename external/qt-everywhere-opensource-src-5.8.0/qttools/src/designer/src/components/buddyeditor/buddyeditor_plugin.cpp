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

#include <QtWidgets/QAction>

#include "buddyeditor_plugin.h"
#include "buddyeditor_tool.h"

#include <QtDesigner/QDesignerFormWindowInterface>
#include <QtDesigner/QDesignerFormWindowManagerInterface>
#include <QtDesigner/QDesignerFormEditorInterface>

QT_BEGIN_NAMESPACE

using namespace qdesigner_internal;

BuddyEditorPlugin::BuddyEditorPlugin()
    : m_initialized(false)
{
}

BuddyEditorPlugin::~BuddyEditorPlugin()
{
}

bool BuddyEditorPlugin::isInitialized() const
{
    return m_initialized;
}

void BuddyEditorPlugin::initialize(QDesignerFormEditorInterface *core)
{
    Q_ASSERT(!isInitialized());

    m_action = new QAction(tr("Edit Buddies"), this);
    m_action->setObjectName(QStringLiteral("__qt_edit_buddies_action"));
    QIcon buddyIcon = QIcon::fromTheme(QStringLiteral("designer-edit-buddy"),
                                       QIcon(core->resourceLocation() + QStringLiteral("/buddytool.png")));
    m_action->setIcon(buddyIcon);
    m_action->setEnabled(false);

    setParent(core);
    m_core = core;
    m_initialized = true;

    connect(core->formWindowManager(), &QDesignerFormWindowManagerInterface::formWindowAdded,
            this, &BuddyEditorPlugin::addFormWindow);

    connect(core->formWindowManager(), &QDesignerFormWindowManagerInterface::formWindowRemoved,
            this, &BuddyEditorPlugin::removeFormWindow);

    connect(core->formWindowManager(), &QDesignerFormWindowManagerInterface::activeFormWindowChanged,
                this, &BuddyEditorPlugin::activeFormWindowChanged);
}

QDesignerFormEditorInterface *BuddyEditorPlugin::core() const
{
    return m_core;
}

void BuddyEditorPlugin::addFormWindow(QDesignerFormWindowInterface *formWindow)
{
    Q_ASSERT(formWindow != 0);
    Q_ASSERT(m_tools.contains(formWindow) == false);

    BuddyEditorTool *tool = new BuddyEditorTool(formWindow, this);
    m_tools[formWindow] = tool;
    connect(m_action, &QAction::triggered, tool->action(), &QAction::trigger);
    formWindow->registerTool(tool);
}

void BuddyEditorPlugin::removeFormWindow(QDesignerFormWindowInterface *formWindow)
{
    Q_ASSERT(formWindow != 0);
    Q_ASSERT(m_tools.contains(formWindow) == true);

    BuddyEditorTool *tool = m_tools.value(formWindow);
    m_tools.remove(formWindow);
    disconnect(m_action, &QAction::triggered, tool->action(), &QAction::trigger);
    // ### FIXME disable the tool

    delete tool;
}

QAction *BuddyEditorPlugin::action() const
{
    return m_action;
}

void BuddyEditorPlugin::activeFormWindowChanged(QDesignerFormWindowInterface *formWindow)
{
    m_action->setEnabled(formWindow != 0);
}

QT_END_NAMESPACE
