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

#include "abstractnewformwidget.h"
#include <newformwidget_p.h>

QT_BEGIN_NAMESPACE

/*!
    \class QDesignerNewFormWidgetInterface
    \since 4.5
    \internal

    \brief QDesignerNewFormWidgetInterface provides an interface for chooser
           widgets that can be used within "New Form" dialogs and wizards.
           It presents the user with a list of choices taken from built-in
           templates, pre-defined template paths and suitable custom widgets.
           It provides a static creation function that returns \QD's
           implementation.

    \inmodule QtDesigner
*/

/*!
    Constructs a QDesignerNewFormWidgetInterface object.
*/

QDesignerNewFormWidgetInterface::QDesignerNewFormWidgetInterface(QWidget *parent) :
    QWidget(parent)
{
}

/*!
    Destroys the QDesignerNewFormWidgetInterface object.
*/

QDesignerNewFormWidgetInterface::~QDesignerNewFormWidgetInterface()
{
}

/*!
    Creates an instance of the QDesignerNewFormWidgetInterface as a child
    of \a parent using \a core.
*/

QDesignerNewFormWidgetInterface *QDesignerNewFormWidgetInterface::createNewFormWidget(QDesignerFormEditorInterface *core, QWidget *parent)
{
    return new qdesigner_internal::NewFormWidget(core, parent);
}

/*!
    \fn bool QDesignerNewFormWidgetInterface::hasCurrentTemplate() const

    Returns whether a form template is currently selected.
*/

/*!
    \fn QString QDesignerNewFormWidgetInterface::currentTemplate(QString *errorMessage = 0)

    Returns the contents of the currently selected template. If the method fails,
    an empty string is returned and \a errorMessage receives an error message.
*/

// Signals

/*!
    \fn void QDesignerNewFormWidgetInterface::templateActivated()

    This signal is emitted whenever the user activates a template by double-clicking.
*/

/*!
    \fn void QDesignerNewFormWidgetInterface::currentTemplateChanged(bool templateSelected)

    This signal is emitted whenever the user changes the current template.
    \a templateSelected indicates whether a template is currently selected.
*/

QT_END_NAMESPACE
