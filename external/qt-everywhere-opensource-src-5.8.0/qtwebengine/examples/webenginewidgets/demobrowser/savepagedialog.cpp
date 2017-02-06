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

#include "savepagedialog.h"
#include "ui_savepagedialog.h"

#include <QtCore/QDir>
#include <QtWidgets/QFileDialog>

const QWebEngineDownloadItem::SavePageFormat SavePageDialog::m_indexToFormatTable[] = {
    QWebEngineDownloadItem::SingleHtmlSaveFormat,
    QWebEngineDownloadItem::CompleteHtmlSaveFormat,
    QWebEngineDownloadItem::MimeHtmlSaveFormat
};

SavePageDialog::SavePageDialog(QWidget *parent, QWebEngineDownloadItem::SavePageFormat format,
                               const QString &filePath)
    : QDialog(parent)
    , ui(new Ui::SavePageDialog)
{
    ui->setupUi(this);
    ui->formatComboBox->setCurrentIndex(formatToIndex(format));
    setFilePath(filePath);
}

SavePageDialog::~SavePageDialog()
{
    delete ui;
}

QWebEngineDownloadItem::SavePageFormat SavePageDialog::pageFormat() const
{
    return indexToFormat(ui->formatComboBox->currentIndex());
}

QString SavePageDialog::filePath() const
{
    return QDir::fromNativeSeparators(ui->filePathLineEdit->text());
}

void SavePageDialog::on_chooseFilePathButton_clicked()
{
    QFileInfo fi(filePath());
    QFileDialog dlg(this, tr("Save Page As"), fi.absolutePath());
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setDefaultSuffix(suffixOfFormat(pageFormat()));
    dlg.selectFile(fi.absoluteFilePath());
    if (dlg.exec() != QDialog::Accepted)
        return;
    setFilePath(dlg.selectedFiles().first());
    ensureFileSuffix(pageFormat());
}

void SavePageDialog::on_formatComboBox_currentIndexChanged(int idx)
{
    ensureFileSuffix(indexToFormat(idx));
}

int SavePageDialog::formatToIndex(QWebEngineDownloadItem::SavePageFormat format)
{
    for (auto i : m_indexToFormatTable) {
        if (m_indexToFormatTable[i] == format)
            return i;
    }
    Q_UNREACHABLE();
}

QWebEngineDownloadItem::SavePageFormat SavePageDialog::indexToFormat(int idx)
{
    Q_ASSERT(idx >= 0 && size_t(idx) < (sizeof(m_indexToFormatTable)
                                        / sizeof(QWebEngineDownloadItem::SavePageFormat)));
    return m_indexToFormatTable[idx];
}

QString SavePageDialog::suffixOfFormat(QWebEngineDownloadItem::SavePageFormat format)
{
    if (format == QWebEngineDownloadItem::MimeHtmlSaveFormat)
        return QStringLiteral(".mhtml");
    return QStringLiteral(".html");
}

void SavePageDialog::setFilePath(const QString &filePath)
{
    ui->filePathLineEdit->setText(QDir::toNativeSeparators(filePath));
}

void SavePageDialog::ensureFileSuffix(QWebEngineDownloadItem::SavePageFormat format)
{
    QFileInfo fi(filePath());
    setFilePath(fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName()
                + suffixOfFormat(format));
}
