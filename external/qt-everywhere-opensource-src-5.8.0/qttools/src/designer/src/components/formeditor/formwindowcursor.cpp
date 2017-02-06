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

#include "formwindowcursor.h"
#include "formwindow.h"

// sdk
#include <QtDesigner/propertysheet.h>
#include <QtDesigner/QExtensionManager>
#include <qdesigner_propertycommand_p.h>

#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {

FormWindowCursor::FormWindowCursor(FormWindow *fw, QObject *parent)
    : QObject(parent),
      m_formWindow(fw)
{
    update();
    connect(fw, &QDesignerFormWindowInterface::changed, this, &FormWindowCursor::update);
}

FormWindowCursor::~FormWindowCursor()
{
}

QDesignerFormWindowInterface *FormWindowCursor::formWindow() const
{
    return m_formWindow;
}

bool FormWindowCursor::movePosition(MoveOperation op, MoveMode mode)
{
    if (widgetCount() == 0)
        return false;

    int iterator = position();

    if (mode == MoveAnchor)
        m_formWindow->clearSelection(false);

    switch (op) {
    case Next:
        ++iterator;
        if (iterator >= widgetCount())
            iterator = 0;

        m_formWindow->selectWidget(m_formWindow->widgetAt(iterator), true);
        return true;

    case Prev:
        --iterator;
        if (iterator < 0)
            iterator = widgetCount() - 1;

        if (iterator < 0)
            return false;

        m_formWindow->selectWidget(m_formWindow->widgetAt(iterator), true);
        return true;

    default:
        return false;
    }
}

int FormWindowCursor::position() const
{
    const int index = m_formWindow->widgets().indexOf(current());
    return index == -1 ? 0 : index;
}

void FormWindowCursor::setPosition(int pos, MoveMode mode)
{
    if (!widgetCount())
        return;

    if (mode == MoveAnchor)
        m_formWindow->clearSelection(false);

    if (pos >= widgetCount())
        pos = 0;

    m_formWindow->selectWidget(m_formWindow->widgetAt(pos), true);
}

QWidget *FormWindowCursor::current() const
{
    return m_formWindow->currentWidget();
}

bool FormWindowCursor::hasSelection() const
{
    return !m_formWindow->selectedWidgets().isEmpty();
}

int FormWindowCursor::selectedWidgetCount() const
{
    int N = m_formWindow->selectedWidgets().count();
    return N ? N : 1;
}

QWidget *FormWindowCursor::selectedWidget(int index) const
{
    return hasSelection()
        ? m_formWindow->selectedWidgets().at(index)
        : m_formWindow->mainContainer();
}

void FormWindowCursor::update()
{
    // ### todo
}

int FormWindowCursor::widgetCount() const
{
    return m_formWindow->widgetCount();
}

QWidget *FormWindowCursor::widget(int index) const
{
    return m_formWindow->widgetAt(index);
}

void FormWindowCursor::setProperty(const QString &name, const QVariant &value)
{

    // build selection
    const int N = selectedWidgetCount();
    Q_ASSERT(N);

    SetPropertyCommand::ObjectList selection;
    for (int i=0; i<N; ++i)
         selection.push_back(selectedWidget(i));


    SetPropertyCommand* setPropertyCommand = new SetPropertyCommand(m_formWindow);
    if (setPropertyCommand->init(selection, name, value, current())) {
        m_formWindow->commandHistory()->push(setPropertyCommand);
    } else {
        delete setPropertyCommand;
        qDebug() << "Unable to set property " << name << '.';
    }
}

void FormWindowCursor::setWidgetProperty(QWidget *widget, const QString &name, const QVariant &value)
{
    SetPropertyCommand *cmd = new SetPropertyCommand(m_formWindow);
    if (cmd->init(widget, name, value)) {
        m_formWindow->commandHistory()->push(cmd);
    } else {
        delete cmd;
        qDebug() << "Unable to set property " << name << '.';
    }
}

void FormWindowCursor::resetWidgetProperty(QWidget *widget, const QString &name)
{
    ResetPropertyCommand *cmd = new ResetPropertyCommand(m_formWindow);
    if (cmd->init(widget, name)) {
        m_formWindow->commandHistory()->push(cmd);
    } else {
        delete cmd;
        qDebug() << "Unable to reset property " << name << '.';
    }
}

}

QT_END_NAMESPACE
