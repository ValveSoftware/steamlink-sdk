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

#include <QtWidgets/QMessageBox>
#include "generalpage.h"

QT_BEGIN_NAMESPACE

GeneralPage::GeneralPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("General Settings"));
    setSubTitle(tr("Specify the namespace and the virtual "
        "folder for the documentation."));

    m_ui.setupUi(this);
    connect(m_ui.namespaceLineEdit, SIGNAL(textChanged(QString)),
        this, SIGNAL(completeChanged()));
    connect(m_ui.folderLineEdit, SIGNAL(textChanged(QString)),
        this, SIGNAL(completeChanged()));

    m_ui.namespaceLineEdit->setText(QLatin1String("mycompany.com"));
    m_ui.folderLineEdit->setText(QLatin1String("product_1.0"));

    registerField(QLatin1String("namespaceName"), m_ui.namespaceLineEdit);
    registerField(QLatin1String("virtualFolder"), m_ui.folderLineEdit);
}

bool GeneralPage::isComplete() const
{
    if (m_ui.namespaceLineEdit->text().isEmpty()
        || m_ui.folderLineEdit->text().isEmpty())
        return false;
    return true;
}

bool GeneralPage::validatePage()
{
    QString s = m_ui.namespaceLineEdit->text();
    if (s.contains(QLatin1Char('/')) || s.contains(QLatin1Char('\\'))) {
        QMessageBox::critical(this, tr("Namespace Error"),
            tr("The namespace contains some invalid characters."));
        return false;
    }
    s = m_ui.folderLineEdit->text();
    if (s.contains(QLatin1Char('/')) || s.contains(QLatin1Char('\\'))) {
        QMessageBox::critical(this, tr("Virtual Folder Error"),
            tr("The virtual folder contains some invalid characters."));
        return false;
    }
    return true;
}

QT_END_NAMESPACE
