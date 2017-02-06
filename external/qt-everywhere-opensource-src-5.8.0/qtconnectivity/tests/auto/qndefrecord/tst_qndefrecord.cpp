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

#include <qndefrecord.h>
#include <qndefnfctextrecord.h>
#include <qndefnfcurirecord.h>
#include <qqmlndefrecord.h>

QT_USE_NAMESPACE

Q_DECLARE_METATYPE(QNdefRecord::TypeNameFormat)

class tst_QNdefRecord : public QObject
{
    Q_OBJECT

public:
    tst_QNdefRecord();
    ~tst_QNdefRecord();

private slots:
    void tst_record();

    void tst_textRecord_data();
    void tst_textRecord();

    void tst_uriRecord_data();
    void tst_uriRecord();

    void tst_declarative_record_data();
    void tst_declarative_record();

    void tst_declarativeChangedSignals();
};

tst_QNdefRecord::tst_QNdefRecord()
{
}

tst_QNdefRecord::~tst_QNdefRecord()
{
}

void tst_QNdefRecord::tst_record()
{
    // test empty record
    {
        QNdefRecord record;

        QVERIFY(record.isEmpty());
        QCOMPARE(record.typeNameFormat(), QNdefRecord::Empty);
        QVERIFY(record.type().isEmpty());
        QVERIFY(record.id().isEmpty());
        QVERIFY(record.payload().isEmpty());

        QVERIFY(!record.isRecordType<QNdefNfcTextRecord>());
        QVERIFY(!record.isRecordType<QNdefNfcUriRecord>());

        QCOMPARE(record, QNdefRecord());
        QVERIFY(!(record != QNdefRecord()));

        QQmlNdefRecord declRecord;
        QCOMPARE(declRecord.record(), record);
        QCOMPARE(declRecord.type(), QString());
        QCOMPARE(declRecord.typeNameFormat(), QQmlNdefRecord::Empty);
    }

    // test type name format
    {
        QNdefRecord record;

        record.setTypeNameFormat(QNdefRecord::Empty);
        QCOMPARE(record.typeNameFormat(), QNdefRecord::Empty);

        record.setTypeNameFormat(QNdefRecord::NfcRtd);
        QCOMPARE(record.typeNameFormat(), QNdefRecord::NfcRtd);

        record.setTypeNameFormat(QNdefRecord::Mime);
        QCOMPARE(record.typeNameFormat(), QNdefRecord::Mime);

        record.setTypeNameFormat(QNdefRecord::Uri);
        QCOMPARE(record.typeNameFormat(), QNdefRecord::Uri);

        record.setTypeNameFormat(QNdefRecord::ExternalRtd);
        QCOMPARE(record.typeNameFormat(), QNdefRecord::ExternalRtd);

        record.setTypeNameFormat(QNdefRecord::Unknown);
        QCOMPARE(record.typeNameFormat(), QNdefRecord::Unknown);

        record.setTypeNameFormat(QNdefRecord::TypeNameFormat(6));
        QCOMPARE(record.typeNameFormat(), QNdefRecord::Unknown);

        record.setTypeNameFormat(QNdefRecord::TypeNameFormat(7));
        QCOMPARE(record.typeNameFormat(), QNdefRecord::Unknown);
    }

    // test type
    {
        QNdefRecord record;

        record.setType("test type");
        QCOMPARE(record.type(), QByteArray("test type"));
    }

    // test id
    {
        QNdefRecord record;

        record.setId("test id");
        QCOMPARE(record.id(), QByteArray("test id"));
    }

    // test payload
    {
        QNdefRecord record;

        record.setPayload("test payload");
        QVERIFY(!record.isEmpty());
        QVERIFY(!record.payload().isEmpty());
        QCOMPARE(record.payload(), QByteArray("test payload"));
    }

    // test copy
    {
        QNdefRecord record;
        record.setTypeNameFormat(QNdefRecord::ExternalRtd);
        record.setType("qt-project.org:test-rtd");
        record.setId("test id");
        record.setPayload("test payload");

        QNdefRecord ccopy(record);

        QCOMPARE(record.typeNameFormat(), ccopy.typeNameFormat());
        QCOMPARE(record.type(), ccopy.type());
        QCOMPARE(record.id(), ccopy.id());
        QCOMPARE(record.payload(), ccopy.payload());

        QVERIFY(record == ccopy);
        QVERIFY(!(record != ccopy));

        QNdefRecord acopy;
        acopy = record;

        QCOMPARE(record.typeNameFormat(), acopy.typeNameFormat());
        QCOMPARE(record.type(), acopy.type());
        QCOMPARE(record.id(), acopy.id());
        QCOMPARE(record.payload(), acopy.payload());

        QVERIFY(record == acopy);
        QVERIFY(!(record != acopy));

        QVERIFY(record != QNdefRecord());
    }

    // test comparison
    {
        QNdefRecord record;
        record.setTypeNameFormat(QNdefRecord::ExternalRtd);
        record.setType("qt-project.org:test-rtd");
        record.setId("test id");
        record.setPayload("test payload");

        QNdefRecord other;
        other.setTypeNameFormat(QNdefRecord::ExternalRtd);
        other.setType("qt-project.org:test-other-rtd");
        other.setId("test other id");
        other.setPayload("test other payload");

        QVERIFY(record != other);
    }
}

void tst_QNdefRecord::tst_textRecord_data()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<QString>("text");
    QTest::addColumn<bool>("utf8");
    QTest::addColumn<QByteArray>("payload");


    QTest::newRow("en_US utf8") << QString::fromLatin1("en_US")
                                << QString::fromLatin1("Test String")
                                << true
                                << QByteArray::fromHex("05656E5F55535465737420537472696E67");
    QTest::newRow("en_US utf16") << QString::fromLatin1("en_US")
                                 << QString::fromLatin1("Test String")
                                 << false
                                 << QByteArray::fromHex("85656E5F5553FEFF005400650073007400200053007400720069006E0067");
    QTest::newRow("ja_JP utf8") << QString::fromLatin1("ja_JP")
                                << QString::fromUtf8("\343\203\206\343\202\271\343\203\210\346"
                                                     "\226\207\345\255\227\345\210\227")
                                << true
                                << QByteArray::fromHex("056A615F4A50E38386E382B9E38388E69687E5AD97E58897");
    QTest::newRow("ja_JP utf16") << QString::fromLatin1("ja_JP")
                                 << QString::fromUtf8("\343\203\206\343\202\271\343\203\210\346"
                                                      "\226\207\345\255\227\345\210\227")
                                 << false
                                 << QByteArray::fromHex("856A615F4A50FEFF30C630B930C865875B575217");
}

void tst_QNdefRecord::tst_textRecord()
{
    QFETCH(QString, locale);
    QFETCH(QString, text);
    QFETCH(bool, utf8);
    QFETCH(QByteArray, payload);

    // test setters
    {
        QNdefNfcTextRecord record;
        record.setLocale(locale);
        record.setText(text);
        record.setEncoding(utf8 ? QNdefNfcTextRecord::Utf8 : QNdefNfcTextRecord::Utf16);

        QCOMPARE(record.payload(), payload);

        QVERIFY(record != QNdefRecord());

        QQmlNdefRecord declRecord(record);
        QCOMPARE(declRecord.record(), QNdefRecord(record));
        QCOMPARE(declRecord.type(), QString("T"));
        QCOMPARE(declRecord.typeNameFormat(), QQmlNdefRecord::NfcRtd);
    }

    // test getters
    {
        QNdefNfcTextRecord record;
        record.setPayload(payload);

        QCOMPARE(record.locale(), locale);
        QCOMPARE(record.text(), text);
        QCOMPARE(record.encoding() == QNdefNfcTextRecord::Utf8, utf8);
    }

    // test copy
    {
        QNdefRecord record;
        record.setTypeNameFormat(QNdefRecord::NfcRtd);
        record.setType("T");
        record.setPayload(payload);

        QVERIFY(!record.isRecordType<QNdefRecord>());
        QVERIFY(record.isRecordType<QNdefNfcTextRecord>());
        QVERIFY(!record.isRecordType<QNdefNfcUriRecord>());

        QNdefNfcTextRecord textRecord(record);

        QVERIFY(!textRecord.isEmpty());

        QVERIFY(record == textRecord);

        QCOMPARE(textRecord.locale(), locale);
        QCOMPARE(textRecord.text(), text);
        QCOMPARE(textRecord.encoding() == QNdefNfcTextRecord::Utf8, utf8);
    }
}

void tst_QNdefRecord::tst_uriRecord_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QByteArray>("payload");


    QTest::newRow("http") << QString::fromLatin1("http://qt-project.org/")
                                << QByteArray::fromHex("0371742d70726f6a6563742e6f72672f");
    QTest::newRow("tel") << QString::fromLatin1("tel:+1234567890")
                         << QByteArray::fromHex("052B31323334353637383930");
    QTest::newRow("mailto") << QString::fromLatin1("mailto:test@example.com")
                            << QByteArray::fromHex("0674657374406578616D706C652E636F6D");
    QTest::newRow("urn") << QString::fromLatin1("urn:nfc:ext:qt-project.org:test")
                         << QByteArray::fromHex("136E66633A6578743A71742D70726F6A6563742E6F72673A74657374");
}

void tst_QNdefRecord::tst_uriRecord()
{
    QFETCH(QString, url);
    QFETCH(QByteArray, payload);

    // test setters
    {
        QNdefNfcUriRecord record;
        record.setUri(QUrl(url));

        QCOMPARE(record.payload(), payload);

        QVERIFY(record != QNdefRecord());

        QQmlNdefRecord declRecord(record);
        QCOMPARE(declRecord.record(), QNdefRecord(record));
        QCOMPARE(declRecord.type(), QString("U"));
        QCOMPARE(declRecord.typeNameFormat(), QQmlNdefRecord::NfcRtd);
    }

    // test getters
    {
        QNdefNfcUriRecord record;
        record.setPayload(payload);

        QCOMPARE(record.uri(), QUrl(url));
    }

    // test copy
    {
        QNdefRecord record;
        record.setTypeNameFormat(QNdefRecord::NfcRtd);
        record.setType("U");
        record.setPayload(payload);

        QVERIFY(!record.isRecordType<QNdefRecord>());
        QVERIFY(!record.isRecordType<QNdefNfcTextRecord>());
        QVERIFY(record.isRecordType<QNdefNfcUriRecord>());

        QNdefNfcUriRecord uriRecord(record);

        QVERIFY(!uriRecord.isEmpty());

        QVERIFY(record == uriRecord);

        QCOMPARE(uriRecord.uri(), QUrl(url));
    }
}

void tst_QNdefRecord::tst_declarative_record_data()
{
    QTest::addColumn<QNdefRecord::TypeNameFormat>("typeNameFormat");
    QTest::addColumn<QByteArray>("type");

    QTest::newRow("NfcRtd:U") << QNdefRecord::NfcRtd << QByteArray("U");
    QTest::newRow("NfcRtd:T") << QNdefRecord::NfcRtd << QByteArray("T");
    QTest::newRow("Empty:BLAH") << QNdefRecord::Empty << QByteArray("BLAH");
    QTest::newRow("Empty") << QNdefRecord::Empty << QByteArray("");
    QTest::newRow("Unknown") << QNdefRecord::Unknown << QByteArray("BLAHfoo");
    QTest::newRow("Mime") << QNdefRecord::Mime << QByteArray("foobar");
    QTest::newRow("ExternalRtd") << QNdefRecord::ExternalRtd << QByteArray("");
}

void tst_QNdefRecord::tst_declarative_record()
{
    QFETCH(QNdefRecord::TypeNameFormat, typeNameFormat);
    QFETCH(QByteArray, type);

    {
        QNdefRecord record;
        record.setTypeNameFormat(typeNameFormat);
        record.setType(type);
        QCOMPARE(record.typeNameFormat(), typeNameFormat);
        QCOMPARE(record.type(), type);

        QQmlNdefRecord declRecord(record);
        QCOMPARE(declRecord.record(), record);
        QCOMPARE(declRecord.record().typeNameFormat(), typeNameFormat);
        QCOMPARE(declRecord.record().type(), type);
        QCOMPARE(declRecord.type(), QString(type));
        QCOMPARE(declRecord.typeNameFormat(), static_cast<QQmlNdefRecord::TypeNameFormat>(typeNameFormat));

        QQmlNdefRecord declRecord2;
        declRecord2.setRecord(record);
        QCOMPARE(declRecord2.record(), record);
        QCOMPARE(declRecord2.record().typeNameFormat(), typeNameFormat);
        QCOMPARE(declRecord2.record().type(), type);
        QCOMPARE(declRecord2.type(), QString(type));
        QCOMPARE(declRecord2.typeNameFormat(), static_cast<QQmlNdefRecord::TypeNameFormat>(typeNameFormat));

        QQmlNdefRecord declRecord3;
        declRecord3.setTypeNameFormat((QQmlNdefRecord::TypeNameFormat)typeNameFormat);
        declRecord3.setType(type);
        QCOMPARE(declRecord3.type(), QString(type));
        QCOMPARE(declRecord3.record().typeNameFormat(), typeNameFormat);
        QCOMPARE(declRecord3.record().type(), type);
        QCOMPARE(declRecord3.typeNameFormat(), static_cast<QQmlNdefRecord::TypeNameFormat>(typeNameFormat));
    }
}

void tst_QNdefRecord::tst_declarativeChangedSignals()
{
    QQmlNdefRecord record;
    QSignalSpy typeSpy(&record, SIGNAL(typeChanged()));
    QSignalSpy tnfSpy(&record, SIGNAL(typeNameFormatChanged()));
    QSignalSpy recordSpy(&record, SIGNAL(recordChanged()));

    QCOMPARE(typeSpy.count(), 0);
    QCOMPARE(recordSpy.count(), 0);

    record.setType("U");
    record.setTypeNameFormat(QQmlNdefRecord::NfcRtd);
    QCOMPARE(typeSpy.count(), 1);
    QCOMPARE(tnfSpy.count(), 1);
    QCOMPARE(recordSpy.count(), 0);
    QCOMPARE(record.type(), QString("U"));
    QCOMPARE(record.record().type(), QByteArray("U"));
    QCOMPARE(record.record().typeNameFormat(), QNdefRecord::NfcRtd);
    QCOMPARE(record.typeNameFormat(), QQmlNdefRecord::NfcRtd);

    record.setType("U"); //same value, no signal
    QCOMPARE(typeSpy.count(), 1);
    QCOMPARE(tnfSpy.count(), 1);
    QCOMPARE(recordSpy.count(), 0);
    QCOMPARE(record.type(), QString("U"));
    QCOMPARE(record.record().type(), QByteArray("U"));
    QCOMPARE(record.record().typeNameFormat(), QNdefRecord::NfcRtd);
    QCOMPARE(record.typeNameFormat(), QQmlNdefRecord::NfcRtd);

    record.setType("blah");
    record.setType("blah2");
    record.setTypeNameFormat(QQmlNdefRecord::ExternalRtd);
    QCOMPARE(typeSpy.count(), 3);
    QCOMPARE(tnfSpy.count(), 2);
    QCOMPARE(recordSpy.count(), 0);
    QCOMPARE(record.type(), QString("blah2"));
    QCOMPARE(record.record().type(), QByteArray("blah2"));
    QCOMPARE(record.record().typeNameFormat(), QNdefRecord::ExternalRtd);
    QCOMPARE(record.typeNameFormat(), QQmlNdefRecord::ExternalRtd);

    record.setType("Rubbish");
    QCOMPARE(typeSpy.count(), 4);
    QCOMPARE(tnfSpy.count(), 2);
    QCOMPARE(recordSpy.count(), 0);
    QCOMPARE(record.type(), QString("Rubbish"));
    QCOMPARE(record.record().type(), QByteArray("Rubbish"));
    QCOMPARE(record.record().typeNameFormat(), QNdefRecord::ExternalRtd);
    QCOMPARE(record.typeNameFormat(), QQmlNdefRecord::ExternalRtd);

    record.setType("QQQQ");
    record.setTypeNameFormat(QQmlNdefRecord::Mime);
    QCOMPARE(typeSpy.count(), 5);
    QCOMPARE(tnfSpy.count(), 3);
    QCOMPARE(recordSpy.count(), 0);
    QCOMPARE(record.type(), QString("QQQQ"));
    QCOMPARE(record.record().type(), QByteArray("QQQQ"));
    QCOMPARE(record.record().typeNameFormat(), QNdefRecord::Mime);
    QCOMPARE(record.typeNameFormat(), QQmlNdefRecord::Mime);

    record.setRecord(QNdefRecord());
    QCOMPARE(typeSpy.count(), 5);  //setting record -> no recordChanged signal
    QCOMPARE(tnfSpy.count(), 3);
    QCOMPARE(recordSpy.count(), 1);
    QCOMPARE(record.type(), QString(""));
    QCOMPARE(record.record().type(), QByteArray());
    QCOMPARE(record.record().typeNameFormat(), QNdefRecord::Empty);
    QCOMPARE(record.typeNameFormat(), QQmlNdefRecord::Empty);
}

QTEST_MAIN(tst_QNdefRecord)

#include "tst_qndefrecord.moc"

