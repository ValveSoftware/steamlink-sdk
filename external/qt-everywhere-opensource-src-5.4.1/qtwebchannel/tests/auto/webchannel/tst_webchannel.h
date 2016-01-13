/****************************************************************************
**
** Copyright (C) 2014 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebChannel module of the Qt Toolkit.
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

#ifndef TST_WEBCHANNEL_H
#define TST_WEBCHANNEL_H

#include <QObject>
#include <QVariant>

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
public:
    explicit TestObject(QObject *parent = 0)
        : QObject(parent)
    { }

    enum Foo {
        Bar,
        Asdf
    };

    Foo foo() const {return Bar;}
    int asdf() const {return 42;}
    QString bar() const {return QString();}

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

public slots:
    void slot1() {}
    void slot2(const QString&) {}

protected slots:
    void slot3() {}

private slots:
    void slot4() {}
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

public:
    explicit TestWebChannel(QObject *parent = 0);
    virtual ~TestWebChannel();

    Q_INVOKABLE void setInt(int i);
    Q_INVOKABLE void setDouble(double d);
    Q_INVOKABLE void setVariant(const QVariant &v);

private slots:
    void testRegisterObjects();
    void testDeregisterObjects();
    void testInfoForObject();
    void testInvokeMethodConversion();
    void testDisconnect();

    void benchClassInfo();
    void benchInitializeClients();
    void benchPropertyUpdates();
    void benchRegisterObjects();

private:
    DummyTransport *m_dummyTransport;

    int m_lastInt;
    double m_lastDouble;
    QVariant m_lastVariant;
};

QT_END_NAMESPACE

#endif // TST_WEBCHANNEL_H
