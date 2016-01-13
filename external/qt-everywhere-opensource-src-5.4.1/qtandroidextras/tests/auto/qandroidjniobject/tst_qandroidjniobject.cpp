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

#include <QString>
#include <QtTest>
#include <QtAndroidExtras/QAndroidJniObject>
#include <QtAndroidExtras/QAndroidJniEnvironment>

class tst_QAndroidJniObject : public QObject
{
    Q_OBJECT

public:
    tst_QAndroidJniObject();

private slots:
    void initTestCase();

    void ctor();
    void callMethodTest();
    void callObjectMethodTest();
    void stringConvertionTest();
    void compareOperatorTests();
    void callStaticObjectMethodClassName();
    void callStaticObjectMethod();
    void callStaticBooleanMethodClassName();
    void callStaticBooleanMethod();
    void callStaticCharMethodClassName();
    void callStaticCharMethod();
    void callStaticIntMethodClassName();
    void callStaticIntMethod();
    void callStaticByteMethodClassName();
    void callStaticByteMethod();
    void callStaticDoubleMethodClassName();
    void callStaticDoubleMethod();
    void callStaticFloatMethodClassName();
    void callStaticFloatMethod();
    void callStaticLongMethodClassName();
    void callStaticLongMethod();
    void callStaticShortMethodClassName();
    void callStaticShortMethod();
    void getStaticObjectFieldClassName();
    void getStaticObjectField();
    void getStaticIntFieldClassName();
    void getStaticIntField();
    void getStaticByteFieldClassName();
    void getStaticByteField();
    void getStaticLongFieldClassName();
    void getStaticLongField();
    void getStaticDoubleFieldClassName();
    void getStaticDoubleField();
    void getStaticFloatFieldClassName();
    void getStaticFloatField();
    void getStaticShortFieldClassName();
    void getStaticShortField();
    void getStaticCharFieldClassName();
    void getStaticCharField();
    void getBooleanField();
    void getIntField();
    void isClassAvailable();

    void cleanupTestCase();
};

tst_QAndroidJniObject::tst_QAndroidJniObject()
{
}

void tst_QAndroidJniObject::initTestCase()
{
}

void tst_QAndroidJniObject::cleanupTestCase()
{
}

void tst_QAndroidJniObject::ctor()
{
    {
        QAndroidJniObject object;
        QVERIFY(!object.isValid());
    }

    {
        QAndroidJniObject object("java/lang/String");
        QVERIFY(object.isValid());
    }

    {
        QAndroidJniObject string = QAndroidJniObject::fromString(QLatin1String("Hello, Java"));
        QAndroidJniObject object("java/lang/String", "(Ljava/lang/String;)V", string.object<jstring>());
        QVERIFY(object.isValid());
        QCOMPARE(string.toString(), object.toString());
    }

    {
        QAndroidJniEnvironment env;
        jclass javaStringClass = env->FindClass("java/lang/String");
        QAndroidJniObject string(javaStringClass);
        QVERIFY(string.isValid());
    }

    {
        QAndroidJniEnvironment env;
        const QString qString = QLatin1String("Hello, Java");
        jclass javaStringClass = env->FindClass("java/lang/String");
        QAndroidJniObject string = QAndroidJniObject::fromString(qString);
        QAndroidJniObject stringCpy(javaStringClass, "(Ljava/lang/String;)V", string.object<jstring>());
        QVERIFY(stringCpy.isValid());
        QCOMPARE(qString, stringCpy.toString());
    }
}

void tst_QAndroidJniObject::callMethodTest()
{
    {
        QAndroidJniObject jString1 = QAndroidJniObject::fromString(QLatin1String("Hello, Java"));
        QAndroidJniObject jString2 = QAndroidJniObject::fromString(QLatin1String("hELLO, jAVA"));
        QVERIFY(jString1 != jString2);

        const jboolean isEmpty = jString1.callMethod<jboolean>("isEmpty");
        QVERIFY(!isEmpty);

        const jint ret = jString1.callMethod<jint>("compareToIgnoreCase",
                                                   "(Ljava/lang/String;)I",
                                                   jString2.object<jstring>());
        QVERIFY(0 == ret);
    }

    {
        jlong jLong = 100;
        QAndroidJniObject longObject("java/lang/Long", "(J)V", jLong);
        jlong ret = longObject.callMethod<jlong>("longValue");
        QCOMPARE(ret, jLong);
    }
}

void tst_QAndroidJniObject::callObjectMethodTest()
{
    const QString qString = QLatin1String("Hello, Java");
    QAndroidJniObject jString = QAndroidJniObject::fromString(qString);
    const QString qStringRet = jString.callObjectMethod<jstring>("toUpperCase").toString();
    QCOMPARE(qString.toUpper(), qStringRet);

    QAndroidJniObject subString = jString.callObjectMethod("substring",
                                                    "(II)Ljava/lang/String;",
                                                    0, 4);
    QCOMPARE(subString.toString(), qString.mid(0, 4));
}

void tst_QAndroidJniObject::stringConvertionTest()
{
    const QString qString(QLatin1String("Hello, Java"));
    QAndroidJniObject jString = QAndroidJniObject::fromString(qString);
    QVERIFY(jString.isValid());
    QString qStringRet = jString.toString();
    QCOMPARE(qString, qStringRet);
}

void tst_QAndroidJniObject::compareOperatorTests()
{
    QString str("hello!");
    QAndroidJniObject stringObject = QAndroidJniObject::fromString(str);

    jobject obj = stringObject.object();
    jobject jobj = stringObject.object<jobject>();
    jstring jsobj = stringObject.object<jstring>();

    QVERIFY(obj == stringObject);
    QVERIFY(jobj == stringObject);
    QVERIFY(stringObject == jobj);
    QVERIFY(jsobj == stringObject);
    QVERIFY(stringObject == jsobj);

    QAndroidJniObject stringObject3 = stringObject.object<jstring>();
    QVERIFY(stringObject3 == stringObject);

    QAndroidJniObject stringObject2 = QAndroidJniObject::fromString(str);
    QVERIFY(stringObject != stringObject2);

    jstring jstrobj = 0;
    QAndroidJniObject invalidStringObject;
    QVERIFY(invalidStringObject == jstrobj);

    QVERIFY(jstrobj != stringObject);
    QVERIFY(stringObject != jstrobj);
    QVERIFY(!invalidStringObject.isValid());
}

void tst_QAndroidJniObject::callStaticObjectMethodClassName()
{
    QAndroidJniObject formatString = QAndroidJniObject::fromString(QLatin1String("test format"));
    QVERIFY(formatString.isValid());

    QVERIFY(QAndroidJniObject::isClassAvailable("java/lang/String"));
    QAndroidJniObject returnValue = QAndroidJniObject::callStaticObjectMethod("java/lang/String",
                                                                "format",
                                                                "(Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/String;",
                                                                formatString.object<jstring>(),
                                                                jobjectArray(0));
    QVERIFY(returnValue.isValid());

    QString returnedString = returnValue.toString();

    QCOMPARE(returnedString, QString::fromLatin1("test format"));
}

void tst_QAndroidJniObject::callStaticObjectMethod()
{
    QAndroidJniEnvironment env;
    jclass cls = env->FindClass("java/lang/String");
    QVERIFY(cls != 0);

    QAndroidJniObject formatString = QAndroidJniObject::fromString(QLatin1String("test format"));
    QVERIFY(formatString.isValid());

    QAndroidJniObject returnValue = QAndroidJniObject::callStaticObjectMethod(cls,
                                                                "format",
                                                                "(Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/String;",
                                                                formatString.object<jstring>(),
                                                                jobjectArray(0));
    QVERIFY(returnValue.isValid());

    QString returnedString = returnValue.toString();

    QCOMPARE(returnedString, QString::fromLatin1("test format"));
}

void tst_QAndroidJniObject::callStaticBooleanMethod()
{
    QAndroidJniEnvironment env;
    jclass cls = env->FindClass("java/lang/Boolean");
    QVERIFY(cls != 0);

    {
        QAndroidJniObject parameter = QAndroidJniObject::fromString("true");
        QVERIFY(parameter.isValid());

        jboolean b = QAndroidJniObject::callStaticMethod<jboolean>(cls,
                                                            "parseBoolean",
                                                            "(Ljava/lang/String;)Z",
                                                            parameter.object<jstring>());
        QVERIFY(b);
    }

    {
        QAndroidJniObject parameter = QAndroidJniObject::fromString("false");
        QVERIFY(parameter.isValid());

        jboolean b = QAndroidJniObject::callStaticMethod<jboolean>(cls,
                                                            "parseBoolean",
                                                            "(Ljava/lang/String;)Z",
                                                            parameter.object<jstring>());
        QVERIFY(!b);
    }
}

void tst_QAndroidJniObject::callStaticBooleanMethodClassName()
{
    {
        QAndroidJniObject parameter = QAndroidJniObject::fromString("true");
        QVERIFY(parameter.isValid());

        jboolean b = QAndroidJniObject::callStaticMethod<jboolean>("java/lang/Boolean",
                                                            "parseBoolean",
                                                            "(Ljava/lang/String;)Z",
                                                            parameter.object<jstring>());
        QVERIFY(b);
    }

    {
        QAndroidJniObject parameter = QAndroidJniObject::fromString("false");
        QVERIFY(parameter.isValid());

        jboolean b = QAndroidJniObject::callStaticMethod<jboolean>("java/lang/Boolean",
                                                            "parseBoolean",
                                                            "(Ljava/lang/String;)Z",
                                                            parameter.object<jstring>());
        QVERIFY(!b);
    }
}

void tst_QAndroidJniObject::callStaticByteMethodClassName()
{
    QString number = QString::number(123);
    QAndroidJniObject parameter = QAndroidJniObject::fromString(number);

    jbyte returnValue = QAndroidJniObject::callStaticMethod<jbyte>("java/lang/Byte",
                                                            "parseByte",
                                                            "(Ljava/lang/String;)B",
                                                            parameter.object<jstring>());
    QCOMPARE(returnValue, jbyte(number.toInt()));
}

void tst_QAndroidJniObject::callStaticByteMethod()
{
    QAndroidJniEnvironment env;
    jclass cls = env->FindClass("java/lang/Byte");
    QVERIFY(cls != 0);

    QString number = QString::number(123);
    QAndroidJniObject parameter = QAndroidJniObject::fromString(number);

    jbyte returnValue = QAndroidJniObject::callStaticMethod<jbyte>(cls,
                                                            "parseByte",
                                                            "(Ljava/lang/String;)B",
                                                            parameter.object<jstring>());
    QCOMPARE(returnValue, jbyte(number.toInt()));
}

void tst_QAndroidJniObject::callStaticIntMethodClassName()
{
    QString number = QString::number(123);
    QAndroidJniObject parameter = QAndroidJniObject::fromString(number);

    jint returnValue = QAndroidJniObject::callStaticMethod<jint>("java/lang/Integer",
                                                          "parseInt",
                                                          "(Ljava/lang/String;)I",
                                                          parameter.object<jstring>());
    QCOMPARE(returnValue, number.toInt());
}


void tst_QAndroidJniObject::callStaticIntMethod()
{
    QAndroidJniEnvironment env;
    jclass cls = env->FindClass("java/lang/Integer");
    QVERIFY(cls != 0);

    QString number = QString::number(123);
    QAndroidJniObject parameter = QAndroidJniObject::fromString(number);

    jint returnValue = QAndroidJniObject::callStaticMethod<jint>(cls,
                                                          "parseInt",
                                                          "(Ljava/lang/String;)I",
                                                          parameter.object<jstring>());
    QCOMPARE(returnValue, number.toInt());
}

void tst_QAndroidJniObject::callStaticCharMethodClassName()
{
    jchar returnValue = QAndroidJniObject::callStaticMethod<jchar>("java/lang/Character",
                                                            "toUpperCase",
                                                            "(C)C",
                                                            jchar('a'));
    QCOMPARE(returnValue, jchar('A'));
}


void tst_QAndroidJniObject::callStaticCharMethod()
{
    QAndroidJniEnvironment env;
    jclass cls = env->FindClass("java/lang/Character");
    QVERIFY(cls != 0);

    jchar returnValue = QAndroidJniObject::callStaticMethod<jchar>(cls,
                                                            "toUpperCase",
                                                            "(C)C",
                                                            jchar('a'));
    QCOMPARE(returnValue, jchar('A'));
}

void tst_QAndroidJniObject::callStaticDoubleMethodClassName    ()
{
    QString number = QString::number(123.45);
    QAndroidJniObject parameter = QAndroidJniObject::fromString(number);

    jdouble returnValue = QAndroidJniObject::callStaticMethod<jdouble>("java/lang/Double",
                                                          "parseDouble",
                                                          "(Ljava/lang/String;)D",
                                                          parameter.object<jstring>());
    QCOMPARE(returnValue, number.toDouble());
}


void tst_QAndroidJniObject::callStaticDoubleMethod()
{
    QAndroidJniEnvironment env;
    jclass cls = env->FindClass("java/lang/Double");
    QVERIFY(cls != 0);

    QString number = QString::number(123.45);
    QAndroidJniObject parameter = QAndroidJniObject::fromString(number);

    jdouble returnValue = QAndroidJniObject::callStaticMethod<jdouble>(cls,
                                                          "parseDouble",
                                                          "(Ljava/lang/String;)D",
                                                          parameter.object<jstring>());
    QCOMPARE(returnValue, number.toDouble());
}

void tst_QAndroidJniObject::callStaticFloatMethodClassName()
{
    QString number = QString::number(123.45);
    QAndroidJniObject parameter = QAndroidJniObject::fromString(number);

    jfloat returnValue = QAndroidJniObject::callStaticMethod<jfloat>("java/lang/Float",
                                                          "parseFloat",
                                                          "(Ljava/lang/String;)F",
                                                          parameter.object<jstring>());
    QCOMPARE(returnValue, number.toFloat());
}


void tst_QAndroidJniObject::callStaticFloatMethod()
{
    QAndroidJniEnvironment env;
    jclass cls = env->FindClass("java/lang/Float");
    QVERIFY(cls != 0);

    QString number = QString::number(123.45);
    QAndroidJniObject parameter = QAndroidJniObject::fromString(number);

    jfloat returnValue = QAndroidJniObject::callStaticMethod<jfloat>(cls,
                                                          "parseFloat",
                                                          "(Ljava/lang/String;)F",
                                                          parameter.object<jstring>());
    QCOMPARE(returnValue, number.toFloat());
}

void tst_QAndroidJniObject::callStaticShortMethodClassName()
{
    QString number = QString::number(123);
    QAndroidJniObject parameter = QAndroidJniObject::fromString(number);

    jshort returnValue = QAndroidJniObject::callStaticMethod<jshort>("java/lang/Short",
                                                          "parseShort",
                                                          "(Ljava/lang/String;)S",
                                                          parameter.object<jstring>());
    QCOMPARE(returnValue, number.toShort());
}


void tst_QAndroidJniObject::callStaticShortMethod()
{
    QAndroidJniEnvironment env;
    jclass cls = env->FindClass("java/lang/Short");
    QVERIFY(cls != 0);

    QString number = QString::number(123);
    QAndroidJniObject parameter = QAndroidJniObject::fromString(number);

    jshort returnValue = QAndroidJniObject::callStaticMethod<jshort>(cls,
                                                          "parseShort",
                                                          "(Ljava/lang/String;)S",
                                                          parameter.object<jstring>());
    QCOMPARE(returnValue, number.toShort());
}

void tst_QAndroidJniObject::callStaticLongMethodClassName()
{
    QString number = QString::number(123);
    QAndroidJniObject parameter = QAndroidJniObject::fromString(number);

    jlong returnValue = QAndroidJniObject::callStaticMethod<jlong>("java/lang/Long",
                                                          "parseLong",
                                                          "(Ljava/lang/String;)J",
                                                          parameter.object<jstring>());
    QCOMPARE(returnValue, jlong(number.toLong()));
}

void tst_QAndroidJniObject::callStaticLongMethod()
{
    QAndroidJniEnvironment env;
    jclass cls = env->FindClass("java/lang/Long");
    QVERIFY(cls != 0);

    QString number = QString::number(123);
    QAndroidJniObject parameter = QAndroidJniObject::fromString(number);

    jlong returnValue = QAndroidJniObject::callStaticMethod<jlong>(cls,
                                                          "parseLong",
                                                          "(Ljava/lang/String;)J",
                                                          parameter.object<jstring>());
    QCOMPARE(returnValue, jlong(number.toLong()));
}

void tst_QAndroidJniObject::getStaticObjectFieldClassName()
{
    {
        QAndroidJniObject boolObject = QAndroidJniObject::getStaticObjectField<jobject>("java/lang/Boolean",
                                                                          "FALSE",
                                                                          "Ljava/lang/Boolean;");
        QVERIFY(boolObject.isValid());

        jboolean booleanValue = boolObject.callMethod<jboolean>("booleanValue");
        QVERIFY(!booleanValue);
    }

    {
        QAndroidJniObject boolObject = QAndroidJniObject::getStaticObjectField<jobject>("java/lang/Boolean",
                                                                 "TRUE",
                                                                 "Ljava/lang/Boolean;");
        QVERIFY(boolObject.isValid());

        jboolean booleanValue = boolObject.callMethod<jboolean>("booleanValue");
        QVERIFY(booleanValue);
    }

    {
        QAndroidJniObject boolObject = QAndroidJniObject::getStaticObjectField("java/lang/Boolean",
                                                                               "FALSE",
                                                                               "Ljava/lang/Boolean;");
        QVERIFY(boolObject.isValid());
        jboolean booleanValue = boolObject.callMethod<jboolean>("booleanValue");
        QVERIFY(!booleanValue);
    }
}

void tst_QAndroidJniObject::getStaticObjectField()
{
    QAndroidJniEnvironment env;
    jclass cls = env->FindClass("java/lang/Boolean");
    QVERIFY(cls != 0);

    {
        QAndroidJniObject boolObject = QAndroidJniObject::getStaticObjectField<jobject>(cls,
                                                                          "FALSE",
                                                                          "Ljava/lang/Boolean;");
        QVERIFY(boolObject.isValid());

        jboolean booleanValue = boolObject.callMethod<jboolean>("booleanValue");
        QVERIFY(!booleanValue);
    }

    {
        QAndroidJniObject boolObject = QAndroidJniObject::getStaticObjectField<jobject>(cls,
                                                                 "TRUE",
                                                                 "Ljava/lang/Boolean;");
        QVERIFY(boolObject.isValid());

        jboolean booleanValue = boolObject.callMethod<jboolean>("booleanValue");
        QVERIFY(booleanValue);
    }

    {
        QAndroidJniObject boolObject = QAndroidJniObject::getStaticObjectField(cls,
                                                                               "FALSE",
                                                                               "Ljava/lang/Boolean;");
        QVERIFY(boolObject.isValid());

        jboolean booleanValue = boolObject.callMethod<jboolean>("booleanValue");
        QVERIFY(!booleanValue);
    }
}

void tst_QAndroidJniObject::getStaticIntFieldClassName()
{
    jint i = QAndroidJniObject::getStaticField<jint>("java/lang/Double", "SIZE");
    QCOMPARE(i, 64);
}

void tst_QAndroidJniObject::getStaticIntField()
{
    QAndroidJniEnvironment env;
    jclass cls = env->FindClass("java/lang/Double");
    QVERIFY(cls != 0);

    jint i = QAndroidJniObject::getStaticField<jint>(cls, "SIZE");
    QCOMPARE(i, 64);
}

void tst_QAndroidJniObject::getStaticByteFieldClassName()
{
    jbyte i = QAndroidJniObject::getStaticField<jbyte>("java/lang/Byte", "MAX_VALUE");
    QCOMPARE(i, jbyte(127));
}

void tst_QAndroidJniObject::getStaticByteField()
{
    QAndroidJniEnvironment env;
    jclass cls = env->FindClass("java/lang/Byte");
    QVERIFY(cls != 0);

    jbyte i = QAndroidJniObject::getStaticField<jbyte>(cls, "MAX_VALUE");
    QCOMPARE(i, jbyte(127));
}

void tst_QAndroidJniObject::getStaticLongFieldClassName()
{
    jlong i = QAndroidJniObject::getStaticField<jlong>("java/lang/Long", "MAX_VALUE");
    QCOMPARE(i, jlong(9223372036854775807L));
}

void tst_QAndroidJniObject::getStaticLongField()
{
    QAndroidJniEnvironment env;
    jclass cls = env->FindClass("java/lang/Long");
    QVERIFY(cls != 0);

    jlong i = QAndroidJniObject::getStaticField<jlong>(cls, "MAX_VALUE");
    QCOMPARE(i, jlong(9223372036854775807L));
}

void tst_QAndroidJniObject::getStaticDoubleFieldClassName()
{
    jdouble i = QAndroidJniObject::getStaticField<jdouble>("java/lang/Double", "NaN");
    jlong *k = reinterpret_cast<jlong*>(&i);
    QCOMPARE(*k, jlong(0x7ff8000000000000L));
}

void tst_QAndroidJniObject::getStaticDoubleField()
{
    QAndroidJniEnvironment env;
    jclass cls = env->FindClass("java/lang/Double");
    QVERIFY(cls != 0);

    jdouble i = QAndroidJniObject::getStaticField<jdouble>(cls, "NaN");
    jlong *k = reinterpret_cast<jlong*>(&i);
    QCOMPARE(*k, jlong(0x7ff8000000000000L));
}

void tst_QAndroidJniObject::getStaticFloatFieldClassName()
{
    jfloat i = QAndroidJniObject::getStaticField<jfloat>("java/lang/Float", "NaN");
    unsigned *k = reinterpret_cast<unsigned*>(&i);
    QCOMPARE(*k, unsigned(0x7fc00000));
}

void tst_QAndroidJniObject::getStaticFloatField()
{
    QAndroidJniEnvironment env;
    jclass cls = env->FindClass("java/lang/Float");
    QVERIFY(cls != 0);

    jfloat i = QAndroidJniObject::getStaticField<jfloat>(cls, "NaN");
    unsigned *k = reinterpret_cast<unsigned*>(&i);
    QCOMPARE(*k, unsigned(0x7fc00000));
}

void tst_QAndroidJniObject::getStaticShortFieldClassName()
{
    jshort i = QAndroidJniObject::getStaticField<jshort>("java/lang/Short", "MAX_VALUE");
    QCOMPARE(i, jshort(32767));
}

void tst_QAndroidJniObject::getStaticShortField()
{
    QAndroidJniEnvironment env;
    jclass cls = env->FindClass("java/lang/Short");
    QVERIFY(cls != 0);

    jshort i = QAndroidJniObject::getStaticField<jshort>(cls, "MAX_VALUE");
    QCOMPARE(i, jshort(32767));
}

void tst_QAndroidJniObject::getStaticCharFieldClassName()
{
    jchar i = QAndroidJniObject::getStaticField<jchar>("java/lang/Character", "MAX_VALUE");
    QCOMPARE(i, jchar(0xffff));
}

void tst_QAndroidJniObject::getStaticCharField()
{
    QAndroidJniEnvironment env;
    jclass cls = env->FindClass("java/lang/Character");
    QVERIFY(cls != 0);

    jchar i = QAndroidJniObject::getStaticField<jchar>(cls, "MAX_VALUE");
    QCOMPARE(i, jchar(0xffff));
}


void tst_QAndroidJniObject::getBooleanField()
{
    QAndroidJniObject obj("org/qtproject/qt5/android/QtActivityDelegate");

    QVERIFY(obj.isValid());
    QVERIFY(!obj.getField<jboolean>("m_fullScreen"));
}

void tst_QAndroidJniObject::getIntField()
{
    QAndroidJniObject obj("org/qtproject/qt5/android/QtActivityDelegate");

    QVERIFY(obj.isValid());
    jint res = obj.getField<jint>("m_currentRotation");
    QCOMPARE(res, -1);
}

void tst_QAndroidJniObject::isClassAvailable()
{
    QVERIFY(QAndroidJniObject::isClassAvailable("java/lang/String"));
    QVERIFY(!QAndroidJniObject::isClassAvailable("class/not/Available"));
    QVERIFY(QAndroidJniObject::isClassAvailable("org/qtproject/qt5/android/QtActivityDelegate"));
}

QTEST_APPLESS_MAIN(tst_QAndroidJniObject)

#include "tst_qandroidjniobject.moc"
