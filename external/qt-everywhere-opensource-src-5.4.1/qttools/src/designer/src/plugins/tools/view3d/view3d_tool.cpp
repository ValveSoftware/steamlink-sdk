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
