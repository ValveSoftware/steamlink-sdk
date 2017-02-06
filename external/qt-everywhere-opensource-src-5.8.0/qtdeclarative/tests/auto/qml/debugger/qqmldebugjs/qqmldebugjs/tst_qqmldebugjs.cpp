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

#include "debugutil_p.h"
#include "../../../../shared/util.h"

#include <private/qqmldebugclient_p.h>
#include <private/qqmldebugconnection_p.h>
#include <private/qpacket_p.h>

#include <QtTest/qtest.h>
#include <QtCore/qprocess.h>
#include <QtCore/qtimer.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qdir.h>
#include <QtCore/qmutex.h>
#include <QtCore/qlibraryinfo.h>
#include <QtQml/qjsengine.h>

const char *V8REQUEST = "v8request";
const char *V8MESSAGE = "v8message";
const char *SEQ = "seq";
const char *TYPE = "type";
const char *COMMAND = "command";
const char *ARGUMENTS = "arguments";
const char *STEPACTION = "stepaction";
const char *STEPCOUNT = "stepcount";
const char *EXPRESSION = "expression";
const char *FRAME = "frame";
const char *GLOBAL = "global";
const char *DISABLEBREAK = "disable_break";
const char *HANDLES = "handles";
const char *INCLUDESOURCE = "includeSource";
const char *FROMFRAME = "fromFrame";
const char *TOFRAME = "toFrame";
const char *BOTTOM = "bottom";
const char *NUMBER = "number";
const char *FRAMENUMBER = "frameNumber";
const char *TYPES = "types";
const char *IDS = "ids";
const char *FILTER = "filter";
const char *FROMLINE = "fromLine";
const char *TOLINE = "toLine";
const char *TARGET = "target";
const char *LINE = "line";
const char *COLUMN = "column";
const char *ENABLED = "enabled";
const char *CONDITION = "condition";
const char *IGNORECOUNT = "ignoreCount";
const char *BREAKPOINT = "breakpoint";
const char *FLAGS = "flags";

const char *CONTINEDEBUGGING = "continue";
const char *EVALUATE = "evaluate";
const char *LOOKUP = "lookup";
const char *BACKTRACE = "backtrace";
const char *SCOPE = "scope";
const char *SCOPES = "scopes";
const char *SCRIPTS = "scripts";
const char *SOURCE = "source";
const char *SETBREAKPOINT = "setbreakpoint";
const char *CLEARBREAKPOINT = "clearbreakpoint";
const char *SETEXCEPTIONBREAK = "setexceptionbreak";
const char *VERSION = "version";
const char *DISCONNECT = "disconnect";
const char *GARBAGECOLLECTOR = "gc";

const char *CONNECT = "connect";
const char *INTERRUPT = "interrupt";

const char *REQUEST = "request";
const char *IN = "in";
const char *NEXT = "next";
const char *OUT = "out";

const char *SCRIPT = "script";
const char *SCRIPTREGEXP = "scriptRegExp";
const char *EVENT = "event";

const char *ALL = "all";
const char *UNCAUGHT = "uncaught";

const char *BLOCKMODE = "-qmljsdebugger=port:3771,3800,block";
const char *NORMALMODE = "-qmljsdebugger=port:3771,3800";
const char *BLOCKRESTRICTEDMODE = "-qmljsdebugger=port:3771,3800,block,services:V8Debugger";
const char *NORMALRESTRICTEDMODE = "-qmljsdebugger=port:3771,3800,services:V8Debugger";
const char *TEST_QMLFILE = "test.qml";
const char *TEST_JSFILE = "test.js";
const char *TIMER_QMLFILE = "timer.qml";
const char *LOADJSFILE_QMLFILE = "loadjsfile.qml";
const char *EXCEPTION_QMLFILE = "exception.qml";
const char *ONCOMPLETED_QMLFILE = "oncompleted.qml";
const char *CREATECOMPONENT_QMLFILE = "createComponent.qml";
const char *CONDITION_QMLFILE = "condition.qml";
const char *QUIT_QMLFILE = "quit.qml";
const char *CHANGEBREAKPOINT_QMLFILE = "changeBreakpoint.qml";
const char *STEPACTION_QMLFILE = "stepAction.qml";
const char *BREAKPOINTRELOCATION_QMLFILE = "breakpointRelocation.qml";

#define VARIANTMAPINIT \
    QString obj("{}"); \
    QJSValue jsonVal = parser.call(QJSValueList() << obj); \
    jsonVal.setProperty(SEQ,QJSValue(seq++)); \
    jsonVal.setProperty(TYPE,REQUEST);


#undef QVERIFY
#define QVERIFY(statement) \
do {\
    if (!QTest::qVerify((statement), #statement, "", __FILE__, __LINE__)) {\
        if (QTest::currentTestFailed()) \
          qDebug().nospace() << "\nDEBUGGEE OUTPUT:\n" << process->output();\
        return;\
    }\
} while (0)


class QJSDebugClient;

class tst_QQmlDebugJS : public QQmlDataTest
{
    Q_OBJECT

    void init(bool qmlscene, const QString &qmlFile = QString(TEST_QMLFILE), bool blockMode = true,
              bool restrictServices = false);

private slots:
    void initTestCase();
    void cleanupTestCase();

    void cleanup();

    void connect_data();
    void connect();
    void interrupt_data() { targetData(); }
    void interrupt();
    void getVersion_data() { targetData(); }
    void getVersion();
    void getVersionWhenAttaching_data() { targetData(); }
    void getVersionWhenAttaching();

    void disconnect_data() { targetData(); }
    void disconnect();

    void setBreakpointInScriptOnCompleted_data() { targetData(); }
    void setBreakpointInScriptOnCompleted();
    void setBreakpointInScriptOnComponentCreated_data() { targetData(); }
    void setBreakpointInScriptOnComponentCreated();
    void setBreakpointInScriptOnTimerCallback_data() { targetData(); }
    void setBreakpointInScriptOnTimerCallback();
    void setBreakpointInScriptInDifferentFile_data() { targetData(); }
    void setBreakpointInScriptInDifferentFile();
    void setBreakpointInScriptOnComment_data() { targetData(); }
    void setBreakpointInScriptOnComment();
    void setBreakpointInScriptOnEmptyLine_data() { targetData(); }
    void setBreakpointInScriptOnEmptyLine();
    void setBreakpointInScriptOnOptimizedBinding_data() { targetData(); }
    void setBreakpointInScriptOnOptimizedBinding();
    void setBreakpointInScriptWithCondition_data() { targetData(); }
    void setBreakpointInScriptWithCondition();
    void setBreakpointInScriptThatQuits_data() { targetData(); }
    void setBreakpointInScriptThatQuits();
    void setBreakpointWhenAttaching();

    void clearBreakpoint_data() { targetData(); }
    void clearBreakpoint();

    void setExceptionBreak_data() { targetData(); }
    void setExceptionBreak();

    void stepNext_data() { targetData(); }
    void stepNext();
    void stepIn_data() { targetData(); }
    void stepIn();
    void stepOut_data() { targetData(); }
    void stepOut();
    void continueDebugging_data() { targetData(); }
    void continueDebugging();

    void backtrace_data() { targetData(); }
    void backtrace();

    void getFrameDetails_data() { targetData(); }
    void getFrameDetails();

    void getScopeDetails_data() { targetData(); }
    void getScopeDetails();

    void evaluateInGlobalScope();
    void evaluateInLocalScope_data() { targetData(); }
    void evaluateInLocalScope();

    void getScripts_data() { targetData(); }
    void getScripts();

private:
    void targetData();

    QQmlDebugProcess *process;
    QJSDebugClient *client;
    QQmlDebugConnection *connection;
    QTime t;
};

class QJSDebugClient : public QQmlDebugClient
{
    Q_OBJECT
public:
    enum StepAction
    {
        Continue,
        In,
        Out,
        Next
    };

    enum Exception
    {
        All,
        Uncaught
    };

    QJSDebugClient(QQmlDebugConnection *connection)
        : QQmlDebugClient(QLatin1String("V8Debugger"), connection),
          seq(0)
    {
        parser = jsEngine.evaluate(QLatin1String("JSON.parse"));
        stringify = jsEngine.evaluate(QLatin1String("JSON.stringify"));
    }

    void connect();
    void interrupt();

    void continueDebugging(StepAction stepAction);
    void evaluate(QString expr, int frame = -1);
    void lookup(QList<int> handles, bool includeSource = false);
    void backtrace(int fromFrame = -1, int toFrame = -1, bool bottom = false);
    void frame(int number = -1);
    void scope(int number = -1, int frameNumber = -1);
    void scripts(int types = 4, QList<int> ids = QList<int>(), bool includeSource = false, QVariant filter = QVariant());
    void setBreakpoint(QString target, int line = -1, int column = -1, bool enabled = true,
                       QString condition = QString(), int ignoreCount = -1);
    void clearBreakpoint(int breakpoint);
    void setExceptionBreak(Exception type, bool enabled = false);
    void version();
    void disconnect();

protected:
    //inherited from QQmlDebugClient
    void stateChanged(State state);
    void messageReceived(const QByteArray &data);

signals:
    void enabled();
    void connected();
    void interruptRequested();
    void result();
    void stopped();

private:
    void sendMessage(const QByteArray &);
    void flushSendBuffer();
    QByteArray packMessage(const QByteArray &type, const QByteArray &message = QByteArray());

private:
    QJSEngine jsEngine;
    int seq;

    QList<QByteArray> sendBuffer;
public:
    QJSValue parser;
    QJSValue stringify;
    QByteArray response;

};

void QJSDebugClient::connect()
{
    sendMessage(packMessage(CONNECT));
}

void QJSDebugClient::interrupt()
{
    sendMessage(packMessage(INTERRUPT));
}

void QJSDebugClient::continueDebugging(StepAction action)
{
    //    { "seq"       : <number>,
    //      "type"      : "request",
    //      "command"   : "continue",
    //      "arguments" : { "stepaction" : <"in", "next" or "out">,
    //                      "stepcount"  : <number of steps (default 1)>
    //                    }
    //    }
    VARIANTMAPINIT;
    jsonVal.setProperty(QLatin1String(COMMAND),QJSValue(QLatin1String(CONTINEDEBUGGING)));

    if (action != Continue) {
        QJSValue args = parser.call(QJSValueList() << obj);
        switch (action) {
        case In: args.setProperty(QLatin1String(STEPACTION),QJSValue(QLatin1String(IN)));
            break;
        case Out: args.setProperty(QLatin1String(STEPACTION),QJSValue(QLatin1String(OUT)));
            break;
        case Next: args.setProperty(QLatin1String(STEPACTION),QJSValue(QLatin1String(NEXT)));
            break;
        default:break;
        }
        if (!args.isUndefined()) {
            jsonVal.setProperty(QLatin1String(ARGUMENTS),args);
        }
    }
    QJSValue json = stringify.call(QJSValueList() << jsonVal);
    sendMessage(packMessage(V8REQUEST, json.toString().toUtf8()));
}

void QJSDebugClient::evaluate(QString expr, int frame)
{
    //    { "seq"       : <number>,
    //      "type"      : "request",
    //      "command"   : "evaluate",
    //      "arguments" : { "expression"    : <expression to evaluate>,
    //                      "frame"         : <number>
    //                    }
    //    }
    VARIANTMAPINIT;
    jsonVal.setProperty(QLatin1String(COMMAND),QJSValue(QLatin1String(EVALUATE)));

    QJSValue args = parser.call(QJSValueList() << obj);
    args.setProperty(QLatin1String(EXPRESSION),QJSValue(expr));

    if (frame != -1)
        args.setProperty(QLatin1String(FRAME),QJSValue(frame));

    if (!args.isUndefined()) {
        jsonVal.setProperty(QLatin1String(ARGUMENTS),args);
    }

    QJSValue json = stringify.call(QJSValueList() << jsonVal);
    sendMessage(packMessage(V8REQUEST, json.toString().toUtf8()));
}

void QJSDebugClient::lookup(QList<int> handles, bool includeSource)
{
    //    { "seq"       : <number>,
    //      "type"      : "request",
    //      "command"   : "lookup",
    //      "arguments" : { "handles"       : <array of handles>,
    //                      "includeSource" : <boolean indicating whether the source will be included when script objects are returned>,
    //                    }
    //    }
    VARIANTMAPINIT;
    jsonVal.setProperty(QLatin1String(COMMAND),QJSValue(QLatin1String(LOOKUP)));

    QJSValue args = parser.call(QJSValueList() << obj);

    QString arr("[]");
    QJSValue array = parser.call(QJSValueList() << arr);
    int index = 0;
    foreach (int handle, handles) {
        array.setProperty(index++,QJSValue(handle));
    }
    args.setProperty(QLatin1String(HANDLES),array);

    if (includeSource)
        args.setProperty(QLatin1String(INCLUDESOURCE),QJSValue(includeSource));

    if (!args.isUndefined()) {
        jsonVal.setProperty(QLatin1String(ARGUMENTS),args);
    }

    QJSValue json = stringify.call(QJSValueList() << jsonVal);
    sendMessage(packMessage(V8REQUEST, json.toString().toUtf8()));
}

void QJSDebugClient::backtrace(int fromFrame, int toFrame, bool bottom)
{
    //    { "seq"       : <number>,
    //      "type"      : "request",
    //      "command"   : "backtrace",
    //      "arguments" : { "fromFrame" : <number>
    //                      "toFrame" : <number>
    //                      "bottom" : <boolean, set to true if the bottom of the stack is requested>
    //                    }
    //    }
    VARIANTMAPINIT;
    jsonVal.setProperty(QLatin1String(COMMAND),QJSValue(QLatin1String(BACKTRACE)));

    QJSValue args = parser.call(QJSValueList() << obj);

    if (fromFrame != -1)
        args.setProperty(QLatin1String(FROMFRAME),QJSValue(fromFrame));

    if (toFrame != -1)
        args.setProperty(QLatin1String(TOFRAME),QJSValue(toFrame));

    if (bottom)
        args.setProperty(QLatin1String(BOTTOM),QJSValue(bottom));

    if (!args.isUndefined()) {
        jsonVal.setProperty(QLatin1String(ARGUMENTS),args);
    }

    QJSValue json = stringify.call(QJSValueList() << jsonVal);
    sendMessage(packMessage(V8REQUEST, json.toString().toUtf8()));
}

void QJSDebugClient::frame(int number)
{
    //    { "seq"       : <number>,
    //      "type"      : "request",
    //      "command"   : "frame",
    //      "arguments" : { "number" : <frame number>
    //                    }
    //    }
    VARIANTMAPINIT;
    jsonVal.setProperty(QLatin1String(COMMAND),QJSValue(QLatin1String(FRAME)));

    if (number != -1) {
        QJSValue args = parser.call(QJSValueList() << obj);
        args.setProperty(QLatin1String(NUMBER),QJSValue(number));

        if (!args.isUndefined()) {
            jsonVal.setProperty(QLatin1String(ARGUMENTS),args);
        }
    }

    QJSValue json = stringify.call(QJSValueList() << jsonVal);
    sendMessage(packMessage(V8REQUEST, json.toString().toUtf8()));
}

void QJSDebugClient::scope(int number, int frameNumber)
{
    //    { "seq"       : <number>,
    //      "type"      : "request",
    //      "command"   : "scope",
    //      "arguments" : { "number" : <scope number>
    //                      "frameNumber" : <frame number, optional uses selected frame if missing>
    //                    }
    //    }
    VARIANTMAPINIT;
    jsonVal.setProperty(QLatin1String(COMMAND),QJSValue(QLatin1String(SCOPE)));

    if (number != -1) {
        QJSValue args = parser.call(QJSValueList() << obj);
        args.setProperty(QLatin1String(NUMBER),QJSValue(number));

        if (frameNumber != -1)
            args.setProperty(QLatin1String(FRAMENUMBER),QJSValue(frameNumber));

        if (!args.isUndefined()) {
            jsonVal.setProperty(QLatin1String(ARGUMENTS),args);
        }
    }

    QJSValue json = stringify.call(QJSValueList() << jsonVal);
    sendMessage(packMessage(V8REQUEST, json.toString().toUtf8()));
}

void QJSDebugClient::scripts(int types, QList<int> ids, bool includeSource, QVariant /*filter*/)
{
    //    { "seq"       : <number>,
    //      "type"      : "request",
    //      "command"   : "scripts",
    //      "arguments" : { "types"         : <types of scripts to retrieve
    //                                           set bit 0 for native scripts
    //                                           set bit 1 for extension scripts
    //                                           set bit 2 for normal scripts
    //                                         (default is 4 for normal scripts)>
    //                      "ids"           : <array of id's of scripts to return. If this is not specified all scripts are requrned>
    //                      "includeSource" : <boolean indicating whether the source code should be included for the scripts returned>
    //                      "filter"        : <string or number: filter string or script id.
    //                                         If a number is specified, then only the script with the same number as its script id will be retrieved.
    //                                         If a string is specified, then only scripts whose names contain the filter string will be retrieved.>
    //                    }
    //    }
    VARIANTMAPINIT;
    jsonVal.setProperty(QLatin1String(COMMAND),QJSValue(QLatin1String(SCRIPTS)));

    QJSValue args = parser.call(QJSValueList() << obj);
    args.setProperty(QLatin1String(TYPES),QJSValue(types));

    if (ids.count()) {
        QString arr("[]");
        QJSValue array = parser.call(QJSValueList() << arr);
        int index = 0;
        foreach (int id, ids) {
            array.setProperty(index++,QJSValue(id));
        }
        args.setProperty(QLatin1String(IDS),array);
    }

    if (includeSource)
        args.setProperty(QLatin1String(INCLUDESOURCE),QJSValue(includeSource));

    if (!args.isUndefined()) {
        jsonVal.setProperty(QLatin1String(ARGUMENTS),args);
    }

    QJSValue json = stringify.call(QJSValueList() << jsonVal);
    sendMessage(packMessage(V8REQUEST, json.toString().toUtf8()));
}

void QJSDebugClient::setBreakpoint(QString target, int line, int column, bool enabled,
                                   QString condition, int ignoreCount)
{
    //    { "seq"       : <number>,
    //      "type"      : "request",
    //      "command"   : "setbreakpoint",
    //      "arguments" : { "type"        : "scriptRegExp"
    //                      "target"      : <function expression or script identification>
    //                      "line"        : <line in script or function>
    //                      "column"      : <character position within the line>
    //                      "enabled"     : <initial enabled state. True or false, default is true>
    //                      "condition"   : <string with break point condition>
    //                      "ignoreCount" : <number specifying the number of break point hits to ignore, default value is 0>
    //                    }
    //    }

    VARIANTMAPINIT;
    jsonVal.setProperty(QLatin1String(COMMAND),QJSValue(QLatin1String(SETBREAKPOINT)));

    QJSValue args = parser.call(QJSValueList() << obj);

    args.setProperty(QLatin1String(TYPE),QJSValue(QLatin1String(SCRIPTREGEXP)));
    args.setProperty(QLatin1String(TARGET),QJSValue(target));

    if (line != -1)
        args.setProperty(QLatin1String(LINE),QJSValue(line));

    if (column != -1)
        args.setProperty(QLatin1String(COLUMN),QJSValue(column));

    args.setProperty(QLatin1String(ENABLED),QJSValue(enabled));

    if (!condition.isEmpty())
        args.setProperty(QLatin1String(CONDITION),QJSValue(condition));

    if (ignoreCount != -1)
        args.setProperty(QLatin1String(IGNORECOUNT),QJSValue(ignoreCount));

    if (!args.isUndefined()) {
        jsonVal.setProperty(QLatin1String(ARGUMENTS),args);
    }

    QJSValue json = stringify.call(QJSValueList() << jsonVal);
    sendMessage(packMessage(V8REQUEST, json.toString().toUtf8()));
}

void QJSDebugClient::clearBreakpoint(int breakpoint)
{
    //    { "seq"       : <number>,
    //      "type"      : "request",
    //      "command"   : "clearbreakpoint",
    //      "arguments" : { "breakpoint" : <number of the break point to clear>
    //                    }
    //    }
    VARIANTMAPINIT;
    jsonVal.setProperty(QLatin1String(COMMAND),QJSValue(QLatin1String(CLEARBREAKPOINT)));

    QJSValue args = parser.call(QJSValueList() << obj);

    args.setProperty(QLatin1String(BREAKPOINT),QJSValue(breakpoint));

    if (!args.isUndefined()) {
        jsonVal.setProperty(QLatin1String(ARGUMENTS),args);
    }

    QJSValue json = stringify.call(QJSValueList() << jsonVal);
    sendMessage(packMessage(V8REQUEST, json.toString().toUtf8()));
}

void QJSDebugClient::setExceptionBreak(Exception type, bool enabled)
{
    //    { "seq"       : <number>,
    //      "type"      : "request",
    //      "command"   : "setexceptionbreak",
    //      "arguments" : { "type"    : <string: "all", or "uncaught">,
    //                      "enabled" : <optional bool: enables the break type if true>
    //                    }
    //    }
    VARIANTMAPINIT;
    jsonVal.setProperty(QLatin1String(COMMAND),QJSValue(QLatin1String(SETEXCEPTIONBREAK)));

    QJSValue args = parser.call(QJSValueList() << obj);

    if (type == All)
        args.setProperty(QLatin1String(TYPE),QJSValue(QLatin1String(ALL)));
    else if (type == Uncaught)
        args.setProperty(QLatin1String(TYPE),QJSValue(QLatin1String(UNCAUGHT)));

    if (enabled)
        args.setProperty(QLatin1String(ENABLED),QJSValue(enabled));

    if (!args.isUndefined()) {
        jsonVal.setProperty(QLatin1String(ARGUMENTS),args);
    }

    QJSValue json = stringify.call(QJSValueList() << jsonVal);
    sendMessage(packMessage(V8REQUEST, json.toString().toUtf8()));
}

void QJSDebugClient::version()
{
    //    { "seq"       : <number>,
    //      "type"      : "request",
    //      "command"   : "version",
    //    }
    VARIANTMAPINIT;
    jsonVal.setProperty(QLatin1String(COMMAND),QJSValue(QLatin1String(VERSION)));

    QJSValue json = stringify.call(QJSValueList() << jsonVal);
    sendMessage(packMessage(V8REQUEST, json.toString().toUtf8()));
}

void QJSDebugClient::disconnect()
{
    //    { "seq"     : <number>,
    //      "type"    : "request",
    //      "command" : "disconnect",
    //    }
    VARIANTMAPINIT;
    jsonVal.setProperty(QLatin1String(COMMAND),QJSValue(QLatin1String(DISCONNECT)));

    QJSValue json = stringify.call(QJSValueList() << jsonVal);
    sendMessage(packMessage(DISCONNECT, json.toString().toUtf8()));
}

void QJSDebugClient::stateChanged(State state)
{
    if (state == Enabled) {
        flushSendBuffer();
        emit enabled();
    }
}

void QJSDebugClient::messageReceived(const QByteArray &data)
{
    QPacket ds(connection()->currentDataStreamVersion(), data);
    QByteArray command;
    ds >> command;

    if (command == "V8DEBUG") {
        QByteArray type;
        ds >> type >> response;

        if (type == CONNECT) {
            emit connected();

        } else if (type == INTERRUPT) {
            emit interruptRequested();

        } else if (type == V8MESSAGE) {
            QString jsonString(response);
            QVariantMap value = parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();
            QString type = value.value("type").toString();

            if (type == "response") {

                if (!value.value("success").toBool()) {
                    qDebug() << "Received success == false response from application";
                    return;
                }

                QString debugCommand(value.value("command").toString());
                if (debugCommand == "backtrace" ||
                        debugCommand == "lookup" ||
                        debugCommand == "setbreakpoint" ||
                        debugCommand == "evaluate" ||
                        debugCommand == "version" ||
                        debugCommand == "disconnect" ||
                        debugCommand == "gc" ||
                        debugCommand == "changebreakpoint" ||
                        debugCommand == "clearbreakpoint" ||
                        debugCommand == "frame" ||
                        debugCommand == "scope" ||
                        debugCommand == "scopes" ||
                        debugCommand == "scripts" ||
                        debugCommand == "source" ||
                        debugCommand == "setexceptionbreak" /*||
                        debugCommand == "profile"*/) {
                    emit result();

                } else {
                    // DO NOTHING
                }

            } else if (type == QLatin1String(EVENT)) {
                QString event(value.value(QLatin1String(EVENT)).toString());

                if (event == "break" ||
                        event == "exception")
                    emit stopped();
                }

        }
    }
}

void QJSDebugClient::sendMessage(const QByteArray &msg)
{
    if (state() == Enabled) {
        QQmlDebugClient::sendMessage(msg);
    } else {
        sendBuffer.append(msg);
    }
}

void QJSDebugClient::flushSendBuffer()
{
    foreach (const QByteArray &msg, sendBuffer)
        QQmlDebugClient::sendMessage(msg);
    sendBuffer.clear();
}

QByteArray QJSDebugClient::packMessage(const QByteArray &type, const QByteArray &message)
{
    QPacket rs(connection()->currentDataStreamVersion());
    QByteArray cmd = "V8DEBUG";
    rs << cmd << type << message;
    return rs.data();
}

void tst_QQmlDebugJS::initTestCase()
{
    QQmlDataTest::initTestCase();
    t.start();
    process = 0;
    client = 0;
    connection = 0;
}

void tst_QQmlDebugJS::cleanupTestCase()
{
    if (process) {
        process->stop();
        delete process;
    }

    if (client)
        delete client;

    if (connection)
        delete connection;
}

void tst_QQmlDebugJS::init(bool qmlscene, const QString &qmlFile, bool blockMode,
                           bool restrictServices)
{
    connection = new QQmlDebugConnection();
    if (qmlscene)
        process = new QQmlDebugProcess(QLibraryInfo::location(QLibraryInfo::BinariesPath) +
                                       "/qmlscene", this);
    else
        process = new QQmlDebugProcess(QCoreApplication::applicationDirPath() +
                                       QLatin1String("/qqmldebugjsserver"), this);
    client = new QJSDebugClient(connection);
    QList<QQmlDebugClient *> others = QQmlDebugTest::createOtherClients(connection);

    const char *args = 0;
    if (blockMode)
        args = restrictServices ? BLOCKRESTRICTEDMODE : BLOCKMODE;
    else
        args = restrictServices ? NORMALRESTRICTEDMODE : NORMALMODE;

    process->start(QStringList() << QLatin1String(args) << testFile(qmlFile));

    QVERIFY(process->waitForSessionStart());

    const int port = process->debugPort();
    connection->connectToHost("127.0.0.1", port);
    QVERIFY(connection->waitForConnected());


    if (client->state() != QQmlDebugClient::Enabled)
        QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(enabled())));

    foreach (QQmlDebugClient *otherClient, others)
        QCOMPARE(otherClient->state(), restrictServices ? QQmlDebugClient::Unavailable :
                                                          QQmlDebugClient::Enabled);
    qDeleteAll(others);
}

void tst_QQmlDebugJS::cleanup()
{
    if (QTest::currentTestFailed()) {
        qDebug() << "Process State:" << process->state();
        qDebug() << "Application Output:" << process->output();
    }

    if (process) {
        process->stop();
        delete process;
    }

    if (client)
        delete client;

    if (connection)
        delete connection;

    process = 0;
    client = 0;
    connection = 0;
}

void tst_QQmlDebugJS::connect_data()
{
    QTest::addColumn<bool>("blockMode");
    QTest::addColumn<bool>("restrictMode");
    QTest::addColumn<bool>("qmlscene");
    QTest::newRow("normal / unrestricted / custom")   << false << false << false;
    QTest::newRow("block  / unrestricted / custom")   << true  << false << false;
    QTest::newRow("normal / restricted   / custom")   << false << true  << false;
    QTest::newRow("block  / restricted   / custom")   << true  << true  << false;
    QTest::newRow("normal / unrestricted / qmlscene") << false << false << true;
    QTest::newRow("block  / unrestricted / qmlscene") << true  << false << true;
    QTest::newRow("normal / restricted   / qmlscene") << false << true  << true;
    QTest::newRow("block  / restricted   / qmlscene") << true  << true  << true;
}

void tst_QQmlDebugJS::connect()
{
    QFETCH(bool, blockMode);
    QFETCH(bool, restrictMode);
    QFETCH(bool, qmlscene);
    init(qmlscene, QString(TEST_QMLFILE), blockMode, restrictMode);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(connected())));
}

void tst_QQmlDebugJS::interrupt()
{
    //void connect()

    QFETCH(bool, qmlscene);
    init(qmlscene);
    client->connect();

    client->interrupt();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(interruptRequested())));
}

void tst_QQmlDebugJS::getVersion()
{
    //void version()

    QFETCH(bool, qmlscene);
    init(qmlscene);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(connected())));

    client->version();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(result())));
}

void tst_QQmlDebugJS::getVersionWhenAttaching()
{
    //void version()
    QFETCH(bool, qmlscene);

    init(qmlscene, QLatin1String(TIMER_QMLFILE), false);
    client->connect();

    client->version();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(result())));
}

void tst_QQmlDebugJS::disconnect()
{
    //void disconnect()

    QFETCH(bool, qmlscene);
    init(qmlscene);
    client->connect();

    client->disconnect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(result())));
}

void tst_QQmlDebugJS::setBreakpointInScriptOnCompleted()
{
    //void setBreakpoint(QString type, QString target, int line = -1, int column = -1, bool enabled = false, QString condition = QString(), int ignoreCount = -1)
    QFETCH(bool, qmlscene);

    int sourceLine = 34;
    init(qmlscene, ONCOMPLETED_QMLFILE);

    client->setBreakpoint(QLatin1String(ONCOMPLETED_QMLFILE), sourceLine, -1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    QVariantMap body = value.value("body").toMap();

    QCOMPARE(body.value("sourceLine").toInt(), sourceLine);
    QCOMPARE(QFileInfo(body.value("script").toMap().value("name").toString()).fileName(), QLatin1String(ONCOMPLETED_QMLFILE));
}

void tst_QQmlDebugJS::setBreakpointInScriptOnComponentCreated()
{
    //void setBreakpoint(QString type, QString target, int line = -1, int column = -1, bool enabled = false, QString condition = QString(), int ignoreCount = -1)
    QFETCH(bool, qmlscene);

    int sourceLine = 34;
    init(qmlscene, CREATECOMPONENT_QMLFILE);

    client->setBreakpoint(QLatin1String(ONCOMPLETED_QMLFILE), sourceLine, -1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    QVariantMap body = value.value("body").toMap();

    QCOMPARE(body.value("sourceLine").toInt(), sourceLine);
    QCOMPARE(QFileInfo(body.value("script").toMap().value("name").toString()).fileName(), QLatin1String(ONCOMPLETED_QMLFILE));
}

void tst_QQmlDebugJS::setBreakpointInScriptOnTimerCallback()
{
    QFETCH(bool, qmlscene);
    int sourceLine = 35;
    init(qmlscene, TIMER_QMLFILE);

    client->connect();
    //We can set the breakpoint after connect() here because the timer is repeating and if we miss
    //its first iteration we can still catch the second one.
    client->setBreakpoint(QLatin1String(TIMER_QMLFILE), sourceLine, -1, true);
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    QVariantMap body = value.value("body").toMap();

    QCOMPARE(body.value("sourceLine").toInt(), sourceLine);
    QCOMPARE(QFileInfo(body.value("script").toMap().value("name").toString()).fileName(), QLatin1String(TIMER_QMLFILE));
}

void tst_QQmlDebugJS::setBreakpointInScriptInDifferentFile()
{
    //void setBreakpoint(QString type, QString target, int line = -1, int column = -1, bool enabled = false, QString condition = QString(), int ignoreCount = -1)
    QFETCH(bool, qmlscene);

    int sourceLine = 31;
    init(qmlscene, LOADJSFILE_QMLFILE);

    client->setBreakpoint(QLatin1String(TEST_JSFILE), sourceLine, -1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    QVariantMap body = value.value("body").toMap();

    QCOMPARE(body.value("sourceLine").toInt(), sourceLine);
    QCOMPARE(QFileInfo(body.value("script").toMap().value("name").toString()).fileName(), QLatin1String(TEST_JSFILE));
}

void tst_QQmlDebugJS::setBreakpointInScriptOnComment()
{
    //void setBreakpoint(QString type, QString target, int line = -1, int column = -1, bool enabled = false, QString condition = QString(), int ignoreCount = -1)
    QFETCH(bool, qmlscene);

    int sourceLine = 34;
    int actualLine = 36;
    init(qmlscene, BREAKPOINTRELOCATION_QMLFILE);

    client->setBreakpoint(QLatin1String(BREAKPOINTRELOCATION_QMLFILE), sourceLine, -1, true);
    client->connect();
    QEXPECT_FAIL("", "Relocation of breakpoints is disabled right now", Abort);
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped()), 1));

    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    QVariantMap body = value.value("body").toMap();

    QCOMPARE(body.value("sourceLine").toInt(), actualLine);
    QCOMPARE(QFileInfo(body.value("script").toMap().value("name").toString()).fileName(), QLatin1String(BREAKPOINTRELOCATION_QMLFILE));
}

void tst_QQmlDebugJS::setBreakpointInScriptOnEmptyLine()
{
    //void setBreakpoint(QString type, QString target, int line = -1, int column = -1, bool enabled = false, QString condition = QString(), int ignoreCount = -1)
    QFETCH(bool, qmlscene);

    int sourceLine = 35;
    int actualLine = 36;
    init(qmlscene, BREAKPOINTRELOCATION_QMLFILE);

    client->setBreakpoint(QLatin1String(BREAKPOINTRELOCATION_QMLFILE), sourceLine, -1, true);
    client->connect();
    QEXPECT_FAIL("", "Relocation of breakpoints is disabled right now", Abort);
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped()), 1));

    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    QVariantMap body = value.value("body").toMap();

    QCOMPARE(body.value("sourceLine").toInt(), actualLine);
    QCOMPARE(QFileInfo(body.value("script").toMap().value("name").toString()).fileName(), QLatin1String(BREAKPOINTRELOCATION_QMLFILE));
}

void tst_QQmlDebugJS::setBreakpointInScriptOnOptimizedBinding()
{
    //void setBreakpoint(QString type, QString target, int line = -1, int column = -1, bool enabled = false, QString condition = QString(), int ignoreCount = -1)
    QFETCH(bool, qmlscene);

    int sourceLine = 39;
    init(qmlscene, BREAKPOINTRELOCATION_QMLFILE);

    client->setBreakpoint(QLatin1String(BREAKPOINTRELOCATION_QMLFILE), sourceLine, -1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    QVariantMap body = value.value("body").toMap();

    QCOMPARE(body.value("sourceLine").toInt(), sourceLine);
    QCOMPARE(QFileInfo(body.value("script").toMap().value("name").toString()).fileName(), QLatin1String(BREAKPOINTRELOCATION_QMLFILE));
}

void tst_QQmlDebugJS::setBreakpointInScriptWithCondition()
{
    QFETCH(bool, qmlscene);
    int out = 10;
    int sourceLine = 37;
    init(qmlscene, CONDITION_QMLFILE);

    client->connect();
    //The breakpoint is in a timer loop so we can set it after connect().
    client->setBreakpoint(QLatin1String(CONDITION_QMLFILE), sourceLine, 1, true, QLatin1String("a > 10"));
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    //Get the frame index
    QString jsonString = client->response;
    QVariantMap value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    {
        QVariantMap body = value.value("body").toMap();
        int frameIndex = body.value("index").toInt();

        //Verify the value of 'result'
        client->evaluate(QLatin1String("a"),frameIndex);
        QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(result())));
    }

    jsonString = client->response;
    QJSValue val = client->parser.call(QJSValueList() << QJSValue(jsonString));
    QVERIFY(val.isObject());
    QJSValue body = val.property(QStringLiteral("body"));
    QVERIFY(body.isObject());
    val = body.property("value");
    QVERIFY(val.isNumber());

    const int a = val.toInt();
    QVERIFY(a > out);
}

void tst_QQmlDebugJS::setBreakpointInScriptThatQuits()
{
    QFETCH(bool, qmlscene);
    init(qmlscene, QUIT_QMLFILE);

    int sourceLine = 36;

    client->setBreakpoint(QLatin1String(QUIT_QMLFILE), sourceLine, -1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    QVariantMap body = value.value("body").toMap();

    QCOMPARE(body.value("sourceLine").toInt(), sourceLine);
    QCOMPARE(QFileInfo(body.value("script").toMap().value("name").toString()).fileName(), QLatin1String(QUIT_QMLFILE));

    client->continueDebugging(QJSDebugClient::Continue);
    QVERIFY(process->waitForFinished());
    QCOMPARE(process->exitStatus(), QProcess::NormalExit);
}

void tst_QQmlDebugJS::setBreakpointWhenAttaching()
{
    int sourceLine = 35;
    init(true, QLatin1String(TIMER_QMLFILE), false);

    client->connect();

    QSKIP("\nThe breakpoint may not hit because the engine may run in JIT mode or not have debug\n"
          "instructions, as we've connected in non-blocking mode above. That means we may have\n"
          "connected after the engine was already running, with all the QML already compiled.");

    //The breakpoint is in a timer loop so we can set it after connect().
    client->setBreakpoint(QLatin1String(TIMER_QMLFILE), sourceLine);

    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));
}

void tst_QQmlDebugJS::clearBreakpoint()
{
    //void clearBreakpoint(int breakpoint);
    QFETCH(bool, qmlscene);

    int sourceLine1 = 37;
    int sourceLine2 = 38;
    init(qmlscene, CHANGEBREAKPOINT_QMLFILE);

    client->connect();
    //The breakpoints are in a timer loop so we can set them after connect().
    //Furthermore the breakpoints should be hit in the right order because setting of breakpoints
    //can only occur in the QML event loop. (see QCOMPARE for sourceLine2 below)
    client->setBreakpoint(QLatin1String(CHANGEBREAKPOINT_QMLFILE), sourceLine1, -1, true);
    client->setBreakpoint(QLatin1String(CHANGEBREAKPOINT_QMLFILE), sourceLine2, -1, true);

    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    //Will hit 1st brakpoint, change this breakpoint enable = false
    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    QVariantMap body = value.value("body").toMap();
    QList<QVariant> breakpointsHit = body.value("breakpoints").toList();

    int breakpoint = breakpointsHit.at(0).toInt();
    client->clearBreakpoint(breakpoint);

    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(result())));

    //Continue with debugging
    client->continueDebugging(QJSDebugClient::Continue);
    //Hit 2nd breakpoint
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    //Continue with debugging
    client->continueDebugging(QJSDebugClient::Continue);
    //Should stop at 2nd breakpoint
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    jsonString = client->response;
    value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    body = value.value("body").toMap();

    QCOMPARE(body.value("sourceLine").toInt(), sourceLine2);
}

void tst_QQmlDebugJS::setExceptionBreak()
{
    //void setExceptionBreak(QString type, bool enabled = false);
    QFETCH(bool, qmlscene);

    init(qmlscene, EXCEPTION_QMLFILE);
    client->setExceptionBreak(QJSDebugClient::All,true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));
}

void tst_QQmlDebugJS::stepNext()
{
    //void continueDebugging(StepAction stepAction, int stepCount = 1);
    QFETCH(bool, qmlscene);

    int sourceLine = 37;
    init(qmlscene, STEPACTION_QMLFILE);

    client->setBreakpoint(QLatin1String(STEPACTION_QMLFILE), sourceLine, -1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    client->continueDebugging(QJSDebugClient::Next);
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    QVariantMap body = value.value("body").toMap();

    QCOMPARE(body.value("sourceLine").toInt(), sourceLine + 1);
    QCOMPARE(QFileInfo(body.value("script").toMap().value("name").toString()).fileName(), QLatin1String(STEPACTION_QMLFILE));
}

void tst_QQmlDebugJS::stepIn()
{
    //void continueDebugging(StepAction stepAction, int stepCount = 1);
    QFETCH(bool, qmlscene);

    int sourceLine = 41;
    int actualLine = 37;
    init(qmlscene, STEPACTION_QMLFILE);

    client->setBreakpoint(QLatin1String(STEPACTION_QMLFILE), sourceLine, 1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    client->continueDebugging(QJSDebugClient::In);
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    QVariantMap body = value.value("body").toMap();

    QCOMPARE(body.value("sourceLine").toInt(), actualLine);
    QCOMPARE(QFileInfo(body.value("script").toMap().value("name").toString()).fileName(), QLatin1String(STEPACTION_QMLFILE));
}

void tst_QQmlDebugJS::stepOut()
{
    //void continueDebugging(StepAction stepAction, int stepCount = 1);
    QFETCH(bool, qmlscene);

    int sourceLine = 37;
    int actualLine = 41;
    init(qmlscene, STEPACTION_QMLFILE);

    client->setBreakpoint(QLatin1String(STEPACTION_QMLFILE), sourceLine, -1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    client->continueDebugging(QJSDebugClient::Out);
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    QVariantMap body = value.value("body").toMap();

    QCOMPARE(body.value("sourceLine").toInt(), actualLine);
    QCOMPARE(QFileInfo(body.value("script").toMap().value("name").toString()).fileName(), QLatin1String(STEPACTION_QMLFILE));
}

void tst_QQmlDebugJS::continueDebugging()
{
    //void continueDebugging(StepAction stepAction, int stepCount = 1);
    QFETCH(bool, qmlscene);

    int sourceLine1 = 41;
    int sourceLine2 = 38;
    init(qmlscene, STEPACTION_QMLFILE);

    client->setBreakpoint(QLatin1String(STEPACTION_QMLFILE), sourceLine1, -1, true);
    client->setBreakpoint(QLatin1String(STEPACTION_QMLFILE), sourceLine2, -1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    client->continueDebugging(QJSDebugClient::Continue);
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    QVariantMap body = value.value("body").toMap();

    QCOMPARE(body.value("sourceLine").toInt(), sourceLine2);
    QCOMPARE(QFileInfo(body.value("script").toMap().value("name").toString()).fileName(), QLatin1String(STEPACTION_QMLFILE));
}

void tst_QQmlDebugJS::backtrace()
{
    //void backtrace(int fromFrame = -1, int toFrame = -1, bool bottom = false);
    QFETCH(bool, qmlscene);

    int sourceLine = 34;
    init(qmlscene, ONCOMPLETED_QMLFILE);

    client->setBreakpoint(QLatin1String(ONCOMPLETED_QMLFILE), sourceLine, -1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    client->backtrace();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(result())));
}

void tst_QQmlDebugJS::getFrameDetails()
{
    //void frame(int number = -1);
    QFETCH(bool, qmlscene);

    int sourceLine = 34;
    init(qmlscene, ONCOMPLETED_QMLFILE);

    client->setBreakpoint(QLatin1String(ONCOMPLETED_QMLFILE), sourceLine, -1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    client->frame();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(result())));
}

void tst_QQmlDebugJS::getScopeDetails()
{
    //void scope(int number = -1, int frameNumber = -1);
    QFETCH(bool, qmlscene);

    int sourceLine = 34;
    init(qmlscene, ONCOMPLETED_QMLFILE);

    client->setBreakpoint(QLatin1String(ONCOMPLETED_QMLFILE), sourceLine, -1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    client->scope();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(result())));
}

void tst_QQmlDebugJS::evaluateInGlobalScope()
{
    //void evaluate(QString expr, int frame = -1);
    init(true);

    client->connect();

    do {
        // The engine might not be initialized, yet. We just try until it shows up.
        client->evaluate(QLatin1String("console.log('Hello World')"));
    } while (!QQmlDebugTest::waitForSignal(client, SIGNAL(result()), 500));

    //Verify the return value of 'console.log()', which is "undefined"
    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();
    QVariantMap body = value.value("body").toMap();
    QCOMPARE(body.value("type").toString(),QLatin1String("undefined"));
}

void tst_QQmlDebugJS::evaluateInLocalScope()
{
    //void evaluate(QString expr, bool global = false, bool disableBreak = false, int frame = -1, const QVariantMap &addContext = QVariantMap());

    QFETCH(bool, qmlscene);
    int sourceLine = 34;
    init(qmlscene, ONCOMPLETED_QMLFILE);

    client->setBreakpoint(QLatin1String(ONCOMPLETED_QMLFILE), sourceLine, -1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    client->frame();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(result())));

    //Get the frame index
    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    QVariantMap body = value.value("body").toMap();

    int frameIndex = body.value("index").toInt();

    client->evaluate(QLatin1String("root.a"), frameIndex);
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(result())));

    //Verify the value of 'timer.interval'
    jsonString = client->response;
    value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    body = value.value("body").toMap();

    QCOMPARE(body.value("value").toInt(),10);
}

void tst_QQmlDebugJS::getScripts()
{
    //void scripts(int types = -1, QList<int> ids = QList<int>(), bool includeSource = false, QVariant filter = QVariant());

    QFETCH(bool, qmlscene);
    init(qmlscene);

    client->setBreakpoint(QString(TEST_QMLFILE), 35, -1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    client->scripts();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(result())));
    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList()
                                            << QJSValue(jsonString)).toVariant().toMap();

    QList<QVariant> scripts = value.value("body").toList();

    QCOMPARE(scripts.count(), 1);
    QVERIFY(scripts.first().toMap()[QStringLiteral("name")].toString().endsWith(QStringLiteral("data/test.qml")));
}

void tst_QQmlDebugJS::targetData()
{
    QTest::addColumn<bool>("qmlscene");
    QTest::newRow("custom") << false;
    QTest::newRow("qmlscene") << true;
}

QTEST_MAIN(tst_QQmlDebugJS)

#include "tst_qqmldebugjs.moc"

