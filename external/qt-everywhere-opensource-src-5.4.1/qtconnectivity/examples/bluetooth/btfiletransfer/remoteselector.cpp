/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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

#include "remoteselector.h"
#include "ui_remoteselector.h"

#include <qbluetoothdeviceinfo.h>
#include <qbluetoothaddress.h>
#include <qbluetoothtransferrequest.h>
#include <qbluetoothtransferreply.h>
#include <qbluetoothlocaldevice.h>

#include <QMovie>
#include <QMessageBox>
#include <QFileDialog>
#include <QCheckBox>

#include "progress.h"
#include "pindisplay.h"

QT_USE_NAMESPACE

RemoteSelector::RemoteSelector(QWidget *parent)
:   QDialog(parent), ui(new Ui::RemoteSelector),
    m_localDevice(new QBluetoothLocalDevice), m_pindisplay(0),
    m_pairingError(false)
{
    ui->setupUi(this);

    //Using default Bluetooth adapter
    QBluetoothAddress adapterAddress = m_localDevice->address();

    /*
     * In case of multiple Bluetooth adapters it is possible to
     * set which adapter will be used by providing MAC Address.
     * Example code:
     *
     * QBluetoothAddress adapterAddress("XX:XX:XX:XX:XX:XX");
     * m_discoveryAgent = new QBluetoothServiceDiscoveryAgent(adapterAddress);
     */

    m_discoveryAgent = new QBluetoothServiceDiscoveryAgent(adapterAddress);

    connect(m_discoveryAgent, SIGNAL(serviceDiscovered(QBluetoothServiceInfo)),
            this, SLOT(serviceDiscovered(QBluetoothServiceInfo)));
    connect(m_discoveryAgent, SIGNAL(finished()), this, SLOT(discoveryFinished()));
    connect(m_discoveryAgent, SIGNAL(canceled()), this, SLOT(discoveryFinished()));

    ui->remoteDevices->setColumnWidth(3, 75);
    ui->remoteDevices->setColumnWidth(4, 100);

    connect(m_localDevice, SIGNAL(pairingDisplayPinCode(QBluetoothAddress,QString)),
            this, SLOT(displayPin(QBluetoothAddress,QString)));
    connect(m_localDevice, SIGNAL(pairingDisplayConfirmation(QBluetoothAddress,QString)),
            this, SLOT(displayConfirmation(QBluetoothAddress,QString)));
    connect(m_localDevice, SIGNAL(pairingFinished(QBluetoothAddress,QBluetoothLocalDevice::Pairing)),
            this, SLOT(pairingFinished(QBluetoothAddress,QBluetoothLocalDevice::Pairing)));
    connect(m_localDevice, SIGNAL(error(QBluetoothLocalDevice::Error)),
            this, SLOT(pairingError(QBluetoothLocalDevice::Error)));

    ui->busyWidget->setMovie(new QMovie(":/icons/busy.gif"));
    ui->busyWidget->movie()->start();

    ui->pairingBusy->setMovie(new QMovie(":/icons/pairing.gif"));
    ui->pairingBusy->hide();

    ui->remoteDevices->clearContents();
    ui->remoteDevices->setRowCount(0);
}

RemoteSelector::~RemoteSelector()
{
    delete ui;
    delete m_discoveryAgent;
    delete m_localDevice;
}

void RemoteSelector::startDiscovery(const QBluetoothUuid &uuid)
{
    ui->stopButton->setDisabled(false);
    if (m_discoveryAgent->isActive())
        m_discoveryAgent->stop();

    m_discoveryAgent->setUuidFilter(uuid);
    m_discoveryAgent->start();

    if (!m_discoveryAgent->isActive() ||
            m_discoveryAgent->error() != QBluetoothServiceDiscoveryAgent::NoError) {
        ui->status->setText(tr("Cannot find remote services."));
    } else {
        ui->status->setText(tr("Scanning..."));
        ui->busyWidget->show();
        ui->busyWidget->movie()->start();
    }
}

QBluetoothServiceInfo RemoteSelector::service() const
{
    return m_service;
}

void RemoteSelector::serviceDiscovered(const QBluetoothServiceInfo &serviceInfo)
{
#if 0
    qDebug() << "Discovered service on"
             << serviceInfo.device().name() << serviceInfo.device().address().toString();
    qDebug() << "\tService name:" << serviceInfo.serviceName();
    qDebug() << "\tDescription:"
             << serviceInfo.attribute(QBluetoothServiceInfo::ServiceDescription).toString();
    qDebug() << "\tProvider:"
             << serviceInfo.attribute(QBluetoothServiceInfo::ServiceProvider).toString();
    qDebug() << "\tL2CAP protocol service multiplexer:"
             << serviceInfo.protocolServiceMultiplexer();
    qDebug() << "\tRFCOMM server channel:" << serviceInfo.serverChannel();
#endif

    QString remoteName;
    if (serviceInfo.device().name().isEmpty())
        remoteName = serviceInfo.device().address().toString();
    else
        remoteName = serviceInfo.device().name();

//    QListWidgetItem *item =
//        new QListWidgetItem(QString::fromLatin1("%1\t%2\t%3").arg(serviceInfo.device().address().toString(),
//                                                             serviceInfo.device().name(), serviceInfo.serviceName()));

    QMutableMapIterator<int, QBluetoothServiceInfo> i(m_discoveredServices);
    while (i.hasNext()){
        i.next();
        if (serviceInfo.device().address() == i.value().device().address()){
            i.setValue(serviceInfo);
            return;
        }
    }

    int row = ui->remoteDevices->rowCount();
    ui->remoteDevices->insertRow(row);
    QTableWidgetItem *item = new QTableWidgetItem(serviceInfo.device().address().toString());
    ui->remoteDevices->setItem(row, 0, item);
    item = new QTableWidgetItem(serviceInfo.device().name());
    ui->remoteDevices->setItem(row, 1, item);
    item = new QTableWidgetItem(serviceInfo.serviceName());

    ui->remoteDevices->setItem(row, 2, item);

    QBluetoothLocalDevice::Pairing p;

    p = m_localDevice->pairingStatus(serviceInfo.device().address());

    ui->remoteDevices->blockSignals(true);

    item = new QTableWidgetItem();
    if ((p&QBluetoothLocalDevice::Paired) || (p&QBluetoothLocalDevice::AuthorizedPaired))
        item->setCheckState(Qt::Checked);
    else
        item->setCheckState(Qt::Unchecked);
    ui->remoteDevices->setItem(row, 3, item);

    item = new QTableWidgetItem();
    if (p&QBluetoothLocalDevice::AuthorizedPaired)
        item->setCheckState(Qt::Checked);
    else
        item->setCheckState(Qt::Unchecked);

    ui->remoteDevices->setItem(row, 4, item);

    ui->remoteDevices->blockSignals(false);


    m_discoveredServices.insert(row, serviceInfo);
}

void RemoteSelector::discoveryFinished()
{
    ui->status->setText(tr("Select the device to send to."));
    ui->stopButton->setDisabled(true);
    ui->busyWidget->movie()->stop();
    ui->busyWidget->hide();
}

void RemoteSelector::startDiscovery()
{
    startDiscovery(QBluetoothUuid(QBluetoothUuid::ObexObjectPush));
}

void RemoteSelector::on_refreshPB_clicked()
{
    startDiscovery();
    ui->stopButton->setDisabled(false);
}

void RemoteSelector::on_fileSelectPB_clicked()
{
    ui->fileName->setText(QFileDialog::getOpenFileName());
    if (m_service.isValid())
        ui->sendButton->setDisabled(false);
}

void RemoteSelector::on_sendButton_clicked()
{
    QBluetoothTransferManager mgr;
    QBluetoothTransferRequest req(m_service.device().address());

    m_file = new QFile(ui->fileName->text());

    Progress *p = new Progress;
    p->setStatus("Sending to: " + m_service.device().name(), "Waiting for start");
    p->show();

    QBluetoothTransferReply *reply = mgr.put(req, m_file);
    //mgr is default parent
    //ensure that mgr doesn't take reply down when leaving scope
    reply->setParent(this);
    if (reply->error()){
        qDebug() << "Failed to send file";
        p->finished(reply);
        reply->deleteLater();
        return;
    }

    connect(reply, SIGNAL(transferProgress(qint64,qint64)), p, SLOT(uploadProgress(qint64,qint64)));
    connect(reply, SIGNAL(finished(QBluetoothTransferReply*)), p, SLOT(finished(QBluetoothTransferReply*)));
    connect(p, SIGNAL(rejected()), reply, SLOT(abort()));
}

void RemoteSelector::on_stopButton_clicked()
{
    m_discoveryAgent->stop();
}

QString RemoteSelector::addressToName(const QBluetoothAddress &address)
{
    QMapIterator<int, QBluetoothServiceInfo> i(m_discoveredServices);
    while (i.hasNext()){
        i.next();
        if (i.value().device().address() == address)
            return i.value().device().name();
    }
    return address.toString();
}

void RemoteSelector::displayPin(const QBluetoothAddress &address, QString pin)
{
    if (m_pindisplay)
        m_pindisplay->deleteLater();
    m_pindisplay = new pinDisplay(QString("Enter pairing pin on: %1").arg(addressToName(address)), pin, this);
    m_pindisplay->show();
}

void RemoteSelector::displayConfirmation(const QBluetoothAddress &address, QString pin)
{
    Q_UNUSED(address);

    if (m_pindisplay)
        m_pindisplay->deleteLater();
    m_pindisplay = new pinDisplay(QString("Confirm this pin is the same"), pin, this);
    connect(m_pindisplay, SIGNAL(accepted()), this, SLOT(displayConfAccepted()));
    connect(m_pindisplay, SIGNAL(rejected()), this, SLOT(displayConfReject()));
    m_pindisplay->setOkCancel();
    m_pindisplay->show();
}

void RemoteSelector::displayConfAccepted()
{
    m_localDevice->pairingConfirmation(true);
}
void RemoteSelector::displayConfReject()
{
    m_localDevice->pairingConfirmation(false);
}

void RemoteSelector::pairingFinished(const QBluetoothAddress &address, QBluetoothLocalDevice::Pairing status)
{
    QBluetoothServiceInfo service;
    int row = 0;

    ui->pairingBusy->hide();
    ui->pairingBusy->movie()->stop();

    ui->remoteDevices->blockSignals(true);

    for (int i = 0; i < m_discoveredServices.count(); i++){
        if (m_discoveredServices.value(i).device().address() == address){
            service = m_discoveredServices.value(i);
            row = i;
            break;
        }
    }

    if (m_pindisplay)
        delete m_pindisplay;

    QMessageBox msgBox;
    if (m_pairingError) {
        msgBox.setText("Pairing failed with " + address.toString());
    } else if (status == QBluetoothLocalDevice::Paired
               || status == QBluetoothLocalDevice::AuthorizedPaired) {
        msgBox.setText("Paired successfully with " + address.toString());
    } else {
        msgBox.setText("Pairing released with " + address.toString());
    }

    if (service.isValid()){
        if (status == QBluetoothLocalDevice::AuthorizedPaired){
            ui->remoteDevices->item(row, 3)->setCheckState(Qt::Checked);
            ui->remoteDevices->item(row, 4)->setCheckState(Qt::Checked);
        }
        else if (status == QBluetoothLocalDevice::Paired){
            ui->remoteDevices->item(row, 3)->setCheckState(Qt::Checked);
            ui->remoteDevices->item(row, 4)->setCheckState(Qt::Unchecked);
        }
        else {
            ui->remoteDevices->item(row, 3)->setCheckState(Qt::Unchecked);
            ui->remoteDevices->item(row, 4)->setCheckState(Qt::Unchecked);
        }
    }

    m_pairingError = false;
    msgBox.exec();

    ui->remoteDevices->blockSignals(false);
}

void RemoteSelector::pairingError(QBluetoothLocalDevice::Error error)
{
    if (error != QBluetoothLocalDevice::PairingError)
        return;

    m_pairingError = true;
    pairingFinished(m_service.device().address(), QBluetoothLocalDevice::Unpaired);
}

void RemoteSelector::on_remoteDevices_cellClicked(int row, int column)
{
    Q_UNUSED(column);

    m_service = m_discoveredServices.value(row);
    if (!ui->fileName->text().isEmpty()) {
        ui->sendButton->setDisabled(false);
    }
}

void RemoteSelector::on_remoteDevices_itemChanged(QTableWidgetItem* item)
{
    int row = item->row();
    int column = item->column();
    m_service = m_discoveredServices.value(row);

    if (column < 3)
        return;

    if (item->checkState() == Qt::Unchecked && column == 3){
        m_localDevice->requestPairing(m_service.device().address(), QBluetoothLocalDevice::Unpaired);
        return; // don't continue and start movie
    }
    else if ((item->checkState() == Qt::Checked && column == 3) ||
            (item->checkState() == Qt::Unchecked && column == 4)){
        m_localDevice->requestPairing(m_service.device().address(), QBluetoothLocalDevice::Paired);
        ui->remoteDevices->blockSignals(true);
        ui->remoteDevices->item(row, column)->setCheckState(Qt::PartiallyChecked);
        ui->remoteDevices->blockSignals(false);
    }
    else if (item->checkState() == Qt::Checked && column == 4){
        m_localDevice->requestPairing(m_service.device().address(), QBluetoothLocalDevice::AuthorizedPaired);
        ui->remoteDevices->blockSignals(true);
        ui->remoteDevices->item(row, column)->setCheckState(Qt::PartiallyChecked);
        ui->remoteDevices->blockSignals(false);
    }
    ui->pairingBusy->show();
    ui->pairingBusy->movie()->start();
}
