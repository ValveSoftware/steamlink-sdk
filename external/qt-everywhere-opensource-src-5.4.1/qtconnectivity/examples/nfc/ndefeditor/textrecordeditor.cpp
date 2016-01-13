/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtNfc module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

#include "textrecordeditor.h"
#include "ui_textrecordeditor.h"

#include <QtCore/QDebug>

TextRecordEditor::TextRecordEditor(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TextRecordEditor)
{
    ui->setupUi(this);
}

TextRecordEditor::~TextRecordEditor()
{
    delete ui;
}

void TextRecordEditor::setRecord(const QNdefNfcTextRecord &textRecord)
{
    ui->text->setText(textRecord.text());
    ui->locale->setText(textRecord.locale());

    if (textRecord.encoding() == QNdefNfcTextRecord::Utf8)
        ui->encoding->setCurrentIndex(0);
    else if (textRecord.encoding() == QNdefNfcTextRecord::Utf16)
        ui->encoding->setCurrentIndex(1);
}

QNdefNfcTextRecord TextRecordEditor::record() const
{
    QNdefNfcTextRecord record;

    if (ui->encoding->currentIndex() == 0)
        record.setEncoding(QNdefNfcTextRecord::Utf8);
    else if (ui->encoding->currentIndex() == 1)
        record.setEncoding(QNdefNfcTextRecord::Utf16);

    record.setLocale(ui->locale->text());

    record.setText(ui->text->text());

    return record;
}
