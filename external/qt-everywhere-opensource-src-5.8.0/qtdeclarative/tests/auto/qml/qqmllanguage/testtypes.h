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
#include <QtCore/qrect.h>
#include <QtCore/qdatetime.h>
#include <QtGui/qmatrix.h>
#include <QtGui/qcolor.h>
#include <QtGui/qvector2d.h>
#include <QtGui/qvector3d.h>
#include <QtGui/qvector4d.h>
#include <QtGui/qquaternion.h>
#include <QtQml/qqml.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlparserstatus.h>
#include <QtQml/qqmlpropertyvaluesource.h>
#include <QtQml/qqmlscriptstring.h>
#include <QtQml/qqmlproperty.h>
#include <private/qqmlcustomparser_p.h>

QVariant myCustomVariantTypeConverter(const QString &data);

class MyInterface
{
public:
    MyInterface() : id(913) {}
    int id;
};

QT_BEGIN_NAMESPACE
#define MyInterface_iid "org.qt-project.Qt.Test.MyInterface"
Q_DECLARE_INTERFACE(MyInterface, MyInterface_iid);
QT_END_NAMESPACE
QML_DECLARE_INTERFACE(MyInterface);

struct MyCustomVariantType
{
    MyCustomVariantType() : a(0) {}
    int a;
};
Q_DECLARE_METATYPE(MyCustomVariantType);

class MyAttachedObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(int value2 READ value2 WRITE setValue2)
public:
    MyAttachedObject(QObject *parent) : QObject(parent), m_value(0), m_value2(0) {}

    int value() const { return m_value; }
    void setValue(int v) { if (m_value != v) { m_value = v; emit valueChanged(); } }

    int value2() const { return m_value2; }
    void setValue2(int v) { m_value2 = v; }

signals:
    void valueChanged();

private:
    int m_value;
    int m_value2;
};

class MyQmlObject : public QObject, public MyInterface
{
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue FINAL)
    Q_PROPERTY(QString readOnlyString READ readOnlyString)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled)
    Q_PROPERTY(QRect rect READ rect WRITE setRect)
    Q_PROPERTY(QMatrix matrix READ matrix WRITE setMatrix)  //assumed to be unsupported by QML
    Q_PROPERTY(MyInterface *interfaceProperty READ interface WRITE setInterface)
    Q_PROPERTY(int onLiteralSignal READ onLiteralSignal WRITE setOnLiteralSignal)
    Q_PROPERTY(MyCustomVariantType customType READ customType WRITE setCustomType)
    Q_PROPERTY(MyQmlObject *qmlobjectProperty READ qmlobject WRITE setQmlobject)
    Q_PROPERTY(int propertyWithNotify READ propertyWithNotify WRITE setPropertyWithNotify NOTIFY oddlyNamedNotifySignal)
    Q_PROPERTY(int nonScriptable READ nonScriptable WRITE setNonScriptable SCRIPTABLE false)
    Q_PROPERTY(QJSValue qjsvalue READ qjsvalue WRITE setQJSValue NOTIFY qjsvalueChanged)

    Q_INTERFACES(MyInterface)
public:
    MyQmlObject();

    int value() const { return m_value; }
    void setValue(int v) { m_value = v; }

    QString readOnlyString() const { return QLatin1String(""); }

    bool enabled() const { return false; }
    void setEnabled(bool) {}

    QRect rect() const { return QRect(); }
    void setRect(const QRect&) {}

    QMatrix matrix() const { return QMatrix(); }
    void setMatrix(const QMatrix&) {}

    MyInterface *interface() const { return m_interface; }
    void setInterface(MyInterface *iface) { m_interface = iface; }

    static MyAttachedObject *qmlAttachedProperties(QObject *other) {
        return new MyAttachedObject(other);
    }
    Q_CLASSINFO("DefaultMethod", "basicSlot()")

    int onLiteralSignal() const { return m_value; }
    void setOnLiteralSignal(int v) { m_value = v; }

    MyQmlObject *qmlobject() const { return m_qmlobject; }
    void setQmlobject(MyQmlObject *o) { m_qmlobject = o; }

    MyCustomVariantType customType() const { return m_custom; }
    void setCustomType(const MyCustomVariantType &v)  { m_custom = v; }

    int propertyWithNotify() const { return m_propertyWithNotify; }
    void setPropertyWithNotify(int i) { m_propertyWithNotify = i; emit oddlyNamedNotifySignal(); }

    int nonScriptable() const { return 0; }
    void setNonScriptable(int) {}

    QJSValue qjsvalue() const { return m_qjsvalue; }
    void setQJSValue(const QJSValue &value) { m_qjsvalue = value; emit qjsvalueChanged(); }

    int childAddedEventCount() const { return m_childAddedEventCount; }

public slots:
    void basicSlot() { qWarning("MyQmlObject::basicSlot"); }
    void basicSlotWithArgs(int v) { qWarning("MyQmlObject::basicSlotWithArgs(%d)", v); }
    void qjsvalueMethod(const QJSValue &v) { m_qjsvalue = v; }

signals:
    void basicSignal();
    void basicParameterizedSignal(int parameter);
    void oddlyNamedNotifySignal();
    void signalWithDefaultArg(int parameter = 5);
    void qjsvalueChanged();

protected:
    virtual bool event(QEvent *event);

private:
    friend class tst_qqmllanguage;
    int m_value;
    MyInterface *m_interface;
    MyQmlObject *m_qmlobject;
    MyCustomVariantType m_custom;
    int m_propertyWithNotify;
    QJSValue m_qjsvalue;
    int m_childAddedEventCount;
};
QML_DECLARE_TYPE(MyQmlObject)
QML_DECLARE_TYPEINFO(MyQmlObject, QML_HAS_ATTACHED_PROPERTIES)

class MyGroupedObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlScriptString script READ script WRITE setScript)
    Q_PROPERTY(int value READ value WRITE setValue)
public:
    QQmlScriptString script() const { return m_script; }
    void setScript(const QQmlScriptString &s) { m_script = s; }

    int value() const { return m_value; }
    void setValue(int v) { m_value = v; }

private:
    int m_value;
    QQmlScriptString m_script;
};


class MyEnumContainer : public QObject
{
    Q_OBJECT
    Q_ENUMS(RelatedEnum)

public:
    enum RelatedEnum { RelatedInvalid = -1, RelatedValue = 42 };
};

class MyTypeObject : public QObject
{
    Q_OBJECT
    Q_ENUMS(MyEnum)
    Q_ENUMS(MyMirroredEnum)
    Q_ENUMS(MyEnumContainer::RelatedEnum)
    Q_FLAGS(MyFlags)

    Q_PROPERTY(QString id READ id WRITE setId)
    Q_PROPERTY(QObject *objectProperty READ objectProperty WRITE setObjectProperty NOTIFY objectPropertyChanged)
    Q_PROPERTY(QQmlComponent *componentProperty READ componentProperty WRITE setComponentProperty)
    Q_PROPERTY(MyFlags flagProperty READ flagProperty WRITE setFlagProperty NOTIFY flagPropertyChanged)
    Q_PROPERTY(MyEnum enumProperty READ enumProperty WRITE setEnumProperty NOTIFY enumPropertyChanged)
    Q_PROPERTY(MyEnum readOnlyEnumProperty READ readOnlyEnumProperty)
    Q_PROPERTY(Qt::TextFormat qtEnumProperty READ qtEnumProperty WRITE setQtEnumProperty NOTIFY qtEnumPropertyChanged)
    Q_PROPERTY(MyMirroredEnum mirroredEnumProperty READ mirroredEnumProperty WRITE setMirroredEnumProperty NOTIFY mirroredEnumPropertyChanged)
    Q_PROPERTY(MyEnumContainer::RelatedEnum relatedEnumProperty READ relatedEnumProperty WRITE setRelatedEnumProperty)
    Q_PROPERTY(QString stringProperty READ stringProperty WRITE setStringProperty NOTIFY stringPropertyChanged)
    Q_PROPERTY(QByteArray byteArrayProperty READ byteArrayProperty WRITE setByteArrayProperty NOTIFY byteArrayPropertyChanged)
    Q_PROPERTY(uint uintProperty READ uintProperty WRITE setUintProperty NOTIFY uintPropertyChanged)
    Q_PROPERTY(int intProperty READ intProperty WRITE setIntProperty NOTIFY intPropertyChanged)
    Q_PROPERTY(qreal realProperty READ realProperty WRITE setRealProperty NOTIFY realPropertyChanged)
    Q_PROPERTY(double doubleProperty READ doubleProperty WRITE setDoubleProperty NOTIFY doublePropertyChanged)
    Q_PROPERTY(float floatProperty READ floatProperty WRITE setFloatProperty NOTIFY floatPropertyChanged)
    Q_PROPERTY(QColor colorProperty READ colorProperty WRITE setColorProperty NOTIFY colorPropertyChanged)
    Q_PROPERTY(QDate dateProperty READ dateProperty WRITE setDateProperty NOTIFY datePropertyChanged)
    Q_PROPERTY(QTime timeProperty READ timeProperty WRITE setTimeProperty NOTIFY timePropertyChanged)
    Q_PROPERTY(QDateTime dateTimeProperty READ dateTimeProperty WRITE setDateTimeProperty NOTIFY dateTimePropertyChanged)
    Q_PROPERTY(QPoint pointProperty READ pointProperty WRITE setPointProperty NOTIFY pointPropertyChanged)
    Q_PROPERTY(QPointF pointFProperty READ pointFProperty WRITE setPointFProperty NOTIFY pointFPropertyChanged)
    Q_PROPERTY(QSize sizeProperty READ sizeProperty WRITE setSizeProperty NOTIFY sizePropertyChanged)
    Q_PROPERTY(QSizeF sizeFProperty READ sizeFProperty WRITE setSizeFProperty NOTIFY sizeFPropertyChanged)
    Q_PROPERTY(QRect rectProperty READ rectProperty WRITE setRectProperty NOTIFY rectPropertyChanged)
    Q_PROPERTY(QRect rectProperty2 READ rectProperty2 WRITE setRectProperty2 )
    Q_PROPERTY(QRectF rectFProperty READ rectFProperty WRITE setRectFProperty NOTIFY rectFPropertyChanged)
    Q_PROPERTY(bool boolProperty READ boolProperty WRITE setBoolProperty NOTIFY boolPropertyChanged)
    Q_PROPERTY(QVariant variantProperty READ variantProperty WRITE setVariantProperty NOTIFY variantPropertyChanged)
    Q_PROPERTY(QVector2D vector2Property READ vector2Property WRITE setVector2Property NOTIFY vector2PropertyChanged)
    Q_PROPERTY(QVector3D vectorProperty READ vectorProperty WRITE setVectorProperty NOTIFY vectorPropertyChanged)
    Q_PROPERTY(QVector4D vector4Property READ vector4Property WRITE setVector4Property NOTIFY vector4PropertyChanged)
    Q_PROPERTY(QQuaternion quaternionProperty READ quaternionProperty WRITE setQuaternionProperty NOTIFY quaternionPropertyChanged)
    Q_PROPERTY(QUrl urlProperty READ urlProperty WRITE setUrlProperty NOTIFY urlPropertyChanged)

    Q_PROPERTY(QQmlScriptString scriptProperty READ scriptProperty WRITE setScriptProperty)
    Q_PROPERTY(MyGroupedObject *grouped READ grouped CONSTANT)
    Q_PROPERTY(MyGroupedObject *nullGrouped READ nullGrouped CONSTANT)

    Q_PROPERTY(MyTypeObject *selfGroupProperty READ selfGroupProperty)

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
        emit objectPropertyChanged();
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
        emit flagPropertyChanged();
    }

    enum MyEnum { EnumVal1, EnumVal2, lowercaseEnumVal };
    MyEnum enumPropertyValue;
    MyEnum enumProperty() const {
        return enumPropertyValue;
    }
    void setEnumProperty(MyEnum v) {
        enumPropertyValue = v;
        emit enumPropertyChanged();
    }

    MyEnum readOnlyEnumProperty() const {
        return EnumVal1;
    }

    Qt::TextFormat qtEnumPropertyValue;
    Qt::TextFormat qtEnumProperty() const {
        return qtEnumPropertyValue;
    }
    void setQtEnumProperty(Qt::TextFormat v) {
        qtEnumPropertyValue = v;
        emit qtEnumPropertyChanged();
    }

    enum MyMirroredEnum {
        MirroredEnumVal1 = Qt::AlignLeft,
        MirroredEnumVal2 = Qt::AlignRight,
        MirroredEnumVal3 = Qt::AlignHCenter };
    MyMirroredEnum mirroredEnumPropertyValue;
    MyMirroredEnum mirroredEnumProperty() const {
        return mirroredEnumPropertyValue;
    }
    void setMirroredEnumProperty(MyMirroredEnum v) {
        mirroredEnumPropertyValue = v;
        emit mirroredEnumPropertyChanged();
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
        emit stringPropertyChanged();
    }

    QByteArray byteArrayPropertyValue;
    QByteArray byteArrayProperty() const {
        return byteArrayPropertyValue;
    }
    void setByteArrayProperty(const QByteArray &v) {
        byteArrayPropertyValue = v;
        emit byteArrayPropertyChanged();
    }

    uint uintPropertyValue;
    uint uintProperty() const {
       return uintPropertyValue;
    }
    void setUintProperty(const uint &v) {
        uintPropertyValue = v;
        emit uintPropertyChanged();
    }

    int intPropertyValue;
    int intProperty() const {
       return intPropertyValue;
    }
    void setIntProperty(const int &v) {
        intPropertyValue = v;
        emit intPropertyChanged();
    }

    qreal realPropertyValue;
    qreal realProperty() const {
       return realPropertyValue;
    }
    void setRealProperty(const qreal &v) {
        realPropertyValue = v;
        emit realPropertyChanged();
    }

    double doublePropertyValue;
    double doubleProperty() const {
       return doublePropertyValue;
    }
    void setDoubleProperty(const double &v) {
        doublePropertyValue = v;
        emit doublePropertyChanged();
    }

    float floatPropertyValue;
    float floatProperty() const {
       return floatPropertyValue;
    }
    void setFloatProperty(const float &v) {
        floatPropertyValue = v;
        emit floatPropertyChanged();
    }

    QColor colorPropertyValue;
    QColor colorProperty() const {
       return colorPropertyValue;
    }
    void setColorProperty(const QColor &v) {
        colorPropertyValue = v;
        emit colorPropertyChanged();
    }

    QDate datePropertyValue;
    QDate dateProperty() const {
       return datePropertyValue;
    }
    void setDateProperty(const QDate &v) {
        datePropertyValue = v;
        emit datePropertyChanged();
    }

    QTime timePropertyValue;
    QTime timeProperty() const {
       return timePropertyValue;
    }
    void setTimeProperty(const QTime &v) {
        timePropertyValue = v;
        emit timePropertyChanged();
    }

    QDateTime dateTimePropertyValue;
    QDateTime dateTimeProperty() const {
       return dateTimePropertyValue;
    }
    void setDateTimeProperty(const QDateTime &v) {
        dateTimePropertyValue = v;
        emit dateTimePropertyChanged();
    }

    QPoint pointPropertyValue;
    QPoint pointProperty() const {
       return pointPropertyValue;
    }
    void setPointProperty(const QPoint &v) {
        pointPropertyValue = v;
        emit pointPropertyChanged();
    }

    QPointF pointFPropertyValue;
    QPointF pointFProperty() const {
       return pointFPropertyValue;
    }
    void setPointFProperty(const QPointF &v) {
        pointFPropertyValue = v;
        emit pointFPropertyChanged();
    }

    QSize sizePropertyValue;
    QSize sizeProperty() const {
       return sizePropertyValue;
    }
    void setSizeProperty(const QSize &v) {
        sizePropertyValue = v;
        emit sizePropertyChanged();
    }

    QSizeF sizeFPropertyValue;
    QSizeF sizeFProperty() const {
       return sizeFPropertyValue;
    }
    void setSizeFProperty(const QSizeF &v) {
        sizeFPropertyValue = v;
        emit sizeFPropertyChanged();
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
        emit rectFPropertyChanged();
    }

    bool boolPropertyValue;
    bool boolProperty() const {
       return boolPropertyValue;
    }
    void setBoolProperty(const bool &v) {
        boolPropertyValue = v;
        emit boolPropertyChanged();
    }

    QVariant variantPropertyValue;
    QVariant variantProperty() const {
       return variantPropertyValue;
    }
    void setVariantProperty(const QVariant &v) {
        variantPropertyValue = v;
        emit variantPropertyChanged();
    }

    QVector3D vectorPropertyValue;
    QVector3D vectorProperty() const {
        return vectorPropertyValue;
    }
    void setVectorProperty(const QVector3D &v) {
        vectorPropertyValue = v;
        emit vectorPropertyChanged();
    }

    QVector2D vector2PropertyValue;
    QVector2D vector2Property() const {
        return vector2PropertyValue;
    }
    void setVector2Property(const QVector2D &v) {
        vector2PropertyValue = v;
        emit vector2PropertyChanged();
    }

    QVector4D vector4PropertyValue;
    QVector4D vector4Property() const {
        return vector4PropertyValue;
    }
    void setVector4Property(const QVector4D &v) {
        vector4PropertyValue = v;
        emit vector4PropertyChanged();
    }

    QQuaternion quaternionPropertyValue;
    QQuaternion quaternionProperty() const {
        return quaternionPropertyValue;
    }
    void setQuaternionProperty(const QQuaternion &v) {
        quaternionPropertyValue = v;
        emit quaternionPropertyChanged();
    }

    QUrl urlPropertyValue;
    QUrl urlProperty() const {
        return urlPropertyValue;
    }
    void setUrlProperty(const QUrl &v) {
        urlPropertyValue = v;
        emit urlPropertyChanged();
    }

    QQmlScriptString scriptPropertyValue;
    QQmlScriptString scriptProperty() const {
        return scriptPropertyValue;
    }
    void setScriptProperty(const QQmlScriptString &v) {
        scriptPropertyValue = v;
    }

    MyGroupedObject groupedValue;
    MyGroupedObject *grouped() { return &groupedValue; }

    MyGroupedObject *nullGrouped() { return 0; }

    MyTypeObject *selfGroupProperty() { return this; }

    void doAction() { emit action(); }
signals:
    void action();

    void objectPropertyChanged();
    void flagPropertyChanged();
    void enumPropertyChanged();
    void qtEnumPropertyChanged();
    void mirroredEnumPropertyChanged();
    void stringPropertyChanged();
    void byteArrayPropertyChanged();
    void uintPropertyChanged();
    void intPropertyChanged();
    void realPropertyChanged();
    void doublePropertyChanged();
    void floatPropertyChanged();
    void colorPropertyChanged();
    void datePropertyChanged();
    void timePropertyChanged();
    void dateTimePropertyChanged();
    void pointPropertyChanged();
    void pointFPropertyChanged();
    void sizePropertyChanged();
    void sizeFPropertyChanged();
    void rectPropertyChanged();
    void rectFPropertyChanged();
    void boolPropertyChanged();
    void variantPropertyChanged();
    void vectorPropertyChanged();
    void vector2PropertyChanged();
    void vector4PropertyChanged();
    void quaternionPropertyChanged();
    void urlPropertyChanged();

};
Q_DECLARE_OPERATORS_FOR_FLAGS(MyTypeObject::MyFlags)

// FIXME: If no subclass is used for the singleton registration with qmlRegisterSingletonType(),
//        the valueTypes() test will fail.
class MyTypeObjectSingleton : public MyTypeObject
{
    Q_OBJECT
};

class MyContainer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<QObject> children READ children)
    Q_PROPERTY(QQmlListProperty<MyContainer> containerChildren READ containerChildren)
    Q_PROPERTY(QQmlListProperty<MyInterface> qlistInterfaces READ qlistInterfaces)
    Q_CLASSINFO("DefaultProperty", "children")
public:
    MyContainer() {}

    QQmlListProperty<QObject> children() { return QQmlListProperty<QObject>(this, m_children); }
    QQmlListProperty<MyContainer> containerChildren() { return QQmlListProperty<MyContainer>(this, m_containerChildren); }
    QList<QObject *> *getChildren() { return &m_children; }
    QQmlListProperty<MyInterface> qlistInterfaces() { return QQmlListProperty<MyInterface>(this, m_interfaces); }
    QList<MyInterface *> *getQListInterfaces() { return &m_interfaces; }

    QList<MyContainer*> m_containerChildren;
    QList<QObject*> m_children;
    QList<MyInterface *> m_interfaces;
};


class MyPropertyValueSource : public QObject, public QQmlPropertyValueSource
{
    Q_OBJECT
    Q_INTERFACES(QQmlPropertyValueSource)
public:
    MyPropertyValueSource()
        : QQmlPropertyValueSource() {}

    QQmlProperty prop;
    virtual void setTarget(const QQmlProperty &p)
    {
        prop = p;
    }
};

class UnavailableType : public QObject
{
    Q_OBJECT
public:
    UnavailableType() {}
};

class MyReceiversTestObject : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int prop READ prop NOTIFY propChanged)
public:
    MyReceiversTestObject()  {}

    int prop() const { return 5; }

    int mySignalCount() { return receivers(SIGNAL(mySignal())); }
    int propChangedCount() { return receivers(SIGNAL(propChanged())); }
    int myUnconnectedSignalCount() { return receivers(SIGNAL(myUnconnectedSignal())); }

signals:
    void mySignal();
    void propChanged();
    void myUnconnectedSignal();

private:
    friend class tst_qqmllanguage;
};

class MyDotPropertyObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(MyQmlObject *obj READ obj)
    Q_PROPERTY(MyQmlObject *readWriteObj READ readWriteObj WRITE setReadWriteObj)
public:
    MyDotPropertyObject() : m_rwobj(0), m_ownRWObj(false) {}
    ~MyDotPropertyObject()
    {
        if (m_ownRWObj)
            delete m_rwobj;
    }

    MyQmlObject *obj() { return 0; }

    MyQmlObject *readWriteObj()
    {
        if (!m_rwobj) {
            m_rwobj = new MyQmlObject;
            m_ownRWObj = true;
        }
        return m_rwobj;
    }

    void setReadWriteObj(MyQmlObject *obj)
    {
        if (m_ownRWObj) {
            delete m_rwobj;
            m_ownRWObj = false;
        }

        m_rwobj = obj;
    }

private:
    MyQmlObject *m_rwobj;
    bool m_ownRWObj;
};


namespace MyNamespace {
    Q_NAMESPACE
    enum MyNSEnum {
        Key1 = 1,
        Key2,
        Key5 = 5
    };
    Q_ENUM_NS(MyNSEnum);

    class MyNamespacedType : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(MyNamespace::MyNSEnum myEnum MEMBER m_myEnum)
        MyNamespace::MyNSEnum m_myEnum = MyNSEnum::Key1;
    };

    class MySecondNamespacedType : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(QQmlListProperty<MyNamespace::MyNamespacedType> list READ list)
    public:
        QQmlListProperty<MyNamespacedType> list() { return QQmlListProperty<MyNamespacedType>(this, m_list); }

    private:
        QList<MyNamespacedType *> m_list;
    };
}

class MyCustomParserType : public QObject
{
    Q_OBJECT
};

class MyCustomParserTypeParser : public QQmlCustomParser
{
public:
    virtual void verifyBindings(const QV4::CompiledData::Unit *, const QList<const QV4::CompiledData::Binding *> &) {}
    virtual void applyBindings(QObject *, QV4::CompiledData::CompilationUnit *, const QList<const QV4::CompiledData::Binding *> &) {}
};

class EnumSupportingCustomParser : public QQmlCustomParser
{
public:
    virtual void verifyBindings(const QV4::CompiledData::Unit *, const QList<const QV4::CompiledData::Binding *> &);
    virtual void applyBindings(QObject *, QV4::CompiledData::CompilationUnit *, const QList<const QV4::CompiledData::Binding *> &) {}
};

class MyParserStatus : public QObject, public QQmlParserStatus
{
    Q_INTERFACES(QQmlParserStatus)
    Q_OBJECT
public:
    MyParserStatus() : m_cbc(0), m_ccc(0) {}

    int classBeginCount() const { return m_cbc; }
    int componentCompleteCount() const { return m_ccc; }

    virtual void classBegin() { m_cbc++; }
    virtual void componentComplete() { m_ccc++; }
private:
    int m_cbc;
    int m_ccc;
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

class MyRevisionedIllegalOverload : public MyRevisionedBaseClassRegistered
{
    Q_OBJECT
    Q_PROPERTY(qreal propA READ propA WRITE setPropA REVISION 1);
};

class MyRevisionedLegalOverload : public MyRevisionedBaseClassRegistered
{
    Q_OBJECT
    Q_PROPERTY(qreal propB READ propB WRITE setPropB REVISION 1);
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
    MyRevisionedClass() : m_p1(1), m_p2(2) {}

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

class MySubclass : public MyRevisionedClass
{
    Q_OBJECT
    Q_PROPERTY(qreal prop5 READ prop5 WRITE setProp5 NOTIFY prop5Changed)

public:
    MySubclass() : m_p5(5) {}

    qreal prop5() const { return m_p5; }
    void setProp5(qreal p) {
        if (p != m_p5) {
            m_p5 = p;
            emit prop5Changed();
        }
    }

    Q_INVOKABLE void method5() { }

signals:
    void prop5Changed();

protected:
    qreal m_p5;
};

class MyUncreateableBaseClass : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool prop1 READ prop1 WRITE setprop1)
    Q_PROPERTY(bool prop2 READ prop2 WRITE setprop2 REVISION 1)
    Q_PROPERTY(bool prop3 READ prop3 WRITE setprop3 REVISION 1)
public:
    explicit MyUncreateableBaseClass(bool /* arg */, QObject *parent = 0)
        : QObject(parent), _prop1(false), _prop2(false), _prop3(false)
    {
    }

    bool _prop1;
    bool prop1() const { return _prop1; }
    void setprop1(bool p) { _prop1 = p; }
    bool _prop2;
    bool prop2() const { return _prop2; }
    void setprop2(bool p) { _prop2 = p; }
    bool _prop3;
    bool prop3() const { return _prop3; }
    void setprop3(bool p) { _prop3 = p; }
};

class MyCreateableDerivedClass : public MyUncreateableBaseClass
{
    Q_OBJECT
    Q_PROPERTY(bool prop2 READ prop2 WRITE setprop2 REVISION 1)

public:
    MyCreateableDerivedClass(QObject *parent = 0)
        : MyUncreateableBaseClass(true, parent)
    {
    }
};

class MyVersion2Class : public QObject
{
    Q_OBJECT
};

class MyEnum1Class : public QObject
{
    Q_OBJECT
    Q_ENUMS(EnumA)

public:
    MyEnum1Class() : value(A_Invalid) {}

    enum EnumA
    {
        A_Invalid = -1,

        A_11 = 11,
        A_13 = 13
    };

    Q_INVOKABLE void setValue(EnumA v) { value = v; }

    EnumA getValue() { return value; }

private:
    EnumA value;
};

class MyEnum2Class : public QObject
{
    Q_OBJECT
    Q_ENUMS(EnumB)
    Q_ENUMS(EnumE)

public:
    MyEnum2Class() : valueA(MyEnum1Class::A_Invalid), valueB(B_Invalid), valueC(Qt::PlainText),
                     valueD(Qt::ElideLeft), valueE(E_Invalid), valueE2(E_Invalid) {}

    enum EnumB
    {
        B_Invalid = -1,

        B_29 = 29,
        B_31 = 31,
        B_37 = 37
    };

    enum EnumE
    {
        E_Invalid = -1,

        E_14 = 14,
        E_76 = 76
    };

    MyEnum1Class::EnumA getValueA() { return valueA; }
    EnumB getValueB() { return valueB; }
    Qt::TextFormat getValueC() { return valueC; }
    Qt::TextElideMode getValueD() { return valueD; }
    EnumE getValueE() { return valueE; }
    EnumE getValueE2() { return valueE2; }

    Q_INVOKABLE void setValueA(MyEnum1Class::EnumA v) { valueA = v; emit valueAChanged(v); }
    Q_INVOKABLE void setValueB(EnumB v) { valueB = v; emit valueBChanged(v); }
    Q_INVOKABLE void setValueC(Qt::TextFormat v) { valueC = v; emit valueCChanged(v); }     //registered
    Q_INVOKABLE void setValueD(Qt::TextElideMode v) { valueD = v; emit valueDChanged(v); }  //unregistered
    Q_INVOKABLE void setValueE(EnumE v) { valueE = v; emit valueEChanged(v); }
    Q_INVOKABLE void setValueE2(MyEnum2Class::EnumE v) { valueE2 = v; emit valueE2Changed(v); }

signals:
    void valueAChanged(MyEnum1Class::EnumA newValue);
    void valueBChanged(MyEnum2Class::EnumB newValue);
    void valueCChanged(Qt::TextFormat newValue);
    void valueDChanged(Qt::TextElideMode newValue);
    void valueEChanged(EnumE newValue);
    void valueE2Changed(MyEnum2Class::EnumE newValue);

private:
    MyEnum1Class::EnumA valueA;
    EnumB valueB;
    Qt::TextFormat valueC;
    Qt::TextElideMode valueD;
    EnumE valueE;
    EnumE valueE2;
};

class MyEnumDerivedClass : public MyEnum2Class
{
    Q_OBJECT
};

class MyCompositeBaseType : public QObject
{
    Q_OBJECT
    Q_ENUMS(CompositeEnum)

public:
    enum CompositeEnum { EnumValue0, EnumValue42 = 42 };
    static QObject *qmlAttachedProperties(QObject *parent) { return new QObject(parent); }
};

class MyArrayBufferTestClass : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QByteArray byteArrayProperty READ byteArrayProperty WRITE setByteArrayProperty NOTIFY byteArrayPropertyChanged)

signals:
    void byteArrayPropertyChanged();
    void byteArraySignal(QByteArray arg);

public:
    QByteArray byteArrayPropertyValue;
    QByteArray byteArrayProperty() const {
        return byteArrayPropertyValue;
    }
    void setByteArrayProperty(const QByteArray &v) {
        byteArrayPropertyValue = v;
        emit byteArrayPropertyChanged();
    }
    Q_INVOKABLE void emitByteArraySignal(char begin, char num) {
        byteArraySignal(byteArrayMethod_CountUp(begin, num));
    }
    Q_INVOKABLE int byteArrayMethod_Sum(QByteArray arg) {
        int sum = 0;
        for (int i = 0; i < arg.size(); ++i) {
            sum += arg[i];
        }
        return sum;
    }
    Q_INVOKABLE QByteArray byteArrayMethod_CountUp(char begin, int num) {
        QByteArray ret;
        for (int i = 0; i < num; ++i) {
            ret.push_back(begin++);
        }
        return ret;
    }
    Q_INVOKABLE bool byteArrayMethod_Overloaded(QByteArray) {
        return true;
    }
    Q_INVOKABLE bool byteArrayMethod_Overloaded(int) {
        return false;
    }
    Q_INVOKABLE bool byteArrayMethod_Overloaded(QString) {
        return false;
    }
    Q_INVOKABLE bool byteArrayMethod_Overloaded(QJSValue) {
        return false;
    }
    Q_INVOKABLE bool byteArrayMethod_Overloaded(QVariant) {
        return false;
    }
};

Q_DECLARE_METATYPE(MyEnum2Class::EnumB)
Q_DECLARE_METATYPE(MyEnum1Class::EnumA)
Q_DECLARE_METATYPE(Qt::TextFormat)
Q_DECLARE_METATYPE(MyCompositeBaseType::CompositeEnum)

QML_DECLARE_TYPE(MyRevisionedBaseClassRegistered)
QML_DECLARE_TYPE(MyRevisionedBaseClassUnregistered)
QML_DECLARE_TYPE(MyRevisionedClass)
QML_DECLARE_TYPE(MyRevisionedSubclass)
QML_DECLARE_TYPE(MySubclass)
QML_DECLARE_TYPE(MyReceiversTestObject)
QML_DECLARE_TYPE(MyCompositeBaseType)
QML_DECLARE_TYPEINFO(MyCompositeBaseType, QML_HAS_ATTACHED_PROPERTIES)

class CustomBinding : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QObject* target READ target WRITE setTarget)
public:

    virtual void classBegin() {}
    virtual void componentComplete();

    QObject *target() const { return m_target; }
    void setTarget(QObject *newTarget) { m_target = newTarget; }

    QPointer<QObject> m_target;
    QQmlRefPointer<QV4::CompiledData::CompilationUnit> compilationUnit;
    QList<const QV4::CompiledData::Binding*> bindings;
    QByteArray m_bindingData;
};

class CustomBindingParser : public QQmlCustomParser
{
    virtual void verifyBindings(const QV4::CompiledData::Unit *, const QList<const QV4::CompiledData::Binding *> &) {}
    virtual void applyBindings(QObject *, QV4::CompiledData::CompilationUnit *, const QList<const QV4::CompiledData::Binding *> &);
};

class SimpleObjectWithCustomParser : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int intProperty READ intProperty WRITE setIntProperty)
public:
    SimpleObjectWithCustomParser()
        : m_intProperty(0)
        , m_customBindingsCount(0)
    {}

    int intProperty() const { return m_intProperty; }
    void setIntProperty(int value) { m_intProperty = value; }

    void setCustomBindingsCount(int count) { m_customBindingsCount = count; }
    int customBindingsCount() const { return m_customBindingsCount; }

private:
    int m_intProperty;
    int m_customBindingsCount;
};

class SimpleObjectExtension : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int extendedProperty READ extendedProperty WRITE setExtendedProperty NOTIFY extendedPropertyChanged)
public:
    SimpleObjectExtension(QObject *parent = 0)
        : QObject(parent)
        , m_extendedProperty(1584)
    {}

    void setExtendedProperty(int extendedProperty) { m_extendedProperty = extendedProperty; emit extendedPropertyChanged(); }
    int extendedProperty() const { return m_extendedProperty; }

signals:
   void extendedPropertyChanged();
private:
   int m_extendedProperty;
};

class SimpleObjectCustomParser : public QQmlCustomParser
{
    virtual void verifyBindings(const QV4::CompiledData::Unit *, const QList<const QV4::CompiledData::Binding *> &) {}
    virtual void applyBindings(QObject *, QV4::CompiledData::CompilationUnit *, const QList<const QV4::CompiledData::Binding *> &);
};

class RootObjectInCreationTester : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *subObject READ subObject WRITE setSubObject FINAL)
    Q_CLASSINFO("DeferredPropertyNames", "subObject");
public:
    RootObjectInCreationTester()
        : obj(0)
    {}

    QObject *subObject() const { return obj; }
    void setSubObject(QObject *o) { obj = o; }

private:
    QObject *obj;
};

void registerTypes();

#endif // TESTTYPES_H
