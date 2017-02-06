/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNfc module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "targetemulator_p.h"

#include <QtCore/QSettings>
#include <QtCore/QDateTime>

#include <QtCore/QDebug>

// Implementation of qNfcChecksum
#include "qnearfieldtarget.h"

QT_BEGIN_NAMESPACE

TagBase::TagBase()
:   lastAccess(0)
{
}

TagBase::~TagBase()
{
}

static inline quint8 blockByteToAddress(quint8 block, quint8 byte)
{
    return ((block & 0x0f) << 3) | (byte & 0x07);
}

NfcTagType1::NfcTagType1()
:   hr0(0x11), hr1(0x00), memory(120, '\0')
{
    // Locked blocks
    memory[(0x0e << 3) | 0x00] = 0x01;
    memory[(0x0e << 3) | 0x01] = 0x60;
}

NfcTagType1::~NfcTagType1()
{
}

void NfcTagType1::load(QSettings *settings)
{
    settings->beginGroup(QStringLiteral("TagType1"));

    hr0 = settings->value(QStringLiteral("HR0"), 0x11).toUInt();

    if (!(hr0 & 0x10)) {
        settings->endGroup();
        return;
    }

    hr1 = settings->value(QStringLiteral("HR1"), 0x00).toUInt();

    memory = settings->value(QStringLiteral("Data")).toByteArray();

    //quint8 nmn = memory.at(8);

    quint8 vno = memory.at(9);
    if (vno != 0x10)
        qWarning("Only NFC TagType1 v1.0 behavior is supported.");

    quint8 tms = memory.at(10);
    if (memory.length() != 8 * (tms + 1))
        qWarning("Static memory size does not match TMS value.");

    quint8 rwa = memory.at(11);
    switch (rwa >> 4) {
    case 0:
        // Unrestricted read access tag
        break;
    default:
        // tag with unknown read attributes
        ;
    }

    switch (rwa & 0x0f) {
    case 0:
        // Unrestricted write access tag
        break;
    case 0x0f:
        // Read only tag
        break;
    default:
        // tag with unknown write attributes
        ;
    }

    //quint16 lock = (quint8(memory[blockByteToAddress(0x0e, 1)]) << 8) |
    //               quint8(memory[blockByteToAddress(0x0e, 0)]);

    settings->endGroup();
}

QByteArray NfcTagType1::uid() const
{
    lastAccess = QDateTime::currentMSecsSinceEpoch();

    return memory.left(7);
}

quint8 NfcTagType1::readData(quint8 block, quint8 byte)
{
    return memory.at((block << 3) | byte);
}

QByteArray NfcTagType1::processCommand(const QByteArray &command)
{
    lastAccess = QDateTime::currentMSecsSinceEpoch();

    QByteArray response;

    bool tagType1 = (hr0 & 0xf0) == 0x10;
    bool dynamic = (hr0 & 0x0f) != 0x01;

    if (command.length() == 9) {
        // static memory model command
        quint8 opcode = command.at(0);
        quint8 address = command.at(1);
        quint8 data = command.at(2);
        QByteArray uid = command.mid(3, 4);

        // check checksum
        if (qNfcChecksum(command.constData(), command.length()) != 0)
            return QByteArray();

        // check UID
        if (uid != memory.left(4))
            return QByteArray();

        switch (opcode) {
        case 0x00:  // RALL
            response.append(hr0);
            response.append(hr1);
            response.append(memory.left(120));
            break;
        case 0x01:  // READ
            response.append(address);
            if (address & 0x80)
                response.append(char(0x00));
            else
                response.append(memory.at(address));
            break;
        case 0x53: { // WRITE-E
            quint8 block = address >> 3;
            if (block == 0x00 || block == 0x0d || block == 0x0e)    // locked blocks
                break;

            quint16 lock = (readData(0x0e, 0x01) << 8) | readData(0x0e, 0x00);
            if ((0x01 << block) & lock)    // locked blocks
                break;

            // FIXME: Test dynamic lock bytes

            memory[address] = data;

            response.append(address);
            response.append(data);
            break;
        }
        case 0x1a: { // WRITE-NE
            quint8 block = address >> 3;
            if (block == 0x00 || block == 0x0d)  // locked blocks
                break;

            quint16 lock = (readData(0x0e, 0x01) << 8) | readData(0x0e, 0x00);
            if ((0x01 << block) & lock)    // locked blocks
                break;


            // FIXME: Test dynamic lock bytes

            memory[address] = memory.at(address) | data;

            response.append(address);
            response.append(memory.at(address));
            break;
        }
        case 0x78:  // RID
            response.append(hr0);
            response.append(hr1);
            response.append(memory.left(4));
            break;
        }
    } else if (tagType1 && dynamic && command.length() == 16) {
        // dynamic memory model command
        quint8 opcode = command.at(0);
        quint8 address = command.at(1);
        QByteArray data = command.mid(2, 8);
        QByteArray uid = command.mid(10, 4);

        // check checksum
        if (qNfcChecksum(command.constData(), command.length()) != 0)
            return QByteArray();

        // check UID
        if (uid != memory.left(4))
            return QByteArray();

        switch (opcode) {
        case 0x10: // RSEG
            response.append(address);
            response.append(memory.mid(128 * (address >> 4), 128));
            break;
        case 0x02:  // READ8
            response.append(address);
            response.append(memory.mid(8 * address, 8));
            break;
        case 0x54: { // WRITE-E8
            // locked blocks
            if (address == 0x00 || address == 0x0d || address == 0x0e || address == 0x0f)
                break;

            quint16 lock = (readData(0x0e, 0x01) << 8) | readData(0x0e, 0x00);
            if (address <= 0x0e && ((0x01 << address) & lock))  // locked blocks
                break;

            // FIXME: Test dynamic lock bytes

            memory.replace(address * 8, 8, data);

            response.append(address);
            response.append(memory.mid(address * 8, 8));
            break;
        }
        case 0x1b:  // WRITE-NE8
            // locked blocks
            if (address == 0x00 || address == 0x0d || address == 0x0e || address == 0x0f)
                break;

            quint16 lock = (readData(0x0e, 0x01) << 8) | readData(0x0e, 0x00);
            if (address <= 0x0e && ((0x01 << address) & lock))  // locked blocks
                break;

            // FIXME: Test dynamic lock bytes

            for (int i = 0; i < 8; ++i)
                memory[address * 8 + i] = memory.at(address * 8 + i) | data.at(i);

            response.append(address);
            response.append(memory.mid(address * 8, 8));
            break;
        }
    }

    if (!response.isEmpty()) {
        quint16 crc = qNfcChecksum(response.constData(), response.length());
        response.append(quint8(crc & 0xff));
        response.append(quint8(crc >> 8));
    }

    return response;
}


NfcTagType2::NfcTagType2()
:   memory(64, 0x00), currentSector(0), expectPacket2(false)
{
}

NfcTagType2::~NfcTagType2()
{
}

void NfcTagType2::load(QSettings *settings)
{
    settings->beginGroup(QStringLiteral("TagType2"));

    memory = settings->value(QStringLiteral("Data")).toByteArray();

    settings->endGroup();
}

QByteArray NfcTagType2::uid() const
{
    lastAccess = QDateTime::currentMSecsSinceEpoch();

    return memory.left(3) + memory.mid(4, 4);
}

#define NACK QByteArray("\x05")
#define ACK QByteArray("\x0a")

QByteArray NfcTagType2::processCommand(const QByteArray &command)
{
    lastAccess = QDateTime::currentMSecsSinceEpoch();

    QByteArray response;

    // check checksum
    if (qNfcChecksum(command.constData(), command.length()) != 0)
        return QByteArray();

    if (expectPacket2) {
        expectPacket2 = false;
        quint8 sector = command.at(0);
        if (sector * 1024 > memory.length())
            return NACK;
        else {
            currentSector = sector;
            return QByteArray();
        }
    }

    quint8 opcode = command.at(0);

    switch (opcode) {
    case 0x30: {    // READ BLOCK
        quint8 block = command.at(1);
        int absoluteBlock = currentSector * 256 + block;

        response.append(memory.mid(absoluteBlock * 4, 16));
        if (response.length() != 16)
            response.append(QByteArray(16 - response.length(), '\0'));

        break;
    }
    case 0xa2: {    // WRITE BLOCK
        quint8 block = command.at(1);
        int absoluteBlock = currentSector * 256 + block;

        // locked blocks
        if (absoluteBlock == 0 || absoluteBlock == 1)
            return NACK;

        const QByteArray data = command.mid(2, 4);

        memory.replace(absoluteBlock * 4, 4, data);

        return ACK;
    }
    case 0xc2:  // SECTOR SELECT - Packet 1
        if (memory.length() > 1024) {
            expectPacket2 = true;
            return ACK;
        }

        return NACK;
    default:
        qDebug() << "Unknown opcode for Tag Type 2" << hex << opcode;
        qDebug() << "command:" << command.toHex();

        return NACK;
        ;
    }

    if (!response.isEmpty()) {
        quint16 crc = qNfcChecksum(response.constData(), response.length());
        response.append(quint8(crc & 0xff));
        response.append(quint8(crc >> 8));
    }

    return response;
}

QT_END_NAMESPACE
