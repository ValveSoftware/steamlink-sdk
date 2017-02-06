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

#include "textedit_taskmenu.h"

#include <QtDesigner/QDesignerFormWindowInterface>

#include <QtWidgets/QAction>
#include <QtCore/QEvent>
#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {

TextEditTaskMenu::TextEditTaskMenu(QTextEdit *textEdit, QObject *parent) :
    QDesignerTaskMenu(textEdit, parent),
    m_format(Qt::RichText),
    m_property(QStringLiteral("html")),
    m_windowTitle(tr("Edit HTML")),
    m_editTextAction(new QAction(tr("Change HTML..."), this))
{
    initialize();
}

TextEditTaskMenu::TextEditTaskMenu(QPlainTextEdit *textEdit, QObject *parent) :
    QDesignerTaskMenu(textEdit, parent),
    m_format(Qt::PlainText),
    m_property(QStringLiteral("plainText")),
    m_windowTitle(tr("Edit Text")),
    m_editTextAction(new QAction(tr("Change Plain Text..."), this))
{
    initialize();
}


void TextEditTaskMenu::initialize()
{
    connect(m_editTextAction, &QAction::triggered, this, &TextEditTaskMenu::editText);
    m_taskActions.append(m_editTextAction);

    QAction *sep = new QAction(this);
    sep->setSeparator(true);
    m_taskActions.append(sep);
}

TextEditTaskMenu::~TextEditTaskMenu()
{
}

QAction *TextEditTaskMenu::preferredEditAction() const
{
    return m_editTextAction;
}

QList<QAction*> TextEditTaskMenu::taskActions() const
{
    return m_taskActions + QDesignerTaskMenu::taskActions();
}

void TextEditTaskMenu::editText()
{
    changeTextProperty(m_property, m_windowTitle, MultiSelectionMode, m_format);
}

}
QT_END_NAMESPACE
