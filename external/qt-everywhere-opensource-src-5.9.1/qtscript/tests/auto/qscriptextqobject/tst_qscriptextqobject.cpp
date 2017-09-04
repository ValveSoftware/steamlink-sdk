/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/


#include <QtTest/QtTest>

#include <qscriptengine.h>
#include <qscriptcontext.h>
#include <qscriptvalueiterator.h>
#include <qwidget.h>
#include <qtextstream.h>
#include <qpushbutton.h>
#include <qlineedit.h>

#include "../shared/util.h"

struct CustomType
{
    QString string;
};
Q_DECLARE_METATYPE(CustomType)

Q_DECLARE_METATYPE(QBrush*)
Q_DECLARE_METATYPE(QObjectList)
Q_DECLARE_METATYPE(QList<int>)
Q_DECLARE_METATYPE(Qt::BrushStyle)
Q_DECLARE_METATYPE(QDir)

static void dirFromScript(const QScriptValue &in, QDir &out)
{
    QScriptValue path = in.property("path");
    if (!path.isValid())
        in.engine()->currentContext()->throwError("No path");
    else
        out.setPath(path.toString());
}

namespace MyNS
{
    class A : public QObject
    {
        Q_OBJECT
    public:
        enum Type {
            Foo,
            Bar
        };
        Q_ENUMS(Type)
    public Q_SLOTS:
        int slotTakingScopedEnumArg(MyNS::A::Type t) {
            return t;
        }
    };
}

class MyQObject : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int intProperty READ intProperty WRITE setIntProperty)
    Q_PROPERTY(QVariant variantProperty READ variantProperty WRITE setVariantProperty)
    Q_PROPERTY(QVariantList variantListProperty READ variantListProperty WRITE setVariantListProperty)
    Q_PROPERTY(QString stringProperty READ stringProperty WRITE setStringProperty)
    Q_PROPERTY(QStringList stringListProperty READ stringListProperty WRITE setStringListProperty)
    Q_PROPERTY(QByteArray byteArrayProperty READ byteArrayProperty WRITE setByteArrayProperty)
    Q_PROPERTY(QBrush brushProperty READ brushProperty WRITE setBrushProperty)
    Q_PROPERTY(double hiddenProperty READ hiddenProperty WRITE setHiddenProperty SCRIPTABLE false)
    Q_PROPERTY(int writeOnlyProperty WRITE setWriteOnlyProperty)
    Q_PROPERTY(int readOnlyProperty READ readOnlyProperty)
    Q_PROPERTY(QKeySequence shortcut READ shortcut WRITE setShortcut)
    Q_PROPERTY(CustomType propWithCustomType READ propWithCustomType WRITE setPropWithCustomType)
    Q_PROPERTY(Policy enumProperty READ enumProperty WRITE setEnumProperty)
    Q_PROPERTY(Ability flagsProperty READ flagsProperty WRITE setFlagsProperty)
    Q_ENUMS(Policy Strategy)
    Q_FLAGS(Ability)

public:
    enum Policy {
        FooPolicy = 0,
        BarPolicy,
        BazPolicy
    };

    enum Strategy {
        FooStrategy = 10,
        BarStrategy,
        BazStrategy
    };

    enum AbilityFlag {
        NoAbility  = 0x000,
        FooAbility = 0x001,
        BarAbility = 0x080,
        BazAbility = 0x200,
        AllAbility = FooAbility | BarAbility | BazAbility
    };

    Q_DECLARE_FLAGS(Ability, AbilityFlag)

    MyQObject(QObject *parent = 0)
        : QObject(parent),
          m_intValue(123),
          m_variantValue(QLatin1String("foo")),
          m_variantListValue(QVariantList() << QVariant(123) << QVariant(QLatin1String("foo"))),
          m_stringValue(QLatin1String("bar")),
          m_stringListValue(QStringList() << QLatin1String("zig") << QLatin1String("zag")),
          m_brushValue(QColor(10, 20, 30, 40)),
          m_hiddenValue(456.0),
          m_writeOnlyValue(789),
          m_readOnlyValue(987),
          m_enumValue(BarPolicy),
          m_flagsValue(FooAbility),
          m_qtFunctionInvoked(-1)
        { }

    int intProperty() const
        { return m_intValue; }
    void setIntProperty(int value)
        { m_intValue = value; }

    QVariant variantProperty() const
        { return m_variantValue; }
    void setVariantProperty(const QVariant &value)
        { m_variantValue = value; }

    QVariantList variantListProperty() const
        { return m_variantListValue; }
    void setVariantListProperty(const QVariantList &value)
        { m_variantListValue = value; }

    QString stringProperty() const
        { return m_stringValue; }
    void setStringProperty(const QString &value)
        { m_stringValue = value; }

    QStringList stringListProperty() const
        { return m_stringListValue; }
    void setStringListProperty(const QStringList &value)
        { m_stringListValue = value; }

    QByteArray byteArrayProperty() const
        { return m_byteArrayValue; }
    void setByteArrayProperty(const QByteArray &value)
        { m_byteArrayValue = value; }

    QBrush brushProperty() const
        { return m_brushValue; }
    Q_INVOKABLE void setBrushProperty(const QBrush &value)
        { m_brushValue = value; }

    double hiddenProperty() const
        { return m_hiddenValue; }
    void setHiddenProperty(double value)
        { m_hiddenValue = value; }

    int writeOnlyProperty() const
        { return m_writeOnlyValue; }
    void setWriteOnlyProperty(int value)
        { m_writeOnlyValue = value; }

    int readOnlyProperty() const
        { return m_readOnlyValue; }

    QKeySequence shortcut() const
        { return m_shortcut; }
    void setShortcut(const QKeySequence &seq)
        { m_shortcut = seq; }

    CustomType propWithCustomType() const
        { return m_customType; }
    void setPropWithCustomType(const CustomType &c)
        { m_customType = c; }

    Policy enumProperty() const
        { return m_enumValue; }
    void setEnumProperty(Policy policy)
        { m_enumValue = policy; }

    Ability flagsProperty() const
        { return m_flagsValue; }
    void setFlagsProperty(Ability ability)
        { m_flagsValue = ability; }

    int qtFunctionInvoked() const
        { return m_qtFunctionInvoked; }

    QVariantList qtFunctionActuals() const
        { return m_actuals; }

    void resetQtFunctionInvoked()
        { m_qtFunctionInvoked = -1; m_actuals.clear(); }

    void clearConnectNotifySignals()
        { m_connectNotifySignals.clear(); }
    void clearDisconnectNotifySignals()
        { m_disconnectNotifySignals.clear(); }
    QList<QMetaMethod> connectNotifySignals() const
        { return m_connectNotifySignals; }
    QList<QMetaMethod> disconnectNotifySignals() const
        { return m_disconnectNotifySignals; }
    bool hasSingleConnectNotifySignal(const QMetaMethod &signal) const
        { return (m_connectNotifySignals.size() == 1) && (m_connectNotifySignals.first() == signal); }
    bool hasConnectNotifySignal(const QMetaMethod &signal) const
        { return m_connectNotifySignals.contains(signal); }
    bool hasSingleDisconnectNotifySignal(const QMetaMethod &signal) const
        { return (m_disconnectNotifySignals.size() == 1) && (m_disconnectNotifySignals.first() == signal); }

    Q_INVOKABLE void myInvokable()
        { m_qtFunctionInvoked = 0; }
    Q_INVOKABLE void myInvokableWithIntArg(int arg)
        { m_qtFunctionInvoked = 1; m_actuals << arg; }
    Q_INVOKABLE void myInvokableWithLonglongArg(qlonglong arg)
        { m_qtFunctionInvoked = 2; m_actuals << arg; }
    Q_INVOKABLE void myInvokableWithFloatArg(float arg)
        { m_qtFunctionInvoked = 3; m_actuals << arg; }
    Q_INVOKABLE void myInvokableWithDoubleArg(double arg)
        { m_qtFunctionInvoked = 4; m_actuals << arg; }
    Q_INVOKABLE void myInvokableWithStringArg(const QString &arg)
        { m_qtFunctionInvoked = 5; m_actuals << arg; }
    Q_INVOKABLE void myInvokableWithIntArgs(int arg1, int arg2)
        { m_qtFunctionInvoked = 6; m_actuals << arg1 << arg2; }
    Q_INVOKABLE int myInvokableReturningInt()
        { m_qtFunctionInvoked = 7; return 123; }
    Q_INVOKABLE qlonglong myInvokableReturningLongLong()
        { m_qtFunctionInvoked = 39; return 456; }
    Q_INVOKABLE QString myInvokableReturningString()
        { m_qtFunctionInvoked = 8; return QLatin1String("ciao"); }
    Q_INVOKABLE QVariant myInvokableReturningVariant()
        { m_qtFunctionInvoked = 60; return 123; }
    Q_INVOKABLE QScriptValue myInvokableReturningScriptValue()
        { m_qtFunctionInvoked = 61; return 456; }
    Q_INVOKABLE void myInvokableWithIntArg(int arg1, int arg2) // overload
        { m_qtFunctionInvoked = 9; m_actuals << arg1 << arg2; }
    Q_INVOKABLE void myInvokableWithEnumArg(Policy policy)
        { m_qtFunctionInvoked = 10; m_actuals << policy; }
    Q_INVOKABLE void myInvokableWithQualifiedEnumArg(MyQObject::Policy policy)
        { m_qtFunctionInvoked = 36; m_actuals << policy; }
    Q_INVOKABLE Policy myInvokableReturningEnum()
        { m_qtFunctionInvoked = 37; return BazPolicy; }
    Q_INVOKABLE MyQObject::Strategy myInvokableReturningQualifiedEnum()
        { m_qtFunctionInvoked = 38; return BazStrategy; }
    Q_INVOKABLE QVector<int> myInvokableReturningVectorOfInt()
        { m_qtFunctionInvoked = 11; return QVector<int>(); }
    Q_INVOKABLE void myInvokableWithVectorOfIntArg(const QVector<int> &)
        { m_qtFunctionInvoked = 12; }
    Q_INVOKABLE QObject *myInvokableReturningQObjectStar()
        { m_qtFunctionInvoked = 13; return this; }
    Q_INVOKABLE QObjectList myInvokableWithQObjectListArg(const QObjectList &lst)
        { m_qtFunctionInvoked = 14; m_actuals << qVariantFromValue(lst); return lst; }
    Q_INVOKABLE QVariant myInvokableWithVariantArg(const QVariant &v)
        { m_qtFunctionInvoked = 15; m_actuals << v; return v; }
    Q_INVOKABLE QVariantMap myInvokableWithVariantMapArg(const QVariantMap &vm)
        { m_qtFunctionInvoked = 16; m_actuals << vm; return vm; }
    Q_INVOKABLE QVariantList myInvokableWithVariantListArg(const QVariantList &lst)
        { m_qtFunctionInvoked = 62; m_actuals.append(QVariant(lst)); return lst; }
    Q_INVOKABLE QList<int> myInvokableWithListOfIntArg(const QList<int> &lst)
        { m_qtFunctionInvoked = 17; m_actuals << qVariantFromValue(lst); return lst; }
    Q_INVOKABLE QObject* myInvokableWithQObjectStarArg(QObject *obj)
        { m_qtFunctionInvoked = 18; m_actuals << qVariantFromValue(obj); return obj; }
    Q_INVOKABLE QBrush myInvokableWithQBrushArg(const QBrush &brush)
        { m_qtFunctionInvoked = 19; m_actuals << qVariantFromValue(brush); return brush; }
    Q_INVOKABLE void myInvokableWithBrushStyleArg(Qt::BrushStyle style)
        { m_qtFunctionInvoked = 43; m_actuals << qVariantFromValue(style); }
    Q_INVOKABLE void myInvokableWithVoidStarArg(void *arg)
        { m_qtFunctionInvoked = 44; m_actuals << qVariantFromValue(arg); }
    Q_INVOKABLE void myInvokableWithAmbiguousArg(int arg)
        { m_qtFunctionInvoked = 45; m_actuals << qVariantFromValue(arg); }
    Q_INVOKABLE void myInvokableWithAmbiguousArg(uint arg)
        { m_qtFunctionInvoked = 46; m_actuals << qVariantFromValue(arg); }
    Q_INVOKABLE void myInvokableWithDefaultArgs(int arg1, const QString &arg2 = "")
        { m_qtFunctionInvoked = 47; m_actuals << qVariantFromValue(arg1) << qVariantFromValue(arg2); }
    Q_INVOKABLE QObject& myInvokableReturningRef()
        { m_qtFunctionInvoked = 48; return *this; }
    Q_INVOKABLE const QObject& myInvokableReturningConstRef() const
        { const_cast<MyQObject*>(this)->m_qtFunctionInvoked = 49; return *this; }
    Q_INVOKABLE void myInvokableWithPointArg(const QPoint &arg)
        { const_cast<MyQObject*>(this)->m_qtFunctionInvoked = 50; m_actuals << qVariantFromValue(arg); }
    Q_INVOKABLE void myInvokableWithPointArg(const QPointF &arg)
        { const_cast<MyQObject*>(this)->m_qtFunctionInvoked = 51; m_actuals << qVariantFromValue(arg); }
    Q_INVOKABLE void myInvokableWithMyQObjectArg(MyQObject *arg)
        { m_qtFunctionInvoked = 52; m_actuals << qVariantFromValue((QObject*)arg); }
    Q_INVOKABLE MyQObject* myInvokableReturningMyQObject()
        { m_qtFunctionInvoked = 53; return this; }
    Q_INVOKABLE void myInvokableWithConstMyQObjectArg(const MyQObject *arg)
        { m_qtFunctionInvoked = 54; m_actuals << qVariantFromValue((QObject*)arg); }
    Q_INVOKABLE void myInvokableWithQDirArg(const QDir &arg)
        { m_qtFunctionInvoked = 55; m_actuals << qVariantFromValue(arg); }
    Q_INVOKABLE QScriptValue myInvokableWithScriptValueArg(const QScriptValue &arg)
        { m_qtFunctionInvoked = 56; return arg; }
    Q_INVOKABLE QObject* myInvokableReturningMyQObjectAsQObject()
        { m_qtFunctionInvoked = 57; return this; }
    Q_INVOKABLE Ability myInvokableWithFlagsArg(Ability arg)
        { m_qtFunctionInvoked = 58; m_actuals << int(arg); return arg; }
    Q_INVOKABLE MyQObject::Ability myInvokableWithQualifiedFlagsArg(MyQObject::Ability arg)
        { m_qtFunctionInvoked = 59; m_actuals << int(arg); return arg; }
    Q_INVOKABLE QWidget *myInvokableWithQWidgetStarArg(QWidget *arg)
        { m_qtFunctionInvoked = 63; m_actuals << qVariantFromValue((QWidget*)arg); return arg; }
    Q_INVOKABLE short myInvokableWithShortArg(short arg)
        { m_qtFunctionInvoked = 64; m_actuals << qVariantFromValue(arg); return arg; }
    Q_INVOKABLE unsigned short myInvokableWithUShortArg(unsigned short arg)
        { m_qtFunctionInvoked = 65; m_actuals << qVariantFromValue(arg); return arg; }
    Q_INVOKABLE char myInvokableWithCharArg(char arg)
        { m_qtFunctionInvoked = 66; m_actuals << qVariantFromValue(arg); return arg; }
    Q_INVOKABLE unsigned char myInvokableWithUCharArg(unsigned char arg)
        { m_qtFunctionInvoked = 67; m_actuals << qVariantFromValue(arg); return arg; }
    Q_INVOKABLE qulonglong myInvokableWithULonglongArg(qulonglong arg)
        { m_qtFunctionInvoked = 68; m_actuals << qVariantFromValue(arg); return arg; }
    Q_INVOKABLE long myInvokableWithLongArg(long arg)
        { m_qtFunctionInvoked = 69; m_actuals << qVariantFromValue(arg); return arg; }
    Q_INVOKABLE unsigned long myInvokableWithULongArg(unsigned long arg)
        { m_qtFunctionInvoked = 70; m_actuals << qVariantFromValue(arg); return arg; }

    Q_INVOKABLE QObjectList findObjects() const
    {  return findChildren<QObject *>();  }
    Q_INVOKABLE QList<int> myInvokableNumbers() const
    {  return QList<int>() << 1 << 2 << 3; }

    void emitMySignal()
        { emit mySignal(); }
    void emitMySignalWithIntArg(int arg)
        { emit mySignalWithIntArg(arg); }
    void emitMySignal2(bool arg)
        { emit mySignal2(arg); }
    void emitMySignal2()
        { emit mySignal2(); }
    void emitMyOverloadedSignal(int arg)
        { emit myOverloadedSignal(arg); }
    void emitMyOverloadedSignal(const QString &arg)
        { emit myOverloadedSignal(arg); }
    void emitMyOtherOverloadedSignal(const QString &arg)
        { emit myOtherOverloadedSignal(arg); }
    void emitMyOtherOverloadedSignal(int arg)
        { emit myOtherOverloadedSignal(arg); }
    void emitMySignalWithDefaultArgWithArg(int arg)
        { emit mySignalWithDefaultArg(arg); }
    void emitMySignalWithDefaultArg()
        { emit mySignalWithDefaultArg(); }
    void emitMySignalWithVariantArg(const QVariant &arg)
        { emit mySignalWithVariantArg(arg); }
    void emitMySignalWithScriptEngineArg(QScriptEngine *arg)
        { emit mySignalWithScriptEngineArg(arg); }

public Q_SLOTS:
    void mySlot()
        { m_qtFunctionInvoked = 20; }
    void mySlotWithIntArg(int arg)
        { m_qtFunctionInvoked = 21; m_actuals << arg; }
    void mySlotWithDoubleArg(double arg)
        { m_qtFunctionInvoked = 22; m_actuals << arg; }
    void mySlotWithStringArg(const QString &arg)
        { m_qtFunctionInvoked = 23; m_actuals << arg; }

    void myOverloadedSlot()
        { m_qtFunctionInvoked = 24; }
    void myOverloadedSlot(QObject *arg)
        { m_qtFunctionInvoked = 41; m_actuals << qVariantFromValue(arg); }
    void myOverloadedSlot(bool arg)
        { m_qtFunctionInvoked = 25; m_actuals << arg; }
    void myOverloadedSlot(const QStringList &arg)
        { m_qtFunctionInvoked = 42; m_actuals << arg; }
    void myOverloadedSlot(double arg)
        { m_qtFunctionInvoked = 26; m_actuals << arg; }
    void myOverloadedSlot(float arg)
        { m_qtFunctionInvoked = 27; m_actuals << arg; }
    void myOverloadedSlot(int arg)
        { m_qtFunctionInvoked = 28; m_actuals << arg; }
    void myOverloadedSlot(const QString &arg)
        { m_qtFunctionInvoked = 29; m_actuals << arg; }
    void myOverloadedSlot(const QColor &arg)
        { m_qtFunctionInvoked = 30; m_actuals << arg; }
    void myOverloadedSlot(const QBrush &arg)
        { m_qtFunctionInvoked = 31; m_actuals << arg; }
    void myOverloadedSlot(const QDateTime &arg)
        { m_qtFunctionInvoked = 32; m_actuals << arg; }
    void myOverloadedSlot(const QDate &arg)
        { m_qtFunctionInvoked = 33; m_actuals << arg; }
    void myOverloadedSlot(const QTime &arg)
        { m_qtFunctionInvoked = 69; m_actuals << arg; }
    void myOverloadedSlot(const QRegExp &arg)
        { m_qtFunctionInvoked = 34; m_actuals << arg; }
    void myOverloadedSlot(const QVariant &arg)
        { m_qtFunctionInvoked = 35; m_actuals << arg; }

    virtual int myVirtualSlot(int arg)
        { m_qtFunctionInvoked = 58; return arg; }

    void qscript_call(int arg)
        { m_qtFunctionInvoked = 40; m_actuals << arg; }

protected Q_SLOTS:
    void myProtectedSlot() { m_qtFunctionInvoked = 36; }

private Q_SLOTS:
    void myPrivateSlot() { }

Q_SIGNALS:
    void mySignal();
    void mySignalWithIntArg(int arg);
    void mySignalWithDoubleArg(double arg);
    void mySignal2(bool arg = false);
    void myOverloadedSignal(int arg);
    void myOverloadedSignal(const QString &arg);
    void myOtherOverloadedSignal(const QString &arg);
    void myOtherOverloadedSignal(int arg);
    void mySignalWithDefaultArg(int arg = 123);
    void mySignalWithVariantArg(const QVariant &arg);
    void mySignalWithScriptEngineArg(QScriptEngine *arg);

protected:
    void connectNotify(const QMetaMethod &signal) {
        m_connectNotifySignals.append(signal);
    }
    void disconnectNotify(const QMetaMethod &signal) {
        m_disconnectNotifySignals.append(signal);
    }

protected:
    int m_intValue;
    QVariant m_variantValue;
    QVariantList m_variantListValue;
    QString m_stringValue;
    QStringList m_stringListValue;
    QByteArray m_byteArrayValue;
    QBrush m_brushValue;
    double m_hiddenValue;
    int m_writeOnlyValue;
    int m_readOnlyValue;
    QKeySequence m_shortcut;
    CustomType m_customType;
    Policy m_enumValue;
    Ability m_flagsValue;
    int m_qtFunctionInvoked;
    QVariantList m_actuals;
    QList<QMetaMethod> m_connectNotifySignals;
    QList<QMetaMethod> m_disconnectNotifySignals;
};

Q_DECLARE_METATYPE(MyQObject*)
Q_DECLARE_METATYPE(MyQObject::Policy)

class MyOtherQObject : public MyQObject
{
    Q_OBJECT
public:
    MyOtherQObject(QObject *parent = 0)
        : MyQObject(parent)
        { }
public Q_SLOTS:
    virtual int myVirtualSlot(int arg)
        { m_qtFunctionInvoked = 59; return arg; }
};

class MyEnumTestQObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString p1 READ p1)
    Q_PROPERTY(QString p2 READ p2)
    Q_PROPERTY(QString p3 READ p3 SCRIPTABLE false)
    Q_PROPERTY(QString p4 READ p4)
    Q_PROPERTY(QString p5 READ p5 SCRIPTABLE false)
    Q_PROPERTY(QString p6 READ p6)
public:
    MyEnumTestQObject(QObject *parent = 0)
        : QObject(parent) { }
    QString p1() const { return QLatin1String("p1"); }
    QString p2() const { return QLatin1String("p2"); }
    QString p3() const { return QLatin1String("p3"); }
    QString p4() const { return QLatin1String("p4"); }
    QString p5() const { return QLatin1String("p5"); }
    QString p6() const { return QLatin1String("p5"); }
public Q_SLOTS:
    void mySlot() { }
    void myOtherSlot() { }
Q_SIGNALS:
    void mySignal();
};

class tst_QScriptExtQObject : public QObject
{
    Q_OBJECT

public:
    tst_QScriptExtQObject();
    virtual ~tst_QScriptExtQObject();

public slots:
    void init();
    void cleanup();

protected slots:
    void onSignalHandlerException(const QScriptValue &exception)
    {
        m_signalHandlerException = exception;
    }

private slots:
    void registeredTypes();
    void getSetStaticProperty();
    void getSetStaticProperty_propertyFlags();
    void getSetStaticProperty_changeInCpp();
    void getSetStaticProperty_changeInJS();
    void getSetStaticProperty_compatibleVariantTypes();
    void getSetStaticProperty_conversion();
    void getSetStaticProperty_delete();
    void getSetStaticProperty_nonScriptable();
    void getSetStaticProperty_writeOnly();
    void getSetStaticProperty_readOnly();
    void getSetStaticProperty_enum();
    void getSetStaticProperty_qflags();
    void getSetStaticProperty_pointerDeref();
    void getSetStaticProperty_customGetterSetter();
    void getSetStaticProperty_methodPersistence();
    void getSetDynamicProperty();
    void getSetDynamicProperty_deleteFromCpp();
    void getSetDynamicProperty_hideChildObject();
    void getSetDynamicProperty_setBeforeGet();
    void getSetDynamicProperty_doNotHideJSProperty();
    void getSetChildren();
    void callQtInvokable();
    void callQtInvokable2();
    void callQtInvokable3();
    void callQtInvokable4();
    void callQtInvokable5();
    void callQtInvokable6();
    void callQtInvokable7();
    void connectAndDisconnect();
    void connectAndDisconnect_emitFromJS();
    void connectAndDisconnect_senderWrapperCollected();
    void connectAndDisconnectWithBadArgs();
    void connectAndDisconnect_senderDeleted();
    void cppConnectAndDisconnect();
    void cppConnectAndDisconnect2();
    void classEnums();
    void classConstructor();
    void overrideInvokable();
    void transferInvokable();
    void findChild();
    void findChildren();
    void childObjects();
    void overloadedSlots();
    void enumerate_data();
    void enumerate();
    void enumerateSpecial();
    void wrapOptions();
    void prototypes();
    void objectDeleted();
    void connectToDestroyedSignal();
    void emitAfterReceiverDeleted();
    void inheritedSlots();
    void enumerateMetaObject();
    void nestedArrayAsSlotArgument_data();
    void nestedArrayAsSlotArgument();
    void nestedObjectAsSlotArgument_data();
    void nestedObjectAsSlotArgument();
    void propertyAccessThroughActivationObject();
    void connectionRemovedAfterQueuedCall();
    void collectQObjectWithClosureSlot();
    void collectQObjectWithClosureSlot2();

private:
    QScriptEngine *m_engine;
    MyQObject *m_myObject;
    QScriptValue m_signalHandlerException;
};

tst_QScriptExtQObject::tst_QScriptExtQObject()
{
}

tst_QScriptExtQObject::~tst_QScriptExtQObject()
{
}

void tst_QScriptExtQObject::init()
{
    m_engine = new QScriptEngine();
    m_myObject = new MyQObject();
    m_engine->globalObject().setProperty("myObject", m_engine->newQObject(m_myObject));
    m_engine->globalObject().setProperty("global", m_engine->globalObject());
}

void tst_QScriptExtQObject::cleanup()
{
    delete m_engine;
    delete m_myObject;
}

// this test has to be first and test that some types are automatically registered
void tst_QScriptExtQObject::registeredTypes()
{
    QScriptEngine e;
    QObject *t = new MyQObject;
    QObject *c = new QObject(t);
    c->setObjectName ("child1");

    e.globalObject().setProperty("MyTest", e.newQObject(t));

    QScriptValue v1 = e.evaluate("MyTest.findObjects()[0].objectName;");
    QCOMPARE(v1.toString(), c->objectName());

    QScriptValue v2 = e.evaluate("MyTest.myInvokableNumbers()");
    QCOMPARE(qscriptvalue_cast<QList<int> >(v2), (QList<int>() << 1 << 2 << 3));
}


static QScriptValue getSetProperty(QScriptContext *ctx, QScriptEngine *)
{
    if (ctx->argumentCount() != 0)
        ctx->callee().setProperty("value", ctx->argument(0));
    return ctx->callee().property("value");
}

static QScriptValue policyToScriptValue(QScriptEngine *engine, const MyQObject::Policy &policy)
{
    return qScriptValueFromValue(engine, policy);
}

static void policyFromScriptValue(const QScriptValue &value, MyQObject::Policy &policy)
{
    QString str = value.toString();
    if (str == QLatin1String("red"))
        policy = MyQObject::FooPolicy;
    else if (str == QLatin1String("green"))
        policy = MyQObject::BarPolicy;
    else if (str == QLatin1String("blue"))
        policy = MyQObject::BazPolicy;
    else
        policy = (MyQObject::Policy)-1;
}

void tst_QScriptExtQObject::getSetStaticProperty()
{
    QCOMPARE(m_engine->evaluate("myObject.noSuchProperty").isUndefined(), true);

    // initial value (set in MyQObject constructor)
    QCOMPARE(m_engine->evaluate("myObject.intProperty")
             .strictlyEquals(QScriptValue(m_engine, 123.0)), true);
    QCOMPARE(m_engine->evaluate("myObject.variantProperty")
             .toVariant(), QVariant(QLatin1String("foo")));
    QCOMPARE(m_engine->evaluate("myObject.stringProperty")
             .strictlyEquals(QScriptValue(m_engine, QLatin1String("bar"))), true);
    QCOMPARE(m_engine->evaluate("myObject.variantListProperty").isArray(), true);
    QCOMPARE(m_engine->evaluate("myObject.variantListProperty.length")
             .strictlyEquals(QScriptValue(m_engine, 2)), true);
    QCOMPARE(m_engine->evaluate("myObject.variantListProperty[0]")
             .strictlyEquals(QScriptValue(m_engine, 123)), true);
    QCOMPARE(m_engine->evaluate("myObject.variantListProperty[1]")
             .strictlyEquals(QScriptValue(m_engine, QLatin1String("foo"))), true);
    QCOMPARE(m_engine->evaluate("myObject.stringListProperty").isArray(), true);
    QCOMPARE(m_engine->evaluate("myObject.stringListProperty.length")
             .strictlyEquals(QScriptValue(m_engine, 2)), true);
    QCOMPARE(m_engine->evaluate("myObject.stringListProperty[0]").isString(), true);
    QCOMPARE(m_engine->evaluate("myObject.stringListProperty[0]").toString(),
             QLatin1String("zig"));
    QCOMPARE(m_engine->evaluate("myObject.stringListProperty[1]").isString(), true);
    QCOMPARE(m_engine->evaluate("myObject.stringListProperty[1]").toString(),
             QLatin1String("zag"));
}

void tst_QScriptExtQObject::getSetStaticProperty_propertyFlags()
{
    // default flags for "normal" properties
    {
        QScriptValue mobj = m_engine->globalObject().property("myObject");
        QVERIFY(!(mobj.propertyFlags("intProperty") & QScriptValue::ReadOnly));
        QVERIFY(mobj.propertyFlags("intProperty") & QScriptValue::Undeletable);
        QVERIFY(mobj.propertyFlags("intProperty") & QScriptValue::PropertyGetter);
        QVERIFY(mobj.propertyFlags("intProperty") & QScriptValue::PropertySetter);
        QVERIFY(!(mobj.propertyFlags("intProperty") & QScriptValue::SkipInEnumeration));
        QVERIFY(mobj.propertyFlags("intProperty") & QScriptValue::QObjectMember);

        QVERIFY(!(mobj.propertyFlags("mySlot") & QScriptValue::ReadOnly));
        QVERIFY(!(mobj.propertyFlags("mySlot") & QScriptValue::Undeletable));
        QVERIFY(!(mobj.propertyFlags("mySlot") & QScriptValue::SkipInEnumeration));
        QVERIFY(mobj.propertyFlags("mySlot") & QScriptValue::QObjectMember);

        // signature-based property
        QVERIFY(!(mobj.propertyFlags("mySlot()") & QScriptValue::ReadOnly));
        QVERIFY(!(mobj.propertyFlags("mySlot()") & QScriptValue::Undeletable));
        QVERIFY(!(mobj.propertyFlags("mySlot()") & QScriptValue::SkipInEnumeration));
        QVERIFY(mobj.propertyFlags("mySlot()") & QScriptValue::QObjectMember);
    }
}

void tst_QScriptExtQObject::getSetStaticProperty_changeInCpp()
{
    // property change in C++ should be reflected in script
    m_myObject->setIntProperty(456);
    QCOMPARE(m_engine->evaluate("myObject.intProperty")
             .strictlyEquals(QScriptValue(m_engine, 456)), true);
    m_myObject->setIntProperty(789);
    QCOMPARE(m_engine->evaluate("myObject.intProperty")
             .strictlyEquals(QScriptValue(m_engine, 789)), true);

    m_myObject->setVariantProperty(QLatin1String("bar"));
    QVERIFY(m_engine->evaluate("myObject.variantProperty")
            .strictlyEquals(QScriptValue(m_engine, QLatin1String("bar"))));
    m_myObject->setVariantProperty(42);
    QCOMPARE(m_engine->evaluate("myObject.variantProperty")
             .toVariant(), QVariant(42));
    m_myObject->setVariantProperty(qVariantFromValue(QBrush()));
    QVERIFY(m_engine->evaluate("myObject.variantProperty").isVariant());

    m_myObject->setStringProperty(QLatin1String("baz"));
    QCOMPARE(m_engine->evaluate("myObject.stringProperty")
             .equals(QScriptValue(m_engine, QLatin1String("baz"))), true);
    m_myObject->setStringProperty(QLatin1String("zab"));
    QCOMPARE(m_engine->evaluate("myObject.stringProperty")
             .equals(QScriptValue(m_engine, QLatin1String("zab"))), true);
}

void tst_QScriptExtQObject::getSetStaticProperty_changeInJS()
{
    // property change in script should be reflected in C++
    QCOMPARE(m_engine->evaluate("myObject.intProperty = 123")
             .strictlyEquals(QScriptValue(m_engine, 123)), true);
    QCOMPARE(m_engine->evaluate("myObject.intProperty")
             .strictlyEquals(QScriptValue(m_engine, 123)), true);
    QCOMPARE(m_myObject->intProperty(), 123);
    QCOMPARE(m_engine->evaluate("myObject.intProperty = 'ciao!';"
                                "myObject.intProperty")
             .strictlyEquals(QScriptValue(m_engine, 0)), true);
    QCOMPARE(m_myObject->intProperty(), 0);
    QCOMPARE(m_engine->evaluate("myObject.intProperty = '123';"
                                "myObject.intProperty")
             .strictlyEquals(QScriptValue(m_engine, 123)), true);
    QCOMPARE(m_myObject->intProperty(), 123);

    QCOMPARE(m_engine->evaluate("myObject.stringProperty = 'ciao'")
             .strictlyEquals(QScriptValue(m_engine, QLatin1String("ciao"))), true);
    QCOMPARE(m_engine->evaluate("myObject.stringProperty")
             .strictlyEquals(QScriptValue(m_engine, QLatin1String("ciao"))), true);
    QCOMPARE(m_myObject->stringProperty(), QLatin1String("ciao"));
    QCOMPARE(m_engine->evaluate("myObject.stringProperty = 123;"
                                "myObject.stringProperty")
             .strictlyEquals(QScriptValue(m_engine, QLatin1String("123"))), true);
    QCOMPARE(m_myObject->stringProperty(), QLatin1String("123"));
    QVERIFY(m_engine->evaluate("myObject.stringProperty = null;"
                               "myObject.stringProperty")
            .strictlyEquals(QScriptValue(m_engine, QString())));
    QCOMPARE(m_myObject->stringProperty(), QString());
    QVERIFY(m_engine->evaluate("myObject.stringProperty = undefined;"
                               "myObject.stringProperty")
            .strictlyEquals(QScriptValue(m_engine, QString())));
    QCOMPARE(m_myObject->stringProperty(), QString());

    QCOMPARE(m_engine->evaluate("myObject.variantProperty = 'foo';"
                                "myObject.variantProperty.valueOf()").toString(), QLatin1String("foo"));
    QCOMPARE(m_myObject->variantProperty(), QVariant(QLatin1String("foo")));
    QVERIFY(m_engine->evaluate("myObject.variantProperty = undefined;"
                               "myObject.variantProperty").isUndefined());
    QVERIFY(!m_myObject->variantProperty().isValid());
    QVERIFY(m_engine->evaluate("myObject.variantProperty = null;"
                               "myObject.variantProperty").isUndefined());
    QVERIFY(!m_myObject->variantProperty().isValid());
    QCOMPARE(m_engine->evaluate("myObject.variantProperty = 42;"
                                "myObject.variantProperty").toNumber(), 42.0);
    QCOMPARE(m_myObject->variantProperty().toDouble(), 42.0);

    QCOMPARE(m_engine->evaluate("myObject.variantListProperty = [1, 'two', true];"
                                "myObject.variantListProperty.length")
             .strictlyEquals(QScriptValue(m_engine, 3)), true);
    QCOMPARE(m_engine->evaluate("myObject.variantListProperty[0]")
             .strictlyEquals(QScriptValue(m_engine, 1)), true);
    QCOMPARE(m_engine->evaluate("myObject.variantListProperty[1]")
             .strictlyEquals(QScriptValue(m_engine, QLatin1String("two"))), true);
    QCOMPARE(m_engine->evaluate("myObject.variantListProperty[2]")
             .strictlyEquals(QScriptValue(m_engine, true)), true);
    {
        QVariantList vl = qscriptvalue_cast<QVariantList>(m_engine->evaluate("myObject.variantListProperty"));
        QCOMPARE(vl, QVariantList()
                 << QVariant(1)
                 << QVariant(QLatin1String("two"))
                 << QVariant(true));
    }

    QCOMPARE(m_engine->evaluate("myObject.stringListProperty = [1, 'two', true];"
                                "myObject.stringListProperty.length")
             .strictlyEquals(QScriptValue(m_engine, 3)), true);
    QCOMPARE(m_engine->evaluate("myObject.stringListProperty[0]").isString(), true);
    QCOMPARE(m_engine->evaluate("myObject.stringListProperty[0]").toString(),
             QLatin1String("1"));
    QCOMPARE(m_engine->evaluate("myObject.stringListProperty[1]").isString(), true);
    QCOMPARE(m_engine->evaluate("myObject.stringListProperty[1]").toString(),
             QLatin1String("two"));
    QCOMPARE(m_engine->evaluate("myObject.stringListProperty[2]").isString(), true);
    QCOMPARE(m_engine->evaluate("myObject.stringListProperty[2]").toString(),
             QLatin1String("true"));
    {
        QStringList sl = qscriptvalue_cast<QStringList>(m_engine->evaluate("myObject.stringListProperty"));
        QCOMPARE(sl, QStringList()
                 << QLatin1String("1")
                 << QLatin1String("two")
                 << QLatin1String("true"));
    }
}

void tst_QScriptExtQObject::getSetStaticProperty_compatibleVariantTypes()
{
    // test setting properties where we can't convert the type natively but where the
    // types happen to be compatible variant types already
    {
        QKeySequence sequence(Qt::ControlModifier + Qt::AltModifier + Qt::Key_Delete);
        QScriptValue mobj = m_engine->globalObject().property("myObject");

        QVERIFY(m_myObject->shortcut().isEmpty());
        mobj.setProperty("shortcut", m_engine->newVariant(sequence));
        QVERIFY(m_myObject->shortcut() == sequence);
    }
    {
        CustomType t; t.string = "hello";
        QScriptValue mobj = m_engine->globalObject().property("myObject");

        QVERIFY(m_myObject->propWithCustomType().string.isEmpty());
        mobj.setProperty("propWithCustomType", m_engine->newVariant(qVariantFromValue(t)));
        QVERIFY(m_myObject->propWithCustomType().string == t.string);
    }
}

void tst_QScriptExtQObject::getSetStaticProperty_conversion()
{
    // test that we do value conversion if necessary when setting properties
    {
        QScriptValue br = m_engine->evaluate("myObject.brushProperty");
        QVERIFY(br.isVariant());
        QVERIFY(!br.strictlyEquals(m_engine->evaluate("myObject.brushProperty")));
        QCOMPARE(qscriptvalue_cast<QBrush>(br), m_myObject->brushProperty());
        QCOMPARE(qscriptvalue_cast<QColor>(br), m_myObject->brushProperty().color());

        QColor newColor(40, 30, 20, 10);
        QScriptValue val = qScriptValueFromValue(m_engine, newColor);
        m_engine->globalObject().setProperty("myColor", val);
        QScriptValue ret = m_engine->evaluate("myObject.brushProperty = myColor");
        QCOMPARE(ret.strictlyEquals(val), true);
        br = m_engine->evaluate("myObject.brushProperty");
        QCOMPARE(qscriptvalue_cast<QBrush>(br), QBrush(newColor));
        QCOMPARE(qscriptvalue_cast<QColor>(br), newColor);

        m_engine->globalObject().setProperty("myColor", QScriptValue());
    }
}

void tst_QScriptExtQObject::getSetStaticProperty_delete()
{
    // try to delete
    QCOMPARE(m_engine->evaluate("delete myObject.intProperty").toBoolean(), false);
    QCOMPARE(m_engine->evaluate("myObject.intProperty").toNumber(), 123.0);

    m_myObject->setVariantProperty(42);
    QCOMPARE(m_engine->evaluate("delete myObject.variantProperty").toBoolean(), false);
    QCOMPARE(m_engine->evaluate("myObject.variantProperty").toNumber(), 42.0);
}

void tst_QScriptExtQObject::getSetStaticProperty_nonScriptable()
{
    // non-scriptable property
    QCOMPARE(m_myObject->hiddenProperty(), 456.0);
    QCOMPARE(m_engine->evaluate("myObject.hiddenProperty").isUndefined(), true);
    QCOMPARE(m_engine->evaluate("myObject.hiddenProperty = 123;"
                                "myObject.hiddenProperty").toInt32(), 123);
    QCOMPARE(m_myObject->hiddenProperty(), 456.0);
}

void tst_QScriptExtQObject::getSetStaticProperty_writeOnly()
{
    // write-only property
    QCOMPARE(m_myObject->writeOnlyProperty(), 789);
    QCOMPARE(m_engine->evaluate("myObject.writeOnlyProperty").isUndefined(), true);
    QCOMPARE(m_engine->evaluate("myObject.writeOnlyProperty = 123;"
                                "myObject.writeOnlyProperty").isUndefined(), true);
    QCOMPARE(m_myObject->writeOnlyProperty(), 123);
}

void tst_QScriptExtQObject::getSetStaticProperty_readOnly()
{
    // read-only property
    QCOMPARE(m_myObject->readOnlyProperty(), 987);
    QCOMPARE(m_engine->evaluate("myObject.readOnlyProperty").toInt32(), 987);
    QCOMPARE(m_engine->evaluate("myObject.readOnlyProperty = 654;"
                                "myObject.readOnlyProperty").toInt32(), 987);
    QCOMPARE(m_myObject->readOnlyProperty(), 987);
    {
        QScriptValue mobj = m_engine->globalObject().property("myObject");
        QCOMPARE(mobj.propertyFlags("readOnlyProperty") & QScriptValue::ReadOnly,
                 QScriptValue::ReadOnly);
    }
}

void tst_QScriptExtQObject::getSetStaticProperty_enum()
{
    // enum property
    QCOMPARE(m_myObject->enumProperty(), MyQObject::BarPolicy);
    {
        QScriptValue val = m_engine->evaluate("myObject.enumProperty");
        QVERIFY(val.isNumber());
        QCOMPARE(val.toInt32(), int(MyQObject::BarPolicy));
    }
    m_engine->evaluate("myObject.enumProperty = 2");
    QCOMPARE(m_myObject->enumProperty(), MyQObject::BazPolicy);
    m_engine->evaluate("myObject.enumProperty = 'BarPolicy'");
    QCOMPARE(m_myObject->enumProperty(), MyQObject::BarPolicy);
    m_engine->evaluate("myObject.enumProperty = 'ScoobyDoo'");
    QCOMPARE(m_myObject->enumProperty(), MyQObject::BarPolicy);
    // enum property with custom conversion
    qScriptRegisterMetaType<MyQObject::Policy>(m_engine, policyToScriptValue, policyFromScriptValue);
    m_engine->evaluate("myObject.enumProperty = 'red'");
    QCOMPARE(m_myObject->enumProperty(), MyQObject::FooPolicy);
    m_engine->evaluate("myObject.enumProperty = 'green'");
    QCOMPARE(m_myObject->enumProperty(), MyQObject::BarPolicy);
    m_engine->evaluate("myObject.enumProperty = 'blue'");
    QCOMPARE(m_myObject->enumProperty(), MyQObject::BazPolicy);
    m_engine->evaluate("myObject.enumProperty = 'nada'");
    QCOMPARE(m_myObject->enumProperty(), (MyQObject::Policy)-1);
}

void tst_QScriptExtQObject::getSetStaticProperty_qflags()
{
    // flags property
    QCOMPARE(m_myObject->flagsProperty(), MyQObject::FooAbility);
    {
        QScriptValue val = m_engine->evaluate("myObject.flagsProperty");
        QVERIFY(val.isNumber());
        QCOMPARE(val.toInt32(), int(MyQObject::FooAbility));
    }
    m_engine->evaluate("myObject.flagsProperty = 0x80");
    QCOMPARE(m_myObject->flagsProperty(), MyQObject::BarAbility);
    m_engine->evaluate("myObject.flagsProperty = 0x81");
    QCOMPARE(m_myObject->flagsProperty(), MyQObject::Ability(MyQObject::FooAbility | MyQObject::BarAbility));
    m_engine->evaluate("myObject.flagsProperty = 123"); // bogus values are accepted
    QCOMPARE(int(m_myObject->flagsProperty()), 123);
    m_engine->evaluate("myObject.flagsProperty = 'BazAbility'");
    QCOMPARE(m_myObject->flagsProperty(),  MyQObject::BazAbility);
    m_engine->evaluate("myObject.flagsProperty = 'ScoobyDoo'");
    QCOMPARE(m_myObject->flagsProperty(),  MyQObject::BazAbility);
}

void tst_QScriptExtQObject::getSetStaticProperty_pointerDeref()
{
    // auto-dereferencing of pointers
    {
        QBrush b = QColor(0xCA, 0xFE, 0xBA, 0xBE);
        QBrush *bp = &b;
        QScriptValue bpValue = m_engine->newVariant(qVariantFromValue(bp));
        m_engine->globalObject().setProperty("brushPointer", bpValue);
        {
            QScriptValue ret = m_engine->evaluate("myObject.setBrushProperty(brushPointer)");
            QCOMPARE(ret.isUndefined(), true);
            QCOMPARE(qscriptvalue_cast<QBrush>(m_engine->evaluate("myObject.brushProperty")), b);
        }
        {
            b = QColor(0xDE, 0xAD, 0xBE, 0xEF);
            QScriptValue ret = m_engine->evaluate("myObject.brushProperty = brushPointer");
            QCOMPARE(ret.strictlyEquals(bpValue), true);
            QCOMPARE(qscriptvalue_cast<QBrush>(m_engine->evaluate("myObject.brushProperty")), b);
        }
        m_engine->globalObject().setProperty("brushPointer", QScriptValue());
    }
}

void tst_QScriptExtQObject::getSetStaticProperty_customGetterSetter()
{
    // install custom property getter+setter
    {
        QScriptValue mobj = m_engine->globalObject().property("myObject");
        mobj.setProperty("intProperty", m_engine->newFunction(getSetProperty),
                         QScriptValue::PropertyGetter | QScriptValue::PropertySetter);
        QVERIFY(mobj.property("intProperty").toInt32() != 321);
        mobj.setProperty("intProperty", 321);
        QCOMPARE(mobj.property("intProperty").toInt32(), 321);
    }
}

void tst_QScriptExtQObject::getSetStaticProperty_methodPersistence()
{
    // method properties are persistent
    {
        QScriptValue slot = m_engine->evaluate("myObject.mySlot");
        QVERIFY(slot.isFunction());
        QScriptValue sameSlot = m_engine->evaluate("myObject.mySlot");
        QVERIFY(sameSlot.strictlyEquals(slot));
        sameSlot = m_engine->evaluate("myObject['mySlot()']");
        QVERIFY(sameSlot.isFunction());
        QEXPECT_FAIL("", "QTBUG-17611: Signature-based method lookup creates new function wrapper object", Continue);
        QVERIFY(sameSlot.strictlyEquals(slot));
    }
}

void tst_QScriptExtQObject::getSetDynamicProperty()
{
    // initially the object does not have the property
    QCOMPARE(m_engine->evaluate("myObject.hasOwnProperty('dynamicProperty')")
             .strictlyEquals(QScriptValue(m_engine, false)), true);

    // add a dynamic property in C++
    QCOMPARE(m_myObject->setProperty("dynamicProperty", 123), false);
    QCOMPARE(m_engine->evaluate("myObject.hasOwnProperty('dynamicProperty')")
             .strictlyEquals(QScriptValue(m_engine, true)), true);
    QCOMPARE(m_engine->evaluate("myObject.dynamicProperty")
             .strictlyEquals(QScriptValue(m_engine, 123)), true);

    // check the flags
    {
        QScriptValue mobj = m_engine->globalObject().property("myObject");
        QVERIFY(!(mobj.propertyFlags("dynamicProperty") & QScriptValue::ReadOnly));
        QVERIFY(!(mobj.propertyFlags("dynamicProperty") & QScriptValue::Undeletable));
        QVERIFY(!(mobj.propertyFlags("dynamicProperty") & QScriptValue::SkipInEnumeration));
        QVERIFY(mobj.propertyFlags("dynamicProperty") & QScriptValue::QObjectMember);
    }

    // property change in script should be reflected in C++
    QCOMPARE(m_engine->evaluate("myObject.dynamicProperty = 'foo';"
                                "myObject.dynamicProperty")
             .strictlyEquals(QScriptValue(m_engine, QLatin1String("foo"))), true);
    QCOMPARE(m_myObject->property("dynamicProperty").toString(), QLatin1String("foo"));

    // delete the property
    QCOMPARE(m_engine->evaluate("delete myObject.dynamicProperty").toBoolean(), true);
    QCOMPARE(m_myObject->property("dynamicProperty").isValid(), false);
    QCOMPARE(m_engine->evaluate("myObject.dynamicProperty").isUndefined(), true);
    QCOMPARE(m_engine->evaluate("myObject.hasOwnProperty('dynamicProperty')").toBoolean(), false);
}

void tst_QScriptExtQObject::getSetDynamicProperty_deleteFromCpp()
{
    QScriptValue val = m_engine->newQObject(m_myObject);

    m_myObject->setProperty("dynamicFromCpp", 1234);
    QVERIFY(val.property("dynamicFromCpp").strictlyEquals(QScriptValue(m_engine, 1234)));

    m_myObject->setProperty("dynamicFromCpp", 4321);
    QVERIFY(val.property("dynamicFromCpp").strictlyEquals(QScriptValue(m_engine, 4321)));
    QCOMPARE(m_myObject->property("dynamicFromCpp").toInt(), 4321);

    // In this case we delete the property from C++
    m_myObject->setProperty("dynamicFromCpp", QVariant());
    QVERIFY(!m_myObject->property("dynamicFromCpp").isValid());
    QVERIFY(!val.property("dynamicFromCpp").isValid());
}

void tst_QScriptExtQObject::getSetDynamicProperty_hideChildObject()
{
    QScriptValue val = m_engine->newQObject(m_myObject);

    // Add our named child and access it
    QObject *child = new QObject(m_myObject);
    child->setObjectName("testName");
    QCOMPARE(val.property("testName").toQObject(), child);

    // Dynamic properties have precedence, hiding the child object
    m_myObject->setProperty("testName", 42);
    QVERIFY(val.property("testName").strictlyEquals(QScriptValue(m_engine, 42)));

    // Remove dynamic property
    m_myObject->setProperty("testName", QVariant());
    QCOMPARE(val.property("testName").toQObject(), child);
}

void tst_QScriptExtQObject::getSetDynamicProperty_setBeforeGet()
{
    QScriptValue val = m_engine->newQObject(m_myObject);

    m_myObject->setProperty("dynamic", 1111);
    val.setProperty("dynamic", 42);

    QVERIFY(val.property("dynamic").strictlyEquals(QScriptValue(m_engine, 42)));
    QCOMPARE(m_myObject->property("dynamic").toInt(), 42);
}

void tst_QScriptExtQObject::getSetDynamicProperty_doNotHideJSProperty()
{
    QScriptValue val = m_engine->newQObject(m_myObject);

    // Set property on JS and dynamically on our QObject
    val.setProperty("x", 42);
    m_myObject->setProperty("x", 2222);

    QEXPECT_FAIL("", "QTBUG-17612: Dynamic C++ property overrides JS property", Continue);

    // JS should see the original JS value
    QVERIFY(val.property("x").strictlyEquals(QScriptValue(m_engine, 42)));

    // The dynamic property is intact
    QCOMPARE(m_myObject->property("x").toInt(), 2222);
}

void tst_QScriptExtQObject::getSetChildren()
{
    QScriptValue mobj = m_engine->evaluate("myObject");

    // initially the object does not have the child
    QCOMPARE(m_engine->evaluate("myObject.hasOwnProperty('child')")
             .strictlyEquals(QScriptValue(m_engine, false)), true);

    // add a child
    MyQObject *child = new MyQObject(m_myObject);
    child->setObjectName("child");
    QCOMPARE(m_engine->evaluate("myObject.hasOwnProperty('child')")
             .strictlyEquals(QScriptValue(m_engine, true)), true);

    QVERIFY(mobj.propertyFlags("child") & QScriptValue::ReadOnly);
    QVERIFY(mobj.propertyFlags("child") & QScriptValue::Undeletable);
    QVERIFY(mobj.propertyFlags("child") & QScriptValue::SkipInEnumeration);
    QVERIFY(!(mobj.propertyFlags("child") & QScriptValue::QObjectMember));

    {
        QScriptValue scriptChild = m_engine->evaluate("myObject.child");
        QVERIFY(scriptChild.isQObject());
        QCOMPARE(scriptChild.toQObject(), (QObject*)child);
        QScriptValue sameChild = m_engine->evaluate("myObject.child");
        QVERIFY(sameChild.strictlyEquals(scriptChild));
    }

    // add a grandchild
    MyQObject *grandChild = new MyQObject(child);
    grandChild->setObjectName("grandChild");
    QCOMPARE(m_engine->evaluate("myObject.child.hasOwnProperty('grandChild')")
             .strictlyEquals(QScriptValue(m_engine, true)), true);

    // delete grandchild
    delete grandChild;
    QCOMPARE(m_engine->evaluate("myObject.child.hasOwnProperty('grandChild')")
             .strictlyEquals(QScriptValue(m_engine, false)), true);

    // delete child
    delete child;
    QCOMPARE(m_engine->evaluate("myObject.hasOwnProperty('child')")
             .strictlyEquals(QScriptValue(m_engine, false)), true);

}

Q_DECLARE_METATYPE(QVector<int>)
Q_DECLARE_METATYPE(QVector<double>)
Q_DECLARE_METATYPE(QVector<QString>)

template <class T>
static QScriptValue qobjectToScriptValue(QScriptEngine *engine, T* const &in)
{ return engine->newQObject(in); }

template <class T>
static void qobjectFromScriptValue(const QScriptValue &object, T* &out)
{ out = qobject_cast<T*>(object.toQObject()); }

void tst_QScriptExtQObject::callQtInvokable()
{
    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.myInvokable()").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 0);
    QCOMPARE(m_myObject->qtFunctionActuals(), QVariantList());

    // extra arguments should silently be ignored
    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.myInvokable(10, 20, 30)").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 0);
    QCOMPARE(m_myObject->qtFunctionActuals(), QVariantList());

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.myInvokableWithIntArg(123)").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), 123);

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.myInvokableWithIntArg('123')").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), 123);

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.myInvokableWithLonglongArg(123)").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 2);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toLongLong(), qlonglong(123));

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.myInvokableWithFloatArg(123.5)").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 3);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toDouble(), 123.5);

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.myInvokableWithDoubleArg(123.5)").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 4);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toDouble(), 123.5);

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.myInvokableWithStringArg('ciao')").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 5);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toString(), QLatin1String("ciao"));

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.myInvokableWithStringArg(123)").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 5);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toString(), QLatin1String("123"));

    m_myObject->resetQtFunctionInvoked();
    QVERIFY(m_engine->evaluate("myObject.myInvokableWithStringArg(null)").isUndefined());
    QCOMPARE(m_myObject->qtFunctionInvoked(), 5);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).type(), QVariant::String);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toString(), QString());

    m_myObject->resetQtFunctionInvoked();
    QVERIFY(m_engine->evaluate("myObject.myInvokableWithStringArg(undefined)").isUndefined());
    QCOMPARE(m_myObject->qtFunctionInvoked(), 5);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).type(), QVariant::String);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toString(), QString());

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.myInvokableWithIntArgs(123, 456)").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 6);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 2);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), 123);
    QCOMPARE(m_myObject->qtFunctionActuals().at(1).toInt(), 456);

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.myInvokableReturningInt()")
             .strictlyEquals(QScriptValue(m_engine, 123)), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 7);
    QCOMPARE(m_myObject->qtFunctionActuals(), QVariantList());

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.myInvokableReturningLongLong()")
             .strictlyEquals(QScriptValue(m_engine, 456)), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 39);
    QCOMPARE(m_myObject->qtFunctionActuals(), QVariantList());

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.myInvokableReturningString()")
             .strictlyEquals(QScriptValue(m_engine, QLatin1String("ciao"))), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 8);
    QCOMPARE(m_myObject->qtFunctionActuals(), QVariantList());

    m_myObject->resetQtFunctionInvoked();
    QVERIFY(m_engine->evaluate("myObject.myInvokableReturningVariant()")
             .strictlyEquals(QScriptValue(m_engine, 123)));
    QCOMPARE(m_myObject->qtFunctionInvoked(), 60);

    m_myObject->resetQtFunctionInvoked();
    QVERIFY(m_engine->evaluate("myObject.myInvokableReturningScriptValue()")
             .strictlyEquals(QScriptValue(m_engine, 456)));
    QCOMPARE(m_myObject->qtFunctionInvoked(), 61);

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.myInvokableWithIntArg(123, 456)").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 9);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 2);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), 123);
    QCOMPARE(m_myObject->qtFunctionActuals().at(1).toInt(), 456);
}

void tst_QScriptExtQObject::callQtInvokable2()
{
    m_myObject->resetQtFunctionInvoked();
    QVERIFY(m_engine->evaluate("myObject.myInvokableWithVoidStarArg(null)").isUndefined());
    QCOMPARE(m_myObject->qtFunctionInvoked(), 44);
    m_myObject->resetQtFunctionInvoked();
    QVERIFY(m_engine->evaluate("myObject.myInvokableWithVoidStarArg(123)").isError());
    QCOMPARE(m_myObject->qtFunctionInvoked(), -1);

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithAmbiguousArg(123)");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("TypeError: ambiguous call of overloaded function myInvokableWithAmbiguousArg(); candidates were\n    myInvokableWithAmbiguousArg(int)\n    myInvokableWithAmbiguousArg(uint)"));
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithDefaultArgs(123, 'hello')");
        QVERIFY(ret.isUndefined());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 47);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 2);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), 123);
        QCOMPARE(m_myObject->qtFunctionActuals().at(1).toString(), QLatin1String("hello"));
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithDefaultArgs(456)");
        QVERIFY(ret.isUndefined());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 47);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 2);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), 456);
        QCOMPARE(m_myObject->qtFunctionActuals().at(1).toString(), QString());
    }

    {
        QScriptValue fun = m_engine->evaluate("myObject.myInvokableWithPointArg");
        QVERIFY(fun.isFunction());
        m_myObject->resetQtFunctionInvoked();
        {
            QScriptValue ret = fun.call(m_engine->evaluate("myObject"),
                                        QScriptValueList() << qScriptValueFromValue(m_engine, QPoint(10, 20)));
            QVERIFY(ret.isUndefined());
            QCOMPARE(m_myObject->qtFunctionInvoked(), 50);
            QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
            QCOMPARE(m_myObject->qtFunctionActuals().at(0).toPoint(), QPoint(10, 20));
        }
        m_myObject->resetQtFunctionInvoked();
        {
            QScriptValue ret = fun.call(m_engine->evaluate("myObject"),
                                        QScriptValueList() << qScriptValueFromValue(m_engine, QPointF(30, 40)));
            QVERIFY(ret.isUndefined());
            QCOMPARE(m_myObject->qtFunctionInvoked(), 51);
            QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
            QCOMPARE(m_myObject->qtFunctionActuals().at(0).toPointF(), QPointF(30, 40));
        }
    }

    // calling function that returns (const)ref
    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableReturningRef()");
        QVERIFY(ret.isUndefined());
        QVERIFY(!m_engine->hasUncaughtException());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 48);
    }
    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableReturningConstRef()");
        QVERIFY(ret.isUndefined());
        QVERIFY(!m_engine->hasUncaughtException());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 49);
    }

    // first time we expect failure because the metatype is not registered
    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.myInvokableReturningVectorOfInt()").isError(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), -1);

    QCOMPARE(m_engine->evaluate("myObject.myInvokableWithVectorOfIntArg(0)").isError(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), -1);

    // now we register it, and it should work
    qScriptRegisterSequenceMetaType<QVector<int> >(m_engine);
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableReturningVectorOfInt()");
        QCOMPARE(ret.isArray(), true);
        QCOMPARE(m_myObject->qtFunctionInvoked(), 11);
    }
}

void tst_QScriptExtQObject::callQtInvokable3()
{
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithVectorOfIntArg(myObject.myInvokableReturningVectorOfInt())");
        QCOMPARE(ret.isUndefined(), true);
        QCOMPARE(m_myObject->qtFunctionInvoked(), 12);
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableReturningQObjectStar()");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 13);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 0);
        QCOMPARE(ret.isQObject(), true);
        QCOMPARE(ret.toQObject(), (QObject *)m_myObject);
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithQObjectListArg([myObject])");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 14);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(ret.isArray(), true);
        QCOMPARE(ret.property(QLatin1String("length"))
                 .strictlyEquals(QScriptValue(m_engine, 1)), true);
        QCOMPARE(ret.property(QLatin1String("0")).isQObject(), true);
        QCOMPARE(ret.property(QLatin1String("0")).toQObject(), (QObject *)m_myObject);
    }

    m_myObject->resetQtFunctionInvoked();
    {
        m_myObject->setVariantProperty(QVariant(123));
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithVariantArg(myObject.variantProperty)");
        QVERIFY(ret.isNumber());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 15);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0), m_myObject->variantProperty());
        QVERIFY(ret.strictlyEquals(QScriptValue(m_engine, 123)));
    }

    m_myObject->resetQtFunctionInvoked();
    {
        m_myObject->setVariantProperty(qVariantFromValue(QBrush()));
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithVariantArg(myObject.variantProperty)");
        QVERIFY(ret.isVariant());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 15);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(ret.toVariant(), m_myObject->qtFunctionActuals().at(0));
        QCOMPARE(ret.toVariant(), m_myObject->variantProperty());
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithVariantArg(123)");
        QVERIFY(ret.isNumber());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 15);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0), QVariant(123));
        QVERIFY(ret.strictlyEquals(QScriptValue(m_engine, 123)));
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithVariantArg('ciao')");
        QVERIFY(ret.isString());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 15);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0), QVariant(QString::fromLatin1("ciao")));
        QVERIFY(ret.strictlyEquals(QScriptValue(m_engine, QString::fromLatin1("ciao"))));
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithVariantArg(null)");
        QVERIFY(ret.isUndefined()); // invalid QVariant is converted to Undefined
        QCOMPARE(m_myObject->qtFunctionInvoked(), 15);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0), QVariant());
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithVariantArg(undefined)");
        QVERIFY(ret.isUndefined());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 15);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0), QVariant());
    }

    m_engine->globalObject().setProperty("fishy", m_engine->newVariant(123));
    m_engine->evaluate("myObject.myInvokableWithStringArg(fishy)");

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithVariantMapArg({ a:123, b:'ciao' })");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 16);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QVariant v = m_myObject->qtFunctionActuals().at(0);
        QCOMPARE(v.userType(), int(QMetaType::QVariantMap));
        QVariantMap vmap = qvariant_cast<QVariantMap>(v);
        QCOMPARE(vmap.keys().size(), 2);
        QCOMPARE(vmap.keys().at(0), QLatin1String("a"));
        QCOMPARE(vmap.value("a"), QVariant(123));
        QCOMPARE(vmap.keys().at(1), QLatin1String("b"));
        QCOMPARE(vmap.value("b"), QVariant("ciao"));

        QCOMPARE(ret.isObject(), true);
        QCOMPARE(ret.property("a").strictlyEquals(QScriptValue(m_engine, 123)), true);
        QCOMPARE(ret.property("b").strictlyEquals(QScriptValue(m_engine, "ciao")), true);
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithListOfIntArg([1, 5])");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 17);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QVariant v = m_myObject->qtFunctionActuals().at(0);
        QCOMPARE(v.userType(), qMetaTypeId<QList<int> >());
        QList<int> ilst = qvariant_cast<QList<int> >(v);
        QCOMPARE(ilst.size(), 2);
        QCOMPARE(ilst.at(0), 1);
        QCOMPARE(ilst.at(1), 5);

        QCOMPARE(ret.isArray(), true);
        QCOMPARE(ret.property("0").strictlyEquals(QScriptValue(m_engine, 1)), true);
        QCOMPARE(ret.property("1").strictlyEquals(QScriptValue(m_engine, 5)), true);
    }
}

void tst_QScriptExtQObject::callQtInvokable4()
{
    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithQObjectStarArg(myObject)");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 18);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QVariant v = m_myObject->qtFunctionActuals().at(0);
        QCOMPARE(v.userType(), int(QMetaType::QObjectStar));
        QCOMPARE(qvariant_cast<QObject*>(v), (QObject *)m_myObject);

        QCOMPARE(ret.isQObject(), true);
        QCOMPARE(qscriptvalue_cast<QObject*>(ret), (QObject *)m_myObject);
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithQWidgetStarArg(null)");
        QVERIFY(ret.isNull());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 63);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QVariant v = m_myObject->qtFunctionActuals().at(0);
        QCOMPARE(qvariant_cast<QWidget*>(v), (QObject *)0);
    }

    m_myObject->resetQtFunctionInvoked();
    {
        // no implicit conversion from integer to QObject*
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithQObjectStarArg(123)");
        QCOMPARE(ret.isError(), true);
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithShortArg(123)");
        QVERIFY(ret.isNumber());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 64);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QVariant v = m_myObject->qtFunctionActuals().at(0);
        QCOMPARE(v.userType(), int(QMetaType::Short));
        QCOMPARE(qvariant_cast<short>(v), short(123));
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithUShortArg(123)");
        QVERIFY(ret.isNumber());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 65);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QVariant v = m_myObject->qtFunctionActuals().at(0);
        QCOMPARE(v.userType(), int(QMetaType::UShort));
        QCOMPARE(qvariant_cast<ushort>(v), ushort(123));
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithCharArg(123)");
        QVERIFY(ret.isNumber());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 66);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QVariant v = m_myObject->qtFunctionActuals().at(0);
        QCOMPARE(v.userType(), int(QMetaType::Char));
        QCOMPARE(qvariant_cast<char>(v), char(123));
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithUCharArg(123)");
        QVERIFY(ret.isNumber());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 67);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QVariant v = m_myObject->qtFunctionActuals().at(0);
        QCOMPARE(v.userType(), int(QMetaType::UChar));
        QCOMPARE(qvariant_cast<uchar>(v), uchar(123));
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithULonglongArg(123)");
        QVERIFY(ret.isNumber());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 68);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QVariant v = m_myObject->qtFunctionActuals().at(0);
        QCOMPARE(v.userType(), int(QMetaType::ULongLong));
        QCOMPARE(qvariant_cast<qulonglong>(v), qulonglong(123));
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithLongArg(123)");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 69);
        QVERIFY(ret.isNumber());
        QCOMPARE(long(ret.toInteger()), long(123));

        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QVariant v = m_myObject->qtFunctionActuals().at(0);
        QCOMPARE(v.userType(), int(QMetaType::Long));
        QCOMPARE(qvariant_cast<long>(v), long(123));
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithULongArg(456)");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 70);
        QVERIFY(ret.isNumber());
        QCOMPARE(ulong(ret.toInteger()), ulong(456));

        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QVariant v = m_myObject->qtFunctionActuals().at(0);
        QCOMPARE(v.userType(), int(QMetaType::ULong));
        QCOMPARE(qvariant_cast<unsigned long>(v), ulong(456));
    }
}

void tst_QScriptExtQObject::callQtInvokable5()
{
    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue fun = m_engine->evaluate("myObject.myInvokableWithQBrushArg");
        QVERIFY(fun.isFunction());
        QColor color(10, 20, 30, 40);
        // QColor should be converted to a QBrush
        QScriptValue ret = fun.call(QScriptValue(), QScriptValueList()
                                    << qScriptValueFromValue(m_engine, color));
        QCOMPARE(m_myObject->qtFunctionInvoked(), 19);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QVariant v = m_myObject->qtFunctionActuals().at(0);
        QCOMPARE(v.userType(), int(QMetaType::QBrush));
        QCOMPARE(qvariant_cast<QColor>(v), color);

        QCOMPARE(qscriptvalue_cast<QColor>(ret), color);
    }

    // private slots should not be part of the QObject binding
    QCOMPARE(m_engine->evaluate("myObject.myPrivateSlot").isUndefined(), true);

    // protected slots should be fine
    m_myObject->resetQtFunctionInvoked();
    m_engine->evaluate("myObject.myProtectedSlot()");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 36);

    // call with too few arguments
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithIntArg()");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("SyntaxError: too few arguments in call to myInvokableWithIntArg(); candidates are\n    myInvokableWithIntArg(int,int)\n    myInvokableWithIntArg(int)"));
    }

    // call function where not all types have been registered
    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithBrushStyleArg(0)");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("TypeError: cannot call myInvokableWithBrushStyleArg(): argument 1 has unknown type `Qt::BrushStyle' (register the type with qScriptRegisterMetaType())"));
        QCOMPARE(m_myObject->qtFunctionInvoked(), -1);
    }

    // call function with incompatible argument type
    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithQBrushArg(null)");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("TypeError: incompatible type of argument(s) in call to myInvokableWithQBrushArg(); candidates were\n    myInvokableWithQBrushArg(QBrush)"));
        QCOMPARE(m_myObject->qtFunctionInvoked(), -1);
    }

    // ability to call a slot with QObject-based arguments, even if those types haven't been registered
    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithMyQObjectArg(myObject)");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 52);
        QVERIFY(ret.isUndefined());
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(qvariant_cast<QObject*>(m_myObject->qtFunctionActuals().at(0)), (QObject*)m_myObject);
    }

    // inability to call a slot returning QObject-based type, when that type hasn't been registered
    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableReturningMyQObject()");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QString::fromLatin1("TypeError: cannot call myInvokableReturningMyQObject(): unknown return type `MyQObject*' (register the type with qScriptRegisterMetaType())"));
    }

    // ability to call a slot returning QObject-based type when that type has been registered
    qRegisterMetaType<MyQObject*>("MyQObject*");
    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableReturningMyQObject()");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 53);
        QVERIFY(ret.isQObject());
        QCOMPARE(*reinterpret_cast<MyQObject* const *>(ret.toVariant().constData()), m_myObject);
    }

    // ability to call a slot with QObject-based argument, when the argument is const
    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithConstMyQObjectArg(myObject)");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 54);
        QVERIFY(ret.isUndefined());
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(qvariant_cast<QObject*>(m_myObject->qtFunctionActuals().at(0)), (QObject*)m_myObject);
    }
}

void tst_QScriptExtQObject::callQtInvokable6()
{
    // QScriptValue arguments should be passed on without conversion
    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithScriptValueArg(123)");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 56);
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 123);
    }
    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithScriptValueArg('ciao')");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 56);
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("ciao"));
    }
    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithScriptValueArg(this)");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 56);
        QVERIFY(ret.isObject());
        QVERIFY(ret.strictlyEquals(m_engine->globalObject()));
    }

    // the prototype specified by a conversion function should not be "down-graded"
    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue qobjectProto = m_engine->newObject();
        qScriptRegisterMetaType<QObject*>(m_engine, qobjectToScriptValue,
                                          qobjectFromScriptValue, qobjectProto);
        QScriptValue myQObjectProto = m_engine->newObject();
        myQObjectProto.setPrototype(qobjectProto);
        qScriptRegisterMetaType<MyQObject*>(m_engine, qobjectToScriptValue,
                                          qobjectFromScriptValue, myQObjectProto);
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableReturningMyQObjectAsQObject()");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 57);
        QVERIFY(ret.isQObject());
        QVERIFY(ret.prototype().strictlyEquals(myQObjectProto));

        qScriptRegisterMetaType<QObject*>(m_engine, 0, 0, QScriptValue());
        qScriptRegisterMetaType<MyQObject*>(m_engine, 0, 0, QScriptValue());
    }

    // detect exceptions during argument conversion
    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue (*dummy)(QScriptEngine *, const QDir &) = 0;
        qScriptRegisterMetaType<QDir>(m_engine, dummy, dirFromScript);
        {
            QVERIFY(!m_engine->hasUncaughtException());
            QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithQDirArg({})");
            QVERIFY(m_engine->hasUncaughtException());
            QVERIFY(ret.isError());
            QCOMPARE(ret.toString(), QString::fromLatin1("Error: No path"));
            QCOMPARE(m_myObject->qtFunctionInvoked(), -1);
        }
        m_engine->clearExceptions();
        {
            QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithQDirArg({path:'.'})");
            QVERIFY(!m_engine->hasUncaughtException());
            QVERIFY(ret.isUndefined());
            QCOMPARE(m_myObject->qtFunctionInvoked(), 55);
        }
    }
}

void tst_QScriptExtQObject::callQtInvokable7()
{
    // qscript_call()
    {
        m_myObject->resetQtFunctionInvoked();
        QScriptValue ret = m_engine->evaluate("new myObject(123)");
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().contains(QString::fromLatin1("TypeError")));
    }
    {
        m_myObject->resetQtFunctionInvoked();
        QScriptValue ret = m_engine->evaluate("myObject(123)");
        QVERIFY(ret.toString().contains(QString::fromLatin1("TypeError")));
    }

    // task 233624
    {
        MyNS::A a;
        m_engine->globalObject().setProperty("anObject", m_engine->newQObject(&a));
        QScriptValue ret = m_engine->evaluate("anObject.slotTakingScopedEnumArg(1)");
        QVERIFY(!ret.isError());
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 1);
        m_engine->globalObject().setProperty("anObject", QScriptValue());
    }

    // virtual slot redeclared in subclass (task 236467)
    {
        MyOtherQObject moq;
        m_engine->globalObject().setProperty("myOtherQObject", m_engine->newQObject(&moq));
        moq.resetQtFunctionInvoked();
        QScriptValue ret = m_engine->evaluate("myOtherQObject.myVirtualSlot(123)");
        QCOMPARE(moq.qtFunctionInvoked(), 59);
        QVERIFY(!ret.isError());
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 123);
    }
}

void tst_QScriptExtQObject::connectAndDisconnect()
{
    // connect(function)
    QCOMPARE(m_engine->evaluate("myObject.mySignal.connect(123)").isError(), true);

    m_engine->evaluate("myHandler = function() { global.gotSignal = true; global.signalArgs = arguments; global.slotThisObject = this; }");

    m_myObject->clearConnectNotifySignals();
    QVERIFY(m_engine->evaluate("myObject.mySignal.connect(myHandler)").isUndefined());
    QVERIFY(m_myObject->hasSingleConnectNotifySignal(QMetaMethod::fromSignal(&MyQObject::mySignal)));

    m_engine->evaluate("gotSignal = false");
    m_engine->evaluate("myObject.mySignal()");
    QCOMPARE(m_engine->evaluate("gotSignal").toBoolean(), true);
    QCOMPARE(m_engine->evaluate("signalArgs.length").toNumber(), 0.0);
    QVERIFY(m_engine->evaluate("slotThisObject").strictlyEquals(m_engine->globalObject()));

    m_engine->evaluate("gotSignal = false");
    m_myObject->emitMySignal();
    QCOMPARE(m_engine->evaluate("gotSignal").toBoolean(), true);
    QCOMPARE(m_engine->evaluate("signalArgs.length").toNumber(), 0.0);

    QVERIFY(m_engine->evaluate("myObject.mySignalWithIntArg.connect(myHandler)").isUndefined());

    m_engine->evaluate("gotSignal = false");
    m_myObject->emitMySignalWithIntArg(123);
    QCOMPARE(m_engine->evaluate("gotSignal").toBoolean(), true);
    QCOMPARE(m_engine->evaluate("signalArgs.length").toNumber(), 1.0);
    QCOMPARE(m_engine->evaluate("signalArgs[0]").toNumber(), 123.0);

    m_myObject->clearDisconnectNotifySignals();
    QVERIFY(m_engine->evaluate("myObject.mySignal.disconnect(myHandler)").isUndefined());
    QVERIFY(m_myObject->hasSingleDisconnectNotifySignal(QMetaMethod::fromSignal(&MyQObject::mySignal)));

    QVERIFY(m_engine->evaluate("myObject.mySignal.disconnect(myHandler)").isError());

    QVERIFY(m_engine->evaluate("myObject.mySignal2.connect(myHandler)").isUndefined());
    QVERIFY(m_engine->evaluate("myObject.mySignalWithIntArg.disconnect(myHandler)").isUndefined());

    m_engine->evaluate("gotSignal = false");
    m_myObject->emitMySignal2(false);
    QCOMPARE(m_engine->evaluate("gotSignal").toBoolean(), true);
    QCOMPARE(m_engine->evaluate("signalArgs.length").toNumber(), 1.0);
    QCOMPARE(m_engine->evaluate("signalArgs[0]").toBoolean(), false);

    m_engine->evaluate("gotSignal = false");
    QVERIFY(m_engine->evaluate("myObject.mySignal2.connect(myHandler)").isUndefined());
    m_myObject->emitMySignal2(true);
    QCOMPARE(m_engine->evaluate("gotSignal").toBoolean(), true);
    QCOMPARE(m_engine->evaluate("signalArgs.length").toNumber(), 1.0);
    QCOMPARE(m_engine->evaluate("signalArgs[0]").toBoolean(), true);

    QVERIFY(m_engine->evaluate("myObject.mySignal2.disconnect(myHandler)").isUndefined());

    QVERIFY(m_engine->evaluate("myObject['mySignal2()'].connect(myHandler)").isUndefined());

    m_engine->evaluate("gotSignal = false");
    m_myObject->emitMySignal2();
    QCOMPARE(m_engine->evaluate("gotSignal").toBoolean(), true);

    QVERIFY(m_engine->evaluate("myObject['mySignal2()'].disconnect(myHandler)").isUndefined());

    // connecting to signal with default args should pick the most generic version (i.e. with all args)
    m_myObject->clearConnectNotifySignals();
    QVERIFY(m_engine->evaluate("myObject.mySignalWithDefaultArg.connect(myHandler)").isUndefined());
    QVERIFY(m_myObject->hasSingleConnectNotifySignal(QMetaMethod::fromSignal(&MyQObject::mySignalWithDefaultArg)));
    m_engine->evaluate("gotSignal = false");
    m_myObject->emitMySignalWithDefaultArgWithArg(456);
    QVERIFY(m_engine->evaluate("gotSignal").toBoolean());
    QCOMPARE(m_engine->evaluate("signalArgs.length").toInt32(), 1);
    QCOMPARE(m_engine->evaluate("signalArgs[0]").toInt32(), 456);

    m_engine->evaluate("gotSignal = false");
    m_myObject->emitMySignalWithDefaultArg();
    QVERIFY(m_engine->evaluate("gotSignal").toBoolean());
    QCOMPARE(m_engine->evaluate("signalArgs.length").toInt32(), 1);
    QCOMPARE(m_engine->evaluate("signalArgs[0]").toInt32(), 123);

    QVERIFY(m_engine->evaluate("myObject.mySignalWithDefaultArg.disconnect(myHandler)").isUndefined());

    m_engine->evaluate("gotSignal = false");
    // connecting to overloaded signal should throw an error
    {
        QScriptValue ret = m_engine->evaluate("myObject.myOverloadedSignal.connect(myHandler)");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QString::fromLatin1("Error: Function.prototype.connect: ambiguous connect to MyQObject::myOverloadedSignal(); candidates are\n"
                                                     "    myOverloadedSignal(int)\n"
                                                     "    myOverloadedSignal(QString)\n"
                                                     "Use e.g. object['myOverloadedSignal(QString)'].connect() to connect to a particular overload"));
    }
    m_myObject->emitMyOverloadedSignal(123);
    QVERIFY(!m_engine->evaluate("gotSignal").toBoolean());
    m_myObject->emitMyOverloadedSignal("ciao");
    QVERIFY(!m_engine->evaluate("gotSignal").toBoolean());

    m_engine->evaluate("gotSignal = false");
    {
        QScriptValue ret = m_engine->evaluate("myObject.myOtherOverloadedSignal.connect(myHandler)");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QString::fromLatin1("Error: Function.prototype.connect: ambiguous connect to MyQObject::myOtherOverloadedSignal(); candidates are\n"
                                                     "    myOtherOverloadedSignal(QString)\n"
                                                     "    myOtherOverloadedSignal(int)\n"
                                                     "Use e.g. object['myOtherOverloadedSignal(int)'].connect() to connect to a particular overload"));
    }
    m_myObject->emitMyOtherOverloadedSignal("ciao");
    QVERIFY(!m_engine->evaluate("gotSignal").toBoolean());
    m_myObject->emitMyOtherOverloadedSignal(123);
    QVERIFY(!m_engine->evaluate("gotSignal").toBoolean());

    // signal with QVariant arg: argument conversion should work
    m_myObject->clearConnectNotifySignals();
    QVERIFY(m_engine->evaluate("myObject.mySignalWithVariantArg.connect(myHandler)").isUndefined());
    QVERIFY(m_myObject->hasSingleConnectNotifySignal(QMetaMethod::fromSignal(&MyQObject::mySignalWithVariantArg)));
    m_engine->evaluate("gotSignal = false");
    m_myObject->emitMySignalWithVariantArg(123);
    QCOMPARE(m_engine->evaluate("gotSignal").toBoolean(), true);
    QCOMPARE(m_engine->evaluate("signalArgs.length").toNumber(), 1.0);
    QVERIFY(m_engine->evaluate("signalArgs[0]").isNumber());
    QCOMPARE(m_engine->evaluate("signalArgs[0]").toNumber(), 123.0);
    QVERIFY(m_engine->evaluate("myObject.mySignalWithVariantArg.disconnect(myHandler)").isUndefined());

    // signal with argument type that's unknown to the meta-type system
    m_myObject->clearConnectNotifySignals();
    QVERIFY(m_engine->evaluate("myObject.mySignalWithScriptEngineArg.connect(myHandler)").isUndefined());
    QVERIFY(m_myObject->hasSingleConnectNotifySignal(QMetaMethod::fromSignal(&MyQObject::mySignalWithScriptEngineArg)));
    m_engine->evaluate("gotSignal = false");
    QTest::ignoreMessage(QtWarningMsg, "QScriptEngine: Unable to handle unregistered datatype 'QScriptEngine*' when invoking handler of signal MyQObject::mySignalWithScriptEngineArg(QScriptEngine*)");
    m_myObject->emitMySignalWithScriptEngineArg(m_engine);
    QCOMPARE(m_engine->evaluate("gotSignal").toBoolean(), true);
    QCOMPARE(m_engine->evaluate("signalArgs.length").toNumber(), 1.0);
    QVERIFY(m_engine->evaluate("signalArgs[0]").isUndefined());
    QVERIFY(m_engine->evaluate("myObject.mySignalWithScriptEngineArg.disconnect(myHandler)").isUndefined());

    // signal with QVariant arg: QVariant should be unwrapped only once
    m_myObject->clearConnectNotifySignals();
    QVERIFY(m_engine->evaluate("myObject.mySignalWithVariantArg.connect(myHandler)").isUndefined());
    QVERIFY(m_myObject->hasSingleConnectNotifySignal(QMetaMethod::fromSignal(&MyQObject::mySignalWithVariantArg)));
    m_engine->evaluate("gotSignal = false");
    QVariant tmp(123);
    QVariant signalArg(QMetaType::QVariant, &tmp);
    m_myObject->emitMySignalWithVariantArg(signalArg);
    QCOMPARE(m_engine->evaluate("gotSignal").toBoolean(), true);
    QCOMPARE(m_engine->evaluate("signalArgs.length").toNumber(), 1.0);
    QVERIFY(m_engine->evaluate("signalArgs[0]").isVariant());
    QCOMPARE(m_engine->evaluate("signalArgs[0]").toVariant().toDouble(), 123.0);
    QVERIFY(m_engine->evaluate("myObject.mySignalWithVariantArg.disconnect(myHandler)").isUndefined());

    // signal with QVariant arg: with an invalid QVariant
    m_myObject->clearConnectNotifySignals();
    QVERIFY(m_engine->evaluate("myObject.mySignalWithVariantArg.connect(myHandler)").isUndefined());
    QVERIFY(m_myObject->hasSingleConnectNotifySignal(QMetaMethod::fromSignal(&MyQObject::mySignalWithVariantArg)));
    m_engine->evaluate("gotSignal = false");
    m_myObject->emitMySignalWithVariantArg(QVariant());
    QCOMPARE(m_engine->evaluate("gotSignal").toBoolean(), true);
    QCOMPARE(m_engine->evaluate("signalArgs.length").toNumber(), 1.0);
    QVERIFY(m_engine->evaluate("signalArgs[0]").isUndefined());
    QVERIFY(m_engine->evaluate("myObject.mySignalWithVariantArg.disconnect(myHandler)").isUndefined());

    // signal with argument type that's unknown to the meta-type system
    m_myObject->clearConnectNotifySignals();
    QVERIFY(m_engine->evaluate("myObject.mySignalWithScriptEngineArg.connect(myHandler)").isUndefined());
    QVERIFY(m_myObject->hasSingleConnectNotifySignal(QMetaMethod::fromSignal(&MyQObject::mySignalWithScriptEngineArg)));
    m_engine->evaluate("gotSignal = false");
    QTest::ignoreMessage(QtWarningMsg, "QScriptEngine: Unable to handle unregistered datatype 'QScriptEngine*' when invoking handler of signal MyQObject::mySignalWithScriptEngineArg(QScriptEngine*)");
    m_myObject->emitMySignalWithScriptEngineArg(m_engine);
    QCOMPARE(m_engine->evaluate("gotSignal").toBoolean(), true);
    QCOMPARE(m_engine->evaluate("signalArgs.length").toNumber(), 1.0);
    QVERIFY(m_engine->evaluate("signalArgs[0]").isUndefined());
    QVERIFY(m_engine->evaluate("myObject.mySignalWithScriptEngineArg.disconnect(myHandler)").isUndefined());

    // connect(object, function)
    m_engine->evaluate("otherObject = { name:'foo' }");
    QVERIFY(m_engine->evaluate("myObject.mySignal.connect(otherObject, myHandler)").isUndefined());
    QVERIFY(m_engine->evaluate("myObject.mySignal.disconnect(otherObject, myHandler)").isUndefined());
    m_engine->evaluate("gotSignal = false");
    m_myObject->emitMySignal();
    QCOMPARE(m_engine->evaluate("gotSignal").toBoolean(), false);

    QVERIFY(m_engine->evaluate("myObject.mySignal.disconnect(otherObject, myHandler)").isError());

    QVERIFY(m_engine->evaluate("myObject.mySignal.connect(otherObject, myHandler)").isUndefined());
    m_engine->evaluate("gotSignal = false");
    m_myObject->emitMySignal();
    QCOMPARE(m_engine->evaluate("gotSignal").toBoolean(), true);
    QCOMPARE(m_engine->evaluate("signalArgs.length").toNumber(), 0.0);
    QVERIFY(m_engine->evaluate("slotThisObject").strictlyEquals(m_engine->evaluate("otherObject")));
    QCOMPARE(m_engine->evaluate("slotThisObject").property("name").toString(), QLatin1String("foo"));
    QVERIFY(m_engine->evaluate("myObject.mySignal.disconnect(otherObject, myHandler)").isUndefined());

    m_engine->evaluate("yetAnotherObject = { name:'bar', func : function() { } }");
    QVERIFY(m_engine->evaluate("myObject.mySignal2.connect(yetAnotherObject, myHandler)").isUndefined());
    m_engine->evaluate("gotSignal = false");
    m_myObject->emitMySignal2(true);
    QCOMPARE(m_engine->evaluate("gotSignal").toBoolean(), true);
    QCOMPARE(m_engine->evaluate("signalArgs.length").toNumber(), 1.0);
    QVERIFY(m_engine->evaluate("slotThisObject").strictlyEquals(m_engine->evaluate("yetAnotherObject")));
    QCOMPARE(m_engine->evaluate("slotThisObject").property("name").toString(), QLatin1String("bar"));
    QVERIFY(m_engine->evaluate("myObject.mySignal2.disconnect(yetAnotherObject, myHandler)").isUndefined());

    QVERIFY(m_engine->evaluate("myObject.mySignal2.connect(myObject, myHandler)").isUndefined());
    m_engine->evaluate("gotSignal = false");
    m_myObject->emitMySignal2(true);
    QCOMPARE(m_engine->evaluate("gotSignal").toBoolean(), true);
    QCOMPARE(m_engine->evaluate("signalArgs.length").toNumber(), 1.0);
    QCOMPARE(m_engine->evaluate("slotThisObject").toQObject(), (QObject *)m_myObject);
    QVERIFY(m_engine->evaluate("myObject.mySignal2.disconnect(myObject, myHandler)").isUndefined());

    // connect(obj, string)
    QVERIFY(m_engine->evaluate("myObject.mySignal.connect(yetAnotherObject, 'func')").isUndefined());
    QVERIFY(m_engine->evaluate("myObject.mySignal.connect(myObject, 'mySlot')").isUndefined());
    QVERIFY(m_engine->evaluate("myObject.mySignal.disconnect(yetAnotherObject, 'func')").isUndefined());
    QVERIFY(m_engine->evaluate("myObject.mySignal.disconnect(myObject, 'mySlot')").isUndefined());
}

void tst_QScriptExtQObject::connectAndDisconnect_emitFromJS()
{
    // no arguments
    QVERIFY(m_engine->evaluate("myObject.mySignal.connect(myObject.mySlot)").isUndefined());
    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.mySignal()").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 20);
    QVERIFY(m_engine->evaluate("myObject.mySignal.disconnect(myObject.mySlot)").isUndefined());

    // one argument
    QVERIFY(m_engine->evaluate("myObject.mySignalWithIntArg.connect(myObject.mySlotWithIntArg)").isUndefined());
    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.mySignalWithIntArg(123)").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 21);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), 123);
    QVERIFY(m_engine->evaluate("myObject.mySignalWithIntArg.disconnect(myObject.mySlotWithIntArg)").isUndefined());

    QVERIFY(m_engine->evaluate("myObject.mySignalWithIntArg.connect(myObject.mySlotWithDoubleArg)").isUndefined());
    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.mySignalWithIntArg(123)").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 22);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toDouble(), 123.0);
    QVERIFY(m_engine->evaluate("myObject.mySignalWithIntArg.disconnect(myObject.mySlotWithDoubleArg)").isUndefined());

    QVERIFY(m_engine->evaluate("myObject.mySignalWithIntArg.connect(myObject.mySlotWithStringArg)").isUndefined());
    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.mySignalWithIntArg(123)").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 23);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toString(), QLatin1String("123"));
    QVERIFY(m_engine->evaluate("myObject.mySignalWithIntArg.disconnect(myObject.mySlotWithStringArg)").isUndefined());

    // connecting to overloaded slot
    QVERIFY(m_engine->evaluate("myObject.mySignalWithIntArg.connect(myObject.myOverloadedSlot)").isUndefined());
    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.mySignalWithIntArg(123)").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 26); // double overload
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), 123);
    QVERIFY(m_engine->evaluate("myObject.mySignalWithIntArg.disconnect(myObject.myOverloadedSlot)").isUndefined());

    QVERIFY(m_engine->evaluate("myObject.mySignalWithIntArg.connect(myObject['myOverloadedSlot(int)'])").isUndefined());
    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.mySignalWithIntArg(456)").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 28); // int overload
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), 456);
    QVERIFY(m_engine->evaluate("myObject.mySignalWithIntArg.disconnect(myObject['myOverloadedSlot(int)'])").isUndefined());
}

void tst_QScriptExtQObject::connectAndDisconnect_senderWrapperCollected()
{
    // when the wrapper dies, the connection stays alive
    QVERIFY(m_engine->evaluate("myObject.mySignal.connect(myObject.mySlot)").isUndefined());
    m_myObject->resetQtFunctionInvoked();
    m_myObject->emitMySignal();
    QCOMPARE(m_myObject->qtFunctionInvoked(), 20);
    m_engine->evaluate("myObject = null");
    m_engine->collectGarbage();
    m_myObject->resetQtFunctionInvoked();
    m_myObject->emitMySignal();
    QCOMPARE(m_myObject->qtFunctionInvoked(), 20);
}

void tst_QScriptExtQObject::connectAndDisconnectWithBadArgs()
{
    {
        QScriptValue ret = m_engine->evaluate("(function() { }).connect()");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("Error: Function.prototype.connect: no arguments given"));
    }
    {
        QScriptValue ret = m_engine->evaluate("var o = { }; o.connect = Function.prototype.connect;  o.connect()");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("Error: Function.prototype.connect: no arguments given"));
    }

    {
        QScriptValue ret = m_engine->evaluate("(function() { }).connect(123)");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("TypeError: Function.prototype.connect: this object is not a signal"));
    }
    {
        QScriptValue ret = m_engine->evaluate("var o = { }; o.connect = Function.prototype.connect;  o.connect(123)");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("TypeError: Function.prototype.connect: this object is not a signal"));
    }

    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokable.connect(123)");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("TypeError: Function.prototype.connect: MyQObject::myInvokable() is not a signal"));
    }
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokable.connect(function() { })");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("TypeError: Function.prototype.connect: MyQObject::myInvokable() is not a signal"));
    }

    {
        QScriptValue ret = m_engine->evaluate("myObject.mySignal.connect(123)");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("TypeError: Function.prototype.connect: target is not a function"));
    }

    {
        QScriptValue ret = m_engine->evaluate("(function() { }).disconnect()");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("Error: Function.prototype.disconnect: no arguments given"));
    }
    {
        QScriptValue ret = m_engine->evaluate("var o = { }; o.disconnect = Function.prototype.disconnect;  o.disconnect()");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("Error: Function.prototype.disconnect: no arguments given"));
    }

    {
        QScriptValue ret = m_engine->evaluate("(function() { }).disconnect(123)");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("TypeError: Function.prototype.disconnect: this object is not a signal"));
    }
    {
        QScriptValue ret = m_engine->evaluate("var o = { }; o.disconnect = Function.prototype.disconnect;  o.disconnect(123)");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("TypeError: Function.prototype.disconnect: this object is not a signal"));
    }

    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokable.disconnect(123)");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("TypeError: Function.prototype.disconnect: MyQObject::myInvokable() is not a signal"));
    }
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokable.disconnect(function() { })");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("TypeError: Function.prototype.disconnect: MyQObject::myInvokable() is not a signal"));
    }

    {
        QScriptValue ret = m_engine->evaluate("myObject.mySignal.disconnect(123)");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("TypeError: Function.prototype.disconnect: target is not a function"));
    }

    {
        QScriptValue ret = m_engine->evaluate("myObject.mySignal.disconnect(function() { })");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("Error: Function.prototype.disconnect: failed to disconnect from MyQObject::mySignal()"));
    }
}

void tst_QScriptExtQObject::connectAndDisconnect_senderDeleted()
{
    QScriptEngine eng;
    QObject *obj = new QObject;
    eng.globalObject().setProperty("obj", eng.newQObject(obj));
    eng.evaluate("signal = obj.destroyed");
    delete obj;
    {
        QScriptValue ret = eng.evaluate("signal.connect(function(){})");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QString::fromLatin1("TypeError: Function.prototype.connect: cannot connect to deleted QObject"));
    }
    {
        QScriptValue ret = eng.evaluate("signal.disconnect(function(){})");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QString::fromLatin1("TypeError: Function.prototype.discconnect: cannot disconnect from deleted QObject"));
    }
}

void tst_QScriptExtQObject::cppConnectAndDisconnect()
{
    QScriptEngine eng;
    QLineEdit edit;
    QLineEdit edit2;
    QScriptValue fun = eng.evaluate("function fun(text) { signalObject = this; signalArg = text; }; fun");
    QVERIFY(fun.isFunction());
    for (int z = 0; z < 2; ++z) {
        QScriptValue receiver;
        if (z == 0)
            receiver = QScriptValue();
        else
            receiver = eng.newObject();
        for (int y = 0; y < 2; ++y) {
            QVERIFY(qScriptConnect(&edit, SIGNAL(textChanged(const QString &)), receiver, fun));
            QVERIFY(qScriptConnect(&edit2, SIGNAL(textChanged(const QString &)), receiver, fun));
            // check signal emission
            for (int x = 0; x < 4; ++x) {
                QLineEdit *ed = (x < 2) ? &edit : &edit2;
                ed->setText((x % 2) ? "foo" : "bar");
                {
                    QScriptValue ret = eng.globalObject().property("signalObject");
                    if (receiver.isObject())
                        QVERIFY(ret.strictlyEquals(receiver));
                    else
                        QVERIFY(ret.strictlyEquals(eng.globalObject()));
                }
                {
                    QScriptValue ret = eng.globalObject().property("signalArg");
                    QVERIFY(ret.isString());
                    QCOMPARE(ret.toString(), ed->text());
                }
                eng.collectGarbage();
            }

            // check disconnect
            QVERIFY(qScriptDisconnect(&edit, SIGNAL(textChanged(const QString &)), receiver, fun));
            eng.globalObject().setProperty("signalObject", QScriptValue());
            eng.globalObject().setProperty("signalArg", QScriptValue());
            edit.setText("something else");
            QVERIFY(!eng.globalObject().property("signalObject").isValid());
            QVERIFY(!eng.globalObject().property("signalArg").isValid());
            QVERIFY(!qScriptDisconnect(&edit, SIGNAL(textChanged(const QString &)), receiver, fun));

            // other object's connection should remain
            edit2.setText(edit.text());
            {
                QScriptValue ret = eng.globalObject().property("signalObject");
                if (receiver.isObject())
                    QVERIFY(ret.strictlyEquals(receiver));
                else
                    QVERIFY(ret.strictlyEquals(eng.globalObject()));
            }
            {
                QScriptValue ret = eng.globalObject().property("signalArg");
                QVERIFY(ret.isString());
                QCOMPARE(ret.toString(), edit2.text());
            }

            // disconnect other object too
            QVERIFY(qScriptDisconnect(&edit2, SIGNAL(textChanged(const QString &)), receiver, fun));
            eng.globalObject().setProperty("signalObject", QScriptValue());
            eng.globalObject().setProperty("signalArg", QScriptValue());
            edit2.setText("even more different");
            QVERIFY(!eng.globalObject().property("signalObject").isValid());
            QVERIFY(!eng.globalObject().property("signalArg").isValid());
            QVERIFY(!qScriptDisconnect(&edit2, SIGNAL(textChanged(const QString &)), receiver, fun));
        }
    }
}

void tst_QScriptExtQObject::cppConnectAndDisconnect2()
{
    QScriptEngine eng;
    QLineEdit edit;
    QLineEdit edit2;
    QScriptValue fun = eng.evaluate("function fun(text) { signalObject = this; signalArg = text; }; fun");
    // make sure we don't crash when engine is deleted
    {
        QScriptEngine *eng2 = new QScriptEngine;
        QScriptValue fun2 = eng2->evaluate("(function(text) { signalObject = this; signalArg = text; })");
        QVERIFY(fun2.isFunction());
        QVERIFY(qScriptConnect(&edit, SIGNAL(textChanged(const QString &)), QScriptValue(), fun2));
        delete eng2;
        edit.setText("ciao");
        QVERIFY(!qScriptDisconnect(&edit, SIGNAL(textChanged(const QString &)), QScriptValue(), fun2));
    }

    // mixing script-side and C++-side connect
    {
        eng.globalObject().setProperty("edit", eng.newQObject(&edit));
        QVERIFY(eng.evaluate("edit.textChanged.connect(fun)").isUndefined());
        QVERIFY(qScriptDisconnect(&edit, SIGNAL(textChanged(const QString &)), QScriptValue(), fun));

        QVERIFY(qScriptConnect(&edit, SIGNAL(textChanged(const QString &)), QScriptValue(), fun));
        QVERIFY(eng.evaluate("edit.textChanged.disconnect(fun)").isUndefined());
    }

    // signalHandlerException()
    {
        connect(&eng, SIGNAL(signalHandlerException(QScriptValue)),
                this, SLOT(onSignalHandlerException(QScriptValue)));

        eng.globalObject().setProperty("edit", eng.newQObject(&edit));
        QScriptValue fun = eng.evaluate("(function() { nonExistingFunction(); })");
        QVERIFY(fun.isFunction());
        QVERIFY(qScriptConnect(&edit, SIGNAL(textChanged(const QString &)), QScriptValue(), fun));

        m_signalHandlerException = QScriptValue();
        QScriptValue ret = eng.evaluate("edit.text = 'trigger a signal handler exception from script'");
        QVERIFY(ret.isError());
        QVERIFY(m_signalHandlerException.strictlyEquals(ret));

        m_signalHandlerException = QScriptValue();
        edit.setText("trigger a signal handler exception from C++");
        QVERIFY(m_signalHandlerException.isError());

        QVERIFY(qScriptDisconnect(&edit, SIGNAL(textChanged(const QString &)), QScriptValue(), fun));

        m_signalHandlerException = QScriptValue();
        eng.evaluate("edit.text = 'no more exception from script'");
        QVERIFY(!m_signalHandlerException.isValid());
        edit.setText("no more exception from C++");
        QVERIFY(!m_signalHandlerException.isValid());

        disconnect(&eng, SIGNAL(signalHandlerException(QScriptValue)),
                   this, SLOT(onSignalHandlerException(QScriptValue)));
    }

    // check that connectNotify() and disconnectNotify() are called (task 232987)
    {
        m_myObject->clearConnectNotifySignals();
        QVERIFY(qScriptConnect(m_myObject, SIGNAL(mySignal()), QScriptValue(), fun));
        QCOMPARE(m_myObject->connectNotifySignals().size(), 2);
        QVERIFY(m_myObject->hasConnectNotifySignal(QMetaMethod::fromSignal(&MyQObject::mySignal)));
        // We get a destroyed() connection as well, used internally by Qt Script
        QVERIFY(m_myObject->hasConnectNotifySignal(QMetaMethod::fromSignal(&QObject::destroyed)));

        m_myObject->clearDisconnectNotifySignals();
        QVERIFY(qScriptDisconnect(m_myObject, SIGNAL(mySignal()), QScriptValue(), fun));
        QVERIFY(m_myObject->hasSingleDisconnectNotifySignal(QMetaMethod::fromSignal(&MyQObject::mySignal)));
    }

    // bad args
    QVERIFY(!qScriptConnect(0, SIGNAL(foo()), QScriptValue(), fun));
    QVERIFY(!qScriptConnect(&edit, 0, QScriptValue(), fun));
    QVERIFY(!qScriptConnect(&edit, SIGNAL(foo()), QScriptValue(), fun));
    QVERIFY(!qScriptConnect(&edit, SIGNAL(textChanged(QString)), QScriptValue(), QScriptValue()));
    QVERIFY(!qScriptDisconnect(0, SIGNAL(foo()), QScriptValue(), fun));
    QVERIFY(!qScriptDisconnect(&edit, 0, QScriptValue(), fun));
    QVERIFY(!qScriptDisconnect(&edit, SIGNAL(foo()), QScriptValue(), fun));
    QVERIFY(!qScriptDisconnect(&edit, SIGNAL(textChanged(QString)), QScriptValue(), QScriptValue()));
    {
        QScriptEngine eng2;
        QScriptValue receiverInDifferentEngine = eng2.newObject();
        QVERIFY(!qScriptConnect(&edit, SIGNAL(textChanged(QString)), receiverInDifferentEngine, fun));
        QVERIFY(!qScriptDisconnect(&edit, SIGNAL(textChanged(QString)), receiverInDifferentEngine, fun));
    }
}

void tst_QScriptExtQObject::classEnums()
{
    QScriptValue myClass = m_engine->newQMetaObject(m_myObject->metaObject(), m_engine->undefinedValue());
    m_engine->globalObject().setProperty("MyQObject", myClass);

    QVERIFY(m_engine->evaluate("MyQObject.FooPolicy").isNumber()); // no strong typing
    QCOMPARE(static_cast<MyQObject::Policy>(m_engine->evaluate("MyQObject.FooPolicy").toInt32()),
             MyQObject::FooPolicy);
    QCOMPARE(static_cast<MyQObject::Policy>(m_engine->evaluate("MyQObject.BarPolicy").toInt32()),
             MyQObject::BarPolicy);
    QCOMPARE(static_cast<MyQObject::Policy>(m_engine->evaluate("MyQObject.BazPolicy").toInt32()),
             MyQObject::BazPolicy);

    QCOMPARE(static_cast<MyQObject::Strategy>(m_engine->evaluate("MyQObject.FooStrategy").toInt32()),
             MyQObject::FooStrategy);
    QCOMPARE(static_cast<MyQObject::Strategy>(m_engine->evaluate("MyQObject.BarStrategy").toInt32()),
             MyQObject::BarStrategy);
    QCOMPARE(static_cast<MyQObject::Strategy>(m_engine->evaluate("MyQObject.BazStrategy").toInt32()),
             MyQObject::BazStrategy);

    QVERIFY(m_engine->evaluate("MyQObject.NoAbility").isNumber()); // no strong typing
    QCOMPARE(MyQObject::Ability(m_engine->evaluate("MyQObject.NoAbility").toInt32()),
             MyQObject::NoAbility);
    QCOMPARE(MyQObject::Ability(m_engine->evaluate("MyQObject.FooAbility").toInt32()),
             MyQObject::FooAbility);
    QCOMPARE(MyQObject::Ability(m_engine->evaluate("MyQObject.BarAbility").toInt32()),
             MyQObject::BarAbility);
    QCOMPARE(MyQObject::Ability(m_engine->evaluate("MyQObject.BazAbility").toInt32()),
             MyQObject::BazAbility);
    QCOMPARE(MyQObject::Ability(m_engine->evaluate("MyQObject.AllAbility").toInt32()),
             MyQObject::AllAbility);

    // Constructors for flags are not provided
    QVERIFY(m_engine->evaluate("MyQObject.Ability").isUndefined());

    QScriptValue::PropertyFlags expectedEnumFlags = QScriptValue::ReadOnly | QScriptValue::Undeletable;
    QCOMPARE(myClass.propertyFlags("FooPolicy"), expectedEnumFlags);
    QCOMPARE(myClass.propertyFlags("BarPolicy"), expectedEnumFlags);
    QCOMPARE(myClass.propertyFlags("BazPolicy"), expectedEnumFlags);

    // enums from Qt are inherited through prototype
    QCOMPARE(static_cast<Qt::FocusPolicy>(m_engine->evaluate("MyQObject.StrongFocus").toInt32()),
             Qt::StrongFocus);
    QCOMPARE(static_cast<Qt::Key>(m_engine->evaluate("MyQObject.Key_Left").toInt32()),
             Qt::Key_Left);

    QCOMPARE(m_engine->evaluate("MyQObject.className()").toString(), QLatin1String("MyQObject"));

    qRegisterMetaType<MyQObject::Policy>("Policy");

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.myInvokableWithEnumArg(MyQObject.BazPolicy)").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 10);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), int(MyQObject::BazPolicy));

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.myInvokableWithEnumArg('BarPolicy')").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 10);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), int(MyQObject::BarPolicy));

    m_myObject->resetQtFunctionInvoked();
    QVERIFY(m_engine->evaluate("myObject.myInvokableWithEnumArg('NoSuchPolicy')").isError());
    QCOMPARE(m_myObject->qtFunctionInvoked(), -1);

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(m_engine->evaluate("myObject.myInvokableWithQualifiedEnumArg(MyQObject.BazPolicy)").isUndefined(), true);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 36);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), int(MyQObject::BazPolicy));

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableReturningEnum()");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 37);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 0);
        QCOMPARE(ret.isVariant(), true);
    }
    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableReturningQualifiedEnum()");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 38);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 0);
        QCOMPARE(ret.isNumber(), true);
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithFlagsArg(MyQObject.FooAbility)");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 58);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), int(MyQObject::FooAbility));
        QCOMPARE(ret.isNumber(), true);
        QCOMPARE(ret.toInt32(), int(MyQObject::FooAbility));
    }
    m_myObject->resetQtFunctionInvoked();
    {
        QScriptValue ret = m_engine->evaluate("myObject.myInvokableWithQualifiedFlagsArg(MyQObject.BarAbility)");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 59);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), int(MyQObject::BarAbility));
        QCOMPARE(ret.isNumber(), true);
        QCOMPARE(ret.toInt32(), int(MyQObject::BarAbility));
    }

    // enum properties are not deletable or writable
    QVERIFY(!m_engine->evaluate("delete MyQObject.BazPolicy").toBool());
    myClass.setProperty("BazPolicy", QScriptValue());
    QCOMPARE(static_cast<MyQObject::Policy>(myClass.property("BazPolicy").toInt32()),
             MyQObject::BazPolicy);
    myClass.setProperty("BazPolicy", MyQObject::FooPolicy);
    QCOMPARE(static_cast<MyQObject::Policy>(myClass.property("BazPolicy").toInt32()),
             MyQObject::BazPolicy);
}

QT_BEGIN_NAMESPACE
Q_SCRIPT_DECLARE_QMETAOBJECT(MyQObject, QObject*)
Q_SCRIPT_DECLARE_QMETAOBJECT(QObject, QObject*)
QT_END_NAMESPACE

class ConstructorTest : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE ConstructorTest(QObject *parent)
        : QObject(parent)
    {
        setProperty("ctorIndex", 0);
    }
    Q_INVOKABLE ConstructorTest(int arg, QObject *parent = 0)
        : QObject(parent)
    {
        setProperty("ctorIndex", 1);
        setProperty("arg", arg);
    }
    Q_INVOKABLE ConstructorTest(const QString &arg, QObject *parent = 0)
        : QObject(parent)
    {
        setProperty("ctorIndex", 2);
        setProperty("arg", arg);
    }
    Q_INVOKABLE ConstructorTest(int arg, const QString &arg2, QObject *parent = 0)
        : QObject(parent)
    {
        setProperty("ctorIndex", 3);
        setProperty("arg", arg);
        setProperty("arg2", arg2);
    }
    Q_INVOKABLE ConstructorTest(const QBrush &arg, QObject *parent = 0)
        : QObject(parent)
    {
        setProperty("ctorIndex", 4);
        setProperty("arg", arg);
    }
};

void tst_QScriptExtQObject::classConstructor()
{
    QScriptValue myClass = qScriptValueFromQMetaObject<MyQObject>(m_engine);
    m_engine->globalObject().setProperty("MyQObject", myClass);

    QScriptValue myObj = m_engine->evaluate("myObj = MyQObject()");
    QObject *qobj = myObj.toQObject();
    QVERIFY(qobj != 0);
    QCOMPARE(qobj->metaObject()->className(), "MyQObject");
    QCOMPARE(qobj->parent(), (QObject *)0);

    QScriptValue qobjectClass = qScriptValueFromQMetaObject<QObject>(m_engine);
    m_engine->globalObject().setProperty("QObject", qobjectClass);

    QScriptValue otherObj = m_engine->evaluate("otherObj = QObject(myObj)");
    QObject *qqobj = otherObj.toQObject();
    QVERIFY(qqobj != 0);
    QCOMPARE(qqobj->metaObject()->className(), "QObject");
    QCOMPARE(qqobj->parent(), qobj);

    delete qobj;

    // Q_INVOKABLE constructors
    {
        QScriptValue klazz = m_engine->newQMetaObject(&ConstructorTest::staticMetaObject);
        {
            QScriptValue obj = klazz.construct();
            QVERIFY(obj.isError());
            QCOMPARE(obj.toString(), QString::fromLatin1("SyntaxError: too few arguments in call to ConstructorTest(); candidates are\n"
                                                         "    ConstructorTest(QBrush)\n"
                                                         "    ConstructorTest(QBrush,QObject*)\n"
                                                         "    ConstructorTest(int,QString)\n"
                                                         "    ConstructorTest(int,QString,QObject*)\n"
                                                         "    ConstructorTest(QString)\n"
                                                         "    ConstructorTest(QString,QObject*)\n"
                                                         "    ConstructorTest(int)\n"
                                                         "    ConstructorTest(int,QObject*)\n"
                                                         "    ConstructorTest(QObject*)"));
        }
        {
            QObject objobj;
            QScriptValue arg = m_engine->newQObject(&objobj);
            QScriptValue obj = klazz.construct(QScriptValueList() << arg);
            QVERIFY(!obj.isError());
            QVERIFY(obj.instanceOf(klazz));
            QVERIFY(obj.property("ctorIndex").isNumber());
            QCOMPARE(obj.property("ctorIndex").toInt32(), 0);
        }
        {
            int arg = 123;
            QScriptValue obj = klazz.construct(QScriptValueList() << arg);
            QVERIFY(!obj.isError());
            QVERIFY(obj.instanceOf(klazz));
            QVERIFY(obj.property("ctorIndex").isNumber());
            QCOMPARE(obj.property("ctorIndex").toInt32(), 1);
            QVERIFY(obj.property("arg").isNumber());
            QCOMPARE(obj.property("arg").toInt32(), arg);
        }
        {
            QString arg = "foo";
            QScriptValue obj = klazz.construct(QScriptValueList() << arg);
            QVERIFY(!obj.isError());
            QVERIFY(obj.instanceOf(klazz));
            QVERIFY(obj.property("ctorIndex").isNumber());
            QCOMPARE(obj.property("ctorIndex").toInt32(), 2);
            QVERIFY(obj.property("arg").isString());
            QCOMPARE(obj.property("arg").toString(), arg);
        }
        {
            int arg = 123;
            QString arg2 = "foo";
            QScriptValue obj = klazz.construct(QScriptValueList() << arg << arg2);
            QVERIFY(!obj.isError());
            QVERIFY(obj.instanceOf(klazz));
            QVERIFY(obj.property("ctorIndex").isNumber());
            QCOMPARE(obj.property("ctorIndex").toInt32(), 3);
            QVERIFY(obj.property("arg").isNumber());
            QCOMPARE(obj.property("arg").toInt32(), arg);
            QVERIFY(obj.property("arg2").isString());
            QCOMPARE(obj.property("arg2").toString(), arg2);
        }
        {
            QBrush arg(Qt::red);
            QScriptValue obj = klazz.construct(QScriptValueList() << qScriptValueFromValue(m_engine, arg));
            QVERIFY(!obj.isError());
            QVERIFY(obj.instanceOf(klazz));
            QVERIFY(obj.property("ctorIndex").isNumber());
            QCOMPARE(obj.property("ctorIndex").toInt32(), 4);
            QVERIFY(obj.property("arg").isVariant());
            QCOMPARE(qvariant_cast<QBrush>(obj.property("arg").toVariant()), arg);
        }
        {
            QDir arg;
            QScriptValue obj = klazz.construct(QScriptValueList()
                                               << qScriptValueFromValue(m_engine, arg));
            QVERIFY(obj.isError());
            QCOMPARE(obj.toString(), QString::fromLatin1("TypeError: ambiguous call of overloaded function ConstructorTest(); candidates were\n"
                                                         "    ConstructorTest(int)\n"
                                                         "    ConstructorTest(QString)"));
        }
    }
}

void tst_QScriptExtQObject::overrideInvokable()
{
    m_myObject->resetQtFunctionInvoked();
    m_engine->evaluate("myObject.myInvokable()");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 0);

    m_myObject->resetQtFunctionInvoked();
    m_engine->evaluate("myObject.myInvokable = function() { global.a = 123; }");
    m_engine->evaluate("myObject.myInvokable()");
    QCOMPARE(m_myObject->qtFunctionInvoked(), -1);
    QCOMPARE(m_engine->evaluate("global.a").toNumber(), 123.0);

    m_engine->evaluate("myObject.myInvokable = function() { global.a = 456; }");
    m_engine->evaluate("myObject.myInvokable()");
    QCOMPARE(m_myObject->qtFunctionInvoked(), -1);
    QCOMPARE(m_engine->evaluate("global.a").toNumber(), 456.0);

    m_engine->evaluate("delete myObject.myInvokable");
    m_engine->evaluate("myObject.myInvokable()");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 0);

    m_myObject->resetQtFunctionInvoked();
    m_engine->evaluate("myObject.myInvokable = myObject.myInvokableWithIntArg");
    m_engine->evaluate("myObject.myInvokable(123)");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 1);

    m_engine->evaluate("delete myObject.myInvokable");
    m_myObject->resetQtFunctionInvoked();
    // this form (with the '()') is read-only
    m_engine->evaluate("myObject['myInvokable()'] = function() { global.a = 123; }");
    m_engine->evaluate("myObject.myInvokable()");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 0);
}

void tst_QScriptExtQObject::transferInvokable()
{
    m_myObject->resetQtFunctionInvoked();
    m_engine->evaluate("myObject.foozball = myObject.myInvokable");
    m_engine->evaluate("myObject.foozball()");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 0);
    m_myObject->resetQtFunctionInvoked();
    m_engine->evaluate("myObject.foozball = myObject.myInvokableWithIntArg");
    m_engine->evaluate("myObject.foozball(123)");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 1);
    m_myObject->resetQtFunctionInvoked();
    m_engine->evaluate("myObject.myInvokable = myObject.myInvokableWithIntArg");
    m_engine->evaluate("myObject.myInvokable(123)");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 1);

    MyOtherQObject other;
    m_engine->globalObject().setProperty(
        "myOtherObject", m_engine->newQObject(&other));
    m_engine->evaluate("myOtherObject.foo = myObject.foozball");
    other.resetQtFunctionInvoked();
    m_engine->evaluate("myOtherObject.foo(456)");
    QCOMPARE(other.qtFunctionInvoked(), 1);
}

void tst_QScriptExtQObject::findChild()
{
    QObject *child = new QObject(m_myObject);
    child->setObjectName(QLatin1String("myChildObject"));

    {
        QScriptValue result = m_engine->evaluate("myObject.findChild('noSuchChild')");
        QCOMPARE(result.isNull(), true);
    }

    {
        QScriptValue result = m_engine->evaluate("myObject.findChild('myChildObject')");
        QCOMPARE(result.isQObject(), true);
        QCOMPARE(result.toQObject(), child);
    }

    delete child;
}

void tst_QScriptExtQObject::findChildren()
{
    QObject *child = new QObject(m_myObject);
    child->setObjectName(QLatin1String("myChildObject"));

    {
        QScriptValue result = m_engine->evaluate("myObject.findChildren('noSuchChild')");
        QCOMPARE(result.isArray(), true);
        QCOMPARE(result.property(QLatin1String("length")).toNumber(), 0.0);
    }

    {
        QScriptValue result = m_engine->evaluate("myObject.findChildren('myChildObject')");
        QCOMPARE(result.isArray(), true);
        QCOMPARE(result.property(QLatin1String("length")).toNumber(), 1.0);
        QCOMPARE(result.property(QLatin1String("0")).toQObject(), child);
    }

    QObject *namelessChild = new QObject(m_myObject);

    {
        QScriptValue result = m_engine->evaluate("myObject.findChildren('myChildObject')");
        QCOMPARE(result.isArray(), true);
        QCOMPARE(result.property(QLatin1String("length")).toNumber(), 1.0);
        QCOMPARE(result.property(QLatin1String("0")).toQObject(), child);
    }

    QObject *anotherChild = new QObject(m_myObject);
    anotherChild->setObjectName(QLatin1String("anotherChildObject"));

    {
        QScriptValue result = m_engine->evaluate("myObject.findChildren('anotherChildObject')");
        QCOMPARE(result.isArray(), true);
        QCOMPARE(result.property(QLatin1String("length")).toNumber(), 1.0);
        QCOMPARE(result.property(QLatin1String("0")).toQObject(), anotherChild);
    }

    anotherChild->setObjectName(QLatin1String("myChildObject"));
    {
        QScriptValue result = m_engine->evaluate("myObject.findChildren('myChildObject')");
        QCOMPARE(result.isArray(), true);
        QCOMPARE(result.property(QLatin1String("length")).toNumber(), 2.0);
        QObject *o1 = result.property(QLatin1String("0")).toQObject();
        QObject *o2 = result.property(QLatin1String("1")).toQObject();
        if (o1 != child) {
            QCOMPARE(o1, anotherChild);
            QCOMPARE(o2, child);
        } else {
            QCOMPARE(o1, child);
            QCOMPARE(o2, anotherChild);
        }
    }

    // find all
    {
        QScriptValue result = m_engine->evaluate("myObject.findChildren()");
        QVERIFY(result.isArray());
        int count = 3;
        QCOMPARE(result.property("length").toInt32(), count);
        for (int i = 0; i < 3; ++i) {
            QObject *o = result.property(i).toQObject();
            if (o == namelessChild || o == child || o == anotherChild)
                --count;
        }
        QVERIFY(count == 0);
    }

    // matchall regexp
    {
        QScriptValue result = m_engine->evaluate("myObject.findChildren(/.*/)");
        QVERIFY(result.isArray());
        int count = 3;
        QCOMPARE(result.property("length").toInt32(), count);
        for (int i = 0; i < 3; ++i) {
            QObject *o = result.property(i).toQObject();
            if (o == namelessChild || o == child || o == anotherChild)
                --count;
        }
        QVERIFY(count == 0);
    }

    // matchall regexp my*
    {
        QScriptValue result = m_engine->evaluate("myObject.findChildren(new RegExp(\"^my.*\"))");
        QCOMPARE(result.isArray(), true);
        QCOMPARE(result.property(QLatin1String("length")).toNumber(), 2.0);
        QObject *o1 = result.property(QLatin1String("0")).toQObject();
        QObject *o2 = result.property(QLatin1String("1")).toQObject();
        if (o1 != child) {
            QCOMPARE(o1, anotherChild);
            QCOMPARE(o2, child);
        } else {
            QCOMPARE(o1, child);
            QCOMPARE(o2, anotherChild);
        }
    }
    delete anotherChild;
    delete namelessChild;
    delete child;
}

void tst_QScriptExtQObject::childObjects()
{
    QObject *child1 = new QObject(m_myObject);
    child1->setObjectName("child1");
    QObject *child2 = new QObject(m_myObject);
    QScriptValue wrapped = m_engine->newQObject(m_myObject);

    QVERIFY(wrapped.property("child1").isQObject());
    QCOMPARE(wrapped.property("child1").toQObject(), child1);
    QVERIFY(!wrapped.property("child2").isQObject());
    QVERIFY(!wrapped.property("child2").isValid());

    // Setting the name later
    child2->setObjectName("child2");

    QVERIFY(wrapped.property("child1").isQObject());
    QCOMPARE(wrapped.property("child1").toQObject(), child1);
    QVERIFY(wrapped.property("child2").isQObject());
    QCOMPARE(wrapped.property("child2").toQObject(), child2);

    // Adding a child later
    QObject *child3 = new QObject(m_myObject);
    child3->setObjectName("child3");

    QVERIFY(wrapped.property("child3").isQObject());
    QCOMPARE(wrapped.property("child3").toQObject(), child3);

    // Changing a child name
    child1->setObjectName("anotherName");

    QVERIFY(!wrapped.property("child1").isValid());
    QVERIFY(wrapped.property("anotherName").isQObject());
    QCOMPARE(wrapped.property("anotherName").toQObject(), child1);

    // Creating another object wrapper excluding child from
    // properties.
    QScriptValue wrapped2 = m_engine->newQObject(m_myObject, QScriptEngine::QtOwnership, QScriptEngine::ExcludeChildObjects);

    QVERIFY(!wrapped2.property("anotherName").isValid());
    QVERIFY(!wrapped2.property("child2").isValid());
    QVERIFY(!wrapped2.property("child3").isValid());
}

void tst_QScriptExtQObject::overloadedSlots()
{
    // should pick myOverloadedSlot(double)
    m_myObject->resetQtFunctionInvoked();
    m_engine->evaluate("myObject.myOverloadedSlot(10)");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 26);

    // should pick myOverloadedSlot(double)
    m_myObject->resetQtFunctionInvoked();
    m_engine->evaluate("myObject.myOverloadedSlot(10.0)");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 26);

    // should pick myOverloadedSlot(QString)
    m_myObject->resetQtFunctionInvoked();
    m_engine->evaluate("myObject.myOverloadedSlot('10')");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 29);

    // should pick myOverloadedSlot(bool)
    m_myObject->resetQtFunctionInvoked();
    m_engine->evaluate("myObject.myOverloadedSlot(true)");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 25);

    // should pick myOverloadedSlot(QDateTime)
    m_myObject->resetQtFunctionInvoked();
    m_engine->evaluate("myObject.myOverloadedSlot(new Date())");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 32);

    // should pick myOverloadedSlot(QRegExp)
    m_myObject->resetQtFunctionInvoked();
    m_engine->evaluate("myObject.myOverloadedSlot(new RegExp())");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 34);

    // should pick myOverloadedSlot(QVariant)
    m_myObject->resetQtFunctionInvoked();
    QScriptValue f = m_engine->evaluate("myObject.myOverloadedSlot");
    f.call(QScriptValue(), QScriptValueList() << m_engine->newVariant(QVariant("ciao")));
    QCOMPARE(m_myObject->qtFunctionInvoked(), 35);

    // should pick myOverloadedSlot(QObject*)
    m_myObject->resetQtFunctionInvoked();
    m_engine->evaluate("myObject.myOverloadedSlot(myObject)");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 41);

    // should pick myOverloadedSlot(QObject*)
    m_myObject->resetQtFunctionInvoked();
    m_engine->evaluate("myObject.myOverloadedSlot(null)");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 41);

    // should pick myOverloadedSlot(QStringList)
    m_myObject->resetQtFunctionInvoked();
    m_engine->evaluate("myObject.myOverloadedSlot(['hello'])");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 42);
}

void tst_QScriptExtQObject::enumerate_data()
{
    QTest::addColumn<int>("wrapOptions");
    QTest::addColumn<QStringList>("expectedNames");

    QTest::newRow( "enumerate all" )
        << 0
        << (QStringList()
            // meta-object-defined properties:
            //   inherited
            << "objectName"
            //   non-inherited
            << "p1" << "p2" << "p4" << "p6"
            // dynamic properties
            << "dp1" << "dp2" << "dp3"
            // inherited signals
            << "destroyed(QObject*)" << "destroyed()"
            << "objectNameChanged(QString)"
            // inherited slots
            << "deleteLater()"
            // not included because it's private:
            // << "_q_reregisterTimers(void*)"
            // signals
            << "mySignal()"
            // slots
            << "mySlot()" << "myOtherSlot()");

    QTest::newRow( "don't enumerate inherited properties" )
        << int(QScriptEngine::ExcludeSuperClassProperties)
        << (QStringList()
            // meta-object-defined properties:
            //   non-inherited
            << "p1" << "p2" << "p4" << "p6"
            // dynamic properties
            << "dp1" << "dp2" << "dp3"
            // inherited signals
            << "destroyed(QObject*)" << "destroyed()"
            << "objectNameChanged(QString)"
            // inherited slots
            << "deleteLater()"
            // not included because it's private:
            // << "_q_reregisterTimers(void*)"
            // signals
            << "mySignal()"
            // slots
            << "mySlot()" << "myOtherSlot()");

    QTest::newRow( "don't enumerate inherited methods" )
        << int(QScriptEngine::ExcludeSuperClassMethods)
        << (QStringList()
            // meta-object-defined properties:
            //   inherited
            << "objectName"
            //   non-inherited
            << "p1" << "p2" << "p4" << "p6"
            // dynamic properties
            << "dp1" << "dp2" << "dp3"
            // signals
            << "mySignal()"
            // slots
            << "mySlot()" << "myOtherSlot()");

    QTest::newRow( "don't enumerate inherited members" )
        << int(QScriptEngine::ExcludeSuperClassMethods
               | QScriptEngine::ExcludeSuperClassProperties)
        << (QStringList()
            // meta-object-defined properties
            << "p1" << "p2" << "p4" << "p6"
            // dynamic properties
            << "dp1" << "dp2" << "dp3"
            // signals
            << "mySignal()"
            // slots
            << "mySlot()" << "myOtherSlot()");

    QTest::newRow( "enumerate properties, not methods" )
        << int(QScriptEngine::SkipMethodsInEnumeration)
        << (QStringList()
            // meta-object-defined properties:
            //   inherited
            << "objectName"
            //   non-inherited
            << "p1" << "p2" << "p4" << "p6"
            // dynamic properties
            << "dp1" << "dp2" << "dp3");

    QTest::newRow( "don't enumerate inherited properties + methods" )
        << int(QScriptEngine::ExcludeSuperClassProperties
            | QScriptEngine::SkipMethodsInEnumeration)
        << (QStringList()
            // meta-object-defined properties:
            //   non-inherited
            << "p1" << "p2" << "p4" << "p6"
            // dynamic properties
            << "dp1" << "dp2" << "dp3");

    QTest::newRow( "don't enumerate inherited members" )
        << int(QScriptEngine::ExcludeSuperClassContents)
        << (QStringList()
            // meta-object-defined properties
            << "p1" << "p2" << "p4" << "p6"
            // dynamic properties
            << "dp1" << "dp2" << "dp3"
            // signals
            << "mySignal()"
            // slots
            << "mySlot()" << "myOtherSlot()");

    QTest::newRow( "don't enumerate deleteLater()" )
        << int(QScriptEngine::ExcludeDeleteLater)
        << (QStringList()
            // meta-object-defined properties:
            //   inherited
            << "objectName"
            //   non-inherited
            << "p1" << "p2" << "p4" << "p6"
            // dynamic properties
            << "dp1" << "dp2" << "dp3"
            // inherited signals
            << "destroyed(QObject*)" << "destroyed()"
            << "objectNameChanged(QString)"
            // not included because it's private:
            // << "_q_reregisterTimers(void*)"
            // signals
            << "mySignal()"
            // slots
            << "mySlot()" << "myOtherSlot()");

    QTest::newRow( "don't enumerate slots" )
        << int(QScriptEngine::ExcludeSlots)
        << (QStringList()
            // meta-object-defined properties:
            //   inherited
            << "objectName"
            //   non-inherited
            << "p1" << "p2" << "p4" << "p6"
            // dynamic properties
            << "dp1" << "dp2" << "dp3"
            // inherited signals
            << "destroyed(QObject*)" << "destroyed()"
            << "objectNameChanged(QString)"
            // signals
            << "mySignal()");
}

// Message for easily identifying mismatches in string list.
static QByteArray msgEnumerationFail(const QStringList &actual, const QStringList &expected)
{
    QString result;
    QTextStream str(&result);
    str << "\nActual " << actual.size() << ":\n";
    for (int i = 0; i < actual.size(); ++i) {
        const int index = expected.indexOf(actual.at(i));
        if (index < 0)
            str << "*** ";
        str << "    #" << i << " '"<< actual.at(i)
            << "'\tin expected at: " << index << '\n';
    }
    str << "Expected " << expected.size() << ":\n";
    for (int i = 0; i < expected.size(); ++i) {
        const int index = actual.indexOf(expected.at(i));
        if (index < 0)
            str << "*** ";
        str << "    #" << i << " '"<< expected.at(i)
            << "'\t  in actual at: " << index << '\n';
    }
    return result.toLocal8Bit();
}

void tst_QScriptExtQObject::enumerate()
{
    QFETCH( int, wrapOptions );
    QFETCH( QStringList, expectedNames );

    QScriptEngine eng;
    MyEnumTestQObject enumQObject;
    // give it some dynamic properties
    enumQObject.setProperty("dp1", "dp1");
    enumQObject.setProperty("dp2", "dp2");
    enumQObject.setProperty("dp3", "dp3");
    QScriptValue obj = eng.newQObject(&enumQObject, QScriptEngine::QtOwnership,
                                      QScriptEngine::QObjectWrapOptions(wrapOptions));

    // enumerate in script
    {
        eng.globalObject().setProperty("myEnumObject", obj);
        eng.evaluate("var enumeratedProperties = []");
        eng.evaluate("for (var p in myEnumObject) { enumeratedProperties.push(p); }");
        QStringList result = qscriptvalue_cast<QStringList>(eng.evaluate("enumeratedProperties"));
        QVERIFY2(result == expectedNames, msgEnumerationFail(result, expectedNames).constData());
    }
    // enumerate in C++
    {
        QScriptValueIterator it(obj);
        QStringList result;
        while (it.hasNext()) {
            it.next();
            QCOMPARE(it.flags(), obj.propertyFlags(it.name()));
            result.append(it.name());
        }
        QVERIFY2(result == expectedNames, msgEnumerationFail(result, expectedNames).constData());
    }
}

class SpecialEnumTestObject : public QObject
{
    Q_OBJECT
    // overriding a property in the super-class to make it non-scriptable
    Q_PROPERTY(QString objectName READ objectName SCRIPTABLE false)
public:
    SpecialEnumTestObject(QObject *parent = 0)
        : QObject(parent) {}
};

class SpecialEnumTestObject2 : public QObject
{
    Q_OBJECT
    // overriding a property in the super-class to make it non-designable (but still scriptable)
    Q_PROPERTY(QString objectName READ objectName DESIGNABLE false)
public:
    SpecialEnumTestObject2(QObject *parent = 0)
        : QObject(parent) {}
};

void tst_QScriptExtQObject::enumerateSpecial()
{
    QScriptEngine eng;
    {
        SpecialEnumTestObject testObj;
        QScriptValueIterator it(eng.newQObject(&testObj));
        bool objectNameEncountered = false;
        while (it.hasNext()) {
            it.next();
            if (it.name() == QLatin1String("objectName")) {
                objectNameEncountered = true;
                break;
            }
        }
        QVERIFY(!objectNameEncountered);
    }
    {
        SpecialEnumTestObject2 testObj;
        testObj.setObjectName("foo");
        QScriptValueList values;
        QScriptValueIterator it(eng.newQObject(&testObj));
        while (it.hasNext()) {
            it.next();
            if (it.name() == "objectName")
                values.append(it.value());
        }
        QCOMPARE(values.size(), 1);
        QCOMPARE(values.at(0).toString(), QString::fromLatin1("foo"));
    }
}

void tst_QScriptExtQObject::wrapOptions()
{
    QCOMPARE(m_myObject->setProperty("dynamicProperty", 123), false);
    MyQObject *child = new MyQObject(m_myObject);
    child->setObjectName("child");
    // exclude child objects
    {
        QScriptValue obj = m_engine->newQObject(m_myObject, QScriptEngine::QtOwnership,
                                                QScriptEngine::ExcludeChildObjects);
        QCOMPARE(obj.property("child").isValid(), false);
        obj.setProperty("child", QScriptValue(m_engine, 123));
        QCOMPARE(obj.property("child")
                 .strictlyEquals(QScriptValue(m_engine, 123)), true);
    }
    // don't auto-create dynamic properties
    {
        QScriptValue obj = m_engine->newQObject(m_myObject);
        QVERIFY(!m_myObject->dynamicPropertyNames().contains("anotherDynamicProperty"));
        obj.setProperty("anotherDynamicProperty", QScriptValue(m_engine, 123));
        QVERIFY(!m_myObject->dynamicPropertyNames().contains("anotherDynamicProperty"));
        QCOMPARE(obj.property("anotherDynamicProperty")
                 .strictlyEquals(QScriptValue(m_engine, 123)), true);
    }
    // auto-create dynamic properties
    {
        QScriptValue obj = m_engine->newQObject(m_myObject, QScriptEngine::QtOwnership,
                                                QScriptEngine::AutoCreateDynamicProperties);
        QVERIFY(!m_myObject->dynamicPropertyNames().contains("anotherDynamicProperty"));
        obj.setProperty("anotherDynamicProperty", QScriptValue(m_engine, 123));
        QVERIFY(m_myObject->dynamicPropertyNames().contains("anotherDynamicProperty"));
        QCOMPARE(obj.property("anotherDynamicProperty")
                 .strictlyEquals(QScriptValue(m_engine, 123)), true);
        // task 236685
        {
            QScriptValue obj2 = m_engine->newObject();
            obj2.setProperty("notADynamicProperty", 456);
            obj.setPrototype(obj2);
            QScriptValue ret = obj.property("notADynamicProperty");
            QVERIFY(ret.isNumber());
            QVERIFY(ret.strictlyEquals(obj2.property("notADynamicProperty")));
        }
    }
    // don't exclude super-class properties
    {
        QScriptValue obj = m_engine->newQObject(m_myObject);
        QVERIFY(obj.property("objectName").isValid());
        QVERIFY(obj.propertyFlags("objectName") & QScriptValue::QObjectMember);
    }
    // exclude super-class properties
    {
        QScriptValue obj = m_engine->newQObject(m_myObject, QScriptEngine::QtOwnership,
                                                QScriptEngine::ExcludeSuperClassProperties);
        QVERIFY(!obj.property("objectName").isValid());
        QVERIFY(!(obj.propertyFlags("objectName") & QScriptValue::QObjectMember));
        QVERIFY(obj.property("intProperty").isValid());
        QVERIFY(obj.propertyFlags("intProperty") & QScriptValue::QObjectMember);
    }
    // don't exclude super-class methods
    {
        QScriptValue obj = m_engine->newQObject(m_myObject);
        QVERIFY(obj.property("deleteLater").isValid());
        QVERIFY(obj.propertyFlags("deleteLater") & QScriptValue::QObjectMember);
    }
    // exclude super-class methods
    {
        QScriptValue obj = m_engine->newQObject(m_myObject, QScriptEngine::QtOwnership,
                                                QScriptEngine::ExcludeSuperClassMethods);
        QVERIFY(!obj.property("deleteLater").isValid());
        QVERIFY(!(obj.propertyFlags("deleteLater") & QScriptValue::QObjectMember));
        QVERIFY(obj.property("mySlot").isValid());
        QVERIFY(obj.propertyFlags("mySlot") & QScriptValue::QObjectMember);
    }
    // exclude all super-class contents
    {
        QScriptValue obj = m_engine->newQObject(m_myObject, QScriptEngine::QtOwnership,
                                                QScriptEngine::ExcludeSuperClassContents);
        QVERIFY(!obj.property("deleteLater").isValid());
        QVERIFY(!(obj.propertyFlags("deleteLater") & QScriptValue::QObjectMember));
        QVERIFY(obj.property("mySlot").isValid());
        QVERIFY(obj.propertyFlags("mySlot") & QScriptValue::QObjectMember);

        QVERIFY(!obj.property("objectName").isValid());
        QVERIFY(!(obj.propertyFlags("objectName") & QScriptValue::QObjectMember));
        QVERIFY(obj.property("intProperty").isValid());
        QVERIFY(obj.propertyFlags("intProperty") & QScriptValue::QObjectMember);
    }
    // exclude deleteLater()
    {
        QScriptValue obj = m_engine->newQObject(m_myObject, QScriptEngine::QtOwnership,
                                                QScriptEngine::ExcludeDeleteLater);
        QVERIFY(!obj.property("deleteLater").isValid());
        QVERIFY(!(obj.propertyFlags("deleteLater") & QScriptValue::QObjectMember));
        QVERIFY(obj.property("mySlot").isValid());
        QVERIFY(obj.propertyFlags("mySlot") & QScriptValue::QObjectMember);

        QVERIFY(obj.property("objectName").isValid());
        QVERIFY(obj.propertyFlags("objectName") & QScriptValue::QObjectMember);
        QVERIFY(obj.property("intProperty").isValid());
        QVERIFY(obj.propertyFlags("intProperty") & QScriptValue::QObjectMember);
    }
    // exclude slots
    {
        QScriptValue obj = m_engine->newQObject(m_myObject, QScriptEngine::QtOwnership,
                                                QScriptEngine::ExcludeSlots);
        QVERIFY(!obj.property("deleteLater").isValid());
        QVERIFY(!(obj.propertyFlags("deleteLater") & QScriptValue::QObjectMember));
        QVERIFY(!obj.property("mySlot").isValid());
        QVERIFY(!(obj.propertyFlags("mySlot") & QScriptValue::QObjectMember));

        QVERIFY(obj.property("myInvokable").isFunction());
        QVERIFY(obj.propertyFlags("myInvokable") & QScriptValue::QObjectMember);

        QVERIFY(obj.property("mySignal").isFunction());
        QVERIFY(obj.propertyFlags("mySignal") & QScriptValue::QObjectMember);
        QVERIFY(obj.property("destroyed").isFunction());
        QVERIFY(obj.propertyFlags("destroyed") & QScriptValue::QObjectMember);

        QVERIFY(obj.property("objectName").isValid());
        QVERIFY(obj.propertyFlags("objectName") & QScriptValue::QObjectMember);
        QVERIFY(obj.property("intProperty").isValid());
        QVERIFY(obj.propertyFlags("intProperty") & QScriptValue::QObjectMember);
    }
    // exclude all that we can
    {
        QScriptValue obj = m_engine->newQObject(m_myObject, QScriptEngine::QtOwnership,
                                                QScriptEngine::ExcludeSuperClassMethods
                                                | QScriptEngine::ExcludeSuperClassProperties
                                                | QScriptEngine::ExcludeChildObjects);
        QVERIFY(!obj.property("deleteLater").isValid());
        QVERIFY(!(obj.propertyFlags("deleteLater") & QScriptValue::QObjectMember));
        QVERIFY(obj.property("mySlot").isValid());
        QVERIFY(obj.propertyFlags("mySlot") & QScriptValue::QObjectMember);

        QVERIFY(!obj.property("objectName").isValid());
        QVERIFY(!(obj.propertyFlags("objectName") & QScriptValue::QObjectMember));
        QVERIFY(obj.property("intProperty").isValid());
        QVERIFY(obj.propertyFlags("intProperty") & QScriptValue::QObjectMember);

        QCOMPARE(obj.property("child").isValid(), false);
        obj.setProperty("child", QScriptValue(m_engine, 123));
        QCOMPARE(obj.property("child")
                 .strictlyEquals(QScriptValue(m_engine, 123)), true);
    }
    // exclude absolutely all that we can
    {
        QScriptValue obj = m_engine->newQObject(m_myObject, QScriptEngine::QtOwnership,
                                                QScriptEngine::ExcludeSuperClassMethods
                                                | QScriptEngine::ExcludeSuperClassProperties
                                                | QScriptEngine::ExcludeChildObjects
                                                | QScriptEngine::ExcludeSlots);
        QVERIFY(!obj.property("deleteLater").isValid());
        QVERIFY(!(obj.propertyFlags("deleteLater") & QScriptValue::QObjectMember));

        QVERIFY(!obj.property("mySlot").isValid());
        QVERIFY(!(obj.propertyFlags("mySlot") & QScriptValue::QObjectMember));

        QVERIFY(obj.property("mySignal").isFunction());
        QVERIFY(obj.propertyFlags("mySignal") & QScriptValue::QObjectMember);

        QVERIFY(obj.property("myInvokable").isFunction());
        QVERIFY(obj.propertyFlags("myInvokable") & QScriptValue::QObjectMember);

        QVERIFY(!obj.property("objectName").isValid());
        QVERIFY(!(obj.propertyFlags("objectName") & QScriptValue::QObjectMember));

        QVERIFY(obj.property("intProperty").isValid());
        QVERIFY(obj.propertyFlags("intProperty") & QScriptValue::QObjectMember);

        QVERIFY(!obj.property("child").isValid());
    }

    delete child;
}

Q_DECLARE_METATYPE(QWidget*)
Q_DECLARE_METATYPE(QPushButton*)

void tst_QScriptExtQObject::prototypes()
{
    QScriptEngine eng;
    QScriptValue widgetProto = eng.newQObject(new QWidget(), QScriptEngine::ScriptOwnership);
    eng.setDefaultPrototype(qMetaTypeId<QWidget*>(), widgetProto);
    QPushButton *pbp = new QPushButton();
    QScriptValue buttonProto = eng.newQObject(pbp, QScriptEngine::ScriptOwnership);
    buttonProto.setPrototype(widgetProto);
    eng.setDefaultPrototype(qMetaTypeId<QPushButton*>(), buttonProto);
    QPushButton *pb = new QPushButton();
    QScriptValue button = eng.newQObject(pb, QScriptEngine::ScriptOwnership);
    QVERIFY(button.prototype().strictlyEquals(buttonProto));

    buttonProto.setProperty("text", QScriptValue(&eng, "prototype button"));
    QCOMPARE(pbp->text(), QLatin1String("prototype button"));
    button.setProperty("text", QScriptValue(&eng, "not the prototype button"));
    QCOMPARE(pb->text(), QLatin1String("not the prototype button"));
    QCOMPARE(pbp->text(), QLatin1String("prototype button"));

    buttonProto.setProperty("objectName", QScriptValue(&eng, "prototype button"));
    QCOMPARE(pbp->objectName(), QLatin1String("prototype button"));
    button.setProperty("objectName", QScriptValue(&eng, "not the prototype button"));
    QCOMPARE(pb->objectName(), QLatin1String("not the prototype button"));
    QCOMPARE(pbp->objectName(), QLatin1String("prototype button"));
}

void tst_QScriptExtQObject::objectDeleted()
{
    QScriptEngine eng;
    MyQObject *qobj = new MyQObject();
    QScriptValue v = eng.newQObject(qobj);
    v.setProperty("objectName", QScriptValue(&eng, "foo"));
    QCOMPARE(qobj->objectName(), QLatin1String("foo"));
    v.setProperty("intProperty", QScriptValue(&eng, 123));
    QCOMPARE(qobj->intProperty(), 123);
    qobj->resetQtFunctionInvoked();
    QScriptValue invokable = v.property("myInvokable");
    invokable.call(v);
    QCOMPARE(qobj->qtFunctionInvoked(), 0);

    // now delete the object
    delete qobj;

    // the documented behavior is: isQObject() should still return true,
    // but toQObject() should return 0
    QVERIFY(v.isQObject());
    QCOMPARE(v.toQObject(), (QObject *)0);

    // any attempt to access properties of the object should result in an exception
    {
        QScriptValue ret = v.property("objectName");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("Error: cannot access member `objectName' of deleted QObject"));
    }
    {
        eng.evaluate("Object");
        QVERIFY(!eng.hasUncaughtException());
        v.setProperty("objectName", QScriptValue(&eng, "foo"));
        QVERIFY(eng.hasUncaughtException());
        QVERIFY(eng.uncaughtException().isError());
        QCOMPARE(eng.uncaughtException().toString(), QLatin1String("Error: cannot access member `objectName' of deleted QObject"));
    }

    {
        QScriptValue ret = v.call();
        QVERIFY(!ret.isValid());
    }

    // myInvokableWithIntArg is not stored in member table (since we've not accessed it)
    {
        QScriptValue ret = v.property("myInvokableWithIntArg");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("Error: cannot access member `myInvokableWithIntArg' of deleted QObject"));
    }

    // Meta-method wrappers are still valid, but throw error when called
    QVERIFY(invokable.isFunction());
    {
        QScriptValue ret = invokable.call(v);
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QString::fromLatin1("Error: cannot call function of deleted QObject"));
    }

    // access from script
    eng.globalObject().setProperty("o", v);
    {
        QScriptValue ret = eng.evaluate("o()");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("TypeError: Result of expression 'o' [] is not a function."));
    }
    {
        QScriptValue ret = eng.evaluate("o.objectName");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("Error: cannot access member `objectName' of deleted QObject"));
    }
    {
        QScriptValue ret = eng.evaluate("o.myInvokable()");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("Error: cannot access member `myInvokable' of deleted QObject"));
    }
    {
        QScriptValue ret = eng.evaluate("o.myInvokableWithIntArg(10)");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QLatin1String("Error: cannot access member `myInvokableWithIntArg' of deleted QObject"));
    }
}

void tst_QScriptExtQObject::connectToDestroyedSignal()
{
    // ### the following test currently depends on signal emission order
#if 0
    {
        // case 1: deleted when the engine is not doing GC
        QScriptEngine eng;
        QObject *obj = new QObject();
        eng.globalObject().setProperty("o", eng.newQObject(obj, QScriptEngine::QtOwnership));
        eng.evaluate("o.destroyed.connect(function() { wasDestroyed = true; })");
        eng.evaluate("wasDestroyed = false");
        delete obj;
        QVERIFY(eng.evaluate("wasDestroyed").toBoolean());
    }
    {
        // case 2: deleted when the engine is doing GC
        QScriptEngine eng;
        QObject *obj = new QObject();
        eng.globalObject().setProperty("o", eng.newQObject(obj, QScriptEngine::ScriptOwnership));
        eng.evaluate("o.destroyed.connect(function() { wasDestroyed = true; })");
        eng.evaluate("wasDestroyed = false");
        eng.evaluate("o = null");
        eng.collectGarbage();
        QVERIFY(eng.evaluate("wasDestroyed").toBoolean());
    }
    {
        // case 3: deleted when the engine is destroyed
        QScriptEngine eng;
        QObject *obj = new QObject();
        eng.globalObject().setProperty("o", eng.newQObject(obj, QScriptEngine::ScriptOwnership));
        eng.evaluate("o.destroyed.connect(function() { })");
        // the signal handler won't get called -- we don't want to crash
    }
#endif
}

void tst_QScriptExtQObject::emitAfterReceiverDeleted()
{
    for (int x = 0; x < 2; ++x) {
        MyQObject *obj = new MyQObject;
        QScriptValue scriptObj = m_engine->newQObject(obj);
        if (x == 0) {
            // Connecting from JS
            m_engine->globalObject().setProperty("obj", scriptObj);
            QVERIFY(m_engine->evaluate("myObject.mySignal.connect(obj, 'mySlot()')").isUndefined());
        } else {
            // Connecting from C++
            qScriptConnect(m_myObject, SIGNAL(mySignal()), scriptObj, scriptObj.property("mySlot"));
        }
        delete obj;
        QSignalSpy signalHandlerExceptionSpy(m_engine, SIGNAL(signalHandlerException(QScriptValue)));
        QVERIFY(!m_engine->hasUncaughtException());
        m_myObject->emitMySignal();
        QCOMPARE(signalHandlerExceptionSpy.count(), 0);
        QVERIFY(!m_engine->hasUncaughtException());
    }
}

void tst_QScriptExtQObject::inheritedSlots()
{
    QScriptEngine eng;

    QPushButton prototypeButton;
    QScriptValue scriptPrototypeButton = eng.newQObject(&prototypeButton);

    QPushButton button;
    QScriptValue scriptButton = eng.newQObject(&button, QScriptEngine::QtOwnership,
                                               QScriptEngine::ExcludeSlots);
    scriptButton.setPrototype(scriptPrototypeButton);

    QVERIFY(scriptButton.property("click").isFunction());
    QVERIFY(scriptButton.property("click").strictlyEquals(scriptPrototypeButton.property("click")));

    QSignalSpy prototypeButtonClickedSpy(&prototypeButton, SIGNAL(clicked()));
    QSignalSpy buttonClickedSpy(&button, SIGNAL(clicked()));
    scriptButton.property("click").call(scriptButton);
    QCOMPARE(buttonClickedSpy.count(), 1);
    QCOMPARE(prototypeButtonClickedSpy.count(), 0);
}

void tst_QScriptExtQObject::enumerateMetaObject()
{
    QScriptValue myClass = m_engine->newQMetaObject(m_myObject->metaObject(), m_engine->undefinedValue());

    QStringList expectedNames;
    expectedNames << "FooPolicy" << "BarPolicy" << "BazPolicy"
                  << "FooStrategy" << "BarStrategy" << "BazStrategy"
                  << "NoAbility" << "FooAbility" << "BarAbility" << "BazAbility" << "AllAbility";

    for (int x = 0; x < 2; ++x) {
        QSet<QString> actualNames;
        if (x == 0) {
            // From C++
            QScriptValueIterator it(myClass);
            while (it.hasNext()) {
                it.next();
                actualNames.insert(it.name());
            }
        } else {
            // From JS
            m_engine->globalObject().setProperty("MyClass", myClass);
            QScriptValue ret = m_engine->evaluate("a=[]; for (var p in MyClass) if (MyClass.hasOwnProperty(p)) a.push(p); a");
            QVERIFY(ret.isArray());
            QStringList strings = qscriptvalue_cast<QStringList>(ret);
            for (int i = 0; i < strings.size(); ++i)
                actualNames.insert(strings.at(i));
        }
        QCOMPARE(actualNames.size(), expectedNames.size());
        for (int i = 0; i < expectedNames.size(); ++i)
            QVERIFY(actualNames.contains(expectedNames.at(i)));
    }
}

void tst_QScriptExtQObject::nestedArrayAsSlotArgument_data()
{
    QTest::addColumn<QString>("program");
    QTest::addColumn<QVariantList>("expected");

    QTest::newRow("[[]]")
        << QString::fromLatin1("[[]]")
        << (QVariantList() << (QVariant(QVariantList())));
    QTest::newRow("[[123]]")
        << QString::fromLatin1("[[123]]")
        << (QVariantList() << (QVariant(QVariantList() << 123)));
    QTest::newRow("[[], 123]")
        << QString::fromLatin1("[[], 123]")
        << (QVariantList() << QVariant(QVariantList()) << 123);

    // Cyclic
    QTest::newRow("var a=[]; a.push(a)")
        << QString::fromLatin1("var a=[]; a.push(a); a")
        << (QVariantList() << QVariant(QVariantList()));
    QTest::newRow("var a=[]; a.push(123, a)")
        << QString::fromLatin1("var a=[]; a.push(123, a); a")
        << (QVariantList() << 123 << QVariant(QVariantList()));
    QTest::newRow("var a=[]; var b=[]; a.push(b); b.push(a)")
        << QString::fromLatin1("var a=[]; var b=[]; a.push(b); b.push(a); a")
        << (QVariantList() << QVariant(QVariantList() << QVariant(QVariantList())));
    QTest::newRow("var a=[]; var b=[]; a.push(123, b); b.push(456, a)")
        << QString::fromLatin1("var a=[]; var b=[]; a.push(123, b); b.push(456, a); a")
        << (QVariantList() << 123 << QVariant(QVariantList() << 456 << QVariant(QVariantList())));
}

void tst_QScriptExtQObject::nestedArrayAsSlotArgument()
{
    QFETCH(QString, program);
    QFETCH(QVariantList, expected);
    QScriptValue a = m_engine->evaluate(program);
    QVERIFY(!a.isError());
    QVERIFY(a.isArray());
    // Slot that takes QVariantList
    {
        QVERIFY(!m_engine->evaluate("myObject.myInvokableWithVariantListArg")
                .call(QScriptValue(), QScriptValueList() << a).isError());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 62);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0).type(), QVariant::List);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0).toList(), expected);
    }
    // Slot that takes QVariant
    {
        m_myObject->resetQtFunctionInvoked();
        QVERIFY(!m_engine->evaluate("myObject.myInvokableWithVariantArg")
                .call(QScriptValue(), QScriptValueList() << a).isError());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 15);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0).type(), QVariant::List);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0).toList(), expected);
    }
}

void tst_QScriptExtQObject::nestedObjectAsSlotArgument_data()
{
    QTest::addColumn<QString>("program");
    QTest::addColumn<QVariantMap>("expected");

    {
        QVariantMap m;
        m["a"] = QVariantMap();
        QTest::newRow("{ a:{} }")
            << QString::fromLatin1("({ a:{} })")
            << m;
    }
    {
        QVariantMap m, m2;
        m2["b"] = 10;
        m2["c"] = 20;
        m["a"] = m2;
        QTest::newRow("{ a:{b:10, c:20} }")
            << QString::fromLatin1("({ a:{b:10, c:20} })")
            << m;
    }
    {
        QVariantMap m;
        m["a"] = 10;
        m["b"] = QVariantList() << 20 << 30;
        QTest::newRow("{ a:10, b:[20, 30]}")
            << QString::fromLatin1("({ a:10, b:[20,30]})")
            << m;
    }

    // Cyclic
    {
        QVariantMap m;
        m["p"] = QVariantMap();
        QTest::newRow("var o={}; o.p=o")
            << QString::fromLatin1("var o={}; o.p=o; o")
            << m;
    }
    {
        QVariantMap m;
        m["p"] = 123;
        m["q"] = QVariantMap();
        QTest::newRow("var o={}; o.p=123; o.q=o")
            << QString::fromLatin1("var o={}; o.p=123; o.q=o; o")
            << m;
    }
}

void tst_QScriptExtQObject::nestedObjectAsSlotArgument()
{
    QFETCH(QString, program);
    QFETCH(QVariantMap, expected);
    QScriptValue o = m_engine->evaluate(program);
    QVERIFY(!o.isError());
    QVERIFY(o.isObject());
    // Slot that takes QVariantMap
    {
        QVERIFY(!m_engine->evaluate("myObject.myInvokableWithVariantMapArg")
                .call(QScriptValue(), QScriptValueList() << o).isError());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 16);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0).type(), QVariant::Map);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0).toMap(), expected);
    }
    // Slot that takes QVariant
    {
        m_myObject->resetQtFunctionInvoked();
        QVERIFY(!m_engine->evaluate("myObject.myInvokableWithVariantArg")
                .call(QScriptValue(), QScriptValueList() << o).isError());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 15);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0).type(), QVariant::Map);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0).toMap(), expected);
    }
}

// QTBUG-21760
void tst_QScriptExtQObject::propertyAccessThroughActivationObject()
{
    QScriptContext *ctx = m_engine->pushContext();
    ctx->setActivationObject(m_engine->newQObject(m_myObject));

    QVERIFY(m_engine->evaluate("intProperty").isNumber());
    QVERIFY(m_engine->evaluate("mySlot()").isUndefined());
    QVERIFY(m_engine->evaluate("mySlotWithStringArg('test')").isUndefined());

    QVERIFY(m_engine->evaluate("dynamicProperty").isError());
    m_myObject->setProperty("dynamicProperty", 123);
    QCOMPARE(m_engine->evaluate("dynamicProperty").toInt32(), 123);

    m_engine->popContext();
}

class SignalEmitterThread : public QThread
{
public:
    SignalEmitterThread(MyQObject *sender)
        : m_sender(sender)
    { }

    void run()
    { m_sender->emitMySignal(); }

private:
    MyQObject *m_sender;
};

// QTBUG-26261
void tst_QScriptExtQObject::connectionRemovedAfterQueuedCall()
{
    QVERIFY(m_engine->evaluate("var pass = true; function onMySignal() { pass = false; }").isUndefined());
    QVERIFY(m_engine->evaluate("myObject.mySignal.connect(onMySignal)").isUndefined());

    SignalEmitterThread thread(m_myObject);
    QVERIFY(m_myObject->thread() != &thread); // Premise for queued call
    thread.start();
    QVERIFY(thread.wait());

    QVERIFY(m_engine->evaluate("myObject.mySignal.disconnect(onMySignal)").isUndefined());
    // Should not crash
    QCoreApplication::processEvents();

    QVERIFY(m_engine->evaluate("pass").toBool());
}

// QTBUG-26590
void tst_QScriptExtQObject::collectQObjectWithClosureSlot()
{
    QScriptEngine eng;
    QScriptValue fun = eng.evaluate("(function(obj) {\n"
        "    obj.mySignal.connect(function() { obj.mySlot(); });\n"
        "})");
    QVERIFY(fun.isFunction());

    QPointer<MyQObject> obj = new MyQObject;
    {
        QScriptValue wrapper = eng.newQObject(obj, QScriptEngine::ScriptOwnership);
        QVERIFY(fun.call(QScriptValue(), QScriptValueList() << wrapper).isUndefined());
    }
    QVERIFY(obj != 0);
    QCOMPARE(obj->qtFunctionInvoked(), -1);
    obj->emitMySignal();
    QCOMPARE(obj->qtFunctionInvoked(), 20);

    collectGarbage_helper(eng);
    // The closure that's connected to obj's signal has the only JS reference
    // to obj, and the closure is only referenced by the connection. Hence, obj
    // should have been collected.
    if (obj != 0)
        QEXPECT_FAIL("", "Test can fail because the garbage collector is conservative", Continue);
    QVERIFY(obj == 0);
}

void tst_QScriptExtQObject::collectQObjectWithClosureSlot2()
{
    QScriptEngine eng;
    QScriptValue fun = eng.evaluate("(function(obj1, obj2) {\n"
        "    obj2.mySignal.connect(function() { obj1.mySlot(); });\n"
        "    obj1.mySignal.connect(function() { obj2.mySlot(); });\n"
        "})");
    QVERIFY(fun.isFunction());

    QPointer<MyQObject> obj1 = new MyQObject;
    QScriptValue wrapper1 = eng.newQObject(obj1, QScriptEngine::ScriptOwnership);
    QPointer<MyQObject> obj2 = new MyQObject;
    {
        QScriptValue wrapper2 = eng.newQObject(obj2, QScriptEngine::ScriptOwnership);
        QVERIFY(fun.call(QScriptValue(), QScriptValueList() << wrapper1 << wrapper2).isUndefined());
    }
    QVERIFY(obj1 != 0);
    QVERIFY(obj2 != 0);

    collectGarbage_helper(eng);
    // obj1 is referenced by a QScriptValue, so it (and its connections) should not be collected.
    QVERIFY(obj1 != 0);
    // obj2 is referenced from the closure that's connected to obj1's signal, so it
    // (and its connections) should not be collected.
    QVERIFY(obj2 != 0);

    QCOMPARE(obj2->qtFunctionInvoked(), -1);
    obj1->emitMySignal();
    QCOMPARE(obj2->qtFunctionInvoked(), 20);

    QCOMPARE(obj1->qtFunctionInvoked(), -1);
    obj2->emitMySignal();
    QCOMPARE(obj1->qtFunctionInvoked(), 20);
}

QTEST_MAIN(tst_QScriptExtQObject)
#include "tst_qscriptextqobject.moc"
