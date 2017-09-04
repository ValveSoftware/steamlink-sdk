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
#include <qndefmessage.h>
#include <private/qnearfieldtagtype2_p.h>
#include <qndefnfctextrecord.h>

QT_USE_NAMESPACE

Q_DECLARE_METATYPE(QNearFieldTarget*)

static const char * const deadbeef = "\xde\xad\xbe\xef";

class tst_QNearFieldTagType2 : public QObject
{
    Q_OBJECT

public:
    tst_QNearFieldTagType2();

private slots:
    void init();
    void cleanup();

    void staticMemoryModel();
    void dynamicMemoryModel();

    void ndefMessages();

private:
    void waitForMatchingTarget();

    QNearFieldManagerPrivateImpl *emulatorBackend;
    QNearFieldManager *manager;
    QNearFieldTagType2 *target;
};

tst_QNearFieldTagType2::tst_QNearFieldTagType2()
:   emulatorBackend(0), manager(0), target(0)
{
    QDir::setCurrent(QLatin1String(SRCDIR));

    qRegisterMetaType<QNdefMessage>();
    qRegisterMetaType<QNearFieldTarget *>();
}

void tst_QNearFieldTagType2::init()
{
    emulatorBackend = new QNearFieldManagerPrivateImpl;
    manager = new QNearFieldManager(emulatorBackend, 0);
}

void tst_QNearFieldTagType2::cleanup()
{
    emulatorBackend->reset();

    delete manager;
    manager = 0;
    emulatorBackend = 0;
    target = 0;
}

void tst_QNearFieldTagType2::waitForMatchingTarget()
{
    QSignalSpy targetDetectedSpy(manager, SIGNAL(targetDetected(QNearFieldTarget*)));

    manager->startTargetDetection();

    QTRY_VERIFY(!targetDetectedSpy.isEmpty());

    target = qobject_cast<QNearFieldTagType2 *>(targetDetectedSpy.first().at(0).value<QNearFieldTarget *>());

    manager->stopTargetDetection();

    QVERIFY(target);

    QCOMPARE(target->type(), QNearFieldTarget::NfcTagType2);
}

void tst_QNearFieldTagType2::staticMemoryModel()
{
    waitForMatchingTarget();

    QVERIFY(target->accessMethods() & QNearFieldTarget::TagTypeSpecificAccess);

    QCOMPARE(target->version(), quint8(0x10));

    // readBlock(), writeBlock()
    {
        for (int i = 0; i < 2; ++i) {
            QNearFieldTarget::RequestId id = target->readBlock(i);
            QVERIFY(target->waitForRequestCompleted(id));

            const QByteArray block = target->requestResponse(id).toByteArray();

            id = target->writeBlock(i, QByteArray(4, 0x55));
            QVERIFY(target->waitForRequestCompleted(id));
            QVERIFY(!target->requestResponse(id).toBool());

            id = target->readBlock(i);
            QVERIFY(target->waitForRequestCompleted(id));

            const QByteArray readBlock = target->requestResponse(id).toByteArray();
            QCOMPARE(readBlock, block);
        }

        for (int i = 3; i < 16; ++i) {
            // Read initial data
            QNearFieldTarget::RequestId id = target->readBlock(i);
            QVERIFY(target->waitForRequestCompleted(id));
            QByteArray initialBlock = target->requestResponse(id).toByteArray();

            // Write 0x55
            id = target->writeBlock(i, QByteArray(4, 0x55));
            QVERIFY(target->waitForRequestCompleted(id));
            QVERIFY(target->requestResponse(id).toBool());

            id = target->readBlock(i);
            QVERIFY(target->waitForRequestCompleted(id));
            QByteArray readBlock = target->requestResponse(id).toByteArray();
            QCOMPARE(readBlock, QByteArray(4, 0x55) + initialBlock.mid(4));

            // Write 0xaa
            id = target->writeBlock(i, QByteArray(4, char(0xaa)));
            QVERIFY(target->waitForRequestCompleted(id));
            QVERIFY(target->requestResponse(id).toBool());

            id = target->readBlock(i);
            QVERIFY(target->waitForRequestCompleted(id));
            readBlock = target->requestResponse(id).toByteArray();
            QCOMPARE(readBlock, QByteArray(4, char(0xaa)) + initialBlock.mid(4));
        }
    }
}

void tst_QNearFieldTagType2::dynamicMemoryModel()
{
    bool testedStatic = false;
    bool testedDynamic = false;

    QList<QByteArray> seenIds;
    forever {
        waitForMatchingTarget();

        QVERIFY(target->accessMethods() & QNearFieldTarget::TagTypeSpecificAccess);

        QNearFieldTarget::RequestId id = target->readBlock(0);
        QVERIFY(target->waitForRequestCompleted(id));

        const QByteArray data = target->requestResponse(id).toByteArray();
        const QByteArray uid = data.left(3) + data.mid(4, 4);

        if (seenIds.contains(uid))
            break;
        else
            seenIds.append(uid);

        QCOMPARE(target->version(), quint8(0x10));

        bool dynamic = target->memorySize() > 1024;

        if (dynamic) {
            testedDynamic = true;

            int totalBlocks = target->memorySize() / 4;
            int sector1Blocks = qMin(totalBlocks - 256, 256);

            // default sector is sector 0
            for (int i = 3; i < 256; ++i) {
                // Write 0x55
                QNearFieldTarget::RequestId id = target->writeBlock(i, deadbeef);
                QVERIFY(target->waitForRequestCompleted(id));
                QVERIFY(target->requestResponse(id).toBool());
            }

            // change to sector 1
            {
                QNearFieldTarget::RequestId id = target->selectSector(1);
                QVERIFY(target->waitForRequestCompleted(id));
                QVERIFY(target->requestResponse(id).toBool());
            }

            for (int i = 0; i < sector1Blocks; ++i) {
                QNearFieldTarget::RequestId id = target->readBlock(i);
                QVERIFY(target->waitForRequestCompleted(id));
                QByteArray initialBlock = target->requestResponse(id).toByteArray();

                QVERIFY(initialBlock.left(4) != deadbeef);

                // Write 0x55
                id = target->writeBlock(i, QByteArray(4, 0x55));
                QVERIFY(target->waitForRequestCompleted(id));
                QVERIFY(target->requestResponse(id).toBool());

                id = target->readBlock(i);
                QVERIFY(target->waitForRequestCompleted(id));
                QByteArray readBlock = target->requestResponse(id).toByteArray();
                QCOMPARE(readBlock, QByteArray(4, 0x55) + initialBlock.mid(4));

                // Write 0xaa
                id = target->writeBlock(i, QByteArray(4, char(0xaa)));
                QVERIFY(target->waitForRequestCompleted(id));
                QVERIFY(target->requestResponse(id).toBool());

                id = target->readBlock(i);
                QVERIFY(target->waitForRequestCompleted(id));
                readBlock = target->requestResponse(id).toByteArray();
                QCOMPARE(readBlock, QByteArray(4, char(0xaa)) + initialBlock.mid(4));
            }

            // change to sector 0
            {
                QNearFieldTarget::RequestId id = target->selectSector(0);
                QVERIFY(target->waitForRequestCompleted(id));
                QVERIFY(target->requestResponse(id).toBool());
            }

            for (int i = 3; i < 256; ++i) {
                QNearFieldTarget::RequestId id = target->readBlock(i);
                QVERIFY(target->waitForRequestCompleted(id));
                QByteArray readBlock = target->requestResponse(id).toByteArray();

                QVERIFY(readBlock.left(4) == deadbeef);
            }
        } else {
            testedStatic = true;

            QNearFieldTarget::RequestId id = target->selectSector(1);
            QVERIFY(target->waitForRequestCompleted(id));
            QVERIFY(!target->requestResponse(id).toBool());
        }
    }

    QVERIFY(testedStatic);
    QVERIFY(testedDynamic);
}

void tst_QNearFieldTagType2::ndefMessages()
{
    QSKIP("Not implemented");

    QByteArray firstId;
    forever {
        waitForMatchingTarget();

        QNearFieldTarget::RequestId id = target->readBlock(0);
        QVERIFY(target->waitForRequestCompleted(id));

        QByteArray uid = target->requestResponse(id).toByteArray().left(3);

        id = target->readBlock(1);
        QVERIFY(target->waitForRequestCompleted(id));
        uid.append(target->requestResponse(id).toByteArray());

        if (firstId.isEmpty())
            firstId = uid;
        else if (firstId == uid)
            break;

        QVERIFY(target->hasNdefMessage());

        QSignalSpy ndefMessageReadSpy(target, SIGNAL(ndefMessageRead(QNdefMessage)));

        target->readNdefMessages();

        QTRY_VERIFY(!ndefMessageReadSpy.isEmpty());

        QList<QNdefMessage> ndefMessages;
        for (int i = 0; i < ndefMessageReadSpy.count(); ++i)
            ndefMessages.append(ndefMessageReadSpy.at(i).first().value<QNdefMessage>());

        QList<QNdefMessage> messages;
        QNdefNfcTextRecord textRecord;
        textRecord.setText(QStringLiteral("tst_QNearFieldTagType2::ndefMessages"));

        QNdefMessage message;
        message.append(textRecord);

        if (target->memorySize() > 120) {
            QNdefRecord record;
            record.setTypeNameFormat(QNdefRecord::ExternalRtd);
            record.setType("org.qt-project:ndefMessagesTest");
            record.setPayload(QByteArray(120, quint8(0x55)));
            message.append(record);
        }

        messages.append(message);

        QSignalSpy ndefMessageWriteSpy(target, SIGNAL(ndefMessagesWritten()));
        target->writeNdefMessages(messages);

        QTRY_VERIFY(!ndefMessageWriteSpy.isEmpty());

        QVERIFY(target->hasNdefMessage());

        ndefMessageReadSpy.clear();

        target->readNdefMessages();

        QTRY_VERIFY(!ndefMessageReadSpy.isEmpty());

        QList<QNdefMessage> storedMessages;
        for (int i = 0; i < ndefMessageReadSpy.count(); ++i)
            storedMessages.append(ndefMessageReadSpy.at(i).first().value<QNdefMessage>());

        QVERIFY(ndefMessages != storedMessages);

        QVERIFY(messages == storedMessages);
    }
}

QTEST_MAIN(tst_QNearFieldTagType2)

// Unset the moc namespace which is not required for the following include.
#undef QT_BEGIN_MOC_NAMESPACE
#define QT_BEGIN_MOC_NAMESPACE
#undef QT_END_MOC_NAMESPACE
#define QT_END_MOC_NAMESPACE

#include "tst_qnearfieldtagtype2.moc"
