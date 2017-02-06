/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the QtSerialBus module.
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

#include "mainwindow.h"
#include "modbustcpclient.h"

#include <QLoggingCategory>
#include <QModbusRtuSerialMaster>
#include <QModbusPdu>
#include <QSerialPortInfo>

#ifndef QT_STATIC
 QT_BEGIN_NAMESPACE
 Q_LOGGING_CATEGORY(QT_MODBUS, "qt.modbus")
 Q_LOGGING_CATEGORY(QT_MODBUS_LOW, "qt.modbus.lowlevel")
 QT_END_NAMESPACE
#endif

QT_USE_NAMESPACE

MainWindow *s_instance = nullptr;

static void HandlerFunction(QtMsgType, const QMessageLogContext &, const QString &msg)
{
    if (auto instance = MainWindow::instance())
        instance->appendToLog(msg);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_debugHandler(HandlerFunction)
{
    setupUi(this);
    s_instance = this;

    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
        serialPortCombo->addItem(info.portName(), false);
    serialPortCombo->insertSeparator(serialPortCombo->count());
    serialPortCombo->addItem(QStringLiteral("Add port..."), true);
    serialPortCombo->setInsertPolicy(QComboBox::InsertAtTop);

    connect(tcpRadio, &QRadioButton::toggled, this, [this](bool toggled) {
        stackedWidget->setCurrentIndex(toggled);
    });
    connect(actionExit, &QAction::triggered, this, &QMainWindow::close);

    QLoggingCategory::setFilterRules(QStringLiteral("qt.modbus* = true"));
}

MainWindow::~MainWindow()
{
    disconnectAndDelete();
    s_instance = nullptr;
}

MainWindow *MainWindow::instance()
{
    return s_instance;
}

void MainWindow::on_sendButton_clicked()
{
    const bool isSerial = serialRadio->isChecked();
    const bool isCustom = (isSerial ? fcSerialDrop : fcTcpDrop)->currentIndex() == 0;
    const QByteArray pduData = QByteArray::fromHex((isSerial ? pduSerialLine : pduTcpLine)->text()
        .toLatin1());

    QModbusReply *reply = nullptr;
    if (isCustom && pduData.isEmpty()) {
        qDebug() << "Error: Cannot send custom PDU without any data.";
        return;
    }

    const quint8 address = (isSerial ? addressSpin : ui1Spin)->value();
    if (isCustom) {
        qDebug() << "Send: Sending custom PDU.";
        reply = m_device->sendRawRequest(QModbusRequest(QModbusRequest::FunctionCode(
            pduData[0]), pduData.mid(1)), address);
    } else {
        qDebug() << "Send: Sending PDU with predefined function code.";
        quint16 fc = (isSerial ? fcSerialDrop : fcTcpDrop)->currentText().left(4).toShort(0, 16);
        reply = m_device->sendRawRequest(QModbusRequest(QModbusRequest::FunctionCode(fc), pduData),
            address);
    }

    if (reply) {
        sendButton->setDisabled(true);
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, [reply, this]() {
                sendButton->setEnabled(true);
                qDebug() << "Receive: Asynchronous response PDU: " << reply->rawResult() << endl;
            });
        } else {
            sendButton->setEnabled(true);
            qDebug() << "Receive: Synchronous response pdu: " << reply->rawResult() << endl;
        }
    }
}

void MainWindow::on_connectButton_clicked()
{
    if (tcpRadio->isChecked()) {
        auto device = new ModbusTcpClient;
        using signature = void (QSpinBox::*)(int);
        connect(ti1Spin, static_cast<signature>(&QSpinBox::valueChanged), device,
            &ModbusTcpClient::valueChanged);
        connect(ti2Spin, static_cast<signature>(&QSpinBox::valueChanged), device,
            &ModbusTcpClient::valueChanged);

        connect(pi1Spin, static_cast<signature>(&QSpinBox::valueChanged), device,
            &ModbusTcpClient::valueChanged);
        connect(pi2Spin, static_cast<signature>(&QSpinBox::valueChanged), device,
            &ModbusTcpClient::valueChanged);

        connect(l1Spin, static_cast<signature>(&QSpinBox::valueChanged), device,
            &ModbusTcpClient::valueChanged);
        connect(l2Spin, static_cast<signature>(&QSpinBox::valueChanged), device,
            &ModbusTcpClient::valueChanged);

        connect(ui1Spin, static_cast<signature>(&QSpinBox::valueChanged), device,
            &ModbusTcpClient::valueChanged);

        m_device = device;
        device->valueChanged(0);    // trigger update
        m_device->setConnectionParameter(QModbusDevice::NetworkAddressParameter,
            tcpAddressEdit->text());
        m_device->setConnectionParameter(QModbusDevice::NetworkPortParameter,
            tcpPortEdit->text());
    } else {
        m_device = new QModbusRtuSerialMaster;
        m_device->setConnectionParameter(QModbusDevice::SerialPortNameParameter,
            serialPortCombo->currentText());

        int parity = parityCombo->currentIndex();
        if (parity > 0)
            parity++;
        m_device->setConnectionParameter(QModbusDevice::SerialParityParameter, parity);
        m_device->setConnectionParameter(QModbusDevice::SerialDataBitsParameter,
            dataBitsCombo->currentText().toInt());
        m_device->setConnectionParameter(QModbusDevice::SerialStopBitsParameter,
            stopBitsCombo->currentText().toInt());
        m_device->setConnectionParameter(QModbusDevice::SerialBaudRateParameter,
            baudRateCombo->currentText().toInt());
    }
    m_device->setTimeout(timeoutSpin->value());
    m_device->setNumberOfRetries(retriesSpin->value());

    connect(m_device, &QModbusDevice::errorOccurred, this, [this](QModbusDevice::Error) {
        qDebug().noquote() << QStringLiteral("Error: %1").arg(m_device->errorString());
        emit disconnectButton->clicked();
    }, Qt::QueuedConnection);

    connect(m_device, &QModbusDevice::stateChanged, [this](QModbusDevice::State state) {
        switch (state) {
        case QModbusDevice::UnconnectedState:
            qDebug().noquote() << QStringLiteral("State: Entered unconnected state.");
            break;
        case QModbusDevice::ConnectingState:
            qDebug().noquote() << QStringLiteral("State: Entered connecting state.");
            break;
        case QModbusDevice::ConnectedState:
            qDebug().noquote() << QStringLiteral("State: Entered connected state.");
            break;
        case QModbusDevice::ClosingState:
            qDebug().noquote() << QStringLiteral("State: Entered closing state.");
            break;
        }
    });
    m_device->connectDevice();
}

void MainWindow::on_disconnectButton_clicked()
{
    disconnectAndDelete();
}

void MainWindow::on_serialPortCombo_currentIndexChanged(int index)
{
    const bool custom = serialPortCombo->itemData(index, Qt::UserRole).toBool();
    serialPortCombo->setEditable(custom);
    if (custom) {
        serialPortCombo->clearEditText();
        serialPortCombo->lineEdit()->setPlaceholderText(QStringLiteral("Type here..."));
    }
}

void MainWindow::disconnectAndDelete()
{
    if (!m_device)
        return;
    m_device->disconnectDevice();
    m_device->disconnect();
    delete m_device;
    m_device = nullptr;
}
