/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNfc module of the Qt Toolkit.
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

#include <private/qnearfieldmanager_emulator_p.h>
#include <qnearfieldmanager.h>
#include <qndefnfctextrecord.h>
#include <qndefnfcurirecord.h>
#include <qndefmessage.h>
#include <qndefrecord.h>

QT_USE_NAMESPACE

Q_DECLARE_METATYPE(QNearFieldTarget*)
Q_DECLARE_METATYPE(QNearFieldTarget::Type)
Q_DECLARE_METATYPE(QNdefFilter)
Q_DECLARE_METATYPE(QNdefRecord::TypeNameFormat)

class tst_QNearFieldManager : public QObject
{
    Q_OBJECT

public:
    tst_QNearFieldManager();

private slots:
    void initTestCase();

    void targetDetected_data();
    void targetDetected();

    void unregisterNdefMessageHandler();

    void registerNdefMessageHandler();

    void registerNdefMessageHandler_type_data();
    void registerNdefMessageHandler_type();

    void registerNdefMessageHandler_filter_data();
    void registerNdefMessageHandler_filter();
};

tst_QNearFieldManager::tst_QNearFieldManager()
{
    QDir::setCurrent(QLatin1String(SRCDIR));

    qRegisterMetaType<QNdefMessage>();
    qRegisterMetaType<QNearFieldTarget *>();
}

void tst_QNearFieldManager::initTestCase()
{
    QNearFieldManagerPrivateImpl *emulatorBackend = new QNearFieldManagerPrivateImpl;
    QNearFieldManager manager(emulatorBackend, 0);

    QVERIFY(manager.isAvailable());
}

void tst_QNearFieldManager::targetDetected_data()
{
    QTest::addColumn<bool>("deleteTarget");

    QTest::newRow("AnyTarget") << false;
    QTest::newRow("NfcTagType1") << false;
    QTest::newRow("Delete Target") << true;
}

void tst_QNearFieldManager::targetDetected()
{
    QFETCH(bool, deleteTarget);

    QNearFieldManagerPrivateImpl *emulatorBackend = new QNearFieldManagerPrivateImpl;
    QNearFieldManager manager(emulatorBackend, 0);

    QSignalSpy targetDetectedSpy(&manager, SIGNAL(targetDetected(QNearFieldTarget*)));
    QSignalSpy targetLostSpy(&manager, SIGNAL(targetLost(QNearFieldTarget*)));

    manager.startTargetDetection();

    QTRY_VERIFY(!targetDetectedSpy.isEmpty());

    QNearFieldTarget *target = targetDetectedSpy.first().at(0).value<QNearFieldTarget *>();

    QSignalSpy disconnectedSpy(target, SIGNAL(disconnected()));

    QVERIFY(target);

    QVERIFY(!target->uid().isEmpty());

    if (!deleteTarget) {
        QTRY_VERIFY(!targetLostSpy.isEmpty());

        QNearFieldTarget *lostTarget = targetLostSpy.first().at(0).value<QNearFieldTarget *>();

        QCOMPARE(target, lostTarget);

        QVERIFY(!disconnectedSpy.isEmpty());
    } else {
        delete target;

        // wait for another targetDetected() without a targetLost() signal in between.
        targetDetectedSpy.clear();
        targetLostSpy.clear();

        QTRY_VERIFY(targetLostSpy.isEmpty() && !targetDetectedSpy.isEmpty());
    }

    manager.stopTargetDetection();
}

void tst_QNearFieldManager::unregisterNdefMessageHandler()
{
    QNearFieldManagerPrivateImpl *emulatorBackend = new QNearFieldManagerPrivateImpl;
    QNearFieldManager manager(emulatorBackend, 0);

    QVERIFY(!manager.unregisterNdefMessageHandler(-1));
    QVERIFY(!manager.unregisterNdefMessageHandler(0));
}

class MessageListener : public QObject
{
    Q_OBJECT

signals:
    void matchedNdefMessage(const QNdefMessage &message, QNearFieldTarget *target);
};

void tst_QNearFieldManager::registerNdefMessageHandler()
{
    QNearFieldManagerPrivateImpl *emulatorBackend = new QNearFieldManagerPrivateImpl;
    QNearFieldManager manager(emulatorBackend, 0);

    MessageListener listener;
    QSignalSpy messageSpy(&listener, SIGNAL(matchedNdefMessage(QNdefMessage,QNearFieldTarget*)));

    int id = manager.registerNdefMessageHandler(&listener,
                                                SIGNAL(matchedNdefMessage(QNdefMessage,QNearFieldTarget*)));

    QVERIFY(id != -1);

    QTRY_VERIFY(!messageSpy.isEmpty());

    const QNdefMessage message = messageSpy.first().at(0).value<QNdefMessage>();
    QNearFieldTarget *target = messageSpy.first().at(1).value<QNearFieldTarget *>();

    QVERIFY(target);

    QVERIFY(manager.unregisterNdefMessageHandler(id));
}

void tst_QNearFieldManager::registerNdefMessageHandler_type_data()
{
    QTest::addColumn<QNdefRecord::TypeNameFormat>("typeNameFormat");
    QTest::addColumn<QByteArray>("type");

    QTest::newRow("Image") << QNdefRecord::Mime << QByteArray("image/png");
    QTest::newRow("URI") << QNdefRecord::NfcRtd << QByteArray("U");
    QTest::newRow("Text") << QNdefRecord::NfcRtd << QByteArray("T");
}

void tst_QNearFieldManager::registerNdefMessageHandler_type()
{
    QFETCH(QNdefRecord::TypeNameFormat, typeNameFormat);
    QFETCH(QByteArray, type);

    QNearFieldManagerPrivateImpl *emulatorBackend = new QNearFieldManagerPrivateImpl;
    QNearFieldManager manager(emulatorBackend, 0);

    MessageListener listener;
    QSignalSpy messageSpy(&listener, SIGNAL(matchedNdefMessage(QNdefMessage,QNearFieldTarget*)));

    int id = manager.registerNdefMessageHandler(typeNameFormat, type, &listener,
                                                SIGNAL(matchedNdefMessage(QNdefMessage,QNearFieldTarget*)));

    QVERIFY(id != -1);

    QTRY_VERIFY(!messageSpy.isEmpty());

    const QNdefMessage message = messageSpy.first().at(0).value<QNdefMessage>();

    bool hasRecord = false;
    foreach (const QNdefRecord &record, message) {
        if (record.typeNameFormat() == typeNameFormat && record.type() == type) {
            hasRecord = true;
            break;
        }
    }

    QVERIFY(hasRecord);

    QNearFieldTarget *target = messageSpy.first().at(1).value<QNearFieldTarget *>();

    QVERIFY(target);
}

void tst_QNearFieldManager::registerNdefMessageHandler_filter_data()
{
    QTest::addColumn<QNdefFilter>("filter");

    QNdefFilter filter;

    QTest::newRow("Empty") << filter;

    filter.clear();
    filter.setOrderMatch(true);
    filter.appendRecord(QNdefRecord::Mime, "image/png");
    filter.appendRecord<QNdefNfcTextRecord>(2, 10);
    filter.appendRecord<QNdefNfcUriRecord>(1, 1);
    QTest::newRow("Image + Multiple Text + URI") << filter;

    filter.clear();
    filter.setOrderMatch(true);
    filter.appendRecord<QNdefNfcTextRecord>(1, 1);
    filter.appendRecord<QNdefNfcUriRecord>(1, 1);
    QTest::newRow("Text + URI") << filter;

    QNdefFilter::Record record;

    filter.clear();
    filter.setOrderMatch(false);
    filter.appendRecord<QNdefNfcUriRecord>(1, 1);
    record.typeNameFormat = QNdefRecord::NfcRtd;
    record.type = "T";
    record.minimum = 1;
    record.maximum = 1;
    filter.appendRecord(record);
    QTest::newRow("Unordered Text + URI") << filter;
}

void tst_QNearFieldManager::registerNdefMessageHandler_filter()
{
    QFETCH(QNdefFilter, filter);

    QNearFieldManagerPrivateImpl *emulatorBackend = new QNearFieldManagerPrivateImpl;
    QNearFieldManager manager(emulatorBackend, 0);

    MessageListener listener;
    QSignalSpy messageSpy(&listener, SIGNAL(matchedNdefMessage(QNdefMessage,QNearFieldTarget*)));

    int id = manager.registerNdefMessageHandler(filter, &listener,
                                                SIGNAL(matchedNdefMessage(QNdefMessage,QNearFieldTarget*)));

    QVERIFY(id != -1);

    QTRY_VERIFY(!messageSpy.isEmpty());

    const QNdefMessage message = messageSpy.first().at(0).value<QNdefMessage>();

    QNearFieldTarget *target = messageSpy.first().at(1).value<QNearFieldTarget *>();

    QVERIFY(target);
}

QTEST_MAIN(tst_QNearFieldManager)

// Unset the moc namespace which is not required for the following include.
#undef QT_BEGIN_MOC_NAMESPACE
#define QT_BEGIN_MOC_NAMESPACE
#undef QT_END_MOC_NAMESPACE
#define QT_END_MOC_NAMESPACE

#include "tst_qnearfieldmanager.moc"
