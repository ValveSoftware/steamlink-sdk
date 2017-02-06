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

#include "view3d_tool.h"

#include <QtWidgets/QAction>
#include <QtDesigner/QDesignerFormWindowInterface>

QView3DTool::QView3DTool(QDesignerFormWindowInterface *formWindow, QObject *parent)
    :  QDesignerFormWindowToolInterface(parent)
{
    m_action = new QAction(tr("3DView"), this);
    m_formWindow = formWindow;
}

QDesignerFormEditorInterface *QView3DTool::core() const
{
    return m_formWindow->core();
}

QDesignerFormWindowInterface *QView3DTool::formWindow() const
{
    return m_formWindow;
}

QWidget *QView3DTool::editor() const
{
    if (m_editor == 0)
        m_editor = new QView3D(formWindow(), 0);

    return m_editor;
}

QAction *QView3DTool::action() const
{
    return m_action;
}

void QView3DTool::activated()
{
    if (m_editor != 0)
        m_editor->updateForm();
}

void QView3DTool::deactivated()
{
}

bool QView3DTool::handleEvent(QWidget*, QWidget*, QEvent*)
{
    return false;
}
