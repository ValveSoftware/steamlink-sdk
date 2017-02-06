/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include "progress.h"
#include "ui_progress.h"

#include <qbluetoothdeviceinfo.h>
#include <qbluetoothaddress.h>
#include <qbluetoothtransferrequest.h>
#include <qbluetoothtransferreply.h>

QT_USE_NAMESPACE

Progress::Progress(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Progress)
{
    ui->setupUi(this);
    ui->progressBar->setRange(0, 1);
    connect(ui->cancelButton, SIGNAL(clicked()), this, SLOT(close()));
}

Progress::~Progress()
{
    delete ui;
}

void Progress::setStatus(QString title, QString filename) {
    ui->titleLabel->setText(title);
    ui->statusLabel->setText(filename);
}

void Progress::finished(QBluetoothTransferReply *reply){
    if (reply->error() != QBluetoothTransferReply::NoError){
        ui->progressBar->setDisabled(true);
        ui->statusLabel->setText(tr("Failed: %1").arg(reply->errorString()));
    }
    else {
        ui->statusLabel->setText(tr("Transfer complete"));
    }
    ui->cancelButton->setText(tr("Dismiss"));
}

void Progress::uploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
    if (bytesSent == 0){
        start.start();
    }

    ui->progressBar->setMaximum(bytesTotal);
    ui->progressBar->setValue(bytesSent);
    if (bytesSent && bytesTotal &&
            (start.elapsed() > 1000) &&
            (bytesSent > start.elapsed()/1000)) {

        ui->statusLabel->setText(tr("Transferring...ETA: %1s")
                .arg(((bytesTotal-bytesSent)/(bytesSent/(start.elapsed()/1000)))));
    }
}

