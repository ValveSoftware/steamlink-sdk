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

#include "abstractformwindow.h"
#include "inplace_editor.h"

#include <QtDesigner/QDesignerFormWindowInterface>
#include <QtDesigner/QDesignerFormWindowCursorInterface>
#include <QtDesigner/QDesignerPropertySheetExtension>
#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QDesignerLanguageExtension>
#include <QtDesigner/QExtensionManager>

#include <QtCore/QVariant>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {

// ----------------- InPlaceEditor

InPlaceEditor::InPlaceEditor(QWidget *widget,
                             TextPropertyValidationMode validationMode,
                             QDesignerFormWindowInterface *fw,
                             const QString& text,
                             const QRect& r) :
    TextPropertyEditor(widget, EmbeddingInPlace, validationMode),
    m_InPlaceWidgetHelper(this, widget, fw)
{
    setAlignment(m_InPlaceWidgetHelper.alignment());
    setObjectName(QStringLiteral("__qt__passive_m_editor"));

    setText(text);
    selectAll();

    setGeometry(QRect(widget->mapTo(widget->window(), r.topLeft()), r.size()));
    setFocus();
    show();

    connect(this, &TextPropertyEditor::editingFinished,this, &QWidget::close);
}


// -------------- TaskMenuInlineEditor

TaskMenuInlineEditor::TaskMenuInlineEditor(QWidget *w, TextPropertyValidationMode vm,
                                           const QString &property, QObject *parent) :
    QObject(parent),
    m_vm(vm),
    m_property(property),
    m_widget(w),
    m_managed(true)
{
}

void TaskMenuInlineEditor::editText()
{
    m_formWindow = QDesignerFormWindowInterface::findFormWindow(m_widget);
    if (m_formWindow.isNull())
        return;
    m_managed = m_formWindow->isManaged(m_widget);
    // Close as soon as a different widget is selected
    connect(m_formWindow.data(), &QDesignerFormWindowInterface::selectionChanged,
            this, &TaskMenuInlineEditor::updateSelection);

    // get old value
    QDesignerFormEditorInterface *core = m_formWindow->core();
    const QDesignerPropertySheetExtension *sheet = qt_extension<QDesignerPropertySheetExtension*>(core->extensionManager(), m_widget);
    const int index = sheet->indexOf(m_property);
    if (index == -1)
        return;
    m_value = qvariant_cast<PropertySheetStringValue>(sheet->property(index));
    const QString oldValue = m_value.value();

    m_editor = new InPlaceEditor(m_widget, m_vm, m_formWindow, oldValue, editRectangle());
    connect(m_editor.data(), &InPlaceEditor::textChanged, this, &TaskMenuInlineEditor::updateText);
}

void TaskMenuInlineEditor::updateText(const QString &text)
{
    // In the [rare] event we are invoked on an unmanaged widget,
    // do not use the cursor selection
    m_value.setValue(text);
    if (m_managed) {
        m_formWindow->cursor()->setProperty(m_property, QVariant::fromValue(m_value));
    } else {
        m_formWindow->cursor()->setWidgetProperty(m_widget, m_property, QVariant::fromValue(m_value));
    }
}

void TaskMenuInlineEditor::updateSelection()
{
    if (m_editor)
        m_editor->deleteLater();
}

}

QT_END_NAMESPACE
