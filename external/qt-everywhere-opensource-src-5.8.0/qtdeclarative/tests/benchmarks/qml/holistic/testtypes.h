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
#ifndef TESTTYPES_H
#define TESTTYPES_H

#include <QtCore/qobject.h>
#include <QtQml/qqml.h>
#include <QtQml/qqmlexpression.h>
#include <QtCore/qpoint.h>
#include <QtCore/qsize.h>
#include <QtQml/qqmllist.h>
#include <QtCore/qrect.h>
#include <QtGui/qmatrix.h>
#include <QtGui/qcolor.h>
#include <QtGui/qpixmap.h>
#include <QtGui/qvector3d.h>
#include <QtCore/qdatetime.h>
#include <QtQml/qjsvalue.h>
#include <QtQml/qqmlscriptstring.h>
#include <QtQml/qqmlcomponent.h>

class MyQmlAttachedObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int value READ value CONSTANT)
    Q_PROPERTY(int value2 READ value2 WRITE setValue2)
public:
    MyQmlAttachedObject(QObject *parent) : QObject(parent), m_value2(0) {}

    int value() const { return 19; }
    int value2() const { return m_value2; }
    void setValue2(int v) { m_value2 = v; }

    void emitMySignal() { emit mySignal(); }

signals:
    void mySignal();

private:
    int m_value2;
};

class MyQmlObject : public QObject
{
    Q_OBJECT
    Q_ENUMS(MyEnum)
    Q_ENUMS(MyEnum2)
    Q_PROPERTY(int deleteOnSet READ deleteOnSet WRITE setDeleteOnSet)
    Q_PROPERTY(bool trueProperty READ trueProperty CONSTANT)
    Q_PROPERTY(bool falseProperty READ falseProperty CONSTANT)
    Q_PROPERTY(int value READ value WRITE setValue)
    Q_PROPERTY(int console READ console CONSTANT)
    Q_PROPERTY(QString stringProperty READ stringProperty WRITE setStringProperty NOTIFY stringChanged)
    Q_PROPERTY(QObject *objectProperty READ objectProperty WRITE setObjectProperty NOTIFY objectChanged)
    Q_PROPERTY(QQmlListProperty<QObject> objectListProperty READ objectListProperty CONSTANT)
    Q_PROPERTY(int resettableProperty READ resettableProperty WRITE setResettableProperty RESET resetProperty)
    Q_PROPERTY(QRegExp regExp READ regExp WRITE setRegExp)
    Q_PROPERTY(int nonscriptable READ nonscriptable WRITE setNonscriptable SCRIPTABLE false)

public:
    MyQmlObject(): myinvokableObject(0), m_methodCalled(false), m_methodIntCalled(false), m_object(0), m_value(0), m_resetProperty(13) {}

    enum MyEnum { EnumValue1 = 0, EnumValue2 = 1 };
    enum MyEnum2 { EnumValue3 = 2, EnumValue4 = 3 };

    bool trueProperty() const { return true; }
    bool falseProperty() const { return false; }

    QString stringProperty() const { return m_string; }
    void setStringProperty(const QString &s)
    {
        if (s == m_string)
            return;
        m_string = s;
        emit stringChanged();
    }

    QObject *objectProperty() const { return m_object; }
    void setObjectProperty(QObject *obj) {
        if (obj == m_object)
            return;
        m_object = obj;
        emit objectChanged();
    }

    QQmlListProperty<QObject> objectListProperty() { return QQmlListProperty<QObject>(this, m_objectQList); }

    bool methodCalled() const { return m_methodCalled; }
    bool methodIntCalled() const { return m_methodIntCalled; }

    QString string() const { return m_string; }

    static MyQmlAttachedObject *qmlAttachedProperties(QObject *o) {
        return new MyQmlAttachedObject(o);
    }

    int deleteOnSet() const { return 1; }
    void setDeleteOnSet(int v) { if(v) delete this; }

    int value() const { return m_value; }
    void setValue(int v) { m_value = v; }

    int resettableProperty() const { return m_resetProperty; }
    void setResettableProperty(int v) { m_resetProperty = v; }
    void resetProperty() { m_resetProperty = 13; }

    QRegExp regExp() { return m_regExp; }
    void setRegExp(const QRegExp &regExp) { m_regExp = regExp; }

    int console() const { return 11; }

    int nonscriptable() const { return 0; }
    void setNonscriptable(int) {}

    MyQmlObject *myinvokableObject;
    Q_INVOKABLE MyQmlObject *returnme() { return this; }

    struct MyType {
        int value;
    };
    QVariant variant() const { return m_variant; }

signals:
    void basicSignal();
    void argumentSignal(int a, QString b, qreal c);
    void stringChanged();
    void objectChanged();
    void anotherBasicSignal();
    void thirdBasicSignal();
    void signalWithUnknownType(const MyQmlObject::MyType &arg);

public slots:
    void deleteMe() { delete this; }
    void methodNoArgs() { m_methodCalled = true; }
    void method(int a) { if(a == 163) m_methodIntCalled = true; }
    void setString(const QString &s) { m_string = s; }
    void myinvokable(MyQmlObject *o) { myinvokableObject = o; }
    void variantMethod(const QVariant &v) { m_variant = v; }

private:
    friend class tst_qdeclarativeecmascript;
    bool m_methodCalled;
    bool m_methodIntCalled;

    QObject *m_object;
    QString m_string;
    QList<QObject *> m_objectQList;
    int m_value;
    int m_resetProperty;
    QRegExp m_regExp;
    QVariant m_variant;
};

QML_DECLARE_TYPEINFO(MyQmlObject, QML_HAS_ATTACHED_PROPERTIES)
Q_DECLARE_METATYPE(MyQmlObject::MyType)

class testQObjectApi : public QObject
{
    Q_OBJECT
    Q_PROPERTY (int qobjectTestProperty READ qobjectTestProperty NOTIFY qobjectTestPropertyChanged)

public:
    testQObjectApi(QObject* parent = 0)
        : QObject(parent), m_testProperty(0)
    {
    }

    ~testQObjectApi() {}

    int qobjectTestProperty() const { return m_testProperty; }
    void setQObjectTestProperty(int tp) { m_testProperty = tp; emit qobjectTestPropertyChanged(tp); }

signals:
    void qobjectTestPropertyChanged(int testProperty);

private:
    int m_testProperty;
};

class ArbitraryVariantProvider : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariant arbitraryVariant READ arbitraryVariant WRITE setArbitraryVariant NOTIFY arbitraryVariantChanged)

public:
    ArbitraryVariantProvider(QObject *parent = 0)
        : QObject(parent), m_value(QVariant(QString(QLatin1String("random string value")))), m_count(1)
    {
    }

    ~ArbitraryVariantProvider() {}

    // the variant provided by the provider
    QVariant arbitraryVariant() const { return m_value; }
    void setArbitraryVariant(const QVariant& value) { m_value = value; emit arbitraryVariantChanged(); }
    Q_INVOKABLE int changeVariant()
    {
        QPixmap pv(150, 150);
        pv.fill(Qt::green);
        int choice = qrand() % 4;
        switch (choice) {
            case 0: setArbitraryVariant(QVariant(QString(QLatin1String("string variant value")))); break;
            case 1: setArbitraryVariant(QVariant(QColor(110, 120, 130))); break;
            case 2: setArbitraryVariant(QVariant(55)); break;
            default: setArbitraryVariant(QVariant(pv)); break;
        }

        m_count += 1;
        return m_count;
    }
    Q_INVOKABLE QVariant setVariantToFilledPixmap(int width, int height, int r, int g, int b)
    {
        QPixmap pv(width % 300, height % 300);
        pv.fill(QColor(r % 256, g % 256, b % 256));
        m_value = pv;
        m_count += 1;
        return m_value;
    }
    Q_INVOKABLE QVariant setVariantAddCount(int addToCount, const QVariant& newValue)
    {
        m_value = newValue;
        m_count += addToCount;
        return m_value;
    }
    Q_INVOKABLE QVariant possibleVariant(int randomFactorOne, int randomFactorTwo, int randomFactorThree) const
    {
        QVariant retn;
        QPixmap pv(randomFactorOne % 300, randomFactorTwo % 300);
        pv.fill(QColor(randomFactorOne % 256, randomFactorTwo % 256, randomFactorThree % 256));
        int choice = qrand() % 4;
        switch (choice) {
            case 0: retn = QVariant(QString(QLatin1String("string variant value"))); break;
            case 1: retn = QVariant(QColor(randomFactorThree % 256, randomFactorTwo % 256, randomFactorOne % 256)); break;
            case 2: retn = QVariant((55 + randomFactorThree)); break;
            default: retn = QVariant(pv); break;
        }
        return retn;
    }

    // the following functions cover permutations of return value and arguments.
    // functions with no return value:
    Q_INVOKABLE void doNothing() const { /* does nothing */ }                      // no args, const
    Q_INVOKABLE void incrementVariantChangeCount() { m_count = m_count + 1; }      // no args, nonconst
    Q_INVOKABLE void doNothing(int) const { /* does nothing. */ }                  // arg, const
    Q_INVOKABLE void setVariantChangeCount(int newCount) { m_count = newCount; }   // arg, nonconst
    // functions with return value:
    Q_INVOKABLE int variantChangeCount() const { return m_count; }                 // no args, const
    Q_INVOKABLE int modifyVariantChangeCount() { m_count += 3; return m_count; }   // no args, nonconst
    Q_INVOKABLE int countPlus(int value) const { return m_count + value; }         // arg, const
    Q_INVOKABLE int modifyVariantChangeCount(int modifier) { m_count += modifier; return m_count; } // arg, nonconst.

signals:
    void arbitraryVariantChanged();

private:
    QVariant m_value;
    int m_count;
};

class ScarceResourceProvider : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QPixmap smallScarceResource READ smallScarceResource WRITE setSmallScarceResource NOTIFY smallScarceResourceChanged)
    Q_PROPERTY(QPixmap largeScarceResource READ largeScarceResource WRITE setLargeScarceResource NOTIFY largeScarceResourceChanged)

public:
    ScarceResourceProvider(QObject *parent = 0)
        : QObject(parent), m_small(100, 100), m_large(1000, 1000), m_colour(1)
    {
        m_small.fill(Qt::blue);
        m_large.fill(Qt::blue);
    }

    ~ScarceResourceProvider() {}

    QPixmap smallScarceResource() const { return m_small; }
    void setSmallScarceResource(QPixmap v) { m_small = v; emit smallScarceResourceChanged(); }
    bool smallScarceResourceIsDetached() const { return m_small.isDetached(); }

    QPixmap largeScarceResource() const { return m_large; }
    void setLargeScarceResource(QPixmap v) { m_large = v; emit largeScarceResourceChanged(); }
    bool largeScarceResourceIsDetached() const { return m_large.isDetached(); }

    Q_INVOKABLE void changeResources()
    {
        QPixmap newSmall(100, 100);
        QPixmap newLarge(1000, 1000);

        if (m_colour == 1) {
            m_colour = 2;
            newSmall.fill(Qt::red);
            newLarge.fill(Qt::red);
        } else {
            m_colour = 1;
            newSmall.fill(Qt::blue);
            newLarge.fill(Qt::blue);
        }

        setSmallScarceResource(newSmall);
        setLargeScarceResource(newLarge);
    }

signals:
    void smallScarceResourceChanged();
    void largeScarceResourceChanged();

private:
    QPixmap m_small;
    QPixmap m_large;

    int m_colour;
};

void registerTypes();

#endif // TESTTYPES_H

