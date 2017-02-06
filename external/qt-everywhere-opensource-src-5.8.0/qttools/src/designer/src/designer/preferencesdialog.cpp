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

#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"
#include "qdesigner_appearanceoptions.h"

#include <QtDesigner/QDesignerOptionsPageInterface>
#include <QtDesigner/QDesignerFormEditorInterface>

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QPushButton>

QT_BEGIN_NAMESPACE

PreferencesDialog::PreferencesDialog(QDesignerFormEditorInterface *core, QWidget *parentWidget) :
    QDialog(parentWidget),
    m_ui(new Ui::PreferencesDialog()),
    m_core(core)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    m_ui->setupUi(this);

    m_optionsPages = core->optionsPages();

    m_ui->m_optionTabWidget->clear();
    foreach (QDesignerOptionsPageInterface *optionsPage, m_optionsPages) {
        QWidget *page = optionsPage->createPage(this);
        m_ui->m_optionTabWidget->addTab(page, optionsPage->name());
        if (QDesignerAppearanceOptionsWidget *appearanceWidget = qobject_cast<QDesignerAppearanceOptionsWidget *>(page))
            connect(appearanceWidget, &QDesignerAppearanceOptionsWidget::uiModeChanged,
                    this, &PreferencesDialog::slotUiModeChanged);
    }

    connect(m_ui->m_dialogButtonBox, &QDialogButtonBox::rejected, this, &PreferencesDialog::slotRejected);
    connect(m_ui->m_dialogButtonBox, &QDialogButtonBox::accepted, this, &PreferencesDialog::slotAccepted);
    connect(applyButton(), &QAbstractButton::clicked, this, &PreferencesDialog::slotApply);
}

PreferencesDialog::~PreferencesDialog()
{
    delete m_ui;
}

QPushButton *PreferencesDialog::applyButton() const
{
    return m_ui->m_dialogButtonBox->button(QDialogButtonBox::Apply);
}

void PreferencesDialog::slotApply()
{
    foreach (QDesignerOptionsPageInterface *optionsPage, m_optionsPages)
        optionsPage->apply();
}

void PreferencesDialog::slotAccepted()
{
    slotApply();
    closeOptionPages();
    accept();
}

void PreferencesDialog::slotRejected()
{
    closeOptionPages();
    reject();
}

void PreferencesDialog::slotUiModeChanged(bool modified)
{
    // Cannot "apply" a ui mode change (destroy the dialogs parent)
    applyButton()->setEnabled(!modified);
}

void PreferencesDialog::closeOptionPages()
{
    foreach (QDesignerOptionsPageInterface *optionsPage, m_optionsPages)
        optionsPage->finish();
}

QT_END_NAMESPACE
