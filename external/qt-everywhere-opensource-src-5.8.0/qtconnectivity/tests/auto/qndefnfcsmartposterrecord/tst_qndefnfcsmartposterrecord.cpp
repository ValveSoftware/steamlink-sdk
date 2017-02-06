/****************************************************************************
**
** Copyright (C) 2016 Research In Motion
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
#include <qndefmessage.h>
#include <qndefnfctextrecord.h>
#include <qndefnfcurirecord.h>
#include <qndefnfcsmartposterrecord.h>

QT_USE_NAMESPACE

static const quint8 payload[] = {
    0xC1, 0x02, 0x00, 0x00, 0x03, 0x0D, 0x53, 0x70, 0x91, 0x01, 0x08, 0x55, 0x01, 0x72, 0x69, 0x6D, 0x2E, 0x63, 0x6F, 0x6D,
    0x11, 0x01, 0x0D, 0x54, 0x02, 0x65, 0x6E, 0x49, 0x63, 0x6F, 0x6E, 0x20, 0x54, 0x69, 0x74, 0x6C, 0x65, 0x11, 0x01, 0x09,
    0x74, 0x74, 0x65, 0x78, 0x74, 0x2F, 0x68, 0x74, 0x6D, 0x6C, 0x11, 0x01, 0x04, 0x73, 0x00, 0x00, 0x04, 0x00, 0x11, 0x03,
    0x01, 0x61, 0x63, 0x74, 0x01, 0x42, 0x09, 0x00, 0x00, 0x02, 0xC5, 0x69, 0x6D, 0x61, 0x67, 0x65, 0x2F, 0x70, 0x6E, 0x67,
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x1E,
    0x00, 0x00, 0x00, 0x1E, 0x08, 0x02, 0x00, 0x00, 0x00, 0xB4, 0x52, 0x39, 0xF5, 0x00, 0x00, 0x02, 0x8C, 0x49, 0x44, 0x41,
    0x54, 0x78, 0x9C, 0x62, 0x60, 0x68, 0x60, 0xA0, 0x11, 0x02, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xA2, 0x99, 0xB9, 0x0D, 0x0C,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xA2, 0xA1, 0xD1, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xA2, 0xA1, 0xD1, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0xA2, 0xA1, 0xD1, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x62, 0x60, 0x60, 0xE8, 0xA6, 0x11, 0x02,
    0x00, 0x00, 0x00, 0xFF, 0xFF, 0xA2, 0xA1, 0xD1, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xA2, 0xA1, 0xD1, 0x00, 0x00, 0x00,
    0x00, 0xFF, 0xFF, 0xA2, 0xA1, 0xD1, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x82, 0x1A, 0x1D, 0x1B, 0xBB, 0x75, 0xE6, 0xCC,
    0x8B, 0x10, 0x34, 0x7F, 0xFE, 0xE5, 0xF2, 0xF2, 0x83, 0x82, 0x82, 0x93, 0x81, 0xE2, 0xF9, 0xF9, 0x7B, 0x81, 0x22, 0x1D,
    0x1D, 0x27, 0xE1, 0x1A, 0x3A, 0x3B, 0x4F, 0x02, 0x45, 0x94, 0x94, 0x66, 0x41, 0xB8, 0x69, 0x69, 0x3B, 0xF7, 0xEF, 0x7F,
    0x74, 0xF7, 0xEE, 0x87, 0xD5, 0xAB, 0x6F, 0x1A, 0x1B, 0x2F, 0x42, 0x36, 0x1A, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x82, 0x1A,
    0x0D, 0x34, 0xEE, 0x3F, 0x06, 0x00, 0x2A, 0x05, 0x6A, 0x83, 0xB3, 0x21, 0x2A, 0xDF, 0xBF, 0xFF, 0x01, 0xE4, 0xBA, 0xB8,
    0xAC, 0x02, 0xB2, 0xD7, 0xAF, 0xBF, 0x8D, 0xA9, 0x05, 0x6E, 0x34, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xC2, 0x67, 0x34, 0xD0,
    0x2D, 0x70, 0xA3, 0xE1, 0x0E, 0x87, 0x1B, 0x1D, 0x1A, 0xBA, 0x09, 0xAE, 0xF2, 0xDD, 0xBB, 0xEF, 0x10, 0x06, 0xC4, 0x4A,
    0x08, 0x02, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x42, 0x31, 0x1A, 0xA8, 0x02, 0xE8, 0x53, 0x7B, 0xFB, 0x15, 0x67, 0xCE, 0xBC,
    0x80, 0x28, 0x85, 0x33, 0x80, 0xD6, 0xA0, 0x19, 0x0D, 0x91, 0x82, 0x68, 0x01, 0x8A, 0x4B, 0x4B, 0x4F, 0x5F, 0xB5, 0xEA,
    0x06, 0xB2, 0xD1, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x42, 0x37, 0x1A, 0xC2, 0x05, 0xAA, 0x80, 0x98, 0x78, 0xE5, 0xCA,
    0x1B, 0xB8, 0xD3, 0x20, 0x46, 0x40, 0x8C, 0x8E, 0x8E, 0xDE, 0x02, 0x11, 0x04, 0xBA, 0x1D, 0x57, 0x34, 0x02, 0x00, 0x00,
    0x00, 0xFF, 0xFF, 0xC2, 0x6E, 0x34, 0x30, 0xC8, 0x90, 0x8D, 0xBE, 0x7F, 0xFF, 0x03, 0x3C, 0x4C, 0x20, 0x46, 0xD7, 0xD5,
    0x1D, 0x81, 0x28, 0xC0, 0x93, 0x42, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xC2, 0x6E, 0x34, 0xD0, 0x14, 0x88, 0xCE, 0x0B,
    0x17, 0x5E, 0x01, 0xC9, 0xFE, 0xFE, 0x33, 0xF0, 0x30, 0x81, 0x18, 0xDD, 0xD5, 0x75, 0x0A, 0x59, 0x3D, 0x30, 0x39, 0x41,
    0x10, 0xB2, 0xD1, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x42, 0x8F, 0x46, 0x60, 0x08, 0x42, 0xDC, 0x08, 0x01, 0x90, 0x68,
    0xAC, 0xAF, 0x3F, 0xBA, 0x7B, 0xF7, 0x03, 0x48, 0x98, 0x40, 0x8C, 0x9E, 0x3A, 0xF5, 0x3C, 0x72, 0x04, 0xC0, 0xA3, 0x04,
    0x98, 0x2E, 0xE1, 0x46, 0x03, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xC2, 0x97, 0x42, 0x80, 0x6E, 0x87, 0x18, 0x0D, 0x4C, 0xE6,
    0xC0, 0xF4, 0x0B, 0x61, 0x10, 0x6F, 0x34, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xC2, 0x62, 0x34, 0x50, 0x33, 0x50, 0x0F, 0xD0,
    0x2C, 0xA0, 0x38, 0xDC, 0x68, 0xA0, 0x67, 0x21, 0x7E, 0x42, 0x0E, 0x10, 0x78, 0x58, 0x03, 0x23, 0x13, 0xA2, 0x12, 0xD9,
    0x68, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xC2, 0x1E, 0xD6, 0x70, 0x04, 0x37, 0x1A, 0xCE, 0x86, 0x80, 0xB4, 0xB4, 0x5D,
    0xC8, 0xC9, 0x06, 0x88, 0x80, 0x81, 0x86, 0x66, 0x34, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x22, 0xC1, 0x68, 0x60, 0xA6, 0x87,
    0x1B, 0x0D, 0x4C, 0x9D, 0x90, 0x6C, 0x02, 0x4C, 0xCB, 0xB8, 0x8C, 0x06, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x22, 0xC1, 0x68,
    0x79, 0xF9, 0x99, 0xC8, 0x46, 0xC3, 0x53, 0x11, 0x30, 0xBB, 0x03, 0x15, 0x9C, 0x3F, 0xFF, 0x12, 0xCD, 0x68, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0x22, 0xC1, 0x68, 0xE4, 0xE8, 0x02, 0x1A, 0x0D, 0x0C, 0x7D, 0x48, 0xB8, 0x23, 0x03, 0x64, 0xA3,
    0x01, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x22, 0xCD, 0x68, 0x20, 0x03, 0x6E, 0x34, 0x90, 0x0B, 0x2C, 0x12, 0x80, 0x11, 0x0E,
    0x37, 0x17, 0x18, 0x38, 0xC8, 0x49, 0x1B, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xA2, 0x42, 0x79, 0x0D, 0xCC, 0xBA, 0x40, 0x84,
    0x91, 0x5F, 0xBA, 0x01, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xA2, 0x61, 0xB5, 0x0B, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xA2, 0xA1,
    0xD1, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xA2, 0xA1, 0xD1, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xA2, 0xA1, 0xD1, 0x00,
    0x00, 0x00, 0x00, 0xFF, 0xFF, 0xA3, 0xA1, 0xD1, 0x00, 0x27, 0x58, 0x04, 0x5A, 0x28, 0x87, 0x48, 0xD1, 0x00, 0x00, 0x00,
    0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
};

class tst_QNdefNfcSmartPosterRecord : public QObject
{
    Q_OBJECT

public:
    tst_QNdefNfcSmartPosterRecord();
    ~tst_QNdefNfcSmartPosterRecord();

private:
    QNdefNfcTextRecord getTextRecord(const QString& locale);
    QString getTitle(const QString& locale);
    void checkLocale(const QNdefNfcSmartPosterRecord& record, const QStringList& localeList);

    QMap<QString, QString> _textRecords;

private slots:
    void tst_emptyRecord();
    void tst_titles();
    void tst_uri();
    void tst_action();
    void tst_icon();
    void tst_size();
    void tst_typeInfo();
    void tst_construct();
    void tst_downcast();
};

tst_QNdefNfcSmartPosterRecord::tst_QNdefNfcSmartPosterRecord()
{
    _textRecords["en"] = "test";
    _textRecords["fr"] = "essai";
    _textRecords["es"] = "prueba";
    _textRecords["de"] = "pruefung";
    _textRecords["nl"] = "proef";
    _textRecords["en-US"] = "trial";
}

tst_QNdefNfcSmartPosterRecord::~tst_QNdefNfcSmartPosterRecord()
{
}

QNdefNfcTextRecord tst_QNdefNfcSmartPosterRecord::getTextRecord(const QString& locale)
{
    QNdefNfcTextRecord record;
    record.setLocale(locale);
    record.setEncoding(QNdefNfcTextRecord::Utf8);
    record.setText(_textRecords[locale]);
    return record;
}

QString tst_QNdefNfcSmartPosterRecord::getTitle(const QString& locale)
{
    return _textRecords[locale];
}

void tst_QNdefNfcSmartPosterRecord::checkLocale(const QNdefNfcSmartPosterRecord& record, const QStringList& localeList)
{
    QList<QString> locales = _textRecords.keys();

    for (int i = 0; i < locales.size(); i++) {
        if (localeList.contains(locales[i])) {
            QVERIFY(record.hasTitle(locales[i]));
        }

        else {
            QVERIFY(!record.hasTitle(locales[i]));
        }
    }

    if (localeList.empty()) {
        QVERIFY(record.hasTitle());
    }
}

void tst_QNdefNfcSmartPosterRecord::tst_emptyRecord()
{
    QNdefNfcSmartPosterRecord record;

    QCOMPARE(record.typeNameFormat(), QNdefRecord::NfcRtd);
    QCOMPARE(record.type(), QByteArray("Sp"));
    QVERIFY(record.id().isEmpty());
    QVERIFY(record.payload().isEmpty());

    QVERIFY(!record.hasTitle());
    QVERIFY(!record.hasAction());
    QVERIFY(!record.hasIcon());
    QVERIFY(!record.hasSize());
    QVERIFY(!record.hasTypeInfo());

    QCOMPARE(record.titleCount(), 0);
    QCOMPARE(record.titleRecord(0), QNdefNfcTextRecord());
    QCOMPARE(record.title(), QString());

    QList<QNdefNfcTextRecord> emptyTitleList;
    QCOMPARE(record.titleRecords(), emptyTitleList);

    QCOMPARE(record.uri(), QUrl());
    QCOMPARE(record.uriRecord(), QNdefNfcUriRecord());
    QCOMPARE(record.action(), QNdefNfcSmartPosterRecord::UnspecifiedAction);
    QCOMPARE(record.iconCount(), 0);
    QCOMPARE(record.iconRecord(0), QNdefNfcIconRecord());
    QCOMPARE(record.icon(), QByteArray());

    QList<QNdefNfcIconRecord> emptyIconList;
    QCOMPARE(record.iconRecords(), emptyIconList);

    quint32 size = 0;
    QCOMPARE(record.size(), size);
    QCOMPARE(record.typeInfo(), QByteArray());

    QVERIFY(record.isRecordType<QNdefNfcSmartPosterRecord>());
    QVERIFY(!record.isRecordType<QNdefNfcTextRecord>());
    QVERIFY(!record.isRecordType<QNdefNfcUriRecord>());

    QCOMPARE(record, QNdefNfcSmartPosterRecord());
    QVERIFY(!(record != QNdefNfcSmartPosterRecord()));
}

void tst_QNdefNfcSmartPosterRecord::tst_titles()
{
    QNdefNfcSmartPosterRecord record;

    // Set "en" locale
    QNdefNfcTextRecord enRecord = getTextRecord("en");
    QVERIFY(record.addTitle(enRecord));

    QCOMPARE(record.titleCount(), 1);
    {
        QStringList locales;
        locales << "en";
        checkLocale(record, locales);
    }

    // Get "en" locale
    QCOMPARE(record.titleRecord(0), enRecord);
    QCOMPARE(record.title(), getTitle("en"));
    QCOMPARE(record.title("fr"), QString());

    QList<QNdefNfcTextRecord> titleRecords = record.titleRecords();
    QCOMPARE(titleRecords.size(), 1);
    QCOMPARE(titleRecords[0], enRecord);

    QVERIFY(!record.addTitle(enRecord));
    QCOMPARE(record.titleCount(), 1);

    // Add "de" locale
    QNdefNfcTextRecord deRecord = getTextRecord("de");
    QVERIFY(record.addTitle(deRecord));

    QCOMPARE(record.titleCount(), 2);
    {
        QStringList locales;
        locales << "en" << "de";
        checkLocale(record, locales);
    }

    // Add "fr" locale
    QNdefNfcTextRecord frRecord = getTextRecord("fr");
    QVERIFY(record.addTitle(frRecord));

    QCOMPARE(record.titleCount(), 3);
    {
        QStringList locales;
        locales << "en" << "de" << "fr";
        checkLocale(record, locales);
    }

    QVERIFY(record.removeTitle(frRecord));

    QCOMPARE(record.titleCount(), 2);
    {
        QStringList locales;
        locales << "en" << "de";
        checkLocale(record, locales);
    }

    QVERIFY(!record.removeTitle(frRecord));
    QCOMPARE(record.titleCount(), 2);

    QVERIFY(record.removeTitle("en"));
    QCOMPARE(record.titleCount(), 1);
    {
        QStringList locales;
        locales << "de";
        checkLocale(record, locales);
    }

    titleRecords = record.titleRecords();
    QCOMPARE(titleRecords.size(), 1);
    QCOMPARE(titleRecords[0], deRecord);

    QCOMPARE(record.title(), getTitle("de"));

    QVERIFY(record.removeTitle(deRecord));
    QCOMPARE(record.titleCount(), 0);

    // Test setTitles
    QNdefNfcTextRecord nlRecord = getTextRecord("nl");
    QNdefNfcTextRecord esRecord = getTextRecord("es");

    QVERIFY(record.addTitle(enRecord));
    QVERIFY(record.addTitle(deRecord));
    QVERIFY(record.addTitle(frRecord));
    QCOMPARE(record.titleCount(), 3);

    QList<QNdefNfcTextRecord> titles;
    titles << nlRecord << esRecord;
    record.setTitles(titles);

    QCOMPARE(record.titleCount(), 2);
    QCOMPARE(record.title("en"), QString());
    QCOMPARE(record.title("de"), QString());
    QCOMPARE(record.title("fr"), QString());

    QCOMPARE(record.title("nl"), getTitle("nl"));
    QCOMPARE(record.title("es"), getTitle("es"));
}

void tst_QNdefNfcSmartPosterRecord::tst_uri()
{
    QNdefNfcSmartPosterRecord record;
    QString qtString = "http://www.qt-project.org";
    QString bbString = "http://www.blackberry.com";

    QCOMPARE(record.uri(), QUrl());
    QCOMPARE(record.uriRecord(), QNdefNfcUriRecord());

    QUrl qtUrl(qtString);
    record.setUri(qtUrl);
    QCOMPARE(record.uri(), qtUrl);

    QNdefNfcUriRecord qtRecord;
    qtRecord.setUri(qtString);
    QCOMPARE(record.uriRecord(), qtRecord);

    QNdefNfcUriRecord bbRecord;
    bbRecord.setUri(bbString);
    record.setUri(bbRecord);
    QCOMPARE(record.uri(), QUrl(bbString));
    QCOMPARE(record.uriRecord(), bbRecord);
}

void tst_QNdefNfcSmartPosterRecord::tst_action()
{
    QNdefNfcSmartPosterRecord record;

    record.setAction(QNdefNfcSmartPosterRecord::DoAction);
    QVERIFY(record.hasAction());
    QCOMPARE(record.action(), QNdefNfcSmartPosterRecord::DoAction);

    record.setAction(QNdefNfcSmartPosterRecord::SaveAction);
    QVERIFY(record.hasAction());
    QCOMPARE(record.action(), QNdefNfcSmartPosterRecord::SaveAction);

    record.setAction(QNdefNfcSmartPosterRecord::UnspecifiedAction);
    QVERIFY(record.hasAction());
    QCOMPARE(record.action(), QNdefNfcSmartPosterRecord::UnspecifiedAction);
}

void tst_QNdefNfcSmartPosterRecord::tst_icon()
{
    QNdefNfcSmartPosterRecord record;

    QNdefNfcIconRecord icon;
    record.addIcon(icon);
    QVERIFY(record.hasIcon());
    QCOMPARE(record.iconCount(), 1);
    QCOMPARE(record.iconRecord(0), icon);
    QCOMPARE(record.icon(), QByteArray());

    QList<QNdefNfcIconRecord> iconRecords = record.iconRecords();
    QCOMPARE(iconRecords.size(), 1);
    QCOMPARE(iconRecords[0], icon);

    QVERIFY(record.removeIcon(QByteArray()));
    QVERIFY(!record.hasIcon());
    QCOMPARE(record.iconCount(), 0);

    QByteArray mimeType = "test/data";
    record.addIcon(mimeType, "icondata");
    QVERIFY(record.hasIcon());
    QVERIFY(record.hasIcon(mimeType));
    QVERIFY(!record.hasIcon(QByteArray("mime")));

    QCOMPARE(record.iconCount(), 1);
    QCOMPARE(record.icon(), QByteArray("icondata"));
    QCOMPARE(record.icon(mimeType), QByteArray("icondata"));

    QNdefNfcIconRecord icon2;
    icon2.setData("iconrecorddata");
    icon2.setType("test/icon");
    record.addIcon(icon2);
    QVERIFY(record.hasIcon(QByteArray("test/icon")));

    QCOMPARE(record.iconCount(), 2);
    QCOMPARE(record.icon("test/icon"), QByteArray("iconrecorddata"));

    iconRecords = record.iconRecords();
    QCOMPARE(iconRecords.size(), 2);

    QNdefNfcIconRecord icon3;
    icon3.setData("icondata");
    icon3.setType("test/data");

    QCOMPARE(iconRecords[0], icon3);
    QCOMPARE(iconRecords[1], icon2);

    QVERIFY(record.removeIcon(mimeType));
    QCOMPARE(record.iconCount(), 1);
    iconRecords = record.iconRecords();
    QCOMPARE(iconRecords.size(), 1);
    QCOMPARE(iconRecords[0], icon2);

    QList<QNdefNfcIconRecord> iconList;

    QNdefNfcIconRecord testIcon;
    testIcon.setData("testicondata");
    testIcon.setType("test/data");

    iconList << testIcon;
    record.setIcons(iconList);

    QCOMPARE(record.iconCount(), 1);
    iconRecords = record.iconRecords();
    QCOMPARE(iconRecords.size(), 1);

    QCOMPARE(iconRecords[0], testIcon);
}

void tst_QNdefNfcSmartPosterRecord::tst_size()
{
    QNdefNfcSmartPosterRecord record;

    quint32 size = 1024;
    record.setSize(size);
    QCOMPARE(record.size(), size);
    QVERIFY(record.hasSize());

    size = 0;
    record.setSize(size);
    QCOMPARE(record.size(), size);
    QVERIFY(record.hasSize());
}

void tst_QNdefNfcSmartPosterRecord::tst_typeInfo()
{
    QNdefNfcSmartPosterRecord record;

    QByteArray typeInfo("typeinfo");
    record.setTypeInfo(typeInfo);
    QCOMPARE(record.typeInfo(), typeInfo);
    QVERIFY(record.hasTypeInfo());

    typeInfo = "moreinfo";
    record.setTypeInfo(typeInfo);
    QCOMPARE(record.typeInfo(), typeInfo);
    QVERIFY(record.hasTypeInfo());
}

void tst_QNdefNfcSmartPosterRecord::tst_construct()
{
    QByteArray data((char *)payload, sizeof(payload));
    QNdefMessage ndefMessage = QNdefMessage::fromByteArray(data);
    QCOMPARE(ndefMessage.size(), 1);
    QNdefRecord record = ndefMessage[0];

    QVERIFY(record.isRecordType<QNdefNfcSmartPosterRecord>());

    // Construct a SP record from a standard QNdefRecord
    QNdefNfcSmartPosterRecord sprecord(record);

    QCOMPARE(sprecord.typeNameFormat(), record.typeNameFormat());
    QCOMPARE(sprecord.type(), record.type());
    QCOMPARE(sprecord.payload(), record.payload());

    QVERIFY(sprecord.hasTitle("en"));
    QCOMPARE(sprecord.title("en"), QString("Icon Title"));

    QCOMPARE(sprecord.titleCount(), 1);
    QList<QNdefNfcTextRecord> titleRecords = sprecord.titleRecords();

    QCOMPARE(titleRecords[0].text(), QString("Icon Title"));
    QCOMPARE(titleRecords[0].locale(), QString("en"));
    QCOMPARE(sprecord.uri(), QUrl("http://www.rim.com"));

    QVERIFY(sprecord.hasAction());
    QCOMPARE(sprecord.action(), QNdefNfcSmartPosterRecord::SaveAction);

    QByteArray mimeType = "image/png";
    QVERIFY(sprecord.hasIcon());
    QVERIFY(sprecord.hasIcon(mimeType));
    QCOMPARE(sprecord.iconCount(), 1);

    QList<QNdefNfcIconRecord> iconRecords = sprecord.iconRecords();
    QCOMPARE(iconRecords.size(), 1);
    QCOMPARE(iconRecords[0].type(), mimeType);
    QVERIFY(!iconRecords[0].data().isEmpty());
    QVERIFY(iconRecords.value(1).type().isEmpty());
    QVERIFY(iconRecords.value(1).data().isEmpty());

    QVERIFY(sprecord.hasSize());
    quint32 size = 1024;
    QCOMPARE(sprecord.size(), size);

    QVERIFY(sprecord.hasTypeInfo());
    QCOMPARE(sprecord.typeInfo(), QByteArray("text/html"));

    QVERIFY(record == sprecord);
    QVERIFY(!(record != sprecord));
}

void tst_QNdefNfcSmartPosterRecord::tst_downcast()
{
    QByteArray data((char *)payload, sizeof(payload));
    QNdefMessage ndefMessage = QNdefMessage::fromByteArray(data);
    QCOMPARE(ndefMessage.size(), 1);
    QNdefRecord record = ndefMessage[0];

    QVERIFY(record.isRecordType<QNdefNfcSmartPosterRecord>());

    // Trying to downcast a QNdefRecord to a QNdefNfcSmartPosterRecord causes
    // an exception because a QNdefRecord does not contain the shared private data.
    // So first need to create a real QNdefNfcSmartPosterRecord and refer to it via its base
    QNdefNfcSmartPosterRecord sprecord(record);
    QNdefRecord *pRecord = (QNdefRecord *)&sprecord;

    QVERIFY(pRecord->isRecordType<QNdefNfcSmartPosterRecord>());

    QCOMPARE(pRecord->typeNameFormat(), sprecord.typeNameFormat());
    QCOMPARE(pRecord->type(), sprecord.type());
    QCOMPARE(pRecord->payload(), sprecord.payload());

    QCOMPARE(pRecord->typeNameFormat(), record.typeNameFormat());
    QCOMPARE(pRecord->type(), record.type());
    QCOMPARE(pRecord->payload(), record.payload());

    // Add another title
    QNdefNfcTextRecord esRecord = getTextRecord("es");
    QVERIFY(sprecord.addTitle(esRecord));

    QCOMPARE(sprecord.titleCount(), 2);
    {
        QStringList locales;
        locales << "en" << "es";
        checkLocale(sprecord, locales);
    }

    // Now make sure the base and SP record's payloads are the same
    QByteArray basePayload = pRecord->payload();
    QByteArray spPayload = sprecord.payload();

    // Check length is longer on the base
    QVERIFY(basePayload.length() > record.payload().length());

    // Check the payloads are the same
    QCOMPARE(basePayload, spPayload);
}

QTEST_MAIN(tst_QNdefNfcSmartPosterRecord)

#include "tst_qndefnfcsmartposterrecord.moc"
