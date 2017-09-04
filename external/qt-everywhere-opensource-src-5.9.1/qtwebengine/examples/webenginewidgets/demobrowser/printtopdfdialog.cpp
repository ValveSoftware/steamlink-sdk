/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "printtopdfdialog.h"
#include "ui_printtopdfdialog.h"

#include <QtCore/QDir>
#ifndef QT_NO_PRINTER
#include <QtPrintSupport/QPageSetupDialog>
#include <QtPrintSupport/QPrinter>
#endif // QT_NO_PRINTER
#include <QtWidgets/QFileDialog>

PrintToPdfDialog::PrintToPdfDialog(const QString &filePath, QWidget *parent) :
    QDialog(parent),
    currentPageLayout(QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Portrait, QMarginsF(0.0, 0.0, 0.0, 0.0))),
    ui(new Ui::PrintToPdfDialog)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    connect(ui->chooseFilePathButton, &QToolButton::clicked, this, &PrintToPdfDialog::onChooseFilePathButtonClicked);
#ifndef QT_NO_PRINTER
    connect(ui->choosePageLayoutButton, &QToolButton::clicked, this, &PrintToPdfDialog::onChoosePageLayoutButtonClicked);
#else
    ui->choosePageLayoutButton->hide();
#endif // QT_NO_PRINTER
    updatePageLayoutLabel();
    setFilePath(filePath);
}

PrintToPdfDialog::~PrintToPdfDialog()
{
    delete ui;
}

void PrintToPdfDialog::onChoosePageLayoutButtonClicked()
{
#ifndef QT_NO_PRINTER
    QPrinter printer;
    printer.setPageLayout(currentPageLayout);

    QPageSetupDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    currentPageLayout.setPageSize(printer.pageLayout().pageSize());
    currentPageLayout.setOrientation(printer.pageLayout().orientation());
    updatePageLayoutLabel();
#endif // QT_NO_PRINTER
}

void PrintToPdfDialog::onChooseFilePathButtonClicked()
{
    QFileInfo fi(filePath());
    QFileDialog dlg(this, tr("Save PDF as"), fi.absolutePath());
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setDefaultSuffix(QStringLiteral(".pdf"));
    dlg.selectFile(fi.absoluteFilePath());
    if (dlg.exec() != QDialog::Accepted)
        return;
    setFilePath(dlg.selectedFiles().first());
}

QString PrintToPdfDialog::filePath() const
{
    return QDir::fromNativeSeparators(ui->filePathLineEdit->text());
}

void PrintToPdfDialog::setFilePath(const QString &filePath)
{
    ui->filePathLineEdit->setText(QDir::toNativeSeparators(filePath));
}

QPageLayout PrintToPdfDialog::pageLayout() const
{
    return currentPageLayout;
}

void PrintToPdfDialog::updatePageLayoutLabel()
{
    ui->pageLayoutLabel->setText(QString("%1, %2").arg(
                                   currentPageLayout.pageSize().name()).arg(
                                   currentPageLayout.orientation() == QPageLayout::Portrait
                                   ? tr("Portrait") : tr("Landscape")
                                   ));
}
