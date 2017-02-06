/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#include <QtTest/QtTest>
#include <QtSerialBus/QModbusReply>

class tst_QModbusReply : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void tst_ctor();
    void tst_setFinished();
    void tst_setError_data();
    void tst_setError();
    void tst_setResult();
};

void tst_QModbusReply::initTestCase()
{
    qRegisterMetaType<QModbusDevice::Error>();
}

void tst_QModbusReply::tst_ctor()
{
    QModbusReply r(QModbusReply::Common, 1, this);
    QCOMPARE(r.type(), QModbusReply::Common);
    QCOMPARE(r.serverAddress(), 1);
    QCOMPARE(r.isFinished(), false);
    QCOMPARE(r.result().isValid(), false);
    QCOMPARE(r.rawResult().isValid(), false);
    QCOMPARE(r.errorString(), QString());
    QCOMPARE(r.error(), QModbusDevice::NoError);

    QModbusReply r2(QModbusReply::Raw, 2, this);
    QCOMPARE(r2.type(), QModbusReply::Raw);
    QCOMPARE(r2.serverAddress(), 2);
    QCOMPARE(r2.isFinished(), false);
    QCOMPARE(r2.result().isValid(), false);
    QCOMPARE(r2.rawResult().isValid(), false);
    QCOMPARE(r2.errorString(), QString());
    QCOMPARE(r2.error(), QModbusDevice::NoError);
}

void tst_QModbusReply::tst_setFinished()
{
    QModbusReply replyTest(QModbusReply::Common, 1);
    QCOMPARE(replyTest.serverAddress(), 1);
    QSignalSpy finishedSpy(&replyTest, SIGNAL(finished()));
    QSignalSpy errorSpy(&replyTest, SIGNAL(errorOccurred(QModbusDevice::Error)));

    QCOMPARE(replyTest.serverAddress(), 1);
    QCOMPARE(replyTest.isFinished(), false);
    QCOMPARE(replyTest.result().isValid(), false);
    QCOMPARE(replyTest.rawResult().isValid(), false);
    QCOMPARE(replyTest.errorString(), QString());
    QCOMPARE(replyTest.error(), QModbusDevice::NoError);

    QVERIFY(finishedSpy.isEmpty());
    QVERIFY(errorSpy.isEmpty());

    replyTest.setFinished(true);
    QVERIFY(finishedSpy.count() == 1);
    QVERIFY(errorSpy.isEmpty());
    QCOMPARE(replyTest.serverAddress(), 1);
    QCOMPARE(replyTest.isFinished(), true);
    QCOMPARE(replyTest.result().isValid(), false);
    QCOMPARE(replyTest.rawResult().isValid(), false);
    QCOMPARE(replyTest.errorString(), QString());
    QCOMPARE(replyTest.error(), QModbusDevice::NoError);

    replyTest.setFinished(false);
    QVERIFY(finishedSpy.count() == 1); // no further signal
    QVERIFY(errorSpy.isEmpty());
    QCOMPARE(replyTest.serverAddress(), 1);
    QCOMPARE(replyTest.isFinished(), false);
    QCOMPARE(replyTest.result().isValid(), false);
    QCOMPARE(replyTest.rawResult().isValid(), false);
    QCOMPARE(replyTest.errorString(), QString());
    QCOMPARE(replyTest.error(), QModbusDevice::NoError);
}

void tst_QModbusReply::tst_setError_data()
{
    QTest::addColumn<QModbusDevice::Error>("error");
    QTest::addColumn<QString>("errorString");

    QTest::newRow("ProtocolError") << QModbusDevice::ProtocolError << QString("ProtocolError");
    QTest::newRow("NoError") << QModbusDevice::NoError << QString("NoError");
    QTest::newRow("NoError-empty") << QModbusDevice::NoError << QString();
    QTest::newRow("TimeoutError") << QModbusDevice::TimeoutError << QString("TimeoutError");
    QTest::newRow("ReplyAbortedError") << QModbusDevice::ReplyAbortedError << QString("AbortedError");
}

void tst_QModbusReply::tst_setError()
{
    QFETCH(QModbusDevice::Error, error);
    QFETCH(QString, errorString);

    QModbusReply replyTest(QModbusReply::Common, 1);
    QCOMPARE(replyTest.serverAddress(), 1);
    QSignalSpy finishedSpy(&replyTest, SIGNAL(finished()));
    QSignalSpy errorSpy(&replyTest, SIGNAL(errorOccurred(QModbusDevice::Error)));

    QVERIFY(finishedSpy.isEmpty());
    QVERIFY(errorSpy.isEmpty());

    replyTest.setError(error, errorString);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(replyTest.rawResult().isValid(), false);
    QCOMPARE(replyTest.error(), error);
    QCOMPARE(replyTest.errorString(), errorString);
    QCOMPARE(errorSpy.at(0).at(0).value<QModbusDevice::Error>(), error);

    replyTest.setError(error, errorString);
    replyTest.setFinished(true);
    QCOMPARE(finishedSpy.count(), 3); //setError() implies call to setFinished()
    QCOMPARE(errorSpy.count(), 2);
}

void tst_QModbusReply::tst_setResult()
{
    QModbusDataUnit unit(QModbusDataUnit::Coils, 5, {4,5,6});
    QCOMPARE(unit.startAddress(), 5);
    QCOMPARE(unit.valueCount(), 3u);
    QCOMPARE(unit.registerType(), QModbusDataUnit::Coils);
    QCOMPARE(unit.isValid(), true);
    QVector<quint16>  reference = { 4,5,6 };
    QCOMPARE(unit.values(), reference);

    QModbusReply replyTest(QModbusReply::Common, 1);
    QCOMPARE(replyTest.serverAddress(), 1);
    QSignalSpy finishedSpy(&replyTest, SIGNAL(finished()));
    QSignalSpy errorSpy(&replyTest, SIGNAL(errorOccurred(QModbusDevice::Error)));

    QVERIFY(finishedSpy.isEmpty());
    QVERIFY(errorSpy.isEmpty());

    QCOMPARE(replyTest.result().startAddress(), -1);
    QCOMPARE(replyTest.result().valueCount(), 0u);
    QCOMPARE(replyTest.result().registerType(), QModbusDataUnit::Invalid);
    QCOMPARE(replyTest.result().isValid(), false);
    QCOMPARE(replyTest.rawResult().isValid(), false);
    QCOMPARE(replyTest.result().values(), QVector<quint16>());

    QModbusResponse response(QModbusResponse::ReadExceptionStatus, quint16(0x0000));
    replyTest.setResult(unit);
    replyTest.setRawResult(response);
    QCOMPARE(finishedSpy.count(), 0);
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(replyTest.result().startAddress(), 5);
    QCOMPARE(replyTest.result().valueCount(), 3u);
    QCOMPARE(replyTest.result().registerType(), QModbusDataUnit::Coils);
    QCOMPARE(replyTest.result().isValid(), true);
    QCOMPARE(replyTest.result().values(), reference);
    QCOMPARE(replyTest.rawResult().isValid(), true);

    auto tmp = replyTest.rawResult();
    QCOMPARE(tmp.functionCode(), QModbusResponse::ReadExceptionStatus);
    QCOMPARE(tmp.data(), QByteArray::fromHex("0000"));

    QModbusReply replyRawTest(QModbusReply::Raw, 1);
    QCOMPARE(replyRawTest.serverAddress(), 1);
    QSignalSpy finishedSpyRaw(&replyRawTest, SIGNAL(finished()));
    QSignalSpy errorSpyRaw(&replyRawTest, SIGNAL(errorOccurred(QModbusDevice::Error)));

    QVERIFY(finishedSpyRaw.isEmpty());
    QVERIFY(errorSpyRaw.isEmpty());

    QCOMPARE(replyRawTest.result().startAddress(), -1);
    QCOMPARE(replyRawTest.result().valueCount(), 0u);
    QCOMPARE(replyRawTest.result().registerType(), QModbusDataUnit::Invalid);
    QCOMPARE(replyRawTest.result().isValid(), false);
    QCOMPARE(replyRawTest.rawResult().isValid(), false);
    QCOMPARE(replyRawTest.result().values(), QVector<quint16>());

    replyRawTest.setResult(unit);
    replyRawTest.setRawResult(response);
    QCOMPARE(finishedSpy.count(), 0);
    QCOMPARE(errorSpyRaw.count(), 0);
    QCOMPARE(replyRawTest.result().isValid(), false);
    QCOMPARE(replyRawTest.rawResult().isValid(), true);

    tmp = replyRawTest.rawResult();
    QCOMPARE(tmp.functionCode(), QModbusResponse::ReadExceptionStatus);
    QCOMPARE(tmp.data(), QByteArray::fromHex("0000"));
}

QTEST_MAIN(tst_QModbusReply)

#include "tst_qmodbusreply.moc"
