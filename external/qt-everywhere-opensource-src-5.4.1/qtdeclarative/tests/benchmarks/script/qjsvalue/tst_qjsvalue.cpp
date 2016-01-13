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
#include <QJSEngine>
#include <QJSValue>
#include <private/v8.h>

QT_BEGIN_NAMESPACE
extern Q_QML_EXPORT v8::Local<v8::Context> qt_QJSEngineV8Context(QJSEngine *);
extern Q_QML_EXPORT v8::Local<v8::Value> qt_QJSValueV8Value(const QJSValue &);
QT_END_NAMESPACE

class tst_QJSValue : public QObject
{
    Q_OBJECT
public:
    tst_QJSValue() {}

private slots:
    void fillArray();
    void fillArray_V8();

    void property();
    void property_V8();

    void setProperty();
    void setProperty_V8();

    void call();
    void call_V8();
};

void tst_QJSValue::fillArray()
{
    QJSEngine eng;
    static const int ArrayLength = 10000;
    QJSValue array = eng.newArray(ArrayLength);
    QBENCHMARK {
        for (int i = 0; i < ArrayLength; ++i)
            array.setProperty(i, i);
    }
}

void tst_QJSValue::fillArray_V8()
{
    QJSEngine eng;
    static const int ArrayLength = 10000;
    QJSValue array = eng.newArray(ArrayLength);

    v8::HandleScope handleScope;
    v8::Local<v8::Array> v8array = qt_QJSValueV8Value(array).As<v8::Array>();
    QBENCHMARK {
        for (int i = 0; i < ArrayLength; ++i)
            v8array->Set(i, v8::Number::New(i));
    }
}

void tst_QJSValue::property()
{
    QJSEngine eng;
    QJSValue object = eng.newObject();
    QString propertyName = QString::fromLatin1("foo");
    object.setProperty(propertyName, 123);
    QVERIFY(object.property(propertyName).isNumber());
    QBENCHMARK {
        object.property(propertyName);
    }
}

void tst_QJSValue::property_V8()
{
    QJSEngine eng;
    QJSValue object = eng.newObject();
    QString propertyName = QString::fromLatin1("foo");
    object.setProperty(propertyName, 123);
    QVERIFY(object.property(propertyName).isNumber());

    v8::HandleScope handleScope;
    v8::Local<v8::Object> v8object = qt_QJSValueV8Value(object).As<v8::Object>();
    v8::Local<v8::String> v8propertyName = v8::String::New("foo");
    QVERIFY(v8object->Get(v8propertyName)->IsNumber());
    QBENCHMARK {
        v8object->Get(v8propertyName);
    }
}

void tst_QJSValue::setProperty()
{
    QJSEngine eng;
    QJSValue object = eng.newObject();
    QString propertyName = QString::fromLatin1("foo");
    QJSValue value(123);
    QBENCHMARK {
        object.setProperty(propertyName, value);
    }
}

void tst_QJSValue::setProperty_V8()
{
    QJSEngine eng;
    QJSValue object = eng.newObject();

    v8::HandleScope handleScope;
    // A context scope is needed for v8::Object::Set(), otherwise we crash.
    v8::Local<v8::Context> context = qt_QJSEngineV8Context(&eng);
    v8::Context::Scope contextScope(context);

    v8::Local<v8::Object> v8object = qt_QJSValueV8Value(object).As<v8::Object>();
    v8::Local<v8::String> v8propertyName = v8::String::New("foo");
    v8::Local<v8::Value> v8value = v8::Number::New(123);
    QBENCHMARK {
        v8object->Set(v8propertyName, v8value);
    }
}

#define TEST_FUNCTION_SOURCE "(function() { return 123; })"

void tst_QJSValue::call()
{
    QJSEngine eng;
    QJSValue fun = eng.evaluate(TEST_FUNCTION_SOURCE);
    QVERIFY(fun.isCallable());
    QJSValueList args;
    QVERIFY(fun.call(args).isNumber());
    QBENCHMARK {
        fun.call(args);
    }
}

void tst_QJSValue::call_V8()
{
    QJSEngine eng;
    QJSValue fun = eng.evaluate(TEST_FUNCTION_SOURCE);
    QVERIFY(fun.isCallable());

    v8::HandleScope handleScope;
    v8::Local<v8::Context> context = qt_QJSEngineV8Context(&eng);
    v8::Context::Scope contextScope(context);

    v8::Local<v8::Function> v8fun = qt_QJSValueV8Value(fun).As<v8::Function>();
    v8::Local<v8::Object> v8thisObject = v8::Object::New();
    QVERIFY(v8fun->Call(v8thisObject, /*argc=*/0, /*argv=*/0)->IsNumber());
    QBENCHMARK {
        v8fun->Call(v8thisObject, /*argc=*/0, /*argv=*/0);
    }
}

QTEST_MAIN(tst_QJSValue)

#include "tst_qjsvalue.moc"
