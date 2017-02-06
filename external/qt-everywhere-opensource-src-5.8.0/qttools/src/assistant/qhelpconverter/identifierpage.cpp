/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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

#include "identifierpage.h"

QT_BEGIN_NAMESPACE

IdentifierPage::IdentifierPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Identifiers"));
    setSubTitle(tr("This page allows you to create identifiers from "
        "the keywords found in the .adp or .dcf file."));

    m_ui.setupUi(this);

    connect(m_ui.identifierCheckBox, SIGNAL(toggled(bool)),
        this, SLOT(setupButtons(bool)));

    registerField(QLatin1String("createIdentifier"), m_ui.identifierCheckBox);
    registerField(QLatin1String("globalPrefix"), m_ui.prefixLineEdit);
    registerField(QLatin1String("fileNamePrefix"), m_ui.fileNameButton);
}

void IdentifierPage::setupButtons(bool checked)
{
    m_ui.globalButton->setEnabled(checked);
    m_ui.fileNameButton->setEnabled(checked);
    m_ui.prefixLineEdit->setEnabled(checked
        && m_ui.globalButton->isChecked());
}

QT_END_NAMESPACE
