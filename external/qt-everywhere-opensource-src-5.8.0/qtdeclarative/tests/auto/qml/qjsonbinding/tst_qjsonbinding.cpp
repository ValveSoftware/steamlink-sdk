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
#include <QtQml/QtQml>
#include "../../shared/util.h"

Q_DECLARE_METATYPE(QJsonValue::Type)

class JsonPropertyObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJsonValue value READ value WRITE setValue)
    Q_PROPERTY(QJsonObject object READ object WRITE setObject)
    Q_PROPERTY(QJsonArray array READ array WRITE setArray)
public:
    QJsonValue value() const { return m_value; }
    void setValue(const QJsonValue &v) { m_value = v; }
    QJsonObject object() const { return m_object; }
    void setObject(const QJsonObject &o) { m_object = o; }
    QJsonArray array() const { return m_array; }
    void setArray(const QJsonArray &a) { m_array = a; }

private:
    QJsonValue m_value;
    QJsonObject m_object;
    QJsonArray m_array;
};

class tst_qjsonbinding : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qjsonbinding() {}

private slots:
    void cppJsConversion_data();
    void cppJsConversion();

    void readValueProperty_data();
    void readValueProperty();
    void readObjectOrArrayProperty_data();
    void readObjectOrArrayProperty();

    void writeValueProperty_data();
    void writeValueProperty();
    void writeObjectOrArrayProperty_data();
    void writeObjectOrArrayProperty();

    void writeProperty_incompatibleType_data();
    void writeProperty_incompatibleType();

    void writeProperty_javascriptExpression_data();
    void writeProperty_javascriptExpression();

private:
    QByteArray readAsUtf8(const QString &fileName);
    static QJsonValue valueFromJson(const QByteArray &json);

    void addPrimitiveDataTestFiles();
    void addObjectDataTestFiles();
    void addArrayDataTestFiles();
};

QByteArray tst_qjsonbinding::readAsUtf8(const QString &fileName)
{
    QFile file(testFile(fileName));
    file.open(QIODevice::ReadOnly);
    QTextStream stream(&file);
    return stream.readAll().trimmed().toUtf8();
}

QJsonValue tst_qjsonbinding::valueFromJson(const QByteArray &json)
{
    if (json.isEmpty())
        return QJsonValue(QJsonValue::Undefined);

    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isEmpty())
        return doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array());

    // QJsonDocument::fromJson() only handles objects and arrays...
    // Wrap the JSON inside a dummy object and extract the value.
    QByteArray wrappedJson = "{\"prop\":" + json + '}';
    doc = QJsonDocument::fromJson(wrappedJson);
    Q_ASSERT(doc.isObject());
    return doc.object().value("prop");
}

void tst_qjsonbinding::addPrimitiveDataTestFiles()
{
    QTest::newRow("true") << "true.json";
    QTest::newRow("false") << "false.json";

    QTest::newRow("null") << "null.json";

    QTest::newRow("number.0") << "number.0.json";
    QTest::newRow("number.1") << "number.1.json";

    QTest::newRow("string.0") << "string.0.json";

    QTest::newRow("undefined") << "empty.json";
}

void tst_qjsonbinding::addObjectDataTestFiles()
{
    QTest::newRow("object.0") << "object.0.json";
    QTest::newRow("object.1") << "object.1.json";
    QTest::newRow("object.2") << "object.2.json";
    QTest::newRow("object.3") << "object.3.json";
    QTest::newRow("object.4") << "object.4.json";
}

void tst_qjsonbinding::addArrayDataTestFiles()
{
    QTest::newRow("array.0") << "array.0.json";
    QTest::newRow("array.1") << "array.1.json";
    QTest::newRow("array.2") << "array.2.json";
    QTest::newRow("array.3") << "array.3.json";
    QTest::newRow("array.4") << "array.4.json";
}

void tst_qjsonbinding::cppJsConversion_data()
{
    QTest::addColumn<QString>("fileName");

    addPrimitiveDataTestFiles();
    addObjectDataTestFiles();
    addArrayDataTestFiles();
}

void tst_qjsonbinding::cppJsConversion()
{
    QFETCH(QString, fileName);

    QByteArray json = readAsUtf8(fileName);
    QJsonValue jsonValue = valueFromJson(json);

    QJSEngine eng;
    QJSValue stringify = eng.globalObject().property("JSON").property("stringify");
    QVERIFY(stringify.isCallable());

    {
        QJSValue jsValue = eng.toScriptValue(jsonValue);
        QVERIFY(!jsValue.isVariant());
        switch (jsonValue.type()) {
        case QJsonValue::Null:
            QVERIFY(jsValue.isNull());
            break;
        case QJsonValue::Bool:
            QVERIFY(jsValue.isBool());
            QCOMPARE(jsValue.toBool(), jsonValue.toBool());
            break;
        case QJsonValue::Double:
            QVERIFY(jsValue.isNumber());
            QCOMPARE(jsValue.toNumber(), jsonValue.toDouble());
            break;
        case QJsonValue::String:
            QVERIFY(jsValue.isString());
            QCOMPARE(jsValue.toString(), jsonValue.toString());
            break;
        case QJsonValue::Array:
            QVERIFY(jsValue.isArray());
            break;
        case QJsonValue::Object:
            QVERIFY(jsValue.isObject());
            break;
        case QJsonValue::Undefined:
            QVERIFY(jsValue.isUndefined());
            break;
        }

        if (jsValue.isUndefined()) {
            QVERIFY(json.isEmpty());
        } else {
            QJSValue stringified = stringify.call(QJSValueList() << jsValue);
            QVERIFY(!stringified.isError());
            QCOMPARE(stringified.toString().toUtf8(), json);
        }

        QJsonValue roundtrip = qjsvalue_cast<QJsonValue>(jsValue);
        // Workarounds for QTBUG-25164
        if (jsonValue.isObject() && jsonValue.toObject().isEmpty())
            QVERIFY(roundtrip.isObject() && roundtrip.toObject().isEmpty());
        else if (jsonValue.isArray() && jsonValue.toArray().isEmpty())
            QVERIFY(roundtrip.isArray() && roundtrip.toArray().isEmpty());
        else
            QCOMPARE(roundtrip, jsonValue);
    }

    if (jsonValue.isObject()) {
        QJsonObject jsonObject = jsonValue.toObject();
        QJSValue jsObject = eng.toScriptValue(jsonObject);
        QVERIFY(!jsObject.isVariant());
        QVERIFY(jsObject.isObject());

        QJSValue stringified = stringify.call(QJSValueList() << jsObject);
        QVERIFY(!stringified.isError());
        QCOMPARE(stringified.toString().toUtf8(), json);

        QJsonObject roundtrip = qjsvalue_cast<QJsonObject>(jsObject);
        QCOMPARE(roundtrip, jsonObject);
    } else if (jsonValue.isArray()) {
        QJsonArray jsonArray = jsonValue.toArray();
        QJSValue jsArray = eng.toScriptValue(jsonArray);
        QVERIFY(!jsArray.isVariant());
        QVERIFY(jsArray.isArray());

        QJSValue stringified = stringify.call(QJSValueList() << jsArray);
        QVERIFY(!stringified.isError());
        QCOMPARE(stringified.toString().toUtf8(), json);

        QJsonArray roundtrip = qjsvalue_cast<QJsonArray>(jsArray);
        QCOMPARE(roundtrip, jsonArray);
    }
}

void tst_qjsonbinding::readValueProperty_data()
{
    cppJsConversion_data();
}

void tst_qjsonbinding::readValueProperty()
{
    QFETCH(QString, fileName);

    QByteArray json = readAsUtf8(fileName);
    QJsonValue jsonValue = valueFromJson(json);

    QJSEngine eng;
    JsonPropertyObject obj;
    obj.setValue(jsonValue);
    eng.globalObject().setProperty("obj", eng.newQObject(&obj));
    QJSValue stringified = eng.evaluate(
                "var v = obj.value; (typeof v == 'undefined') ? '' : JSON.stringify(v)");
    QVERIFY(!stringified.isError());
    QCOMPARE(stringified.toString().toUtf8(), json);
}

void tst_qjsonbinding::readObjectOrArrayProperty_data()
{
    QTest::addColumn<QString>("fileName");

    addObjectDataTestFiles();
    addArrayDataTestFiles();
}

void tst_qjsonbinding::readObjectOrArrayProperty()
{
    QFETCH(QString, fileName);

    QByteArray json = readAsUtf8(fileName);
    QJsonValue jsonValue = valueFromJson(json);
    QVERIFY(jsonValue.isObject() || jsonValue.isArray());

    QJSEngine eng;
    JsonPropertyObject obj;
    if (jsonValue.isObject())
        obj.setObject(jsonValue.toObject());
    else
        obj.setArray(jsonValue.toArray());
    eng.globalObject().setProperty("obj", eng.newQObject(&obj));

    QJSValue stringified = eng.evaluate(
                QString::fromLatin1("JSON.stringify(obj.%0)").arg(
                    jsonValue.isObject() ? "object" : "array"));
    QVERIFY(!stringified.isError());
    QCOMPARE(stringified.toString().toUtf8(), json);
}

void tst_qjsonbinding::writeValueProperty_data()
{
    readValueProperty_data();
}

void tst_qjsonbinding::writeValueProperty()
{
    QFETCH(QString, fileName);

    QByteArray json = readAsUtf8(fileName);
    QJsonValue jsonValue = valueFromJson(json);

    QJSEngine eng;
    JsonPropertyObject obj;
    eng.globalObject().setProperty("obj", eng.newQObject(&obj));

    QJSValue fun = eng.evaluate(
                "(function(json) {"
                "  void(obj.value = (json == '') ? undefined : JSON.parse(json));"
                "})");
    QVERIFY(fun.isCallable());

    QVERIFY(obj.value().isNull());
    QVERIFY(fun.call(QJSValueList() << QString::fromUtf8(json)).isUndefined());

    // Workarounds for QTBUG-25164
    if (jsonValue.isObject() && jsonValue.toObject().isEmpty())
        QVERIFY(obj.value().isObject() && obj.value().toObject().isEmpty());
    else if (jsonValue.isArray() && jsonValue.toArray().isEmpty())
        QVERIFY(obj.value().isArray() && obj.value().toArray().isEmpty());
    else
        QCOMPARE(obj.value(), jsonValue);
}

void tst_qjsonbinding::writeObjectOrArrayProperty_data()
{
    readObjectOrArrayProperty_data();
}

void tst_qjsonbinding::writeObjectOrArrayProperty()
{
    QFETCH(QString, fileName);

    QByteArray json = readAsUtf8(fileName);
    QJsonValue jsonValue = valueFromJson(json);
    QVERIFY(jsonValue.isObject() || jsonValue.isArray());

    QJSEngine eng;
    JsonPropertyObject obj;
    eng.globalObject().setProperty("obj", eng.newQObject(&obj));

    QJSValue fun = eng.evaluate(
                QString::fromLatin1(
                    "(function(json) {"
                    "  void(obj.%0 = JSON.parse(json));"
                    "})").arg(jsonValue.isObject() ? "object" : "array")
                );
    QVERIFY(fun.isCallable());

    QVERIFY(obj.object().isEmpty() && obj.array().isEmpty());
    QVERIFY(fun.call(QJSValueList() << QString::fromUtf8(json)).isUndefined());

    if (jsonValue.isObject())
        QCOMPARE(obj.object(), jsonValue.toObject());
    else
        QCOMPARE(obj.array(), jsonValue.toArray());
}

void tst_qjsonbinding::writeProperty_incompatibleType_data()
{
    QTest::addColumn<QString>("property");
    QTest::addColumn<QString>("expression");

    QTest::newRow("value=function") << "value" << "(function(){})";

    QTest::newRow("object=undefined") << "object" << "undefined";
    QTest::newRow("object=null") << "object" << "null";
    QTest::newRow("object=false") << "object" << "false";
    QTest::newRow("object=true") << "object" << "true";
    QTest::newRow("object=123") << "object" << "123";
    QTest::newRow("object=42.35") << "object" << "42.35";
    QTest::newRow("object='foo'") << "object" << "'foo'";
    QTest::newRow("object=[]") << "object" << "[]";
    QTest::newRow("object=function") << "object" << "(function(){})";

    QTest::newRow("array=undefined") << "array" << "undefined";
    QTest::newRow("array=null") << "array" << "null";
    QTest::newRow("array=false") << "array" << "false";
    QTest::newRow("array=true") << "array" << "true";
    QTest::newRow("array=123") << "array" << "123";
    QTest::newRow("array=42.35") << "array" << "42.35";
    QTest::newRow("array='foo'") << "array" << "'foo'";
    QTest::newRow("array={}") << "array" << "{}";
    QTest::newRow("array=function") << "array" << "(function(){})";
}

void tst_qjsonbinding::writeProperty_incompatibleType()
{
    QFETCH(QString, property);
    QFETCH(QString, expression);

    QJSEngine eng;
    JsonPropertyObject obj;
    eng.globalObject().setProperty("obj", eng.newQObject(&obj));

    QJSValue ret = eng.evaluate(QString::fromLatin1("obj.%0 = %1")
                                .arg(property).arg(expression));
    QVERIFY(ret.isError());
    QVERIFY(ret.toString().contains("Cannot assign"));
}

void tst_qjsonbinding::writeProperty_javascriptExpression_data()
{
    QTest::addColumn<QString>("property");
    QTest::addColumn<QString>("expression");
    QTest::addColumn<QString>("expectedJson");

    // Function properties should be omitted.
    QTest::newRow("value = object with function property")
            << "value" << "{ foo: function() {} }" << "{}";
    QTest::newRow("object = object with function property")
            << "object" << "{ foo: function() {} }" << "{}";
    QTest::newRow("array = array with function property")
            << "array" << "[function() {}]" << "[null]";

    // Inherited properties should not be included.
    QTest::newRow("value = object with inherited property")
            << "value" << "{ __proto__: { proto_foo: 123 } }"
            << "{}";
    QTest::newRow("value = object with inherited property 2")
            << "value" << "{ foo: 123, __proto__: { proto_foo: 456 } }"
            << "{\"foo\":123}";
    QTest::newRow("value = array with inherited property")
            << "value" << "(function() { var a = []; a.__proto__ = { proto_foo: 123 }; return a; })()"
            << "[]";
    QTest::newRow("value = array with inherited property 2")
            << "value" << "(function() { var a = [10, 20]; a.__proto__ = { proto_foo: 123 }; return a; })()"
            << "[10,20]";

    QTest::newRow("object = object with inherited property")
            << "object" << "{ __proto__: { proto_foo: 123 } }"
            << "{}";
    QTest::newRow("object = object with inherited property 2")
            << "object" << "{ foo: 123, __proto__: { proto_foo: 456 } }"
            << "{\"foo\":123}";
    QTest::newRow("array = array with inherited property")
            << "array" << "(function() { var a = []; a.__proto__ = { proto_foo: 123 }; return a; })()"
            << "[]";
    QTest::newRow("array = array with inherited property 2")
            << "array" << "(function() { var a = [10, 20]; a.__proto__ = { proto_foo: 123 }; return a; })()"
            << "[10,20]";

    // Non-enumerable properties should not be included.
    QTest::newRow("value = object with non-enumerable property")
            << "value" << "Object.defineProperty({}, 'foo', { value: 123, enumerable: false })"
            << "{}";
    QTest::newRow("object = object with non-enumerable property")
            << "object" << "Object.defineProperty({}, 'foo', { value: 123, enumerable: false })"
            << "{}";

    // Cyclic data structures are permitted, but the cyclic links become
    // empty objects.
    QTest::newRow("value = cyclic object")
            << "value" << "(function() { var o = { foo: 123 }; o.o = o; return o; })()"
            << "{\"foo\":123,\"o\":{}}";
    QTest::newRow("value = cyclic array")
            << "value" << "(function() { var a = [10, 20]; a.push(a); return a; })()"
            << "[10,20,[]]";
    QTest::newRow("object = cyclic object")
            << "object" << "(function() { var o = { bar: true }; o.o = o; return o; })()"
            << "{\"bar\":true,\"o\":{}}";
    QTest::newRow("array = cyclic array")
            << "array" << "(function() { var a = [30, 40]; a.unshift(a); return a; })()"
            << "[[],30,40]";

    // Properties with undefined value are excluded.
    QTest::newRow("value = { foo: undefined }")
            << "value" << "{ foo: undefined }" << "{}";
    QTest::newRow("value = { foo: undefined, bar: 123 }")
            << "value" << "{ foo: undefined, bar: 123 }" << "{\"bar\":123}";
    QTest::newRow("value = { foo: 456, bar: undefined }")
            << "value" << "{ foo: 456, bar: undefined }" << "{\"foo\":456}";

    QTest::newRow("object = { foo: undefined }")
            << "object" << "{ foo: undefined }" << "{}";
    QTest::newRow("object = { foo: undefined, bar: 123 }")
            << "object" << "{ foo: undefined, bar: 123 }" << "{\"bar\":123}";
    QTest::newRow("object = { foo: 456, bar: undefined }")
            << "object" << "{ foo: 456, bar: undefined }" << "{\"foo\":456}";

    // QJsonArray::append() implicitly converts undefined values to null.
    QTest::newRow("value = [undefined]")
            << "value" << "[undefined]" << "[null]";
    QTest::newRow("value = [undefined, 10]")
            << "value" << "[undefined, 10]" << "[null,10]";
    QTest::newRow("value = [10, undefined, 20]")
            << "value" << "[10, undefined, 20]" << "[10,null,20]";

    QTest::newRow("array = [undefined]")
            << "array" << "[undefined]" << "[null]";
    QTest::newRow("array = [undefined, 10]")
            << "array" << "[undefined, 10]" << "[null,10]";
    QTest::newRow("array = [10, undefined, 20]")
            << "array" << "[10, undefined, 20]" << "[10,null,20]";
}

void tst_qjsonbinding::writeProperty_javascriptExpression()
{
    QFETCH(QString, property);
    QFETCH(QString, expression);
    QFETCH(QString, expectedJson);

    QJSEngine eng;
    JsonPropertyObject obj;
    eng.globalObject().setProperty("obj", eng.newQObject(&obj));

    QJSValue ret = eng.evaluate(QString::fromLatin1("obj.%0 = %1; JSON.stringify(obj.%0)")
                                .arg(property).arg(expression));
    QVERIFY(!ret.isError());
    QCOMPARE(ret.toString(), expectedJson);
}

QTEST_MAIN(tst_qjsonbinding)

#include "tst_qjsonbinding.moc"
