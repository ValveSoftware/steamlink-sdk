/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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
#include <QtCore/QVariant>
#include <QtCore/QDateTime>
#include <QtCore/QMetaType>

#include <qt_windows.h>

QT_BEGIN_NAMESPACE

// Conversion functions from statically linked library (axtypes.h)
bool QVariantToVARIANT_container(const QVariant &var, VARIANT &arg,
                                 const QByteArray &typeName = QByteArray(),
                                 bool out = false);

QVariant VARIANTToQVariant_container(const VARIANT &arg, const QByteArray &typeName,
                                     uint type = 0);

QT_END_NAMESPACE

class tst_Conversion : public QObject
{
    Q_OBJECT

private slots:
    void conversion_data();
    void conversion();
};

enum Mode {
    ByValue,
    ByReference, // Allocate new value
    OutParameter // Pre-allocated out-parameter by reference (test works only for types < qint64)
};

Q_DECLARE_METATYPE(Mode)

void tst_Conversion::conversion_data()
{
    QTest::addColumn<QVariant>("value");
    QTest::addColumn<uint>("expectedType");
    QTest::addColumn<QByteArray>("typeName");
    QTest::addColumn<Mode>("mode");

    QVariant qvar;
    QByteArray typeName;

    qvar = QVariant('a');
    typeName = QByteArrayLiteral("char");
    QTest::newRow("char")
        << qvar << uint(VT_I1) << typeName << ByValue;
    QTest::newRow("char-ref")
        << qvar << uint(VT_I1 | VT_BYREF) << typeName << ByReference;
    QTest::newRow("char-out")
        << qvar << uint(VT_I1 | VT_BYREF) << typeName << OutParameter;

    qvar = QVariant(uchar(197));
    typeName = QByteArrayLiteral("uchar");
    QTest::newRow("uchar")
        << qvar << uint(VT_UI1) << typeName << ByValue;
    QTest::newRow("uchar-ref")
        << qvar << uint(VT_UI1 | VT_BYREF) << typeName << ByReference;
    QTest::newRow("uchar-out")
        << qvar << uint(VT_UI1 | VT_BYREF) << typeName << OutParameter;

    qvar = QVariant(ushort(42));
    typeName = QByteArrayLiteral("ushort");
    QTest::newRow("ushort")
        << qvar << uint(VT_UI2) << typeName << ByValue;
    QTest::newRow("ushort-ref")
        << qvar << uint(VT_UI2 | VT_BYREF) << typeName << ByReference;
    QTest::newRow("ushort-out")
        << qvar << uint(VT_UI2 | VT_BYREF) << typeName << OutParameter;

    qvar = QVariant(short(42));
    typeName = QByteArrayLiteral("short");
    QTest::newRow("short")
        << qvar << uint(VT_I2) << typeName << ByValue;
    QTest::newRow("short-ref")
        << qvar << uint(VT_I2 | VT_BYREF) << typeName << ByReference;
    QTest::newRow("short-out")
        << qvar << uint(VT_I2 | VT_BYREF) << typeName << OutParameter;

    qvar = QVariant(42);
    typeName.clear();
    QTest::newRow("int")
        << qvar << uint(VT_I4) << typeName << ByValue;
    QTest::newRow("int-ref")
        << qvar << uint(VT_I4 | VT_BYREF) << typeName << ByReference;
    QTest::newRow("int-out")
        << qvar << uint(VT_I4 | VT_BYREF) << typeName << OutParameter;

    qvar = QVariant(42u);
    typeName.clear();
    QTest::newRow("uint")
        << qvar << uint(VT_UI4) << typeName << ByValue;
    QTest::newRow("uint-ref")
        << qvar << uint(VT_UI4 | VT_BYREF) << typeName << ByReference;
    QTest::newRow("uint-out")
        << qvar << uint(VT_UI4 | VT_BYREF) << typeName << OutParameter;

    qvar = QVariant(qint64(42));
    typeName.clear();
    QTest::newRow("int64")
        << qvar << uint(VT_I8) << typeName << ByValue;
    QTest::newRow("int64-ref")
        << qvar << uint(VT_I8 | VT_BYREF) << typeName << ByReference;
    QTest::newRow("int64-out")
        << qvar << uint(VT_I8 | VT_BYREF) << typeName << OutParameter;

    qvar = QVariant(quint64(42u));
    typeName.clear();
    QTest::newRow("uint64")
        << qvar << uint(VT_UI8) << typeName << ByValue;
    QTest::newRow("uint64-ref")
        << qvar << uint(VT_UI8 | VT_BYREF) << typeName << ByReference;
    QTest::newRow("uint64-out")
        << qvar << uint(VT_UI8 | VT_BYREF) << typeName << OutParameter;

    qvar = QVariant(3.141f);
    typeName = QByteArrayLiteral("float");
    QTest::newRow("float")
        << qvar << uint(VT_R4) << typeName << ByValue;
    QTest::newRow("float-ref")
        << qvar << uint(VT_R4 | VT_BYREF) << typeName << ByReference;
    QTest::newRow("float-out")
        << qvar << uint(VT_R4 | VT_BYREF) << typeName << OutParameter;

    qvar = QVariant(3.141);
    typeName.clear();
    QTest::newRow("double")
        << qvar << uint(VT_R8) << typeName << ByValue;
    QTest::newRow("double-ref")
        << qvar << uint(VT_R8 | VT_BYREF) << typeName << ByReference;
    QTest::newRow("double-out")
        << qvar << uint(VT_R8 | VT_BYREF) << typeName << OutParameter;

    qvar = QDateTime(QDate(1968, 3, 9), QTime(10, 0));
    typeName.clear();
    QTest::newRow("datetime")
        << qvar << uint(VT_DATE) << typeName << ByValue;
    QTest::newRow("datetime-ref")
        << qvar << uint(VT_DATE | VT_BYREF) << typeName << ByReference;
    QTest::newRow("datetime-out")
        << qvar << uint(VT_DATE | VT_BYREF) << typeName << OutParameter;
}

void tst_Conversion::conversion()
{
    QFETCH(QVariant, value);
    QFETCH(uint, expectedType);
    QFETCH(QByteArray, typeName);
    QFETCH(Mode, mode);

    VARIANT variant;
    VariantInit(&variant);
    if (mode == OutParameter) {
        variant.vt = expectedType | VT_BYREF;
        variant.pullVal = new ULONGLONG(0);
    }

    QVERIFY(QVariantToVARIANT_container(value, variant, typeName, mode != ByValue));
    QCOMPARE(uint(variant.vt), expectedType);
    const QVariant converted = VARIANTToQVariant_container(variant, QByteArray());
    QCOMPARE(converted, value);

    if (mode == OutParameter)
        delete variant.pullVal;
}

QTEST_MAIN(tst_Conversion)
#include "tst_conversion.moc"
