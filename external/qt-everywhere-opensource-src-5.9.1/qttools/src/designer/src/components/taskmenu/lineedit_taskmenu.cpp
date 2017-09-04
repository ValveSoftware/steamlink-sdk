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

#include "lineedit_taskmenu.h"
#include "inplace_editor.h"

#include <QtDesigner/QDesignerFormWindowInterface>

#include <QtWidgets/QAction>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {

// -------- LineEditTaskMenuInlineEditor
class LineEditTaskMenuInlineEditor : public  TaskMenuInlineEditor
{
public:
    LineEditTaskMenuInlineEditor(QLineEdit *button, QObject *parent);

protected:
    QRect editRectangle() const Q_DECL_OVERRIDE;
};

LineEditTaskMenuInlineEditor::LineEditTaskMenuInlineEditor(QLineEdit *w, QObject *parent) :
      TaskMenuInlineEditor(w, ValidationSingleLine, QStringLiteral("text"), parent)
{
}

QRect LineEditTaskMenuInlineEditor::editRectangle() const
{
    QStyleOption opt;
    opt.init(widget());
    return opt.rect;
}

// --------------- LineEditTaskMenu
LineEditTaskMenu::LineEditTaskMenu(QLineEdit *lineEdit, QObject *parent) :
    QDesignerTaskMenu(lineEdit, parent),
    m_editTextAction(new QAction(tr("Change text..."), this))
{
    TaskMenuInlineEditor *editor = new LineEditTaskMenuInlineEditor(lineEdit, this);
    connect(m_editTextAction, &QAction::triggered, editor, &LineEditTaskMenuInlineEditor::editText);
    m_taskActions.append(m_editTextAction);

    QAction *sep = new QAction(this);
    sep->setSeparator(true);
    m_taskActions.append(sep);
}

QAction *LineEditTaskMenu::preferredEditAction() const
{
    return m_editTextAction;
}

QList<QAction*> LineEditTaskMenu::taskActions() const
{
    return m_taskActions + QDesignerTaskMenu::taskActions();
}

}

QT_END_NAMESPACE
