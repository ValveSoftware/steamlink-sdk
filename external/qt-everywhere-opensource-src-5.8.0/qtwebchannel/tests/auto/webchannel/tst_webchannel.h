/****************************************************************************
**
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebChannel module of the Qt Toolkit.
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

#ifndef TST_WEBCHANNEL_H
#define TST_WEBCHANNEL_H

#include <QObject>
#include <QVariant>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>

#include <QtWebChannel/QWebChannelAbstractTransport>

QT_BEGIN_NAMESPACE

class DummyTransport : public QWebChannelAbstractTransport
{
    Q_OBJECT
public:
    explicit DummyTransport(QObject *parent)
        : QWebChannelAbstractTransport(parent)
    {}
    ~DummyTransport() {};

    void emitMessageReceived(const QJsonObject &message)
    {
        emit messageReceived(message, this);
    }

public slots:
    void sendMessage(const QJsonObject &/*message*/) Q_DECL_OVERRIDE
    {
    }
};

class TestObject : public QObject
{
    Q_OBJECT
    Q_ENUMS(Foo)

    Q_PROPERTY(Foo foo READ foo CONSTANT)
    Q_PROPERTY(int asdf READ asdf NOTIFY asdfChanged)
    Q_PROPERTY(QString bar READ bar NOTIFY theBarHasChanged)
    Q_PROPERTY(QObject * objectProperty READ objectProperty WRITE setObjectProperty NOTIFY objectPropertyChanged)
    Q_PROPERTY(TestObject * returnedObject READ returnedObject WRITE setReturnedObject NOTIFY returnedObjectChanged)
    Q_PROPERTY(QString prop READ prop WRITE setProp NOTIFY propChanged)

public:
    explicit TestObject(QObject *parent = 0)
        : QObject(parent)
        , mObjectProperty(0)
        , mReturnedObject(Q_NULLPTR)
    { }

    enum Foo {
        Bar,
        Asdf
    };

    Foo foo() const {return Bar;}
    int asdf() const {return 42;}
    QString bar() const {return QString();}

    QObject *objectProperty() const
    {
        return mObjectProperty;
    }

    TestObject *returnedObject() const
    {
        return mReturnedObject;
    }

    QString prop() const
    {
        return mProp;
    }

    Q_INVOKABLE void method1() {}

protected:
    Q_INVOKABLE void method2() {}

private:
    Q_INVOKABLE void method3() {}

signals:
    void sig1();
    void sig2(const QString&);
    void asdfChanged();
    void theBarHasChanged();
    void objectPropertyChanged();
    void returnedObjectChanged();
    void propChanged(const QString&);
    void replay();

public slots:
    void slot1() {}
    void slot2(const QString&) {}

    void setReturnedObject(TestObject *obj)
    {
        mReturnedObject = obj;
        emit returnedObjectChanged();
    }

    void setObjectProperty(QObject *object)
    {
        mObjectProperty = object;
        emit objectPropertyChanged();
    }

    void setProp(const QString&prop) {emit propChanged(mProp=prop);}
    void fire() {emit replay();}

protected slots:
    void slot3() {}

private slots:
    void slot4() {}

public:
    QObject *mObjectProperty;
    TestObject *mReturnedObject;
    QString mProp;
};

class BenchObject : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int p0 MEMBER m_p0 NOTIFY p0Changed)
    Q_PROPERTY(int p1 MEMBER m_p1 NOTIFY p1Changed)
    Q_PROPERTY(int p2 MEMBER m_p2 NOTIFY p2Changed)
    Q_PROPERTY(int p3 MEMBER m_p3 NOTIFY p3Changed)
    Q_PROPERTY(int p4 MEMBER m_p4 NOTIFY p4Changed)
    Q_PROPERTY(int p5 MEMBER m_p5 NOTIFY p5Changed)
    Q_PROPERTY(int p6 MEMBER m_p6 NOTIFY p6Changed)
    Q_PROPERTY(int p7 MEMBER m_p7 NOTIFY p7Changed)
    Q_PROPERTY(int p8 MEMBER m_p8 NOTIFY p8Changed)
    Q_PROPERTY(int p9 MEMBER m_p9 NOTIFY p9Changed)
public:
    explicit BenchObject(QObject *parent = 0)
        : QObject(parent)
        , m_p0(0)
        , m_p1(0)
        , m_p2(0)
        , m_p3(0)
        , m_p4(0)
        , m_p5(0)
        , m_p6(0)
        , m_p7(0)
        , m_p8(0)
        , m_p9(0)
    { }

    void change()
    {
        m_p0++;
        m_p1++;
        m_p2++;
        m_p3++;
        m_p4++;
        m_p5++;
        m_p6++;
        m_p7++;
        m_p8++;
        m_p9++;
        emit p0Changed(m_p0);
        emit p1Changed(m_p1);
        emit p2Changed(m_p2);
        emit p3Changed(m_p3);
        emit p4Changed(m_p4);
        emit p5Changed(m_p5);
        emit p6Changed(m_p6);
        emit p7Changed(m_p7);
        emit p8Changed(m_p8);
        emit p9Changed(m_p9);
    }

signals:
    void s0();
    void s1();
    void s2();
    void s3();
    void s4();
    void s5();
    void s6();
    void s7();
    void s8();
    void s9();

    void p0Changed(int);
    void p1Changed(int);
    void p2Changed(int);
    void p3Changed(int);
    void p4Changed(int);
    void p5Changed(int);
    void p6Changed(int);
    void p7Changed(int);
    void p8Changed(int);
    void p9Changed(int);

public slots:
    void m0(){};
    void m1(){};
    void m2(){};
    void m3(){};
    void m4(){};
    void m5(){};
    void m6(){};
    void m7(){};
    void m8(){};
    void m9(){};

private:
    int m_p0, m_p1, m_p2, m_p3, m_p4, m_p5, m_p6, m_p7, m_p8, m_p9;
};

class TestWebChannel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int lastInt READ readInt WRITE setInt NOTIFY lastIntChanged);
    Q_PROPERTY(bool lastBool READ readBool WRITE setBool NOTIFY lastBoolChanged);
    Q_PROPERTY(double lastDouble READ readDouble WRITE setDouble NOTIFY lastDoubleChanged);
    Q_PROPERTY(QVariant lastVariant READ readVariant WRITE setVariant NOTIFY lastVariantChanged);
    Q_PROPERTY(QJsonValue lastJsonValue READ readJsonValue WRITE setJsonValue NOTIFY lastJsonValueChanged);
    Q_PROPERTY(QJsonObject lastJsonObject READ readJsonObject WRITE setJsonObject NOTIFY lastJsonObjectChanged);
    Q_PROPERTY(QJsonArray lastJsonArray READ readJsonArray WRITE setJsonArray NOTIFY lastJsonArrayChanged);
public:
    explicit TestWebChannel(QObject *parent = 0);
    virtual ~TestWebChannel();

    int readInt() const;
    Q_INVOKABLE void setInt(int i);
    bool readBool() const;
    Q_INVOKABLE void setBool(bool b);
    double readDouble() const;
    Q_INVOKABLE void setDouble(double d);
    QVariant readVariant() const;
    Q_INVOKABLE void setVariant(const QVariant &v);
    QJsonValue readJsonValue() const;
    Q_INVOKABLE void setJsonValue(const QJsonValue &v);
    QJsonObject readJsonObject() const;
    Q_INVOKABLE void setJsonObject(const QJsonObject &v);
    QJsonArray readJsonArray() const;
    Q_INVOKABLE void setJsonArray(const QJsonArray &v);

signals:
    void lastIntChanged();
    void lastBoolChanged();
    void lastDoubleChanged();
    void lastVariantChanged();
    void lastJsonValueChanged();
    void lastJsonObjectChanged();
    void lastJsonArrayChanged();

private slots:
    void testRegisterObjects();
    void testDeregisterObjects();
    void testInfoForObject();
    void testInvokeMethodConversion();
    void testSetPropertyConversion();
    void testDisconnect();
    void testWrapRegisteredObject();
    void testRemoveUnusedTransports();
    void testPassWrappedObjectBack();
    void testInfiniteRecursion();
    void testAsyncObject();

    void benchClassInfo();
    void benchInitializeClients();
    void benchPropertyUpdates();
    void benchRegisterObjects();
    void benchRemoveTransport();

    void qtbug46548_overriddenProperties();

private:
    DummyTransport *m_dummyTransport;

    int m_lastInt;
    bool m_lastBool;
    double m_lastDouble;
    QVariant m_lastVariant;
    QJsonValue m_lastJsonValue;
    QJsonObject m_lastJsonObject;
    QJsonArray m_lastJsonArray;
};

QT_END_NAMESPACE

#endif // TST_WEBCHANNEL_H
