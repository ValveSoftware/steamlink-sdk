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
#include <QtGui/qvector3d.h>
#include <QtGui/QFont>
#include <QtGui/QPixmap>
#include <QtCore/qdatetime.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>
#include <QtQml/qjsvalue.h>
#include <QtQml/qqmlscriptstring.h>
#include <QtQml/qqmlcomponent.h>

#include <private/qqmlengine_p.h>
#include <private/qv8engine_p.h>
#include <private/qv4qobjectwrapper_p.h>

class MyQmlAttachedObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int value READ value CONSTANT)
    Q_PROPERTY(int value2 READ value2 WRITE setValue2 NOTIFY value2Changed)
public:
    MyQmlAttachedObject(QObject *parent) : QObject(parent), m_value2(0) {}

    int value() const { return 19; }
    int value2() const { return m_value2; }
    void setValue2(int v) { if (m_value2 == v) return; m_value2 = v; emit value2Changed(); }

    void emitMySignal() { emit mySignal(); }

signals:
    void value2Changed();
    void mySignal();

private:
    int m_value2;
};

class MyEnumContainer : public QObject
{
    Q_OBJECT
    Q_ENUMS(RelatedEnum)

public:
    enum RelatedEnum { RelatedInvalid = -1, RelatedValue = 42, MultiplyDefined = 666 };
};

class MyQmlObject : public QObject
{
    Q_OBJECT
    Q_ENUMS(MyEnum)
    Q_ENUMS(MyEnum2)
    Q_ENUMS(MyEnum3)
    Q_ENUMS(MyEnumContainer::RelatedEnum)
    Q_PROPERTY(int deleteOnSet READ deleteOnSet WRITE setDeleteOnSet)
    Q_PROPERTY(bool trueProperty READ trueProperty CONSTANT)
    Q_PROPERTY(bool falseProperty READ falseProperty CONSTANT)
    Q_PROPERTY(int value READ value WRITE setValue)
    Q_PROPERTY(int console READ console CONSTANT)
    Q_PROPERTY(QString stringProperty READ stringProperty WRITE setStringProperty NOTIFY stringChanged)
    Q_PROPERTY(QUrl urlProperty READ urlProperty WRITE setUrlProperty NOTIFY urlChanged)
    Q_PROPERTY(QObject *objectProperty READ objectProperty WRITE setObjectProperty NOTIFY objectChanged)
    Q_PROPERTY(QQmlListProperty<QObject> objectListProperty READ objectListProperty CONSTANT)
    Q_PROPERTY(int resettableProperty READ resettableProperty WRITE setResettableProperty RESET resetProperty)
    Q_PROPERTY(QRegExp regExp READ regExp WRITE setRegExp)
    Q_PROPERTY(int nonscriptable READ nonscriptable WRITE setNonscriptable SCRIPTABLE false)
    Q_PROPERTY(int intProperty READ intProperty WRITE setIntProperty NOTIFY intChanged)
    Q_PROPERTY(QJSValue qjsvalue READ qjsvalue WRITE setQJSValue NOTIFY qjsvalueChanged)
    Q_PROPERTY(QJSValue qjsvalueWithReset READ qjsvalue WRITE setQJSValue RESET resetQJSValue NOTIFY qjsvalueChanged)
    Q_PROPERTY(MyEnum enumProperty READ enumProperty WRITE setEnumProperty)
    Q_PROPERTY(MyEnumContainer::RelatedEnum relatedEnumProperty READ relatedEnumProperty WRITE setRelatedEnumProperty)
    Q_PROPERTY(MyEnumContainer::RelatedEnum unrelatedEnumProperty READ unrelatedEnumProperty WRITE setUnrelatedEnumProperty)
    Q_PROPERTY(MyEnum qtEnumProperty READ qtEnumProperty WRITE setQtEnumProperty)

public:
    MyQmlObject(): myinvokableObject(0), m_methodCalled(false), m_methodIntCalled(false), m_object(0), m_value(0), m_resetProperty(13), m_intProperty(0), m_buttons(0) {}

    enum MyEnum { EnumValue1 = 0, EnumValue2 = 1 };
    enum MyEnum2 { EnumValue3 = 2, EnumValue4 = 3, EnumValue5 = -1 };
    enum MyEnum3 { MultiplyDefined = 333 };

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

    QUrl urlProperty() const { return m_url; }
    void setUrlProperty(const QUrl &url)
    {
        if (url == m_url)
            return;
        m_url = url;
        emit urlChanged();
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
    struct MyOtherType {
        int value;
    };
    QVariant variant() const { return m_variant; }
    QJSValue qjsvalue() const { return m_qjsvalue; }
    void setQJSValue(const QJSValue &value) { m_qjsvalue = value; emit qjsvalueChanged(); }
    void resetQJSValue() { m_qjsvalue = QJSValue(QLatin1String("Reset!")); emit qjsvalueChanged(); }

    Qt::MouseButtons buttons() const { return m_buttons; }

    int intProperty() const { return m_intProperty; }
    void setIntProperty(int i) { m_intProperty = i; emit intChanged(); }

    Q_INVOKABLE MyEnum2 getEnumValue() const { return EnumValue4; }

    MyEnum enumPropertyValue;
    MyEnum enumProperty() const {
        return enumPropertyValue;
    }
    void setEnumProperty(MyEnum v) {
        enumPropertyValue = v;
    }

    MyEnumContainer::RelatedEnum relatedEnumPropertyValue;
    MyEnumContainer::RelatedEnum relatedEnumProperty() const {
        return relatedEnumPropertyValue;
    }
    void setRelatedEnumProperty(MyEnumContainer::RelatedEnum v) {
        relatedEnumPropertyValue = v;
    }

    MyEnumContainer::RelatedEnum unrelatedEnumPropertyValue;
    MyEnumContainer::RelatedEnum unrelatedEnumProperty() const {
        return unrelatedEnumPropertyValue;
    }
    void setUnrelatedEnumProperty(MyEnumContainer::RelatedEnum v) {
        unrelatedEnumPropertyValue = v;
    }

    MyEnum qtEnumPropertyValue;
    MyEnum qtEnumProperty() const {
        return qtEnumPropertyValue;
    }
    void setQtEnumProperty(MyEnum v) {
        qtEnumPropertyValue = v;
    }

signals:
    void basicSignal();
    void argumentSignal(int a, QString b, qreal c, MyEnum2 d, Qt::MouseButtons e);
    void unnamedArgumentSignal(int a, qreal, QString c);
    void stringChanged();
    void urlChanged();
    void objectChanged();
    void anotherBasicSignal();
    void thirdBasicSignal();
    void signalWithUnknownType(const MyQmlObject::MyType &arg);
    void signalWithCompletelyUnknownType(const MyQmlObject::MyOtherType &arg);
    void signalWithVariant(const QVariant &arg);
    void signalWithQJSValue(const QJSValue &arg);
    void signalWithGlobalName(int parseInt);
    void intChanged();
    void qjsvalueChanged();

public slots:
    void deleteMe() { delete this; }
    void methodNoArgs() { m_methodCalled = true; }
    void method(int a) { if(a == 163) m_methodIntCalled = true; }
    void setString(const QString &s) { m_string = s; }
    void myinvokable(MyQmlObject *o) { myinvokableObject = o; }
    void variantMethod(const QVariant &v) { m_variant = v; }
    void qjsvalueMethod(const QJSValue &v) { m_qjsvalue = v; }
    void v8function(QQmlV4Function*);
    void registeredFlagMethod(Qt::MouseButtons v) { m_buttons = v; }
    QString slotWithReturnValue(const QString &arg) { return arg; }

private:
    friend class tst_qqmlecmascript;
    bool m_methodCalled;
    bool m_methodIntCalled;

    QObject *m_object;
    QString m_string;
    QUrl m_url;
    QList<QObject *> m_objectQList;
    int m_value;
    int m_resetProperty;
    QRegExp m_regExp;
    QVariant m_variant;
    QJSValue m_qjsvalue;
    int m_intProperty;
    Qt::MouseButtons m_buttons;
};
Q_DECLARE_METATYPE(QQmlListProperty<MyQmlObject>)

QML_DECLARE_TYPEINFO(MyQmlObject, QML_HAS_ATTACHED_PROPERTIES)

class MyInheritedQmlObject : public MyQmlObject
{
    Q_OBJECT
    Q_PROPERTY(MyInheritedQmlObject *myInheritedQmlObjectProperty READ myInheritedQmlObject WRITE setMyInheritedQmlObject)
    Q_PROPERTY(MyQmlObject *myQmlObjectProperty READ myQmlObject WRITE setMyQmlObject)
    Q_PROPERTY(QObject *qobjectProperty READ qobject WRITE setQObject)
public:
    MyInheritedQmlObject() : m_myInheritedQmlObject(0), m_myQmlObject(0), m_qobject(0) {}

    MyInheritedQmlObject *myInheritedQmlObject() const { return m_myInheritedQmlObject; }
    void setMyInheritedQmlObject(MyInheritedQmlObject * o) { m_myInheritedQmlObject = o; }

    MyQmlObject *myQmlObject() const { return m_myQmlObject; }
    void setMyQmlObject(MyQmlObject * o) { m_myQmlObject = o; }

    QObject *qobject() const { return m_qobject; }
    void setQObject(QObject * o) { m_qobject = o; }

    Q_INVOKABLE bool isItYouQObject(QObject *o);
    Q_INVOKABLE bool isItYouMyQmlObject(MyQmlObject *o);
    Q_INVOKABLE bool isItYouMyInheritedQmlObject(MyInheritedQmlObject *o);
private:
    MyInheritedQmlObject *m_myInheritedQmlObject;
    MyQmlObject *m_myQmlObject;
    QObject *m_qobject;
};
QML_DECLARE_TYPE(MyInheritedQmlObject)

class MyQmlContainer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<MyQmlObject> children READ children CONSTANT)
public:
    MyQmlContainer() {}

    QQmlListProperty<MyQmlObject> children() { return QQmlListProperty<MyQmlObject>(this, m_children); }

private:
    QList<MyQmlObject*> m_children;
};


class MyExpression : public QQmlExpression
{
    Q_OBJECT
public:
    MyExpression(QQmlContext *ctxt, const QString &expr)
        : QQmlExpression(ctxt, 0, expr), changed(false)
    {
        QObject::connect(this, SIGNAL(valueChanged()), this, SLOT(expressionValueChanged()));
        setNotifyOnValueChanged(true);
    }

    bool changed;

public slots:
    void expressionValueChanged() {
        changed = true;
    }
};


class MyDefaultObject1 : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int horseLegs READ horseLegs CONSTANT)
    Q_PROPERTY(int antLegs READ antLegs CONSTANT)
    Q_PROPERTY(int emuLegs READ emuLegs CONSTANT)
public:
    int horseLegs() const { return 4; }
    int antLegs() const { return 6; }
    int emuLegs() const { return 2; }
};

class MyDefaultObject3 : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int antLegs READ antLegs CONSTANT)
    Q_PROPERTY(int humanLegs READ humanLegs CONSTANT)
public:
    int antLegs() const { return 7; } // Mutant
    int humanLegs() const { return 2; }
    int millipedeLegs() const { return 1000; }
};

class MyDeferredObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(QObject *objectProperty READ objectProperty WRITE setObjectProperty)
    Q_PROPERTY(QObject *objectProperty2 READ objectProperty2 WRITE setObjectProperty2)
    Q_CLASSINFO("DeferredPropertyNames", "value,objectProperty,objectProperty2")

public:
    MyDeferredObject() : m_value(0), m_object(0), m_object2(0) {}

    int value() const { return m_value; }
    void setValue(int v) { m_value = v; emit valueChanged(); }

    QObject *objectProperty() const { return m_object; }
    void setObjectProperty(QObject *obj) { m_object = obj; }

    QObject *objectProperty2() const { return m_object2; }
    void setObjectProperty2(QObject *obj) { m_object2 = obj; }

signals:
    void valueChanged();

private:
    int m_value;
    QObject *m_object;
    QObject *m_object2;
};

class MyVeryDeferredObject : public QObject
{
    Q_OBJECT
    //For inDestruction test
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(QObject *objectProperty READ objectProperty WRITE setObjectProperty)
    Q_CLASSINFO("DeferredPropertyNames", "objectProperty")

public:
    MyVeryDeferredObject() : m_value(0), m_object(0) {}
    ~MyVeryDeferredObject() {
        qmlExecuteDeferred(this); //Not a realistic case, see QTBUG-33112 to see how this could happen in practice
    }

    int value() const { return m_value; }
    void setValue(int v) { m_value = v; emit valueChanged(); }

    QObject *objectProperty() const { return m_object; }
    void setObjectProperty(QObject *obj) { m_object = obj; }

signals:
    void valueChanged();

private:
    int m_value;
    QObject *m_object;
};

class MyBaseExtendedObject : public QObject
{
Q_OBJECT
Q_PROPERTY(int baseProperty READ baseProperty WRITE setBaseProperty)
public:
    MyBaseExtendedObject() : m_value(0) {}

    int baseProperty() const { return m_value; }
    void setBaseProperty(int v) { m_value = v; }

private:
    int m_value;
};

class MyExtendedObject : public MyBaseExtendedObject
{
Q_OBJECT
Q_PROPERTY(int coreProperty READ coreProperty WRITE setCoreProperty)
public:
    MyExtendedObject() : m_value(0) {}

    int coreProperty() const { return m_value; }
    void setCoreProperty(int v) { m_value = v; }

private:
    int m_value;
};

class MyTypeObject : public QObject
{
    Q_OBJECT
    Q_ENUMS(MyEnum)
    Q_ENUMS(MyEnumContainer::RelatedEnum)
    Q_FLAGS(MyFlags)

    Q_PROPERTY(QString id READ id WRITE setId)
    Q_PROPERTY(QObject *objectProperty READ objectProperty WRITE setObjectProperty)
    Q_PROPERTY(QQmlComponent *componentProperty READ componentProperty WRITE setComponentProperty)
    Q_PROPERTY(MyFlags flagProperty READ flagProperty WRITE setFlagProperty)
    Q_PROPERTY(MyEnum enumProperty READ enumProperty WRITE setEnumProperty)
    Q_PROPERTY(MyEnumContainer::RelatedEnum relatedEnumProperty READ relatedEnumProperty WRITE setRelatedEnumProperty)
    Q_PROPERTY(QString stringProperty READ stringProperty WRITE setStringProperty)
    Q_PROPERTY(uint uintProperty READ uintProperty WRITE setUintProperty)
    Q_PROPERTY(int intProperty READ intProperty WRITE setIntProperty)
    Q_PROPERTY(qreal realProperty READ realProperty WRITE setRealProperty)
    Q_PROPERTY(double doubleProperty READ doubleProperty WRITE setDoubleProperty)
    Q_PROPERTY(float floatProperty READ floatProperty WRITE setFloatProperty)
    Q_PROPERTY(QColor colorProperty READ colorProperty WRITE setColorProperty)
    Q_PROPERTY(QDate dateProperty READ dateProperty WRITE setDateProperty)
    Q_PROPERTY(QTime timeProperty READ timeProperty WRITE setTimeProperty)
    Q_PROPERTY(QDateTime dateTimeProperty READ dateTimeProperty WRITE setDateTimeProperty)
    Q_PROPERTY(QDateTime dateTimeProperty2 READ dateTimeProperty2 WRITE setDateTimeProperty2)
    Q_PROPERTY(QPoint pointProperty READ pointProperty WRITE setPointProperty)
    Q_PROPERTY(QPointF pointFProperty READ pointFProperty WRITE setPointFProperty)
    Q_PROPERTY(QSize sizeProperty READ sizeProperty WRITE setSizeProperty)
    Q_PROPERTY(QSizeF sizeFProperty READ sizeFProperty WRITE setSizeFProperty)
    Q_PROPERTY(QRect rectProperty READ rectProperty WRITE setRectProperty NOTIFY rectPropertyChanged)
    Q_PROPERTY(QRect rectProperty2 READ rectProperty2 WRITE setRectProperty2)
    Q_PROPERTY(QRectF rectFProperty READ rectFProperty WRITE setRectFProperty)
    Q_PROPERTY(bool boolProperty READ boolProperty WRITE setBoolProperty)
    Q_PROPERTY(QVariant variantProperty READ variantProperty WRITE setVariantProperty)
    Q_PROPERTY(QVector3D vectorProperty READ vectorProperty WRITE setVectorProperty)
    Q_PROPERTY(QUrl urlProperty READ urlProperty WRITE setUrlProperty)

    Q_PROPERTY(QQmlScriptString scriptProperty READ scriptProperty WRITE setScriptProperty)

public:
    MyTypeObject()
        : objectPropertyValue(0), componentPropertyValue(0) {}

    QString idValue;
    QString id() const {
        return idValue;
    }
    void setId(const QString &v) {
        idValue = v;
    }

    QObject *objectPropertyValue;
    QObject *objectProperty() const {
        return objectPropertyValue;
    }
    void setObjectProperty(QObject *v) {
        objectPropertyValue = v;
    }

    QQmlComponent *componentPropertyValue;
    QQmlComponent *componentProperty() const {
        return componentPropertyValue;
    }
    void setComponentProperty(QQmlComponent *v) {
        componentPropertyValue = v;
    }

    enum MyFlag { FlagVal1 = 0x01, FlagVal2 = 0x02, FlagVal3 = 0x04 };
    Q_DECLARE_FLAGS(MyFlags, MyFlag)
    MyFlags flagPropertyValue;
    MyFlags flagProperty() const {
        return flagPropertyValue;
    }
    void setFlagProperty(MyFlags v) {
        flagPropertyValue = v;
    }

    enum MyEnum { EnumVal1, EnumVal2 };
    MyEnum enumPropertyValue;
    MyEnum enumProperty() const {
        return enumPropertyValue;
    }
    void setEnumProperty(MyEnum v) {
        enumPropertyValue = v;
    }

    MyEnumContainer::RelatedEnum relatedEnumPropertyValue;
    MyEnumContainer::RelatedEnum relatedEnumProperty() const {
        return relatedEnumPropertyValue;
    }
    void setRelatedEnumProperty(MyEnumContainer::RelatedEnum v) {
        relatedEnumPropertyValue = v;
    }

    QString stringPropertyValue;
    QString stringProperty() const {
       return stringPropertyValue;
    }
    void setStringProperty(const QString &v) {
        stringPropertyValue = v;
    }

    uint uintPropertyValue;
    uint uintProperty() const {
       return uintPropertyValue;
    }
    void setUintProperty(const uint &v) {
        uintPropertyValue = v;
    }

    int intPropertyValue;
    int intProperty() const {
       return intPropertyValue;
    }
    void setIntProperty(const int &v) {
        intPropertyValue = v;
    }

    qreal realPropertyValue;
    qreal realProperty() const {
       return realPropertyValue;
    }
    void setRealProperty(const qreal &v) {
        realPropertyValue = v;
    }

    double doublePropertyValue;
    double doubleProperty() const {
       return doublePropertyValue;
    }
    void setDoubleProperty(const double &v) {
        doublePropertyValue = v;
    }

    float floatPropertyValue;
    float floatProperty() const {
       return floatPropertyValue;
    }
    void setFloatProperty(const float &v) {
        floatPropertyValue = v;
    }

    QColor colorPropertyValue;
    QColor colorProperty() const {
       return colorPropertyValue;
    }
    void setColorProperty(const QColor &v) {
        colorPropertyValue = v;
    }

    QDate datePropertyValue;
    QDate dateProperty() const {
       return datePropertyValue;
    }
    void setDateProperty(const QDate &v) {
        datePropertyValue = v;
    }

    QTime timePropertyValue;
    QTime timeProperty() const {
       return timePropertyValue;
    }
    void setTimeProperty(const QTime &v) {
        timePropertyValue = v;
    }

    QDateTime dateTimePropertyValue;
    QDateTime dateTimeProperty() const {
       return dateTimePropertyValue;
    }
    void setDateTimeProperty(const QDateTime &v) {
        dateTimePropertyValue = v;
    }

    QDateTime dateTimePropertyValue2;
    QDateTime dateTimeProperty2() const {
       return dateTimePropertyValue2;
    }
    void setDateTimeProperty2(const QDateTime &v) {
        dateTimePropertyValue2 = v;
    }

    QPoint pointPropertyValue;
    QPoint pointProperty() const {
       return pointPropertyValue;
    }
    void setPointProperty(const QPoint &v) {
        pointPropertyValue = v;
    }

    QPointF pointFPropertyValue;
    QPointF pointFProperty() const {
       return pointFPropertyValue;
    }
    void setPointFProperty(const QPointF &v) {
        pointFPropertyValue = v;
    }

    QSize sizePropertyValue;
    QSize sizeProperty() const {
       return sizePropertyValue;
    }
    void setSizeProperty(const QSize &v) {
        sizePropertyValue = v;
    }

    QSizeF sizeFPropertyValue;
    QSizeF sizeFProperty() const {
       return sizeFPropertyValue;
    }
    void setSizeFProperty(const QSizeF &v) {
        sizeFPropertyValue = v;
    }

    QRect rectPropertyValue;
    QRect rectProperty() const {
       return rectPropertyValue;
    }
    void setRectProperty(const QRect &v) {
        rectPropertyValue = v;
        emit rectPropertyChanged();
    }

    QRect rectPropertyValue2;
    QRect rectProperty2() const {
       return rectPropertyValue2;
    }
    void setRectProperty2(const QRect &v) {
        rectPropertyValue2 = v;
    }

    QRectF rectFPropertyValue;
    QRectF rectFProperty() const {
       return rectFPropertyValue;
    }
    void setRectFProperty(const QRectF &v) {
        rectFPropertyValue = v;
    }

    bool boolPropertyValue;
    bool boolProperty() const {
       return boolPropertyValue;
    }
    void setBoolProperty(const bool &v) {
        boolPropertyValue = v;
    }

    QVariant variantPropertyValue;
    QVariant variantProperty() const {
       return variantPropertyValue;
    }
    void setVariantProperty(const QVariant &v) {
        variantPropertyValue = v;
    }

    QVector3D vectorPropertyValue;
    QVector3D vectorProperty() const {
        return vectorPropertyValue;
    }
    void setVectorProperty(const QVector3D &v) {
        vectorPropertyValue = v;
    }

    QUrl urlPropertyValue;
    QUrl urlProperty() const {
        return urlPropertyValue;
    }
    void setUrlProperty(const QUrl &v) {
        urlPropertyValue = v;
    }

    QQmlScriptString scriptPropertyValue;
    QQmlScriptString scriptProperty() const {
        return scriptPropertyValue;
    }
    void setScriptProperty(const QQmlScriptString &v) {
        scriptPropertyValue = v;
    }

    void doAction() { emit action(); }
signals:
    void action();
    void rectPropertyChanged();
};
Q_DECLARE_OPERATORS_FOR_FLAGS(MyTypeObject::MyFlags)

class MyDerivedObject : public MyTypeObject
{
    Q_OBJECT
public:
    Q_INVOKABLE bool intProperty() const {
        return true;
    }
};

class MyInvokableBaseObject : public QObject
{
    Q_OBJECT
public:
    inline ~MyInvokableBaseObject() = 0;

    Q_INVOKABLE inline void method_inherited(int a);
    Q_INVOKABLE inline void method_overload();
};

struct NonRegisteredType
{

};

class MyInvokableObject : public MyInvokableBaseObject
{
    Q_OBJECT
    Q_ENUMS(TestEnum)
public:
    enum TestEnum { EnumValue1, EnumValue2 };
    MyInvokableObject() { reset(); }

    int invoked() const { return m_invoked; }
    bool error() const { return m_invokedError; }
    const QVariantList &actuals() const { return m_actuals; }
    void reset() { m_invoked = -1; m_invokedError = false; m_actuals.clear(); }

    Q_INVOKABLE QPointF method_get_QPointF() { return QPointF(99.3, -10.2); }
    Q_INVOKABLE QPoint method_get_QPoint() { return QPoint(9, 12); }

    Q_INVOKABLE void method_NoArgs() { invoke(0); }
    Q_INVOKABLE int method_NoArgs_int() { invoke(1); return 6; }
    Q_INVOKABLE qreal method_NoArgs_real() { invoke(2); return 19.75; }
    Q_INVOKABLE QPointF method_NoArgs_QPointF() { invoke(3); return QPointF(123, 4.5); }
    Q_INVOKABLE QObject *method_NoArgs_QObject() { invoke(4); return this; }
    Q_INVOKABLE MyInvokableObject *method_NoArgs_unknown() { invoke(5); return this; }
    Q_INVOKABLE QJSValue method_NoArgs_QScriptValue() { invoke(6); return QJSValue("Hello world"); }
    Q_INVOKABLE QVariant method_NoArgs_QVariant() { invoke(7); return QVariant("QML rocks"); }

    Q_INVOKABLE void method_int(int a) { invoke(8); m_actuals << a; }
    Q_INVOKABLE void method_intint(int a, int b) { invoke(9); m_actuals << a << b; }
    Q_INVOKABLE void method_real(qreal a) { invoke(10); m_actuals << a; }
    Q_INVOKABLE void method_QString(QString a) { invoke(11); m_actuals << a; }
    Q_INVOKABLE void method_QPointF(QPointF a) { invoke(12); m_actuals << a; }
    Q_INVOKABLE void method_QObject(QObject *a) { invoke(13); m_actuals << qVariantFromValue(a); }
    Q_INVOKABLE void method_QScriptValue(QJSValue a) { invoke(14); m_actuals << qVariantFromValue(a); }
    Q_INVOKABLE void method_intQScriptValue(int a, QJSValue b) { invoke(15); m_actuals << a << qVariantFromValue(b); }
    Q_INVOKABLE void method_QByteArray(QByteArray value) { invoke(29); m_actuals << value; }
    Q_INVOKABLE QJSValue method_intQJSValue(int a, QJSValue b) { invoke(30); m_actuals << a << qVariantFromValue(b); return b.call(); }
    Q_INVOKABLE QJSValue method_intQJSValue(int a, int b) { m_actuals << a << b; return QJSValue();} // Should never be called.

    Q_INVOKABLE void method_overload(int a) { invoke(16); m_actuals << a; }
    Q_INVOKABLE void method_overload(int a, int b) { invoke(17); m_actuals << a << b; }
    Q_INVOKABLE void method_overload(QString a) { invoke(18); m_actuals << a; }

    Q_INVOKABLE void method_with_enum(TestEnum e) { invoke(19); m_actuals << (int)e; }

    Q_INVOKABLE int method_default(int a, int b = 19) { invoke(20); m_actuals << a << b; return b; }

    Q_INVOKABLE void method_QVariant(QVariant a, QVariant b = QVariant()) { invoke(21); m_actuals << a << b; }

    Q_INVOKABLE void method_QJsonObject(const QJsonObject &a) { invoke(22); m_actuals << QVariant::fromValue(a); }
    Q_INVOKABLE void method_QJsonArray(const QJsonArray &a) { invoke(23); m_actuals << QVariant::fromValue(a); }
    Q_INVOKABLE void method_QJsonValue(const QJsonValue &a) { invoke(24); m_actuals << QVariant::fromValue(a); }

    Q_INVOKABLE void method_overload(const QJsonObject &a) { invoke(25); m_actuals << QVariant::fromValue(a); }
    Q_INVOKABLE void method_overload(const QJsonArray &a) { invoke(26); m_actuals << QVariant::fromValue(a); }
    Q_INVOKABLE void method_overload(const QJsonValue &a) { invoke(27); m_actuals << QVariant::fromValue(a); }

    Q_INVOKABLE void method_unknown(NonRegisteredType) { invoke(28); }

private:
    friend class MyInvokableBaseObject;
    void invoke(int idx) { if (m_invoked != -1) m_invokedError = true; m_invoked = idx;}
    int m_invoked;
    bool m_invokedError;
    QVariantList m_actuals;
};

MyInvokableBaseObject::~MyInvokableBaseObject() {}

void MyInvokableBaseObject::method_inherited(int a)
{
    static_cast<MyInvokableObject *>(this)->invoke(-3);
    static_cast<MyInvokableObject *>(this)->m_actuals << a;
}

// This is a hidden overload of the MyInvokableObject::method_overload() method
void MyInvokableBaseObject::method_overload()
{
    static_cast<MyInvokableObject *>(this)->invoke(-2);
}

class NumberAssignment : public QObject
{
    Q_OBJECT
public:
    Q_PROPERTY(qreal test1 READ test1 WRITE setTest1)
    qreal _test1;
    qreal test1() const { return _test1; }
    void setTest1(qreal v) { _test1 = v; }

    Q_PROPERTY(qreal test2 READ test2 WRITE setTest2)
    qreal _test2;
    qreal test2() const { return _test2; }
    void setTest2(qreal v) { _test2 = v; }

    Q_PROPERTY(qreal test3 READ test3 WRITE setTest3)
    qreal _test3;
    qreal test3() const { return _test3; }
    void setTest3(qreal v) { _test3 = v; }

    Q_PROPERTY(qreal test4 READ test4 WRITE setTest4)
    qreal _test4;
    qreal test4() const { return _test4; }
    void setTest4(qreal v) { _test4 = v; }

    Q_PROPERTY(int test5 READ test5 WRITE setTest5)
    int _test5;
    int test5() const { return _test5; }
    void setTest5(int v) { _test5 = v; }

    Q_PROPERTY(int test6 READ test6 WRITE setTest6)
    int _test6;
    int test6() const { return _test6; }
    void setTest6(int v) { _test6 = v; }

    Q_PROPERTY(int test7 READ test7 WRITE setTest7)
    int _test7;
    int test7() const { return _test7; }
    void setTest7(int v) { _test7 = v; }

    Q_PROPERTY(int test8 READ test8 WRITE setTest8)
    int _test8;
    int test8() const { return _test8; }
    void setTest8(int v) { _test8 = v; }

    Q_PROPERTY(unsigned int test9 READ test9 WRITE setTest9)
    unsigned int _test9;
    unsigned int test9() const { return _test9; }
    void setTest9(unsigned int v) { _test9 = v; }

    Q_PROPERTY(unsigned int test10 READ test10 WRITE setTest10)
    unsigned int _test10;
    unsigned int test10() const { return _test10; }
    void setTest10(unsigned int v) { _test10 = v; }

    Q_PROPERTY(unsigned int test11 READ test11 WRITE setTest11)
    unsigned int _test11;
    unsigned int test11() const { return _test11; }
    void setTest11(unsigned int v) { _test11 = v; }

    Q_PROPERTY(unsigned int test12 READ test12 WRITE setTest12)
    unsigned int _test12;
    unsigned int test12() const { return _test12; }
    void setTest12(unsigned int v) { _test12 = v; }
};

class DefaultPropertyExtendedObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *firstProperty READ firstProperty WRITE setFirstProperty)
    Q_PROPERTY(QObject *secondProperty READ secondProperty WRITE setSecondProperty)
public:
    DefaultPropertyExtendedObject(QObject *parent = 0) : QObject(parent), m_firstProperty(0), m_secondProperty(0) {}

    QObject *firstProperty() const { return m_firstProperty; }
    QObject *secondProperty() const { return m_secondProperty; }
    void setFirstProperty(QObject *property) { m_firstProperty = property; }
    void setSecondProperty(QObject *property) { m_secondProperty = property; }
private:
    QObject* m_firstProperty;
    QObject* m_secondProperty;
};

class OverrideDefaultPropertyObject : public DefaultPropertyExtendedObject
{
    Q_OBJECT
    Q_CLASSINFO("DefaultProperty", "secondProperty")
public:
    OverrideDefaultPropertyObject() {}
};

class MyRevisionedBaseClassRegistered : public QObject
{
Q_OBJECT
    Q_PROPERTY(qreal propA READ propA WRITE setPropA NOTIFY propAChanged)
    Q_PROPERTY(qreal propB READ propB WRITE setPropB NOTIFY propBChanged REVISION 1)

public:
    MyRevisionedBaseClassRegistered() : m_pa(1), m_pb(2) {}

    qreal propA() const { return m_pa; }
    void setPropA(qreal p) {
        if (p != m_pa) {
            m_pa = p;
            emit propAChanged();
        }
    }
    qreal propB() const { return m_pb; }
    void setPropB(qreal p) {
        if (p != m_pb) {
            m_pb = p;
            emit propBChanged();
        }
    }

    Q_INVOKABLE void methodA() { }
    Q_INVOKABLE Q_REVISION(1) void methodB() { }

signals:
    void propAChanged();
    void propBChanged();

    void signalA();
    Q_REVISION(1) void signalB();

protected:
    qreal m_pa;
    qreal m_pb;
};

class MyRevisionedBaseClassUnregistered : public MyRevisionedBaseClassRegistered
{
Q_OBJECT
    Q_PROPERTY(qreal propC READ propC WRITE setPropC NOTIFY propCChanged)
    Q_PROPERTY(qreal propD READ propD WRITE setPropD NOTIFY propDChanged REVISION 1)

public:
    MyRevisionedBaseClassUnregistered() : m_pc(1), m_pd(2) {}

    qreal propC() const { return m_pc; }
    void setPropC(qreal p) {
        if (p != m_pc) {
            m_pc = p;
            emit propCChanged();
        }
    }
    qreal propD() const { return m_pd; }
    void setPropD(qreal p) {
        if (p != m_pd) {
            m_pd = p;
            emit propDChanged();
        }
    }

    Q_INVOKABLE void methodC() { }
    Q_INVOKABLE Q_REVISION(1) void methodD() { }

signals:
    void propCChanged();
    void propDChanged();

    void signalC();
    Q_REVISION(1) void signalD();

protected:
    qreal m_pc;
    qreal m_pd;
};

class MyRevisionedClass : public MyRevisionedBaseClassUnregistered
{
    Q_OBJECT
    Q_PROPERTY(qreal prop1 READ prop1 WRITE setProp1 NOTIFY prop1Changed)
    Q_PROPERTY(qreal prop2 READ prop2 WRITE setProp2 NOTIFY prop2Changed REVISION 1)

public:
    MyRevisionedClass() : m_p1(0), m_p2(0) {}

    qreal prop1() const { return m_p1; }
    void setProp1(qreal p) {
        if (p != m_p1) {
            m_p1 = p;
            emit prop1Changed();
        }
    }
    qreal prop2() const { return m_p2; }
    void setProp2(qreal p) {
        if (p != m_p2) {
            m_p2 = p;
            emit prop2Changed();
        }
    }

    Q_INVOKABLE void method1() { }
    Q_INVOKABLE Q_REVISION(1) void method2() { }

signals:
    void prop1Changed();
    void prop2Changed();

    void signal1();
    Q_REVISION(1) void signal2();

protected:
    qreal m_p1;
    qreal m_p2;
};

class MyRevisionedSubclass : public MyRevisionedClass
{
    Q_OBJECT
    Q_PROPERTY(qreal prop3 READ prop3 WRITE setProp3 NOTIFY prop3Changed)
    Q_PROPERTY(qreal prop4 READ prop4 WRITE setProp4 NOTIFY prop4Changed REVISION 1)

public:
    MyRevisionedSubclass() : m_p3(3), m_p4(4) {}

    qreal prop3() const { return m_p3; }
    void setProp3(qreal p) {
        if (p != m_p3) {
            m_p3 = p;
            emit prop3Changed();
        }
    }
    qreal prop4() const { return m_p4; }
    void setProp4(qreal p) {
        if (p != m_p4) {
            m_p4 = p;
            emit prop4Changed();
        }
    }

    Q_INVOKABLE void method3() { }
    Q_INVOKABLE Q_REVISION(1) void method4() { }

signals:
    void prop3Changed();
    void prop4Changed();

    void signal3();
    Q_REVISION(1) void signal4();

protected:
    qreal m_p3;
    qreal m_p4;
};


class MyItemUsingRevisionedObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(MyRevisionedClass *revisioned READ revisioned)

public:
    MyItemUsingRevisionedObject() {
        m_revisioned = new MyRevisionedClass;
    }

    MyRevisionedClass *revisioned() const { return m_revisioned; }
private:
    MyRevisionedClass *m_revisioned;
};

QML_DECLARE_TYPE(MyRevisionedBaseClassRegistered)
QML_DECLARE_TYPE(MyRevisionedBaseClassUnregistered)
QML_DECLARE_TYPE(MyRevisionedClass)
QML_DECLARE_TYPE(MyRevisionedSubclass)
QML_DECLARE_TYPE(MyItemUsingRevisionedObject)
Q_DECLARE_METATYPE(MyQmlObject::MyType)


class ScarceResourceObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QPixmap scarceResource READ scarceResource WRITE setScarceResource NOTIFY scarceResourceChanged)
public:
    ScarceResourceObject(QObject *parent = 0) : QObject(parent), m_value(100, 100) { m_value.fill(Qt::blue); }
    ~ScarceResourceObject() {}

    QPixmap scarceResource() const { return m_value; }
    void setScarceResource(QPixmap v) { m_value = v; emit scarceResourceChanged(); }

    bool scarceResourceIsDetached() const { return m_value.isDetached(); }

    // this particular one returns a new one each time
    // this means that every Scarce Resource Copy will
    // consume resources (so that we can track disposal
    // of v8 handles with circular references).
    Q_INVOKABLE QPixmap newScarceResource() const
    {
        QPixmap retn(800, 600);
        retn.fill(QColor(100, 110, 120, 45));
        return retn;
    }

signals:
    void scarceResourceChanged();

private:
    QPixmap m_value;
};
QML_DECLARE_TYPE(ScarceResourceObject)

class testQObjectApi : public QObject
{
    Q_OBJECT
    Q_ENUMS(MyEnum)
    Q_PROPERTY (int qobjectTestProperty READ qobjectTestProperty NOTIFY qobjectTestPropertyChanged FINAL)
    Q_PROPERTY (int qobjectTestWritableProperty READ qobjectTestWritableProperty WRITE setQObjectTestWritableProperty NOTIFY qobjectTestWritablePropertyChanged)
    Q_PROPERTY (int qobjectTestWritableFinalProperty READ qobjectTestWritableFinalProperty WRITE setQObjectTestWritableFinalProperty NOTIFY qobjectTestWritableFinalPropertyChanged FINAL)

public:
    testQObjectApi(QObject* parent = 0)
        : QObject(parent), m_testProperty(0), m_testWritableProperty(0), m_testWritableFinalProperty(0), m_methodCallCount(0), m_trackedObject(0)
    {
    }

    ~testQObjectApi() {}

    enum MyEnum { EnumValue1 = 25, EnumValue2 = 42 };
    Q_INVOKABLE int qobjectEnumTestMethod(MyEnum val) { return (static_cast<int>(val) + 5); }
    Q_INVOKABLE int qobjectTestMethod(int increment = 1) { m_methodCallCount += increment; return m_methodCallCount; }

    Q_INVOKABLE void trackObject(QObject *obj) { m_trackedObject = obj; }
    Q_INVOKABLE QObject *trackedObject() const { return m_trackedObject; }
    Q_INVOKABLE void setTrackedObjectProperty(const QString &propName) const { m_trackedObject->setProperty(qPrintable(propName), QVariant(5)); }
    Q_INVOKABLE QVariant trackedObjectProperty(const QString &propName) const { return m_trackedObject->property(qPrintable(propName)); }

    Q_INVOKABLE void setSpecificProperty(QObject *obj, const QString & propName, const QVariant & v) const { obj->setProperty(qPrintable(propName), v); }
    Q_INVOKABLE void changeQObjectParent(QObject *obj) { obj->setParent(this); }

    int qobjectTestProperty() const { return m_testProperty; }
    void setQObjectTestProperty(int tp) { m_testProperty = tp; emit qobjectTestPropertyChanged(tp); }

    int qobjectTestWritableProperty() const { return m_testWritableProperty; }
    void setQObjectTestWritableProperty(int tp) { m_testWritableProperty = tp; emit qobjectTestWritablePropertyChanged(tp); }

    int qobjectTestWritableFinalProperty() const { return m_testWritableFinalProperty; }
    void setQObjectTestWritableFinalProperty(int tp) { m_testWritableFinalProperty = tp; emit qobjectTestWritableFinalPropertyChanged(); }

    Q_INVOKABLE bool trackedObjectHasJsOwnership() {
        QObject * object = m_trackedObject;

        if (!object)
            return false;

        QQmlData *ddata = QQmlData::get(object, false);
        if (!ddata)
            return false;
        else
            return ddata->indestructible?false:true;
    }

    Q_INVOKABLE void deleteQObject(QObject *obj) { delete obj; }

signals:
    void qobjectTestPropertyChanged(int testProperty);
    void qobjectTestWritablePropertyChanged(int testWritableProperty);
    void qobjectTestWritableFinalPropertyChanged();

private:
    int m_testProperty;
    int m_testWritableProperty;
    int m_testWritableFinalProperty;
    int m_methodCallCount;
    QObject *m_trackedObject;
};

class testQObjectApiTwo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int twoTestProperty READ twoTestProperty WRITE setTwoTestProperty NOTIFY twoTestPropertyChanged)

public:
    testQObjectApiTwo(QObject *parent = 0) : QObject(parent), m_ttp(42) {}
    ~testQObjectApiTwo() {}

    void setTwoTestProperty(int v) { m_ttp = v; emit twoTestPropertyChanged(); }
    int twoTestProperty() const { return m_ttp; }

signals:
    void twoTestPropertyChanged();

private:
    int m_ttp;
};

class testImportOrderApi : public QObject
{
    Q_OBJECT

public:
    testImportOrderApi(int value, QObject *parent = 0) : QObject(parent), m_value(value) {}

    Q_PROPERTY(int value READ value)

    int value() const { return m_value; }

private:
    int m_value;
};

class CircularReferenceObject : public QObject
{
    Q_OBJECT

public:
    CircularReferenceObject(QObject *parent = 0)
        : QObject(parent), m_dtorCount(0)
    {
    }

    ~CircularReferenceObject()
    {
        if (m_dtorCount) *m_dtorCount = *m_dtorCount + 1;
    }

    Q_INVOKABLE void setDtorCount(int *dtorCount)
    {
        m_dtorCount = dtorCount;
    }

    Q_INVOKABLE CircularReferenceObject *generate(QObject *parent = 0)
    {
        CircularReferenceObject *retn = new CircularReferenceObject(parent);
        retn->m_dtorCount = m_dtorCount;
        retn->m_engine = m_engine;
        return retn;
    }

    Q_INVOKABLE void addReference(QObject *other)
    {
        QQmlData *ddata = QQmlData::get(this);
        Q_ASSERT(ddata);
        QV4::ExecutionEngine *v4 = ddata->jsWrapper.engine();
        Q_ASSERT(v4);
        QV4::Scope scope(v4);
        QV4::Scoped<QV4::QObjectWrapper> thisObject(scope, ddata->jsWrapper.value());
        Q_ASSERT(thisObject);

        QQmlData *otherDData = QQmlData::get(other);
        Q_ASSERT(otherDData);

        QV4::ScopedValue v(scope, otherDData->jsWrapper.value());
        thisObject->defineDefaultProperty(QStringLiteral("autoTestStrongRef"), v);
    }

    void setEngine(QQmlEngine* declarativeEngine)
    {
        m_engine = QQmlEnginePrivate::get(declarativeEngine)->v8engine();
    }

private:
    int *m_dtorCount;
    QV8Engine* m_engine;
};
Q_DECLARE_METATYPE(CircularReferenceObject*)

class MyDynamicCreationDestructionObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY (int intProperty READ intProperty WRITE setIntProperty NOTIFY intPropertyChanged)

public:
    MyDynamicCreationDestructionObject(QObject *parent = 0) : QObject(parent), m_intProperty(0), m_dtorCount(0)
    {
    }

    ~MyDynamicCreationDestructionObject()
    {
        if (m_dtorCount) {
            (*m_dtorCount)++;
        }
    }

    int intProperty() const { return m_intProperty; }
    void setIntProperty(int val) { m_intProperty = val; emit intPropertyChanged(); }

    Q_INVOKABLE MyDynamicCreationDestructionObject *createNew()
    {
        // no parent == ownership transfers to JS; same dtor counter.
        MyDynamicCreationDestructionObject *retn = new MyDynamicCreationDestructionObject;
        retn->setDtorCount(m_dtorCount);
        return retn;
    }

    void setDtorCount(int *dtorCount)
    {
        m_dtorCount = dtorCount;
    }

signals:
    void intPropertyChanged();

private:
    int m_intProperty;
    int *m_dtorCount;
};

class WriteCounter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue);
public:
    WriteCounter() : m_value(0), m_count(0) {}

    int value() const { return m_value; }
    void setValue(int v) { m_value = v; ++m_count; }

    int count() const { return m_count; }

private:
    int m_value;
    int m_count;
};

class MySequenceConversionObject : public QObject
{
    Q_OBJECT

    Q_PROPERTY (QList<int> intListProperty READ intListProperty WRITE setIntListProperty NOTIFY intListPropertyChanged)
    Q_PROPERTY (QList<int> intListProperty2 READ intListProperty2 WRITE setIntListProperty2 NOTIFY intListProperty2Changed)
    Q_PROPERTY (QList<qreal> qrealListProperty READ qrealListProperty WRITE setQrealListProperty NOTIFY qrealListPropertyChanged)
    Q_PROPERTY (QList<bool> boolListProperty READ boolListProperty WRITE setBoolListProperty NOTIFY boolListPropertyChanged)
    Q_PROPERTY (QList<QString> stringListProperty READ stringListProperty WRITE setStringListProperty NOTIFY stringListPropertyChanged)
    Q_PROPERTY (QList<QUrl> urlListProperty READ urlListProperty WRITE setUrlListProperty NOTIFY urlListPropertyChanged)
    Q_PROPERTY (QStringList qstringListProperty READ qstringListProperty WRITE setQStringListProperty NOTIFY qstringListPropertyChanged)

    Q_PROPERTY (QList<QPoint> pointListProperty READ pointListProperty WRITE setPointListProperty NOTIFY pointListPropertyChanged)
    Q_PROPERTY (QList<NonRegisteredType> typeListProperty READ typeListProperty WRITE setTypeListProperty NOTIFY typeListPropertyChanged)
    Q_PROPERTY (QList<QVariant> variantListProperty READ variantListProperty WRITE setVariantListProperty NOTIFY variantListPropertyChanged)

    Q_PROPERTY (qint32 maxIndex READ maxIndex CONSTANT)
    Q_PROPERTY (quint32 tooBigIndex READ tooBigIndex CONSTANT)
    Q_PROPERTY (qint32 negativeIndex READ negativeIndex CONSTANT)

public:
    MySequenceConversionObject()
    {
        m_intList << 1 << 2 << 3 << 4;
        m_intList2 << 1 << 2 << 3 << 4;
        m_qrealList << 1.1 << 2.2 << 3.3 << 4.4;
        m_boolList << true << false << true << false;
        m_stringList << QLatin1String("first") << QLatin1String("second") << QLatin1String("third") << QLatin1String("fourth");
        m_urlList << QUrl("http://www.example1.com") << QUrl("http://www.example2.com") << QUrl("http://www.example3.com");
        m_qstringList << QLatin1String("first") << QLatin1String("second") << QLatin1String("third") << QLatin1String("fourth");

        m_pointList << QPoint(1, 2) << QPoint(3, 4) << QPoint(5, 6);
        m_variantList << QVariant(QLatin1String("one")) << QVariant(true) << QVariant(3);
    }

    ~MySequenceConversionObject() {}

    qint32 maxIndex() const
    {
        return INT_MAX;
    }
    quint32 tooBigIndex() const
    {
        quint32 retn = 7;
        retn += INT_MAX;
        return retn;
    }
    qint32 negativeIndex() const
    {
        return -5;
    }

    QList<int> intListProperty() const { return m_intList; }
    void setIntListProperty(const QList<int> &list) { m_intList = list; emit intListPropertyChanged(); }
    QList<int> intListProperty2() const { return m_intList2; }
    void setIntListProperty2(const QList<int> &list) { m_intList2 = list; emit intListProperty2Changed(); }
    QList<qreal> qrealListProperty() const { return m_qrealList; }
    void setQrealListProperty(const QList<qreal> &list) { m_qrealList = list; emit qrealListPropertyChanged(); }
    QList<bool> boolListProperty() const { return m_boolList; }
    void setBoolListProperty(const QList<bool> &list) { m_boolList = list; emit boolListPropertyChanged(); }
    QList<QString> stringListProperty() const { return m_stringList; }
    void setStringListProperty(const QList<QString> &list) { m_stringList = list; emit stringListPropertyChanged(); }
    QList<QUrl> urlListProperty() const { return m_urlList; }
    void setUrlListProperty(const QList<QUrl> &list) { m_urlList = list; emit urlListPropertyChanged(); }
    QStringList qstringListProperty() const { return m_qstringList; }
    void setQStringListProperty(const QStringList &list) { m_qstringList = list; emit qstringListPropertyChanged(); }
    QList<QPoint> pointListProperty() const { return m_pointList; }
    void setPointListProperty(const QList<QPoint> &list) { m_pointList = list; emit pointListPropertyChanged(); }
    QList<NonRegisteredType> typeListProperty() const { return m_typeList; }
    void setTypeListProperty(const QList<NonRegisteredType> &list) { m_typeList = list; emit typeListPropertyChanged(); }
    QList<QVariant> variantListProperty() const { return m_variantList; }
    void setVariantListProperty(const QList<QVariant> &list) { m_variantList = list; emit variantListPropertyChanged(); }

    // now for "copy resource" sequences:
    Q_INVOKABLE QList<int> generateIntSequence() const { QList<int> retn; retn << 1 << 2 << 3; return retn; }
    Q_INVOKABLE QList<qreal> generateQrealSequence() const { QList<qreal> retn; retn << 1.1 << 2.2 << 3.3; return retn; }
    Q_INVOKABLE QList<bool> generateBoolSequence() const { QList<bool> retn; retn << true << false << true; return retn; }
    Q_INVOKABLE QList<QString> generateStringSequence() const { QList<QString> retn; retn << "one" << "two" << "three"; return retn; }
    Q_INVOKABLE QList<QUrl> generateUrlSequence() const { QList<QUrl> retn; retn << QUrl("http://www.example1.com") << QUrl("http://www.example2.com") << QUrl("http://www.example3.com"); return retn; }
    Q_INVOKABLE QStringList generateQStringSequence() const { QStringList retn; retn << "one" << "two" << "three"; return retn; }
    Q_INVOKABLE bool parameterEqualsGeneratedIntSequence(const QList<int>& param) const { return (param == generateIntSequence()); }

    // "reference resource" underlying qobject deletion test:
    Q_INVOKABLE MySequenceConversionObject *generateTestObject() const { return new MySequenceConversionObject; }
    Q_INVOKABLE void deleteTestObject(QObject *object) const { delete object; }

signals:
    void intListPropertyChanged();
    void intListProperty2Changed();
    void qrealListPropertyChanged();
    void boolListPropertyChanged();
    void stringListPropertyChanged();
    void urlListPropertyChanged();
    void qstringListPropertyChanged();
    void pointListPropertyChanged();
    void typeListPropertyChanged();
    void variantListPropertyChanged();

private:
    QList<int> m_intList;
    QList<int> m_intList2;
    QList<qreal> m_qrealList;
    QList<bool> m_boolList;
    QList<QString> m_stringList;
    QList<QUrl> m_urlList;
    QStringList m_qstringList;

    QList<QPoint> m_pointList;
    QList<NonRegisteredType> m_typeList; // not a supported sequence type
    QList<QVariant> m_variantList; // not a supported sequence type, but QVariantList support is hardcoded.
};

class MyDeleteObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *nestedObject READ nestedObject NOTIFY nestedObjectChanged)
    Q_PROPERTY(int deleteNestedObject READ deleteNestedObject NOTIFY deleteNestedObjectChanged)
    Q_PROPERTY(QObject *object2 READ object2 NOTIFY object2Changed)

public:
    MyDeleteObject() : m_nestedObject(new MyQmlObject), m_object1(0), m_object2(0) {}

    Q_INVOKABLE QObject *object1() const { return m_object1; }
    Q_INVOKABLE QObject *object2() const { return m_object2; }
    void setObject1(QObject *object) { m_object1 = object; }
    void setObject2(QObject *object) { m_object2 = object; emit object2Changed(); }
    QObject *nestedObject() const { return m_nestedObject; }
    int deleteNestedObject() { delete m_nestedObject; m_nestedObject = 0; return 1; }

signals:
    void nestedObjectChanged();
    void deleteNestedObjectChanged();
    void object2Changed();

private:
    MyQmlObject *m_nestedObject;
    QObject *m_object1;
    QObject *m_object2;
};

class DateTimeExporter : public QObject
{
    Q_OBJECT

public:
    DateTimeExporter(const QDateTime &dt) : m_datetime(dt), m_offset(0), m_timespec("UTC")
    {
        switch (m_datetime.timeSpec()) {
        case Qt::LocalTime:
            {
            QDateTime utc(m_datetime.toUTC());
            utc.setTimeSpec(Qt::LocalTime);
            m_offset = m_datetime.secsTo(utc) / 60;
            m_timespec = "LocalTime";
            }
            break;
        case Qt::OffsetFromUTC:
            m_offset = m_datetime.utcOffset() / 60;
            m_timespec = QString("%1%2:%3").arg(m_offset < 0 ? '-' : '+')
                                           .arg(abs(m_offset) / 60)
                                           .arg(abs(m_offset) % 60);
        default:
            break;
        }
    }

    Q_INVOKABLE QDate getDate() const { return m_datetime.date(); }
    Q_INVOKABLE QDateTime getDateTime() const { return m_datetime; }
    Q_INVOKABLE int getDateTimeOffset() const { return m_offset; }
    Q_INVOKABLE QString getTimeSpec() const { return m_timespec; }

private:
    QDateTime m_datetime;
    int m_offset;
    QString m_timespec;
};

class MyWorkerObject : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void doIt();

Q_SIGNALS:
    void done(const QString &result);
};

class MyUnregisteredEnumTypeObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(MyEnum enumProperty READ enumProperty WRITE setEnumProperty)

public:
    MyUnregisteredEnumTypeObject() : QObject(), m_ev(FirstValue) {}
    ~MyUnregisteredEnumTypeObject() {}

    enum MyEnum {
        FirstValue = 1,
        SecondValue = 2
    };

    MyEnum enumProperty() const { return m_ev; }
    void setEnumProperty(MyEnum v) { m_ev = v; }

private:
    MyEnum m_ev;
};

class FallbackBindingsObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY (int test READ test NOTIFY testChanged)
public:
    FallbackBindingsObject(QObject* parent = 0)
        : QObject(parent), m_test(100)
    {
    }

    int test() const { return m_test; }

Q_SIGNALS:
    void testChanged();

private:
    int m_test;
};

class FallbackBindingsDerived : public FallbackBindingsObject
{
    Q_OBJECT
    Q_PROPERTY (QString test READ test NOTIFY testChanged)
public:
    FallbackBindingsDerived(QObject* parent = 0)
        : FallbackBindingsObject(parent), m_test("hello")
    {
    }

    QString test() const { return m_test; }

Q_SIGNALS:
    void testChanged();

private:
    QString m_test;
};

class FallbackBindingsAttachedObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY (int test READ test NOTIFY testChanged)
public:
    FallbackBindingsAttachedObject(QObject *parent) : QObject(parent), m_test(100) {}

    int test() const { return m_test; }

Q_SIGNALS:
    void testChanged();

private:
    int m_test;
};

class FallbackBindingsAttachedDerived : public FallbackBindingsAttachedObject
{
    Q_OBJECT
    Q_PROPERTY (QString test READ test NOTIFY testChanged)
public:
    FallbackBindingsAttachedDerived(QObject* parent = 0)
        : FallbackBindingsAttachedObject(parent), m_test("hello")
    {
    }

    QString test() const { return m_test; }

Q_SIGNALS:
    void testChanged();

private:
    QString m_test;
};

class FallbackBindingsTypeObject : public QObject
{
    Q_OBJECT
public:
    FallbackBindingsTypeObject() : QObject() {}

    static FallbackBindingsAttachedObject *qmlAttachedProperties(QObject *o) {
        return new FallbackBindingsAttachedObject(o);
    }
};

class FallbackBindingsTypeDerived : public QObject
{
    Q_OBJECT
public:
    FallbackBindingsTypeDerived() : QObject() {}

    static FallbackBindingsAttachedObject *qmlAttachedProperties(QObject *o) {
        return new FallbackBindingsAttachedDerived(o);
    }
};

QML_DECLARE_TYPEINFO(FallbackBindingsTypeObject, QML_HAS_ATTACHED_PROPERTIES)
QML_DECLARE_TYPEINFO(FallbackBindingsTypeDerived, QML_HAS_ATTACHED_PROPERTIES)

class SingletonWithEnum : public QObject
{
    Q_OBJECT
    Q_ENUMS(TestEnum)
public:
    enum TestEnum {
        TestValue = 42
    };
};

// Like QtObject, but with default property
class QObjectContainer : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("DefaultProperty", "data")
    Q_PROPERTY(QQmlListProperty<QObject> data READ data DESIGNABLE false)
public:
    QObjectContainer();

    QQmlListProperty<QObject> data();

    static void children_append(QQmlListProperty<QObject> *prop, QObject *o);
    static int children_count(QQmlListProperty<QObject> *prop);
    static QObject *children_at(QQmlListProperty<QObject> *prop, int index);
    static void children_clear(QQmlListProperty<QObject> *prop);

    QList<QObject*> dataChildren;
    QWidget *widgetParent;
    bool gcOnAppend;

protected slots:
    void childDestroyed(QObject *child);
};

class QObjectContainerWithGCOnAppend : public QObjectContainer
{
    Q_OBJECT
public:
    QObjectContainerWithGCOnAppend()
    {
        gcOnAppend = true;
    }
};

class FloatingQObject : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
public:
    FloatingQObject() {}

    virtual void classBegin();
    virtual void componentComplete();
};

void registerTypes();

#endif // TESTTYPES_H

