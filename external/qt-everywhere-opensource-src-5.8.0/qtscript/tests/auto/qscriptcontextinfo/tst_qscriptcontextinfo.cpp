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

#include <QtScript/qscriptcontextinfo.h>
#include <QtScript/qscriptcontext.h>
#include <QtScript/qscriptengine.h>
#include <QtScript/qscriptable.h>
#include <QtScript/qscriptengineagent.h>

Q_DECLARE_METATYPE(QScriptValue)
Q_DECLARE_METATYPE(QScriptContextInfo)
Q_DECLARE_METATYPE(QList<QScriptContextInfo>)

class tst_QScriptContextInfo : public QObject, public QScriptable
{
    Q_OBJECT
    Q_PROPERTY(QScriptValue testProperty READ testProperty WRITE setTestProperty)
public:
    tst_QScriptContextInfo();
    virtual ~tst_QScriptContextInfo();

    QScriptValue testProperty() const
    {
        return engine()->globalObject().property("getContextInfoList").call();
    }

    QScriptValue setTestProperty(const QScriptValue &) const
    {
        return engine()->globalObject().property("getContextInfoList").call();
    }

public slots:
    QScriptValue testSlot(double a, double b)
    {
        Q_UNUSED(a); Q_UNUSED(b);
        return engine()->globalObject().property("getContextInfoList").call();
    }

    QScriptValue testSlot(const QString &s)
    {
        Q_UNUSED(s);
        return engine()->globalObject().property("getContextInfoList").call();
    }

private slots:
    void nativeFunction();
    void scriptFunction();
    void qtFunction();
    void qtPropertyFunction();
    void nullContext();
    void streaming();
    void comparison_null();
    void assignmentAndComparison();
};

tst_QScriptContextInfo::tst_QScriptContextInfo()
{
}

tst_QScriptContextInfo::~tst_QScriptContextInfo()
{
}

static QScriptValue getContextInfoList(QScriptContext *ctx, QScriptEngine *eng)
{
    QList<QScriptContextInfo> lst;
    while (ctx) {
        QScriptContextInfo info(ctx);
        lst.append(info);
        ctx = ctx->parentContext();
    }
    return qScriptValueFromValue(eng, lst);
}

void tst_QScriptContextInfo::nativeFunction()
{
    QScriptEngine eng;
    eng.globalObject().setProperty("getContextInfoList", eng.newFunction(getContextInfoList));

    QString fileName = "foo.qs";
    int lineNumber = 123;
    QScriptValue ret = eng.evaluate("getContextInfoList()", fileName, lineNumber);
    QList<QScriptContextInfo> lst = qscriptvalue_cast<QList<QScriptContextInfo> >(ret);
    QCOMPARE(lst.size(), 2);

    {
        // getContextInfoList()
        QScriptContextInfo info = lst.at(0);
        QVERIFY(!info.isNull());
        QCOMPARE(info.functionType(), QScriptContextInfo::NativeFunction);
        QCOMPARE(info.scriptId(), (qint64)-1);
        QCOMPARE(info.fileName(), QString());
        QCOMPARE(info.lineNumber(), -1);
        QCOMPARE(info.columnNumber(), -1);
        QCOMPARE(info.functionName(), QString());
        QCOMPARE(info.functionEndLineNumber(), -1);
        QCOMPARE(info.functionStartLineNumber(), -1);
        QCOMPARE(info.functionParameterNames().size(), 0);
        QCOMPARE(info.functionMetaIndex(), -1);
    }

    {
        // evaluate()
        QScriptContextInfo info = lst.at(1);
        QVERIFY(!info.isNull());
        QCOMPARE(info.functionType(), QScriptContextInfo::NativeFunction);
        QVERIFY(info.scriptId() != -1);
        QCOMPARE(info.fileName(), fileName);
        QCOMPARE(info.lineNumber(), lineNumber);
        QEXPECT_FAIL("", "QTBUG-17602: columnNumber doesn't work", Continue);
        QCOMPARE(info.columnNumber(), 1);
        QCOMPARE(info.functionName(), QString());
        QCOMPARE(info.functionEndLineNumber(), -1);
        QCOMPARE(info.functionStartLineNumber(), -1);
        QCOMPARE(info.functionParameterNames().size(), 0);
        QCOMPARE(info.functionMetaIndex(), -1);
    }
}

void tst_QScriptContextInfo::scriptFunction()
{
    QScriptEngine eng;
    eng.globalObject().setProperty("getContextInfoList", eng.newFunction(getContextInfoList));

    QString fileName = "ciao.qs";
    int lineNumber = 456;
    QScriptValue ret = eng.evaluate("function bar(a, b, c) {\n return getContextInfoList();\n}\nbar()",
                                    fileName, lineNumber);
    QList<QScriptContextInfo> lst = qscriptvalue_cast<QList<QScriptContextInfo> >(ret);
    QCOMPARE(lst.size(), 3);

    // getContextInfoList()
    QCOMPARE(lst.at(0).functionType(), QScriptContextInfo::NativeFunction);

    {
        // bar()
        QScriptContextInfo info = lst.at(1);
        QCOMPARE(info.functionType(), QScriptContextInfo::ScriptFunction);
        QVERIFY(info.scriptId() != -1);
        QCOMPARE(info.fileName(), fileName);
        QCOMPARE(info.lineNumber(), lineNumber + 1);
        QEXPECT_FAIL("", "QTBUG-17602: columnNumber doesn't work", Continue);
        QCOMPARE(info.columnNumber(), 2);
        QCOMPARE(info.functionName(), QString::fromLatin1("bar"));
        QCOMPARE(info.functionStartLineNumber(), lineNumber);
        QCOMPARE(info.functionEndLineNumber(), lineNumber + 2);
        QCOMPARE(info.functionParameterNames().size(), 3);
        QCOMPARE(info.functionParameterNames().at(0), QString::fromLatin1("a"));
        QCOMPARE(info.functionParameterNames().at(1), QString::fromLatin1("b"));
        QCOMPARE(info.functionParameterNames().at(2), QString::fromLatin1("c"));
        QCOMPARE(info.functionMetaIndex(), -1);
    }

    {
        // evaluate()
        QScriptContextInfo info = lst.at(2);
        QCOMPARE(info.functionType(), QScriptContextInfo::NativeFunction);
        QVERIFY(info.scriptId() != -1);
        QCOMPARE(info.fileName(), fileName);
        QCOMPARE(info.lineNumber(), lineNumber + 3);
        QEXPECT_FAIL("", "QTBUG-17602: columnNumber doesn't work", Continue);
        QCOMPARE(info.columnNumber(), 1);
        QCOMPARE(info.functionName(), QString());
        QCOMPARE(info.functionEndLineNumber(), -1);
        QCOMPARE(info.functionStartLineNumber(), -1);
        QCOMPARE(info.functionParameterNames().size(), 0);
        QCOMPARE(info.functionMetaIndex(), -1);
    }
}

void tst_QScriptContextInfo::qtFunction()
{
    QScriptEngine eng;
    eng.globalObject().setProperty("getContextInfoList", eng.newFunction(getContextInfoList));
    eng.globalObject().setProperty("qobj", eng.newQObject(this));

    for (int x = 0; x < 2; ++x) { // twice to test overloaded slot as well
        QString code;
        const char *sig;
        QStringList pnames;
        if (x == 0) {
            code = "qobj.testSlot(1, 2)";
            sig = "testSlot(double,double)";
            pnames << "a" << "b";
        } else {
            code = "qobj.testSlot('ciao')";
            sig = "testSlot(QString)";
            pnames << "s";
        }
        QScriptValue ret = eng.evaluate(code);
        QList<QScriptContextInfo> lst = qscriptvalue_cast<QList<QScriptContextInfo> >(ret);
        QCOMPARE(lst.size(), 3);

        // getContextInfoList()
        QCOMPARE(lst.at(0).functionType(), QScriptContextInfo::NativeFunction);

        {
            // testSlot(double a, double b)
            QScriptContextInfo info = lst.at(1);
            QCOMPARE(info.functionType(), QScriptContextInfo::QtFunction);
            QCOMPARE(info.scriptId(), (qint64)-1);
            QCOMPARE(info.fileName(), QString());
            QCOMPARE(info.lineNumber(), -1);
            QCOMPARE(info.columnNumber(), -1);
            QCOMPARE(info.functionName(), QString::fromLatin1("testSlot"));
            QCOMPARE(info.functionEndLineNumber(), -1);
            QCOMPARE(info.functionStartLineNumber(), -1);
            QCOMPARE(info.functionParameterNames().size(), pnames.size());
            QCOMPARE(info.functionParameterNames(), pnames);
            QCOMPARE(info.functionMetaIndex(), metaObject()->indexOfMethod(sig));
        }

        // evaluate()
        QCOMPARE(lst.at(0).functionType(), QScriptContextInfo::NativeFunction);
    }
}

void tst_QScriptContextInfo::qtPropertyFunction()
{
    QScriptEngine eng;
    eng.globalObject().setProperty("getContextInfoList", eng.newFunction(getContextInfoList));
    eng.globalObject().setProperty("qobj", eng.newQObject(this));

    QScriptValue ret = eng.evaluate("qobj.testProperty");
    QList<QScriptContextInfo> lst = qscriptvalue_cast<QList<QScriptContextInfo> >(ret);
    QCOMPARE(lst.size(), 3);

    // getContextInfoList()
    QCOMPARE(lst.at(0).functionType(), QScriptContextInfo::NativeFunction);

    {
        // testProperty()
        QScriptContextInfo info = lst.at(1);
        QCOMPARE(info.functionType(), QScriptContextInfo::QtPropertyFunction);
        QCOMPARE(info.scriptId(), (qint64)-1);
        QCOMPARE(info.fileName(), QString());
        QCOMPARE(info.lineNumber(), -1);
        QCOMPARE(info.columnNumber(), -1);
        QCOMPARE(info.functionName(), QString::fromLatin1("testProperty"));
        QCOMPARE(info.functionEndLineNumber(), -1);
        QCOMPARE(info.functionStartLineNumber(), -1);
        QCOMPARE(info.functionParameterNames().size(), 0);
        QCOMPARE(info.functionMetaIndex(), metaObject()->indexOfProperty("testProperty"));
    }

    // evaluate()
    QCOMPARE(lst.at(0).functionType(), QScriptContextInfo::NativeFunction);
}

void tst_QScriptContextInfo::nullContext()
{
    QScriptContextInfo info((QScriptContext*)0);
    QVERIFY(info.isNull());
    QCOMPARE(info.columnNumber(), -1);
    QCOMPARE(info.scriptId(), (qint64)-1);
    QCOMPARE(info.fileName(), QString());
    QCOMPARE(info.functionEndLineNumber(), -1);
    QCOMPARE(info.functionMetaIndex(), -1);
    QCOMPARE(info.functionName(), QString());
    QCOMPARE(info.functionParameterNames().size(), 0);
    QCOMPARE(info.functionStartLineNumber(), -1);
    QCOMPARE(info.functionType(), QScriptContextInfo::NativeFunction);
}

void tst_QScriptContextInfo::streaming()
{
    {
        QScriptContextInfo info((QScriptContext*)0);
        QByteArray ba;
        QDataStream stream(&ba, QIODevice::ReadWrite);
        stream << info;
        stream.device()->seek(0);
        QScriptContextInfo info2;
        stream >> info2;
        QVERIFY(stream.device()->atEnd());
        QCOMPARE(info.functionType(), info2.functionType());
        QCOMPARE(info.scriptId(), info2.scriptId());
        QCOMPARE(info.fileName(), info2.fileName());
        QCOMPARE(info.lineNumber(), info2.lineNumber());
        QCOMPARE(info.columnNumber(), info2.columnNumber());
        QCOMPARE(info.functionName(), info2.functionName());
        QCOMPARE(info.functionEndLineNumber(), info2.functionEndLineNumber());
        QCOMPARE(info.functionStartLineNumber(), info2.functionStartLineNumber());
        QCOMPARE(info.functionParameterNames(), info2.functionParameterNames());
        QCOMPARE(info.functionMetaIndex(), info2.functionMetaIndex());
    }
    {
        QScriptEngine eng;
        eng.globalObject().setProperty("getContextInfoList", eng.newFunction(getContextInfoList));

        QString fileName = "ciao.qs";
        int lineNumber = 456;
        QScriptValue ret = eng.evaluate("function bar(a, b, c) {\n return getContextInfoList();\n}\nbar()",
                                        fileName, lineNumber);
        QList<QScriptContextInfo> lst = qscriptvalue_cast<QList<QScriptContextInfo> >(ret);
        QCOMPARE(lst.size(), 3);
        for (int i = 0; i < lst.size(); ++i) {
            const QScriptContextInfo &info = lst.at(i);
            QByteArray ba;
            QDataStream stream(&ba, QIODevice::ReadWrite);
            stream << info;
            stream.device()->seek(0);
            QScriptContextInfo info2;
            stream >> info2;
            QVERIFY(stream.device()->atEnd());
            QCOMPARE(info.functionType(), info2.functionType());
            QCOMPARE(info.scriptId(), info2.scriptId());
            QCOMPARE(info.fileName(), info2.fileName());
            QCOMPARE(info.lineNumber(), info2.lineNumber());
            QCOMPARE(info.columnNumber(), info2.columnNumber());
            QCOMPARE(info.functionName(), info2.functionName());
            QCOMPARE(info.functionEndLineNumber(), info2.functionEndLineNumber());
            QCOMPARE(info.functionStartLineNumber(), info2.functionStartLineNumber());
            QCOMPARE(info.functionParameterNames(), info2.functionParameterNames());
            QCOMPARE(info.functionMetaIndex(), info2.functionMetaIndex());
        }
    }
}

void tst_QScriptContextInfo::comparison_null()
{
    QScriptContextInfo info1, info2, info3(0);
    QCOMPARE(info1, info2);
    QCOMPARE(info1, info3);
}

void tst_QScriptContextInfo::assignmentAndComparison()
{
    QScriptEngine eng;
    eng.globalObject().setProperty("getContextInfoList", eng.newFunction(getContextInfoList));
    QString fileName = "ciao.qs";
    int lineNumber = 456;
    QScriptValue ret = eng.evaluate("function bar(a, b, c) {\n return getContextInfoList();\n}\nbar()",
                                    fileName, lineNumber);
    QList<QScriptContextInfo> lst = qscriptvalue_cast<QList<QScriptContextInfo> >(ret);
    QCOMPARE(lst.size(), 3);
    QScriptContextInfo ci = lst.at(0);
    QScriptContextInfo same = ci;
    QVERIFY(ci == same);
    QVERIFY(!(ci != same));
    QScriptContextInfo other = lst.at(1);
    QVERIFY(!(ci == other));
    QVERIFY(ci != other);
}

QTEST_MAIN(tst_QScriptContextInfo)
#include "tst_qscriptcontextinfo.moc"
