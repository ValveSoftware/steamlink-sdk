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

#include "plaintexteditor_p.h"

#include <QtDesigner/QDesignerSettingsInterface>
#include <QtDesigner/QDesignerFormEditorInterface>

#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>

QT_BEGIN_NAMESPACE

static const char *PlainTextDialogC = "PlainTextDialog";
static const char *Geometry = "Geometry";


namespace qdesigner_internal {

PlainTextEditorDialog::PlainTextEditorDialog(QDesignerFormEditorInterface *core, QWidget *parent)  :
    QDialog(parent),
    m_editor(new QPlainTextEdit),
    m_core(core)
{
    setWindowTitle(tr("Edit text"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QVBoxLayout *vlayout = new QVBoxLayout(this);
    vlayout->addWidget(m_editor);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal);
    QPushButton *ok_button = buttonBox->button(QDialogButtonBox::Ok);
    ok_button->setDefault(true);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vlayout->addWidget(buttonBox);

    QDesignerSettingsInterface *settings = core->settingsManager();
    settings->beginGroup(QLatin1String(PlainTextDialogC));

    if (settings->contains(QLatin1String(Geometry)))
        restoreGeometry(settings->value(QLatin1String(Geometry)).toByteArray());

    settings->endGroup();
}

PlainTextEditorDialog::~PlainTextEditorDialog()
{
    QDesignerSettingsInterface *settings = m_core->settingsManager();
    settings->beginGroup(QLatin1String(PlainTextDialogC));

    settings->setValue(QLatin1String(Geometry), saveGeometry());
    settings->endGroup();
}

int PlainTextEditorDialog::showDialog()
{
    m_editor->setFocus();
    return exec();
}

void PlainTextEditorDialog::setDefaultFont(const QFont &font)
{
    m_editor->setFont(font);
}

void PlainTextEditorDialog::setText(const QString &text)
{
    m_editor->setPlainText(text);
}

QString PlainTextEditorDialog::text() const
{
    return m_editor->toPlainText();
}

} // namespace qdesigner_internal

QT_END_NAMESPACE
