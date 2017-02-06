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
#include <private/qnearfieldtagtype1_p.h>
#include <qndefnfctextrecord.h>

QT_USE_NAMESPACE

Q_DECLARE_METATYPE(QNearFieldTarget*)

class tst_QNearFieldTagType1 : public QObject
{
    Q_OBJECT

public:
    tst_QNearFieldTagType1();

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
    QNearFieldTagType1 *target;
};

tst_QNearFieldTagType1::tst_QNearFieldTagType1()
:   emulatorBackend(0), manager(0), target(0)
{
    QDir::setCurrent(QLatin1String(SRCDIR));

    qRegisterMetaType<QNdefMessage>();
    qRegisterMetaType<QNearFieldTarget *>();
}

void tst_QNearFieldTagType1::init()
{
    emulatorBackend = new QNearFieldManagerPrivateImpl;
    manager = new QNearFieldManager(emulatorBackend, 0);
}

void tst_QNearFieldTagType1::cleanup()
{
    emulatorBackend->reset();

    delete manager;
    manager = 0;
    emulatorBackend = 0;
    target = 0;
}

void tst_QNearFieldTagType1::waitForMatchingTarget()
{
    QSignalSpy targetDetectedSpy(manager, SIGNAL(targetDetected(QNearFieldTarget*)));

    manager->startTargetDetection();

    QTRY_VERIFY(!targetDetectedSpy.isEmpty());

    target =
        qobject_cast<QNearFieldTagType1 *>(targetDetectedSpy.first().at(0).value<QNearFieldTarget *>());

    manager->stopTargetDetection();

    QVERIFY(target);

    QCOMPARE(target->type(), QNearFieldTarget::NfcTagType1);
}

void tst_QNearFieldTagType1::staticMemoryModel()
{
    waitForMatchingTarget();

    QVERIFY(target->accessMethods() & QNearFieldTarget::TagTypeSpecificAccess);

    QCOMPARE(target->version(), quint8(0x10));

    // readIdentification()
    {
        QNearFieldTarget::RequestId id = target->readIdentification();
        QVERIFY(target->waitForRequestCompleted(id));

        const QByteArray data = target->requestResponse(id).toByteArray();

        QCOMPARE(data.length(), 6);

        quint8 hr0 = data.at(0);
        //quint8 hr1 = data.at(1);
        const QByteArray uid4 = data.mid(2, 4);

        // verify NfcTagType1
        QVERIFY(hr0 & 0x10);

        QCOMPARE(uid4, target->uid().left(4));
    }

    // readAll()
    {
        // read all reads the first 120 bytes, prepended with HR0 and HR1.
        QNearFieldTarget::RequestId id = target->readAll();
        QVERIFY(target->waitForRequestCompleted(id));

        const QByteArray data = target->requestResponse(id).toByteArray();
        QCOMPARE(data.length(), 122);

        // verify NfcTagType1.
        QVERIFY(data.at(0) & 0x10);

        // NFC Magic Number means NDEF message present.
        QCOMPARE(quint8(data.at(10)) == 0xe1, target->hasNdefMessage());
    }

    // readByte()
    {
        QNearFieldTarget::RequestId id = target->readAll();
        QVERIFY(target->waitForRequestCompleted(id));

        const QByteArray data = target->requestResponse(id).toByteArray();

        for (int i = 0; i < 120; ++i) {
            id = target->readByte(i);
            QVERIFY(target->waitForRequestCompleted(id));

            quint8 byte = target->requestResponse(id).toUInt();

            QCOMPARE(byte, quint8(data.at(i + 2)));
        }
    }

    // writeByte()
    {
        for (int i = 0; i < 8; ++i) {
            QNearFieldTarget::RequestId id = target->readByte(i);
            QVERIFY(target->waitForRequestCompleted(id));

            quint8 byte = target->requestResponse(id).toUInt();

            id = target->writeByte(i, 0x55);
            QVERIFY(!target->waitForRequestCompleted(id,50));

            QVERIFY(!target->requestResponse(id).isValid());

            id = target->readByte(i);
            QVERIFY(target->waitForRequestCompleted(id));

            quint8 readByte = target->requestResponse(id).toUInt();
            QCOMPARE(readByte, byte);
        }

        for (int i = 8; i < 104; ++i) {
            // Write 0x55
            QNearFieldTarget::RequestId id = target->writeByte(i, 0x55);
            QVERIFY(target->waitForRequestCompleted(id));
            QVERIFY(target->requestResponse(id).toBool());

            id = target->readByte(i);
            QVERIFY(target->waitForRequestCompleted(id));
            quint8 readByte = target->requestResponse(id).toUInt();
            QCOMPARE(readByte, quint8(0x55));

            // Write 0xaa
            id = target->writeByte(i, 0xaa);
            QVERIFY(target->waitForRequestCompleted(id));
            QVERIFY(target->requestResponse(id).toBool());

            id = target->readByte(i);
            QVERIFY(target->waitForRequestCompleted(id));
            readByte = target->requestResponse(id).toUInt();
            QCOMPARE(readByte, quint8(0xaa));

            // Write 0x55 without erase, result should be 0xff
            id = target->writeByte(i, 0x55, QNearFieldTagType1::WriteOnly);
            QVERIFY(target->waitForRequestCompleted(id));
            QVERIFY(target->requestResponse(id).toBool());

            id = target->readByte(i);
            QVERIFY(target->waitForRequestCompleted(id));
            readByte = target->requestResponse(id).toUInt();
            QCOMPARE(readByte, quint8(0xff));
        }

        for (int i = 104; i < 120; ++i) {
            QNearFieldTarget::RequestId id = target->readByte(i);
            QVERIFY(target->waitForRequestCompleted(id));

            quint8 byte = target->requestResponse(id).toUInt();

            id = target->writeByte(i, 0x55);
            QVERIFY(!target->waitForRequestCompleted(id,50));

            QVERIFY(!target->requestResponse(id).isValid());

            id = target->readByte(i);
            QVERIFY(target->waitForRequestCompleted(id));

            quint8 readByte = target->requestResponse(id).toUInt();
            QCOMPARE(readByte, byte);
        }
    }
}

void tst_QNearFieldTagType1::dynamicMemoryModel()
{
    bool testedStatic = false;
    bool testedDynamic = false;

    QList<QByteArray> seenIds;
    forever {
        waitForMatchingTarget();

        QNearFieldTarget::RequestId id = target->readIdentification();
        QVERIFY(target->waitForRequestCompleted(id));

        const QByteArray data = target->requestResponse(id).toByteArray();
        if (seenIds.contains(data))
            break;
        else
            seenIds.append(data);

        quint8 hr0 = data.at(0);
        bool dynamic = (((hr0 & 0xf0) == 0x10) && ((hr0 & 0x0f) != 0x01));

        if (dynamic) {
            testedDynamic = true;

            // block 0, UID is locked
            {
                QNearFieldTarget::RequestId id = target->readBlock(0x00);
                QVERIFY(target->waitForRequestCompleted(id));

                const QByteArray block = target->requestResponse(id).toByteArray();

                QCOMPARE(target->uid(), block.left(7));
                QCOMPARE(quint8(block.at(7)), quint8(0x00));

                id = target->writeBlock(0x00, QByteArray(8, quint8(0x55)));
                QVERIFY(!target->waitForRequestCompleted(id,50));
                QVERIFY(!target->requestResponse(id).isValid());

                QCOMPARE(target->uid(), block.left(7));
                QCOMPARE(quint8(block.at(7)), quint8(0x00));
            }

            // static data area
            QNearFieldTarget::RequestId id = target->readSegment(0);
            QVERIFY(target->waitForRequestCompleted(id));
            QByteArray segment = target->requestResponse(id).toByteArray();
            for (int i = 1; i < 0x0d; ++i) {
                // Write 0x55
                id = target->writeBlock(i, QByteArray(8, quint8(0x55)));
                QVERIFY(target->waitForRequestCompleted(id));
                QVERIFY(target->requestResponse(id).toBool());

                id = target->readBlock(i);
                QVERIFY(target->waitForRequestCompleted(id));
                QCOMPARE(target->requestResponse(id).toByteArray(), QByteArray(8, quint8(0x55)));

                segment.replace(i * 8, 8, QByteArray(8, quint8(0x55)));

                id = target->readSegment(0);
                QVERIFY(target->waitForRequestCompleted(id));
                QCOMPARE(target->requestResponse(id).toByteArray(), segment);

                // Write 0xaa
                id = target->writeBlock(i, QByteArray(8, quint8(0xaa)));
                QVERIFY(target->waitForRequestCompleted(id));
                QVERIFY(target->requestResponse(id).toBool());

                id = target->readBlock(i);
                QVERIFY(target->waitForRequestCompleted(id));
                QCOMPARE(target->requestResponse(id).toByteArray(), QByteArray(8, quint8(0xaa)));

                segment.replace(i * 8, 8, QByteArray(8, quint8(0xaa)));

                id = target->readSegment(0);
                QVERIFY(target->waitForRequestCompleted(id));
                QCOMPARE(target->requestResponse(id).toByteArray(), segment);

                // Write 0x55 without erase, result should be 0xff
                id = target->writeBlock(i, QByteArray(8, quint8(0x55)),
                                        QNearFieldTagType1::WriteOnly);
                QVERIFY(target->waitForRequestCompleted(id));
                QVERIFY(target->requestResponse(id).toBool());

                id = target->readBlock(i);
                QVERIFY(target->waitForRequestCompleted(id));
                QCOMPARE(target->requestResponse(id).toByteArray(), QByteArray(8, quint8(0xff)));

                segment.replace(i * 8, 8, QByteArray(8, quint8(0xff)));

                id = target->readSegment(0);
                QVERIFY(target->waitForRequestCompleted(id));
                QCOMPARE(target->requestResponse(id).toByteArray(), segment);
            }

            // static / dynamic reserved lock area
            for (int i = 0x0d; i < 0x10; ++i) {
                id = target->readBlock(i);
                QVERIFY(target->waitForRequestCompleted(id));
                QByteArray block = target->requestResponse(id).toByteArray();

                id = target->writeBlock(i, QByteArray(8, quint8(0x55)));
                QVERIFY(!target->waitForRequestCompleted(id,50));
                QVERIFY(!target->requestResponse(id).isValid());

                id = target->readBlock(i);
                QVERIFY(target->waitForRequestCompleted(id));
                QCOMPARE(target->requestResponse(id).toByteArray(), block);
            }
        } else {
            testedStatic = true;

            for (int i = 0; i < 256; ++i) {
                QNearFieldTarget::RequestId id = target->readBlock(i);
                QVERIFY(!target->waitForRequestCompleted(id,50));

                id = target->writeBlock(i, QByteArray(8, quint8(0x55)));
                QVERIFY(!target->waitForRequestCompleted(id,50));
            }
            for (int i = 0; i < 16; ++i) {
                QNearFieldTarget::RequestId id = target->readSegment(i);
                QVERIFY(!target->waitForRequestCompleted(id,50));
            }
        }
    }

    QVERIFY(testedStatic);
    QVERIFY(testedDynamic);
}

void tst_QNearFieldTagType1::ndefMessages()
{
    QByteArray firstId;
    forever {
        waitForMatchingTarget();

        QNearFieldTarget::RequestId id = target->readIdentification();
        QVERIFY(target->waitForRequestCompleted(id));

        const QByteArray uid = target->requestResponse(id).toByteArray();
        if (firstId.isEmpty())
            firstId = uid;
        else if (firstId == uid)
            break;

        QVERIFY(target->hasNdefMessage());

        QSignalSpy ndefMessageReadSpy(target, SIGNAL(ndefMessageRead(QNdefMessage)));
        QSignalSpy requestCompletedSpy(target,
                                       SIGNAL(requestCompleted(QNearFieldTarget::RequestId)));
        QSignalSpy errorSpy(target,
                            SIGNAL(error(QNearFieldTarget::Error,QNearFieldTarget::RequestId)));

        QNearFieldTarget::RequestId readId = target->readNdefMessages();

        QVERIFY(readId.isValid());

        QNearFieldTarget::RequestId completedId;

        while (completedId != readId) {
            QTRY_VERIFY(!requestCompletedSpy.isEmpty() && errorSpy.isEmpty());

            completedId =
                requestCompletedSpy.takeFirst().first().value<QNearFieldTarget::RequestId>();
        }

        QList<QNdefMessage> ndefMessages;
        for (int i = 0; i < ndefMessageReadSpy.count(); ++i)
            ndefMessages.append(ndefMessageReadSpy.at(i).first().value<QNdefMessage>());

        QList<QNdefMessage> messages;
        QNdefNfcTextRecord textRecord;
        textRecord.setText(QStringLiteral("tst_QNearFieldTagType1::ndefMessages"));

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

        requestCompletedSpy.clear();
        errorSpy.clear();

        QSignalSpy ndefMessageWriteSpy(target, SIGNAL(ndefMessagesWritten()));
        QNearFieldTarget::RequestId writeId = target->writeNdefMessages(messages);

        QVERIFY(writeId.isValid());

        completedId = QNearFieldTarget::RequestId();

        while (completedId != writeId) {
            QTRY_VERIFY(!requestCompletedSpy.isEmpty() && errorSpy.isEmpty());

            completedId =
                requestCompletedSpy.takeFirst().first().value<QNearFieldTarget::RequestId>();
        }

        QVERIFY(!ndefMessageWriteSpy.isEmpty());

        QVERIFY(target->hasNdefMessage());

        ndefMessageReadSpy.clear();
        requestCompletedSpy.clear();
        errorSpy.clear();

        readId = target->readNdefMessages();

        QVERIFY(readId.isValid());

        completedId = QNearFieldTarget::RequestId();

        while (completedId != readId) {
            QTRY_VERIFY(!requestCompletedSpy.isEmpty() && errorSpy.isEmpty());

            completedId =
                requestCompletedSpy.takeFirst().first().value<QNearFieldTarget::RequestId>();
        }

        QList<QNdefMessage> storedMessages;
        for (int i = 0; i < ndefMessageReadSpy.count(); ++i)
            storedMessages.append(ndefMessageReadSpy.at(i).first().value<QNdefMessage>());

        QVERIFY(ndefMessages != storedMessages);

        QCOMPARE(messages, storedMessages);
    }
}

QTEST_MAIN(tst_QNearFieldTagType1)

// Unset the moc namespace which is not required for the following include.
#undef QT_BEGIN_MOC_NAMESPACE
#define QT_BEGIN_MOC_NAMESPACE
#undef QT_END_MOC_NAMESPACE
#define QT_END_MOC_NAMESPACE

#include "tst_qnearfieldtagtype1.moc"
