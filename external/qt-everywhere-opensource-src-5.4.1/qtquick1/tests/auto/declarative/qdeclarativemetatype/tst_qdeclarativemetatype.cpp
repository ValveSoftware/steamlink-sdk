/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qtest.h>
#include <QLocale>
#include <QPixmap>
#include <QBitmap>
#include <QPen>
#include <QTextLength>
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QQuaternion>
#include <QPalette>
#include <QIcon>
#include <QCursor>
#include <qdeclarative.h>

#include <private/qdeclarativemetatype_p.h>

class tst_qdeclarativemetatype : public QObject
{
    Q_OBJECT
public:
    tst_qdeclarativemetatype() {}

private slots:
    void initTestCase();

    void copy();

    void qmlParserStatusCast();
    void qmlPropertyValueSourceCast();
    void qmlPropertyValueInterceptorCast();

    void isList();

    void defaultObject();
};

class TestType : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int foo READ foo)

    Q_CLASSINFO("DefaultProperty", "foo")
public:
    int foo() { return 0; }
};
QML_DECLARE_TYPE(TestType);

class ParserStatusTestType : public QObject, public QDeclarativeParserStatus
{
    Q_OBJECT
    void classBegin(){}
    void componentComplete(){}
    Q_CLASSINFO("DefaultProperty", "foo") // Missing default property
    Q_INTERFACES(QDeclarativeParserStatus)
};
QML_DECLARE_TYPE(ParserStatusTestType);

class ValueSourceTestType : public QObject, public QDeclarativePropertyValueSource
{
    Q_OBJECT
    Q_INTERFACES(QDeclarativePropertyValueSource)
public:
    virtual void setTarget(const QDeclarativeProperty &) {}
};
QML_DECLARE_TYPE(ValueSourceTestType);

class ValueInterceptorTestType : public QObject, public QDeclarativePropertyValueInterceptor
{
    Q_OBJECT
    Q_INTERFACES(QDeclarativePropertyValueInterceptor)
public:
    virtual void setTarget(const QDeclarativeProperty &) {}
    virtual void write(const QVariant &) {}
};
QML_DECLARE_TYPE(ValueInterceptorTestType);


#define COPY_TEST(cpptype, metatype, value, defaultvalue) \
{ \
    cpptype v = (value); cpptype v2 = (value); \
    QVERIFY(QDeclarativeMetaType::copy(QMetaType:: metatype, &v, 0)); \
    QCOMPARE((cpptype)(v),(cpptype)(defaultvalue)); \
    QVERIFY(v == (defaultvalue)); \
    QVERIFY(QDeclarativeMetaType::copy(QMetaType:: metatype, &v, &v2)); \
    QVERIFY(v == (value)); \
}

#define QT_COPY_TEST(type, value) \
{ \
    type v = (value); type v2 = (value); \
    QVERIFY(QDeclarativeMetaType::copy(QMetaType:: type, &v, 0)); \
    QVERIFY(v == (type ())); \
    QVERIFY(QDeclarativeMetaType::copy(QMetaType:: type, &v, &v2)); \
    QVERIFY(v == (value)); \
}

void tst_qdeclarativemetatype::initTestCase()
{
    qmlRegisterType<TestType>("Test", 1, 0, "TestType");
    qmlRegisterType<ParserStatusTestType>("Test", 1, 0, "ParserStatusTestType");
    qmlRegisterType<ValueSourceTestType>("Test", 1, 0, "ValueSourceTestType");
    qmlRegisterType<ValueInterceptorTestType>("Test", 1, 0, "ValueInterceptorTestType");
}

void tst_qdeclarativemetatype::copy()
{
    QVERIFY(QDeclarativeMetaType::copy(QMetaType::Void, 0, 0));

    COPY_TEST(bool, Bool, true, false);
    COPY_TEST(int, Int, 10, 0);
    COPY_TEST(unsigned int, UInt, 10, 0);
    COPY_TEST(long long, LongLong, 10, 0);
    COPY_TEST(unsigned long long, ULongLong, 10, 0);
    COPY_TEST(double, Double, 19.2, 0);

    QT_COPY_TEST(QChar, QChar('a'));

    QVariantMap variantMap;
    variantMap.insert("Hello World!", QVariant(10));
    QT_COPY_TEST(QVariantMap, variantMap);

    QT_COPY_TEST(QVariantList, QVariantList() << QVariant(19.2));
    QT_COPY_TEST(QString, QString("QML Rocks!"));
    QT_COPY_TEST(QStringList, QStringList() << "QML" << "Rocks");
    QT_COPY_TEST(QByteArray, QByteArray("0x1102DDD"));
    QT_COPY_TEST(QBitArray, QBitArray(102, true));
    QDate cd = QDate::currentDate();
    QT_COPY_TEST(QDate, cd);
    QTime ct = QTime::currentTime();
    QT_COPY_TEST(QTime, ct);
    QDateTime cdt = QDateTime::currentDateTime();
    QT_COPY_TEST(QDateTime, cdt);
    QT_COPY_TEST(QUrl, QUrl("http://www.qt-project.org"));
    QT_COPY_TEST(QLocale, QLocale(QLocale::English, QLocale::Australia));
    QT_COPY_TEST(QRect, QRect(-10, 10, 102, 99));
    QT_COPY_TEST(QRectF, QRectF(-10.2, 1.2, 102, 99.6));
    QT_COPY_TEST(QSize, QSize(100, 2));
    QT_COPY_TEST(QSizeF, QSizeF(20.2, -100234.2));
    QT_COPY_TEST(QLine, QLine(0, 0, 100, 100));
    QT_COPY_TEST(QLineF, QLineF(-10.2, 0, 103, 1));
    QT_COPY_TEST(QPoint, QPoint(-1912, 1613));
    QT_COPY_TEST(QPointF, QPointF(-908.1, 1612));
    QT_COPY_TEST(QRegExp, QRegExp("(\\d+)(?:\\s*)(cm|inch)"));

    QVariantHash variantHash;
    variantHash.insert("Hello World!", QVariant(19));
    QT_COPY_TEST(QVariantHash, variantHash);

#ifdef QT3_SUPPORT
    QT_COPY_TEST(QColorGroup, QColorGroup(Qt::red, Qt::red, Qt::red, Qt::red, Qt::red, Qt::red, Qt::red));
#endif

    QT_COPY_TEST(QFont, QFont("Helvetica", 1024));

    {
        QPixmap v = QPixmap(100, 100); QPixmap v2 = QPixmap(100, 100);
        QVERIFY(QDeclarativeMetaType::copy(QMetaType::QPixmap, &v, 0));
        QVERIFY(v.size() == QPixmap().size());
        QVERIFY(QDeclarativeMetaType::copy(QMetaType::QPixmap , &v, &v2));
        QVERIFY(v.size() == QPixmap(100,100).size());
    }

    QT_COPY_TEST(QBrush, QBrush(Qt::blue));
    QT_COPY_TEST(QColor, QColor("lightsteelblue"));
    QT_COPY_TEST(QPalette, QPalette(Qt::green));

    {
        QPixmap icon(100, 100);

        QIcon v = QIcon(icon); QIcon v2 = QIcon(icon);
        QVERIFY(QDeclarativeMetaType::copy(QMetaType::QIcon, &v, 0));
        QVERIFY(v.isNull() == QIcon().isNull());
        QVERIFY(QDeclarativeMetaType::copy(QMetaType::QIcon , &v, &v2));
        QVERIFY(v.isNull() == QIcon(icon).isNull());
    }

    {
        QImage v = QImage(100, 100, QImage::Format_RGB32);
        QImage v2 = QImage(100, 100, QImage::Format_RGB32);
        QVERIFY(QDeclarativeMetaType::copy(QMetaType::QImage, &v, 0));
        QVERIFY(v.size() == QImage().size());
        QVERIFY(QDeclarativeMetaType::copy(QMetaType::QImage , &v, &v2));
        QVERIFY(v.size() == QImage(100,100, QImage::Format_RGB32).size());
    }

    QT_COPY_TEST(QPolygon, QPolygon(QRect(100, 100, 200, 103)));
    QT_COPY_TEST(QRegion, QRegion(QRect(0, 10, 99, 87)));

    {
        QBitmap v = QBitmap(100, 100); QBitmap v2 = QBitmap(100, 100);
        QVERIFY(QDeclarativeMetaType::copy(QMetaType::QBitmap, &v, 0));
        QVERIFY(v.size() == QBitmap().size());
        QVERIFY(QDeclarativeMetaType::copy(QMetaType::QBitmap , &v, &v2));
        QVERIFY(v.size() == QBitmap(100,100).size());
    }

    {
        QCursor v = QCursor(Qt::SizeFDiagCursor); QCursor v2 = QCursor(Qt::SizeFDiagCursor);
        QVERIFY(QDeclarativeMetaType::copy(QMetaType::QCursor, &v, 0));
        QVERIFY(v.shape() == QCursor().shape());
        QVERIFY(QDeclarativeMetaType::copy(QMetaType::QCursor , &v, &v2));
        QVERIFY(v.shape() == QCursor(Qt::SizeFDiagCursor).shape());
    }

    QT_COPY_TEST(QSizePolicy, QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum));
    QT_COPY_TEST(QKeySequence, QKeySequence("Ctrl+O"));
    QT_COPY_TEST(QPen, QPen(Qt::red));
    QT_COPY_TEST(QTextLength, QTextLength(QTextLength::FixedLength, 10.2));
    QT_COPY_TEST(QTextFormat, QTextFormat(QTextFormat::ListFormat));
    QT_COPY_TEST(QMatrix, QMatrix().translate(10, 10));
    QT_COPY_TEST(QTransform, QTransform().translate(10, 10));
    QT_COPY_TEST(QMatrix4x4, QMatrix4x4(1,0,2,3,0,1,0,0,9,0,1,0,0,0,10,1));
    QT_COPY_TEST(QVector2D, QVector2D(10.2f, 1));
    QT_COPY_TEST(QVector3D, QVector3D(10.2f, 1, -2));
    QT_COPY_TEST(QVector4D, QVector4D(10.2f, 1, -2, 1.2f));
    QT_COPY_TEST(QQuaternion, QQuaternion(1.0, 10.2f, 1, -2));

    int voidValue;
    COPY_TEST(void *, VoidStar, (void *)&voidValue, (void *)0);
    COPY_TEST(long, Long, 10, 0);
    COPY_TEST(short, Short, 10, 0);
    COPY_TEST(char, Char, 'a', 0);
    COPY_TEST(unsigned long, ULong, 10, 0);
    COPY_TEST(unsigned short, UShort, 10, 0);
    COPY_TEST(unsigned char, UChar, 'a', 0);
    COPY_TEST(float, Float, 10.5, 0);

    QObject objectValue;
    COPY_TEST(QObject *, QObjectStar, &objectValue, 0);
    COPY_TEST(qreal, QReal, 10.5, 0);

    {
        QVariant tv = QVariant::fromValue(QVariant(10));
        QVariant v(tv); QVariant v2(tv);
        QVERIFY(QDeclarativeMetaType::copy(qMetaTypeId<QVariant>(), &v, 0));
        QVERIFY(v == QVariant());
        QVERIFY(QDeclarativeMetaType::copy(qMetaTypeId<QVariant>(), &v, &v2));
        QVERIFY(v == tv);
    }

    {
        TestType t;  QVariant tv = QVariant::fromValue(&t);

        QVariant v(tv); QVariant v2(tv);
        QVERIFY(QDeclarativeMetaType::copy(qMetaTypeId<TestType *>(), &v, 0));
        QVERIFY(v == QVariant::fromValue((TestType *)0));
        QVERIFY(QDeclarativeMetaType::copy(qMetaTypeId<TestType *>(), &v, &v2));
        QVERIFY(v == tv);
    }
}

void tst_qdeclarativemetatype::qmlParserStatusCast()
{
    QVERIFY(QDeclarativeMetaType::qmlType(QVariant::Int) == 0);
    QVERIFY(QDeclarativeMetaType::qmlType(qMetaTypeId<TestType *>()) != 0);
    QCOMPARE(QDeclarativeMetaType::qmlType(qMetaTypeId<TestType *>())->parserStatusCast(), -1);
    QVERIFY(QDeclarativeMetaType::qmlType(qMetaTypeId<ValueSourceTestType *>()) != 0);
    QCOMPARE(QDeclarativeMetaType::qmlType(qMetaTypeId<ValueSourceTestType *>())->parserStatusCast(), -1);

    QVERIFY(QDeclarativeMetaType::qmlType(qMetaTypeId<ParserStatusTestType *>()) != 0);
    int cast = QDeclarativeMetaType::qmlType(qMetaTypeId<ParserStatusTestType *>())->parserStatusCast();
    QVERIFY(cast != -1);
    QVERIFY(cast != 0);

    ParserStatusTestType t;
    QVERIFY(reinterpret_cast<char *>((QObject *)&t) != reinterpret_cast<char *>((QDeclarativeParserStatus *)&t));

    QDeclarativeParserStatus *status = reinterpret_cast<QDeclarativeParserStatus *>(reinterpret_cast<char *>((QObject *)&t) + cast);
    QCOMPARE(status, (QDeclarativeParserStatus*)&t);
}

void tst_qdeclarativemetatype::qmlPropertyValueSourceCast()
{
    QVERIFY(QDeclarativeMetaType::qmlType(QVariant::Int) == 0);
    QVERIFY(QDeclarativeMetaType::qmlType(qMetaTypeId<TestType *>()) != 0);
    QCOMPARE(QDeclarativeMetaType::qmlType(qMetaTypeId<TestType *>())->propertyValueSourceCast(), -1);
    QVERIFY(QDeclarativeMetaType::qmlType(qMetaTypeId<ParserStatusTestType *>()) != 0);
    QCOMPARE(QDeclarativeMetaType::qmlType(qMetaTypeId<ParserStatusTestType *>())->propertyValueSourceCast(), -1);

    QVERIFY(QDeclarativeMetaType::qmlType(qMetaTypeId<ValueSourceTestType *>()) != 0);
    int cast = QDeclarativeMetaType::qmlType(qMetaTypeId<ValueSourceTestType *>())->propertyValueSourceCast();
    QVERIFY(cast != -1);
    QVERIFY(cast != 0);

    ValueSourceTestType t;
    QVERIFY(reinterpret_cast<char *>((QObject *)&t) != reinterpret_cast<char *>((QDeclarativePropertyValueSource *)&t));

    QDeclarativePropertyValueSource *source = reinterpret_cast<QDeclarativePropertyValueSource *>(reinterpret_cast<char *>((QObject *)&t) + cast);
    QCOMPARE(source, (QDeclarativePropertyValueSource*)&t);
}

void tst_qdeclarativemetatype::qmlPropertyValueInterceptorCast()
{
    QVERIFY(QDeclarativeMetaType::qmlType(QVariant::Int) == 0);
    QVERIFY(QDeclarativeMetaType::qmlType(qMetaTypeId<TestType *>()) != 0);
    QCOMPARE(QDeclarativeMetaType::qmlType(qMetaTypeId<TestType *>())->propertyValueInterceptorCast(), -1);
    QVERIFY(QDeclarativeMetaType::qmlType(qMetaTypeId<ParserStatusTestType *>()) != 0);
    QCOMPARE(QDeclarativeMetaType::qmlType(qMetaTypeId<ParserStatusTestType *>())->propertyValueInterceptorCast(), -1);

    QVERIFY(QDeclarativeMetaType::qmlType(qMetaTypeId<ValueInterceptorTestType *>()) != 0);
    int cast = QDeclarativeMetaType::qmlType(qMetaTypeId<ValueInterceptorTestType *>())->propertyValueInterceptorCast();
    QVERIFY(cast != -1);
    QVERIFY(cast != 0);

    ValueInterceptorTestType t;
    QVERIFY(reinterpret_cast<char *>((QObject *)&t) != reinterpret_cast<char *>((QDeclarativePropertyValueInterceptor *)&t));

    QDeclarativePropertyValueInterceptor *interceptor = reinterpret_cast<QDeclarativePropertyValueInterceptor *>(reinterpret_cast<char *>((QObject *)&t) + cast);
    QCOMPARE(interceptor, (QDeclarativePropertyValueInterceptor*)&t);
}

void tst_qdeclarativemetatype::isList()
{
    QCOMPARE(QDeclarativeMetaType::isList(QVariant::Invalid), false);
    QCOMPARE(QDeclarativeMetaType::isList(QVariant::Int), false);

    QDeclarativeListProperty<TestType> list;

    QCOMPARE(QDeclarativeMetaType::isList(qMetaTypeId<QDeclarativeListProperty<TestType> >()), true);
}

void tst_qdeclarativemetatype::defaultObject()
{
    QVERIFY(QDeclarativeMetaType::defaultProperty(&QObject::staticMetaObject).name() == 0);
    QVERIFY(QDeclarativeMetaType::defaultProperty(&ParserStatusTestType::staticMetaObject).name() == 0);
    QCOMPARE(QString(QDeclarativeMetaType::defaultProperty(&TestType::staticMetaObject).name()), QString("foo"));

    QObject o;
    TestType t;
    ParserStatusTestType p;

    QVERIFY(QDeclarativeMetaType::defaultProperty((QObject *)0).name() == 0);
    QVERIFY(QDeclarativeMetaType::defaultProperty(&o).name() == 0);
    QVERIFY(QDeclarativeMetaType::defaultProperty(&p).name() == 0);
    QCOMPARE(QString(QDeclarativeMetaType::defaultProperty(&t).name()), QString("foo"));
}

QTEST_MAIN(tst_qdeclarativemetatype)

#include "tst_qdeclarativemetatype.moc"
