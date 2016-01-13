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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "textrecordeditor.h"
#include "urirecordeditor.h"
#include "mimeimagerecordeditor.h"

#include <QtCore/QTime>
#include <QMenu>
#include <QVBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QFileDialog>

#include <qnearfieldmanager.h>
#include <qnearfieldtarget.h>
#include <qndefrecord.h>
#include <qndefnfctextrecord.h>
#include <qndefnfcurirecord.h>
#include <qndefmessage.h>

#include <QtCore/QDebug>

class EmptyRecordLabel : public QLabel
{
    Q_OBJECT

public:
    EmptyRecordLabel() : QLabel(tr("Empty Record")) { }
    ~EmptyRecordLabel() { }

    void setRecord(const QNdefRecord &record)
    {
        Q_UNUSED(record);
    }

    QNdefRecord record() const
    {
        return QNdefRecord();
    }
};

class UnknownRecordLabel : public QLabel
{
    Q_OBJECT

public:
    UnknownRecordLabel() : QLabel(tr("Unknown Record Type")) { }
    ~UnknownRecordLabel() { }

    void setRecord(const QNdefRecord &record) { m_record = record; }
    QNdefRecord record() const { return m_record; }

private:
    QNdefRecord m_record;
};

template <typename T>
void addRecord(Ui::MainWindow *ui, const QNdefRecord &record = QNdefRecord())
{
    QVBoxLayout *vbox = qobject_cast<QVBoxLayout *>(ui->scrollAreaWidgetContents->layout());
    if (!vbox)
        return;

    if (!vbox->isEmpty()) {
        QFrame *hline = new QFrame;
        hline->setFrameShape(QFrame::HLine);
        hline->setObjectName(QStringLiteral("line-spacer"));

        vbox->addWidget(hline);
    }

    T *recordEditor = new T;
    recordEditor->setObjectName(QStringLiteral("record-editor"));

    if (!record.isEmpty())
        recordEditor->setRecord(record);

    vbox->addWidget(recordEditor);
}

MainWindow::MainWindow(QWidget *parent)
:   QMainWindow(parent), ui(new Ui::MainWindow), m_touchAction(NoAction)
{
    ui->setupUi(this);

    QMenu *addRecordMenu = new QMenu(this);
    addRecordMenu->addAction(tr("NFC Text Record"), this, SLOT(addNfcTextRecord()));
    addRecordMenu->addAction(tr("NFC URI Record"), this, SLOT(addNfcUriRecord()));
    addRecordMenu->addAction(tr("MIME Image Record"), this, SLOT(addMimeImageRecord()));
    addRecordMenu->addAction(tr("Empty Record"), this, SLOT(addEmptyRecord()));
    ui->addRecord->setMenu(addRecordMenu);

    QVBoxLayout *vbox = new QVBoxLayout;
    ui->scrollAreaWidgetContents->setLayout(vbox);

    //! [QNearFieldManager init]
    m_manager = new QNearFieldManager(this);
    connect(m_manager, SIGNAL(targetDetected(QNearFieldTarget*)),
            this, SLOT(targetDetected(QNearFieldTarget*)));
    connect(m_manager, SIGNAL(targetLost(QNearFieldTarget*)),
            this, SLOT(targetLost(QNearFieldTarget*)));
    //! [QNearFieldManager init]
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::addNfcTextRecord()
{
    addRecord<TextRecordEditor>(ui);
}

void MainWindow::addNfcUriRecord()
{
    addRecord<UriRecordEditor>(ui);
}

void MainWindow::addMimeImageRecord()
{
    addRecord<MimeImageRecordEditor>(ui);
}

void MainWindow::addEmptyRecord()
{
    addRecord<EmptyRecordLabel>(ui);
}

void MainWindow::loadMessage()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Select NDEF Message"));
    if (filename.isEmpty())
        return;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QByteArray ndef = file.readAll();

    ndefMessageRead(QNdefMessage::fromByteArray(ndef));

    file.close();
}

void MainWindow::saveMessage()
{
    QString filename = QFileDialog::getSaveFileName(this, tr("Select NDEF Message"));
    if (filename.isEmpty())
        return;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
        return;

    file.write(ndefMessage().toByteArray());

    file.close();
}

void MainWindow::touchReceive()
{
    ui->status->setStyleSheet(QStringLiteral("background: blue"));

    m_touchAction = ReadNdef;

    m_manager->setTargetAccessModes(QNearFieldManager::NdefReadTargetAccess);
    //! [QNearFieldManager start detection]
    m_manager->startTargetDetection();
    //! [QNearFieldManager start detection]
}

void MainWindow::touchStore()
{
    ui->status->setStyleSheet(QStringLiteral("background: yellow"));

    m_touchAction = WriteNdef;

    m_manager->setTargetAccessModes(QNearFieldManager::NdefWriteTargetAccess);
    m_manager->startTargetDetection();
}

//! [QNearFieldTarget detected]
void MainWindow::targetDetected(QNearFieldTarget *target)
{
    switch (m_touchAction) {
    case NoAction:
        break;
    case ReadNdef:
        connect(target, SIGNAL(ndefMessageRead(QNdefMessage)),
                this, SLOT(ndefMessageRead(QNdefMessage)));
        connect(target, SIGNAL(error(QNearFieldTarget::Error,QNearFieldTarget::RequestId)),
                this, SLOT(targetError(QNearFieldTarget::Error,QNearFieldTarget::RequestId)));

        m_request = target->readNdefMessages();
        break;
    case WriteNdef:
        connect(target, SIGNAL(ndefMessagesWritten()), this, SLOT(ndefMessageWritten()));
        connect(target, SIGNAL(error(QNearFieldTarget::Error,QNearFieldTarget::RequestId)),
                this, SLOT(targetError(QNearFieldTarget::Error,QNearFieldTarget::RequestId)));

        m_request = target->writeNdefMessages(QList<QNdefMessage>() << ndefMessage());
        break;
    }
}
//! [QNearFieldTarget detected]

//! [QNearFieldTarget lost]
void MainWindow::targetLost(QNearFieldTarget *target)
{
    target->deleteLater();
}
//! [QNearFieldTarget lost]

void MainWindow::ndefMessageRead(const QNdefMessage &message)
{
    clearMessage();

    foreach (const QNdefRecord &record, message) {
        if (record.isRecordType<QNdefNfcTextRecord>()) {
            addRecord<TextRecordEditor>(ui, record);
        } else if (record.isRecordType<QNdefNfcUriRecord>()) {
            addRecord<UriRecordEditor>(ui, record);
        } else if (record.typeNameFormat() == QNdefRecord::Mime &&
                   record.type().startsWith("image/")) {
            addRecord<MimeImageRecordEditor>(ui, record);
        } else if (record.isEmpty()) {
            addRecord<EmptyRecordLabel>(ui);
        } else {
            addRecord<UnknownRecordLabel>(ui, record);
        }
    }

    ui->status->setStyleSheet(QString());
    m_manager->setTargetAccessModes(QNearFieldManager::NoTargetAccess);
    //! [QNearFieldManager stop detection]
    m_manager->stopTargetDetection();
    //! [QNearFieldManager stop detection]
    m_request = QNearFieldTarget::RequestId();
    ui->statusBar->clearMessage();
}

void MainWindow::ndefMessageWritten()
{
    ui->status->setStyleSheet(QString());
    m_manager->setTargetAccessModes(QNearFieldManager::NoTargetAccess);
    m_manager->stopTargetDetection();
    m_request = QNearFieldTarget::RequestId();
    ui->statusBar->clearMessage();
}

void MainWindow::targetError(QNearFieldTarget::Error error, const QNearFieldTarget::RequestId &id)
{
    Q_UNUSED(error);
    Q_UNUSED(id);

    if (m_request == id) {
        switch (error) {
        case QNearFieldTarget::NoError:
            ui->statusBar->clearMessage();
            break;
        case QNearFieldTarget::UnsupportedError:
            ui->statusBar->showMessage(tr("Unsupported tag"));
            break;
        case QNearFieldTarget::TargetOutOfRangeError:
            ui->statusBar->showMessage(tr("Tag removed from field"));
            break;
        case QNearFieldTarget::NoResponseError:
            ui->statusBar->showMessage(tr("No response from tag"));
            break;
        case QNearFieldTarget::ChecksumMismatchError:
            ui->statusBar->showMessage(tr("Checksum mismatch"));
            break;
        case QNearFieldTarget::InvalidParametersError:
            ui->statusBar->showMessage(tr("Invalid parameters"));
            break;
        case QNearFieldTarget::NdefReadError:
            ui->statusBar->showMessage(tr("NDEF read error"));
            break;
        case QNearFieldTarget::NdefWriteError:
            ui->statusBar->showMessage(tr("NDEF write error"));
            break;
        default:
            ui->statusBar->showMessage(tr("Unknown error"));
        }

        ui->status->setStyleSheet(QString());
        m_manager->setTargetAccessModes(QNearFieldManager::NoTargetAccess);
        m_manager->stopTargetDetection();
        m_request = QNearFieldTarget::RequestId();
    }
}

void MainWindow::clearMessage()
{
    QWidget *scrollArea = ui->scrollAreaWidgetContents;

    qDeleteAll(scrollArea->findChildren<QWidget *>(QStringLiteral("line-spacer")));
    qDeleteAll(scrollArea->findChildren<QWidget *>(QStringLiteral("record-editor")));
}

QNdefMessage MainWindow::ndefMessage() const
{
    QVBoxLayout *vbox = qobject_cast<QVBoxLayout *>(ui->scrollAreaWidgetContents->layout());
    if (!vbox)
        return QNdefMessage();

    QNdefMessage message;

    for (int i = 0; i < vbox->count(); ++i) {
        QWidget *widget = vbox->itemAt(i)->widget();

        if (TextRecordEditor *editor = qobject_cast<TextRecordEditor *>(widget)) {
            message.append(editor->record());
        } else if (UriRecordEditor *editor = qobject_cast<UriRecordEditor *>(widget)) {
            message.append(editor->record());
        } else if (MimeImageRecordEditor *editor = qobject_cast<MimeImageRecordEditor *>(widget)) {
            message.append(editor->record());
        } else if (qobject_cast<EmptyRecordLabel *>(widget)) {
            message.append(QNdefRecord());
        } else if (UnknownRecordLabel *label = qobject_cast<UnknownRecordLabel *>(widget)) {
            message.append(label->record());
        }
    }

    return message;
}

#include "mainwindow.moc"
