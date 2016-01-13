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

#include "mimeimagerecordeditor.h"
#include "ui_mimeimagerecordeditor.h"

#include <QBuffer>
#include <QFileDialog>
#include <QImageReader>

static QString imageFormatToMimeType(const QByteArray &format)
{
    if (format == "bmp")
        return QStringLiteral("image/bmp");
    else if (format == "gif")
        return QStringLiteral("image/gif");
    else if (format == "jpg" || format == "jpeg")
        return QStringLiteral("image/jpeg");
    else if (format == "mng")
        return QStringLiteral("video/x-mng");
    else if (format == "png")
        return QStringLiteral("image/png");
    else if (format == "pbm")
        return QStringLiteral("image/x-portable-bitmap");
    else if (format == "pgm")
        return QStringLiteral("image/x-portable-graymap");
    else if (format == "ppm")
        return QStringLiteral("image/x-portable-pixmap");
    else if (format == "tiff")
        return QStringLiteral("image/tiff");
    else if (format == "xbm")
        return QStringLiteral("image/x-xbitmap");
    else if (format == "xpm")
        return QStringLiteral("image/x-xpixmap");
    else if (format == "svg")
        return QStringLiteral("image/svg+xml");
    else
        return QString();
}

MimeImageRecordEditor::MimeImageRecordEditor(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MimeImageRecordEditor)
{
    ui->setupUi(this);
}

MimeImageRecordEditor::~MimeImageRecordEditor()
{
    delete ui;
}

void MimeImageRecordEditor::setRecord(const QNdefRecord &record)
{
    m_record = record;

    QByteArray data = record.payload();
    QBuffer buffer(&data);
    buffer.open(QIODevice::ReadOnly);

    QImageReader reader(&buffer);

    ui->mimeImageType->setText(imageFormatToMimeType(reader.format()));

    ui->mimeImageImage->setPixmap(QPixmap::fromImage(reader.read()));
    ui->mimeImageFile->clear();
}

QNdefRecord MimeImageRecordEditor::record() const
{
    return m_record;
}

void MimeImageRecordEditor::on_mimeImageOpen_clicked()
{
    QString mimeDataFile = QFileDialog::getOpenFileName(this, tr("Select Image File"));
    if (mimeDataFile.isEmpty())
        return;

    QFile imageFile(mimeDataFile);
    if (!imageFile.open(QIODevice::ReadOnly)) {
        ui->mimeImageFile->clear();
        ui->mimeImageImage->clear();
    }

    QByteArray imageData = imageFile.readAll();

    QBuffer buffer(&imageData);
    buffer.open(QIODevice::ReadOnly);

    QImageReader reader(&buffer);
    QString mimeType = imageFormatToMimeType(reader.format());
    ui->mimeImageType->setText(mimeType);

    QImage image = reader.read();

    ui->mimeImageFile->setText(mimeDataFile);
    ui->mimeImageImage->setPixmap(QPixmap::fromImage(image));

    m_record.setType(mimeType.toLatin1());
    m_record.setPayload(imageData);
}
