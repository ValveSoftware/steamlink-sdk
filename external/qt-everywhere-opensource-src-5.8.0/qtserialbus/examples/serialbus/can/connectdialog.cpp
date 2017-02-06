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

#include "connectdialog.h"
#include "ui_connectdialog.h"

#include <QCanBus>
#include <QDebug>

ConnectDialog::ConnectDialog(QWidget *parent) :
    QDialog(parent),
    m_ui(new Ui::ConnectDialog),
    m_customSpeedValidator(0)
{
    m_ui->setupUi(this);

    m_customSpeedValidator = new QIntValidator(0, 1000000, this);
    m_ui->errorFilterEdit->setValidator(new QIntValidator(0, 0x1FFFFFFFU, this));

    m_ui->loopbackBox->addItem(tr("unspecified"), QVariant());
    m_ui->loopbackBox->addItem(tr("false"), QVariant(false));
    m_ui->loopbackBox->addItem(tr("true"), QVariant(true));

    m_ui->receiveOwnBox->addItem(tr("unspecified"), QVariant());
    m_ui->receiveOwnBox->addItem(tr("false"), QVariant(false));
    m_ui->receiveOwnBox->addItem(tr("true"), QVariant(true));

    m_ui->canFdBox->addItem(tr("false"), QVariant(false));
    m_ui->canFdBox->addItem(tr("true"), QVariant(true));

    connect(m_ui->okButton, &QPushButton::clicked, this, &ConnectDialog::ok);
    connect(m_ui->cancelButton, &QPushButton::clicked, this, &ConnectDialog::cancel);
    connect(m_ui->useConfigurationBox, &QCheckBox::clicked, m_ui->configurationBox, &QGroupBox::setEnabled);

    connect(m_ui->speedBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &ConnectDialog::checkCustomSpeedPolicy);
    connect(m_ui->backendListBox, &QComboBox::currentTextChanged,
            this, &ConnectDialog::backendChanged);
    m_ui->rawFilterEdit->hide();
    m_ui->rawFilterLabel->hide();

    m_ui->backendListBox->addItems(QCanBus::instance()->plugins());
    fillSpeeds();

    updateSettings();
}

ConnectDialog::~ConnectDialog()
{
    delete m_ui;
}

ConnectDialog::Settings ConnectDialog::settings() const
{
    return m_currentSettings;
}

void ConnectDialog::checkCustomSpeedPolicy(int idx)
{
    const bool isCustomSpeed = !m_ui->speedBox->itemData(idx).isValid();
    m_ui->speedBox->setEditable(isCustomSpeed);
    if (isCustomSpeed) {
        m_ui->speedBox->clearEditText();
        QLineEdit *edit = m_ui->speedBox->lineEdit();
        edit->setValidator(m_customSpeedValidator);
    }
}

void ConnectDialog::backendChanged(const QString &backend)
{
    if (backend == QStringLiteral("generic"))
        m_ui->interfaceNameEdit->setPlaceholderText(QStringLiteral("can0"));
    else if (backend == QStringLiteral("peakcan"))
        m_ui->interfaceNameEdit->setPlaceholderText(QStringLiteral("usb0"));
    else if (backend == QStringLiteral("socketcan"))
        m_ui->interfaceNameEdit->setPlaceholderText(QStringLiteral("can0"));
    else if (backend == QStringLiteral("tinycan"))
        m_ui->interfaceNameEdit->setPlaceholderText(QStringLiteral("can0.0"));
    else if (backend == QStringLiteral("vectorcan"))
        m_ui->interfaceNameEdit->setPlaceholderText(QStringLiteral("can0"));
}

void ConnectDialog::ok()
{
    updateSettings();
    accept();
}

void ConnectDialog::cancel()
{
    revertSettings();
    reject();
}

QString ConnectDialog::configurationValue(QCanBusDevice::ConfigurationKey key)
{
    QVariant result;

    foreach (const ConfigurationItem &item, m_currentSettings.configurations) {
        if (item.first == key) {
            result = item.second;
            break;
        }
    }

    if (result.isNull() && (
                key == QCanBusDevice::LoopbackKey ||
                key == QCanBusDevice::ReceiveOwnKey)) {
        return tr("unspecified");
    }

    return result.toString();
}

void ConnectDialog::revertSettings()
{
    m_ui->backendListBox->setCurrentText(m_currentSettings.backendName);
    m_ui->interfaceNameEdit->setText(m_currentSettings.deviceInterfaceName);
    m_ui->useConfigurationBox->setChecked(m_currentSettings.useConfigurationEnabled);

    QString value = configurationValue(QCanBusDevice::LoopbackKey);
    m_ui->loopbackBox->setCurrentText(value);

    value = configurationValue(QCanBusDevice::ReceiveOwnKey);
    m_ui->receiveOwnBox->setCurrentText(value);

    value = configurationValue(QCanBusDevice::ErrorFilterKey);
    m_ui->errorFilterEdit->setText(value);

    value = configurationValue(QCanBusDevice::BitRateKey);
    m_ui->speedBox->setCurrentText(value);

    value = configurationValue(QCanBusDevice::CanFdKey);
    m_ui->canFdBox->setCurrentText(value);
}

void ConnectDialog::updateSettings()
{
    m_currentSettings.backendName = m_ui->backendListBox->currentText();
    m_currentSettings.deviceInterfaceName = m_ui->interfaceNameEdit->text();
    m_currentSettings.useConfigurationEnabled = m_ui->useConfigurationBox->isChecked();

    if (m_currentSettings.useConfigurationEnabled) {
        m_currentSettings.configurations.clear();
        // process LoopBack
        if (m_ui->loopbackBox->currentIndex() != 0) {
            ConfigurationItem item;
            item.first = QCanBusDevice::LoopbackKey;
            item.second = m_ui->loopbackBox->currentData();
            m_currentSettings.configurations.append(item);
        }

        // process ReceiveOwnKey
        if (m_ui->receiveOwnBox->currentIndex() != 0) {
            ConfigurationItem item;
            item.first = QCanBusDevice::ReceiveOwnKey;
            item.second = m_ui->receiveOwnBox->currentData();
            m_currentSettings.configurations.append(item);
        }

        // process error filter
        if (!m_ui->errorFilterEdit->text().isEmpty()) {
            QString value = m_ui->errorFilterEdit->text();
            bool ok = false;
            int dec = value.toInt(&ok);
            if (ok) {
                ConfigurationItem item;
                item.first = QCanBusDevice::ErrorFilterKey;
                item.second = QVariant::fromValue(QCanBusFrame::FrameErrors(dec));
                m_currentSettings.configurations.append(item);
            }
        }

        // process raw filter list
        if (!m_ui->rawFilterEdit->text().isEmpty()) {
            //TODO current ui not sfficient to reflect this param
        }

        // process bitrate
        bool ok = false;
        int bitrate = 0;
        if (m_ui->speedBox->currentIndex() == (m_ui->speedBox->count() - 1))
            bitrate = m_ui->speedBox->currentText().toInt(&ok);
        else
            bitrate = m_ui->speedBox->itemData(m_ui->speedBox->currentIndex()).toInt(&ok);

        if (ok && (bitrate > 0)) {
            ConfigurationItem item;
            item.first = QCanBusDevice::BitRateKey;
            item.second = QVariant(bitrate);
            m_currentSettings.configurations.append(item);
        }

        // process CAN FD setting
        ConfigurationItem fdItem;
        fdItem.first = QCanBusDevice::CanFdKey;
        fdItem.second = m_ui->canFdBox->currentData();
        m_currentSettings.configurations.append(fdItem);
    }
}

void ConnectDialog::fillSpeeds()
{
    m_ui->speedBox->addItem(QStringLiteral("10000"), 10000);
    m_ui->speedBox->addItem(QStringLiteral("20000"), 20000);
    m_ui->speedBox->addItem(QStringLiteral("50000"), 50000);
    m_ui->speedBox->addItem(QStringLiteral("100000"), 100000);
    m_ui->speedBox->addItem(QStringLiteral("125000"), 125000);
    m_ui->speedBox->addItem(QStringLiteral("250000"), 250000);
    m_ui->speedBox->addItem(QStringLiteral("500000"), 500000);
    m_ui->speedBox->addItem(QStringLiteral("800000"), 800000);
    m_ui->speedBox->addItem(QStringLiteral("1000000"), 1000000);
    m_ui->speedBox->addItem(tr("Custom"));

    m_ui->speedBox->setCurrentIndex(6); // setup 500000 bits/sec by default
}
