/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialBus module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <private/qmodbuscommevent_p.h>

#include <QtTest/QtTest>

class tst_QModbusCommEvent : public QObject
{
    Q_OBJECT

private slots:
    void testConstructor()
    {
        QCOMPARE(quint8(QModbusCommEvent(QModbusCommEvent::SentEvent)), quint8(0x40));
        QCOMPARE(quint8(QModbusCommEvent(QModbusCommEvent::ReceiveEvent)), quint8(0x80));
        QCOMPARE(quint8(QModbusCommEvent(QModbusCommEvent::EnteredListenOnlyMode)), quint8(0x04));
        QCOMPARE(quint8(QModbusCommEvent(QModbusCommEvent::InitiatedCommunicationRestart)),
                 quint8(0x00));
    }

    void testAssignmentOperator()
    {
        QModbusCommEvent e = QModbusCommEvent::SentEvent;
        QCOMPARE(quint8(e), quint8(0x40));
        e = QModbusCommEvent::ReceiveEvent;
        QCOMPARE(quint8(e), quint8(0x80));
        e = QModbusCommEvent::EnteredListenOnlyMode;
        QCOMPARE(quint8(e), quint8(0x04));
        e = QModbusCommEvent::InitiatedCommunicationRestart;
        QCOMPARE(quint8(e), quint8(0x00));
    }

    void testOperatorOr()
    {
        QModbusCommEvent sent = QModbusCommEvent::SentEvent;
        QCOMPARE(quint8(sent | QModbusCommEvent::SendFlag::CurrentlyInListenOnlyMode),
                 quint8(0x60));

        QModbusCommEvent receive = QModbusCommEvent::ReceiveEvent;
        QCOMPARE(quint8(receive | QModbusCommEvent::ReceiveFlag::CurrentlyInListenOnlyMode),
                 quint8(0xa0));
    }

    void testOperatorOrAssign()
    {
        QModbusCommEvent sent = QModbusCommEvent::SentEvent;
        sent |= QModbusCommEvent::SendFlag::ReadExceptionSent;
        QCOMPARE(quint8(sent), quint8(0x41));

        QModbusCommEvent receive = QModbusCommEvent::ReceiveEvent;
        receive |= QModbusCommEvent::ReceiveFlag::CurrentlyInListenOnlyMode;
        QCOMPARE(quint8(receive), quint8(0xa0));
    }
};

QTEST_MAIN(tst_QModbusCommEvent)

#include "tst_qmodbuscommevent.moc"
