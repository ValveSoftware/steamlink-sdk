/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>

#include <QDebug>
#include <QMap>
#include <QTextStream>

#include <qbluetoothtransferrequest.h>
#include <qbluetoothtransfermanager.h>
#include <qbluetoothtransferreply.h>
#include <qbluetoothaddress.h>
#include <qbluetoothlocaldevice.h>

#include <qbluetoothdeviceinfo.h>
#include <qbluetoothserviceinfo.h>
#include <qbluetoothservicediscoveryagent.h>

/*
 * Some tests require a Bluetooth device within the vincinity of the test
 * machine executing this test. The tests require manual interaction
 * as pairing and file transfer requests must be accepted.
 * The remote device's address must be passed
 * via the BT_TEST_DEVICE env variable. The remote device must be
 * discoverable and the object push service must be accessible. Any
 **/

QT_USE_NAMESPACE

typedef QMap<QBluetoothTransferRequest::Attribute,QVariant> tst_QBluetoothTransferManager_QParameterMap;
Q_DECLARE_METATYPE(tst_QBluetoothTransferManager_QParameterMap)

static const int MaxConnectTime = 60 * 1000;   // 1 minute in ms

class tst_QBluetoothTransferManager : public QObject
{
    Q_OBJECT

public:
    tst_QBluetoothTransferManager();
    ~tst_QBluetoothTransferManager();

private slots:
    void initTestCase();

    void tst_construction();

    void tst_request_data();
    void tst_request();

    void tst_sendFile_data();
    void tst_sendFile();

    void tst_sendBuffer_data();
    void tst_sendBuffer();

    void tst_sendNullPointer();
private:
    QBluetoothAddress remoteAddress;
};

tst_QBluetoothTransferManager::tst_QBluetoothTransferManager()
{
}

tst_QBluetoothTransferManager::~tst_QBluetoothTransferManager()
{
}

void tst_QBluetoothTransferManager::initTestCase()
{
    const QString remote = qgetenv("BT_TEST_DEVICE");
    if (!remote.isEmpty()) {
        remoteAddress = QBluetoothAddress(remote);
        QVERIFY(!remoteAddress.isNull());
        qWarning() << "Using remote device " << remote << " for testing. Ensure that the device is discoverable for pairing requests";
    } else {
        qWarning() << "Not using any remote device for testing. Set BT_TEST_DEVICE env to run manual tests involving a remote device";
        QSKIP("Remote upload test not possible. Set BT_TEST_DEVICE");
    }

    if (!QBluetoothLocalDevice::allDevices().count())
        QSKIP("Skipping test due to missing Bluetooth device");

    // start Bluetooth if not started
    QBluetoothLocalDevice *device = new QBluetoothLocalDevice();
    device->powerOn();
    delete device;
}

void tst_QBluetoothTransferManager::tst_construction()
{
    QBluetoothTransferManager *manager = new QBluetoothTransferManager();

    QVERIFY(manager);
    delete manager;
}

void tst_QBluetoothTransferManager::tst_request_data()
{
    QTest::addColumn<QBluetoothAddress>("address");
    QTest::addColumn<QMap<QBluetoothTransferRequest::Attribute, QVariant> >("parameters");

    QMap<QBluetoothTransferRequest::Attribute, QVariant> inparameters;
    inparameters.insert(QBluetoothTransferRequest::DescriptionAttribute, "Description");
    inparameters.insert(QBluetoothTransferRequest::LengthAttribute, QVariant(1024));
    inparameters.insert(QBluetoothTransferRequest::TypeAttribute, "OPP");
    inparameters.insert(QBluetoothTransferRequest::NameAttribute, "name");
    inparameters.insert(QBluetoothTransferRequest::TimeAttribute, QDateTime::currentDateTime());

    QTest::newRow("TESTDATA") << QBluetoothAddress("00:11:22:33:44:55:66") << inparameters;
}

void tst_QBluetoothTransferManager::tst_request()
{
    QFETCH(QBluetoothAddress, address);
    QFETCH(tst_QBluetoothTransferManager_QParameterMap, parameters);

    QBluetoothTransferRequest transferRequest(address);
    foreach (QBluetoothTransferRequest::Attribute key, parameters.keys())
        QCOMPARE(transferRequest.attribute(key), QVariant());

    foreach (QBluetoothTransferRequest::Attribute key, parameters.keys())
        transferRequest.setAttribute((QBluetoothTransferRequest::Attribute)key, parameters[key]);

    QCOMPARE(transferRequest.address(), address);
    foreach (QBluetoothTransferRequest::Attribute key, parameters.keys())
        QCOMPARE(transferRequest.attribute(key), parameters[key]);

    //test copy constructor
    QBluetoothTransferRequest constructorCopy = transferRequest;
    QVERIFY(constructorCopy == transferRequest);
    QVERIFY(!(constructorCopy != transferRequest));
    QCOMPARE(constructorCopy.address(), address);
    foreach (QBluetoothTransferRequest::Attribute key, parameters.keys())
        QCOMPARE(constructorCopy.attribute(key), parameters[key]);

    //test assignment operator
    QBluetoothTransferRequest request;
    QVERIFY(request.address().isNull());
    foreach (QBluetoothTransferRequest::Attribute key, parameters.keys())
        QCOMPARE(request.attribute(key), QVariant());
    request = transferRequest;
    QCOMPARE(request.address(), address);
    foreach (QBluetoothTransferRequest::Attribute key, parameters.keys())
        QCOMPARE(request.attribute(key), parameters[key]);

    //test that it's a true and independent copy
    constructorCopy.setAttribute(QBluetoothTransferRequest::DescriptionAttribute, "newDescription");
    request.setAttribute(QBluetoothTransferRequest::TypeAttribute, "FTP");

    QCOMPARE(constructorCopy.attribute(QBluetoothTransferRequest::DescriptionAttribute).toString(),QString("newDescription"));
    QCOMPARE(request.attribute(QBluetoothTransferRequest::DescriptionAttribute).toString(),QString("Description"));
    QCOMPARE(transferRequest.attribute(QBluetoothTransferRequest::DescriptionAttribute).toString(),QString("Description"));

    QCOMPARE(constructorCopy.attribute(QBluetoothTransferRequest::TypeAttribute).toString(),QString("OPP"));
    QCOMPARE(request.attribute(QBluetoothTransferRequest::TypeAttribute).toString(),QString("FTP"));
    QCOMPARE(transferRequest.attribute(QBluetoothTransferRequest::TypeAttribute).toString(),QString("OPP"));
}

void tst_QBluetoothTransferManager::tst_sendFile_data()
{
    QTest::addColumn<QBluetoothAddress>("deviceAddress");
    QTest::addColumn<bool>("expectSuccess");
    QTest::addColumn<bool>("isInvalidFile");

    QTest::newRow("Push to remote test device") << remoteAddress << true << false;
    QTest::newRow("Push of non-existing file") << remoteAddress << false << true;
    QTest::newRow("Push to invalid address") << QBluetoothAddress() << false << false;
    QTest::newRow("Push to non-existend device") << QBluetoothAddress("11:22:33:44:55:66") << false << false;

}

void tst_QBluetoothTransferManager::tst_sendFile()
{
    QFETCH(QBluetoothAddress, deviceAddress);
    QFETCH(bool, expectSuccess);
    QFETCH(bool, isInvalidFile);

    QBluetoothLocalDevice dev;
    if (expectSuccess) {
        dev.requestPairing(deviceAddress, QBluetoothLocalDevice::Paired);
        QTest::qWait(5000);
        QCOMPARE(dev.pairingStatus(deviceAddress), QBluetoothLocalDevice::Paired);
    }

    QBluetoothTransferRequest request(deviceAddress);
    QCOMPARE(request.address(), deviceAddress);

    QBluetoothTransferManager manager;
    QString fileHandle;
    if (!isInvalidFile) {
        fileHandle = QFINDTESTDATA("testfile.txt");
        QVERIFY(!fileHandle.isEmpty());
    } else {
        fileHandle = ("arbitraryFileName.txt"); //file doesn't exist
    }
    QFile f(fileHandle);
    QCOMPARE(f.exists(), !isInvalidFile);


    qDebug() << "Transferring file to " << deviceAddress.toString();
    if (expectSuccess)
        qDebug() << "Please accept Object push request on remote device";
    QBluetoothTransferReply* reply = manager.put(request, &f);
    QSignalSpy finishedSpy(reply, SIGNAL(finished(QBluetoothTransferReply*)));
    QSignalSpy progressSpy(reply, SIGNAL(transferProgress(qint64,qint64)));
    QSignalSpy errorSpy(reply, SIGNAL(error(QBluetoothTransferReply::TransferError)));

    QCOMPARE(reply->request(), request);
    QVERIFY(reply->manager() == &manager);
    QVERIFY(!reply->isFinished());
    QVERIFY(reply->isRunning());

    const int maxWaitTime = 20 * 1000; //20s
    for (int time = 0;
                time<maxWaitTime && (finishedSpy.count()==0);
             time+=1000) {
        QTest::qWait(1000); //if interval
    }

    QVERIFY(finishedSpy.count()>0);
    if (expectSuccess) {
        QVERIFY(progressSpy.count()>0);
        QCOMPARE(reply->error(), QBluetoothTransferReply::NoError);
        QCOMPARE(reply->errorString(), QString());
        QVERIFY(errorSpy.isEmpty());
    } else {
        QVERIFY(progressSpy.count() == 0);
        if (isInvalidFile)
            QVERIFY(reply->error() == QBluetoothTransferReply::FileNotFoundError);
        else
            QVERIFY(reply->error() != QBluetoothTransferReply::NoError);
        QVERIFY(!reply->errorString().isEmpty());
        QCOMPARE(errorSpy.count(), 1);
    }

    QVERIFY(reply->isFinished());
    QVERIFY(!reply->isRunning());
}

void tst_QBluetoothTransferManager::tst_sendBuffer_data()
{

    QTest::addColumn<QBluetoothAddress>("deviceAddress");
    QTest::addColumn<bool>("expectSuccess");
    QTest::addColumn<QByteArray>("data");

    QTest::newRow("Push to remote test device") << remoteAddress << true <<
                        QByteArray("This is a very long byte array which we are going to access via a QBuffer");                                                       ;
    QTest::newRow("Push to invalid address") << QBluetoothAddress() << false << QByteArray("test");
    QTest::newRow("Push to non-existend device") << QBluetoothAddress("11:22:33:44:55:66") << false << QByteArray("test");
}



void tst_QBluetoothTransferManager::tst_sendBuffer()
{
    QFETCH(QBluetoothAddress, deviceAddress);
    QFETCH(bool, expectSuccess);
    QFETCH(QByteArray, data);

    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    buffer.seek(0);

    QBluetoothLocalDevice dev;
    if (expectSuccess) {
        dev.requestPairing(deviceAddress, QBluetoothLocalDevice::Paired);
        QTest::qWait(2000);
        QCOMPARE(dev.pairingStatus(deviceAddress), QBluetoothLocalDevice::Paired);
    }

    QBluetoothTransferRequest request(deviceAddress);
    QCOMPARE(request.address(), deviceAddress);

    QBluetoothTransferManager manager;

    qDebug() << "Transferring test buffer to " << deviceAddress.toString();
    if (expectSuccess)
        qDebug() << "Please accept Object push request on remote device";
    QBluetoothTransferReply* reply = manager.put(request, &buffer);
    QSignalSpy finishedSpy(reply, SIGNAL(finished(QBluetoothTransferReply*)));
    QSignalSpy progressSpy(reply, SIGNAL(transferProgress(qint64,qint64)));
    QSignalSpy errorSpy(reply, SIGNAL(error(QBluetoothTransferReply::TransferError)));

    QCOMPARE(reply->request(), request);
    QVERIFY(reply->manager() == &manager);
    QVERIFY(!reply->isFinished());
    QVERIFY(reply->isRunning());

    const int maxWaitTime = 20 * 1000; //20s
    for (int time = 0;
                time<maxWaitTime && (finishedSpy.count()==0);
             time+=10000) {
        QTest::qWait(10000); //if interval
    }

    QVERIFY(finishedSpy.count()>0);
    if (expectSuccess) {
        QVERIFY(progressSpy.count()>0);
        QVERIFY(errorSpy.isEmpty());
        QCOMPARE(reply->error(), QBluetoothTransferReply::NoError);
        QCOMPARE(reply->errorString(), QString());
    } else {
        QVERIFY(progressSpy.count() == 0);
        QVERIFY(reply->error() != QBluetoothTransferReply::NoError);
        QVERIFY(!reply->errorString().isEmpty());
        QCOMPARE(errorSpy.count(), 1);
    }

    QVERIFY(reply->isFinished());
    QVERIFY(!reply->isRunning());
}

void tst_QBluetoothTransferManager::tst_sendNullPointer()
{
    QBluetoothTransferRequest request(remoteAddress);
    QBluetoothTransferManager manager;
    QBluetoothTransferReply *reply = manager.put(request, 0);

    QVERIFY(reply);
    QCOMPARE(reply->isFinished(), true);
    QCOMPARE(reply->isRunning(), false);
    QCOMPARE(reply->manager(), &manager);
    QCOMPARE(reply->request(), request);
    QCOMPARE(reply->error(), QBluetoothTransferReply::FileNotFoundError);
}

QTEST_MAIN(tst_QBluetoothTransferManager)

#include "tst_qbluetoothtransfermanager.moc"
