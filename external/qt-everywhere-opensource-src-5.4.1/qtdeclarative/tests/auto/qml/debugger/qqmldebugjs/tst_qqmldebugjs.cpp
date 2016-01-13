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
#include <QtCore/QProcess>
#include <QtCore/QTimer>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QMutex>
#include <QtCore/QLibraryInfo>
#include <QtQml/QJSEngine>

//QQmlDebugTest
#include "debugutil_p.h"
#include "qqmldebugclient.h"
#include "../../../shared/util.h"

#if defined (Q_OS_WINCE)
#undef IN
#undef OUT
#endif

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
//const char *PROFILE = "profile";

const char *CONNECT = "connect";
const char *INTERRUPT = "interrupt";

const char *REQUEST = "request";
const char *IN = "in";
const char *NEXT = "next";
const char *OUT = "out";

const char *FUNCTION = "function";
const char *SCRIPT = "script";
const char *SCRIPTREGEXP = "scriptRegExp";
const char *EVENT = "event";

const char *ALL = "all";
const char *UNCAUGHT = "uncaught";

//const char *PAUSE = "pause";
//const char *RESUME = "resume";

const char *BLOCKMODE = "-qmljsdebugger=port:3771,3800,block";
const char *NORMALMODE = "-qmljsdebugger=port:3771,3800";
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

    bool init(const QString &qmlFile = QString(TEST_QMLFILE), bool blockMode = true);

private slots:
    void initTestCase();
    void cleanupTestCase();

    void cleanup();

    void connect();
    void interrupt();
    void getVersion();
//    void getVersionWhenAttaching();

    void disconnect();

    void setBreakpointInScriptOnCompleted();
    void setBreakpointInScriptOnComponentCreated();
    void setBreakpointInScriptOnTimerCallback();
    void setBreakpointInScriptInDifferentFile();
    void setBreakpointInScriptOnComment();
    void setBreakpointInScriptOnEmptyLine();
    void setBreakpointInScriptOnOptimizedBinding();
    void setBreakpointInScriptWithCondition();
    void setBreakpointInScriptThatQuits();
    //void setBreakpointInFunction(); //NOT SUPPORTED
//    void setBreakpointOnEvent();
//    void setBreakpointWhenAttaching();

    void clearBreakpoint();

    void setExceptionBreak();

    void stepNext();
    void stepIn();
    void stepOut();
    void continueDebugging();

    void backtrace();

    void getFrameDetails();

    void getScopeDetails();

//    void evaluateInGlobalScope(); // Not supported yet.
//    void evaluateInLocalScope(); // Not supported yet.

    void getScripts();

    //    void profile(); //NOT SUPPORTED

    //    void verifyQMLOptimizerDisabled();

private:
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

//    enum ProfileCommand
//    {
//        Pause,
//        Resume
//    };

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
    void evaluate(QString expr, bool global = false, bool disableBreak = false, int frame = -1, const QVariantMap &addContext = QVariantMap());
    void lookup(QList<int> handles, bool includeSource = false);
    void backtrace(int fromFrame = -1, int toFrame = -1, bool bottom = false);
    void frame(int number = -1);
    void scope(int number = -1, int frameNumber = -1);
    void scripts(int types = 4, QList<int> ids = QList<int>(), bool includeSource = false, QVariant filter = QVariant());
    void setBreakpoint(QString type, QString target, int line = -1, int column = -1, bool enabled = true, QString condition = QString(), int ignoreCount = -1);
    void clearBreakpoint(int breakpoint);
    void setExceptionBreak(Exception type, bool enabled = false);
    void version();
    //void profile(ProfileCommand command); //NOT SUPPORTED
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
    void scriptsResult();
    void evaluateResult();

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

void QJSDebugClient::evaluate(QString expr, bool global, bool disableBreak, int frame, const QVariantMap &/*addContext*/)
{
    //    { "seq"       : <number>,
    //      "type"      : "request",
    //      "command"   : "evaluate",
    //      "arguments" : { "expression"    : <expression to evaluate>,
    //                      "frame"         : <number>,
    //                      "global"        : <boolean>,
    //                      "disable_break" : <boolean>,
    //                      "additional_context" : [
    //                           { "name" : <name1>, "handle" : <handle1> },
    //                           { "name" : <name2>, "handle" : <handle2> },
    //                           ...
    //                      ]
    //                    }
    //    }
    VARIANTMAPINIT;
    jsonVal.setProperty(QLatin1String(COMMAND),QJSValue(QLatin1String(EVALUATE)));

    QJSValue args = parser.call(QJSValueList() << obj);
    args.setProperty(QLatin1String(EXPRESSION),QJSValue(expr));

    if (frame != -1)
        args.setProperty(QLatin1String(FRAME),QJSValue(frame));

    if (global)
        args.setProperty(QLatin1String(GLOBAL),QJSValue(global));

    if (disableBreak)
        args.setProperty(QLatin1String(DISABLEBREAK),QJSValue(disableBreak));

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

void QJSDebugClient::setBreakpoint(QString type, QString target, int line, int column, bool enabled, QString condition, int ignoreCount)
{
    //    { "seq"       : <number>,
    //      "type"      : "request",
    //      "command"   : "setbreakpoint",
    //      "arguments" : { "type"        : <"function" or "script" or "scriptId" or "scriptRegExp">
    //                      "target"      : <function expression or script identification>
    //                      "line"        : <line in script or function>
    //                      "column"      : <character position within the line>
    //                      "enabled"     : <initial enabled state. True or false, default is true>
    //                      "condition"   : <string with break point condition>
    //                      "ignoreCount" : <number specifying the number of break point hits to ignore, default value is 0>
    //                    }
    //    }

    if (type == QLatin1String(EVENT)) {
        QByteArray reply;
        QDataStream rs(&reply, QIODevice::WriteOnly);
        rs <<  target.toUtf8() << enabled;
        sendMessage(packMessage(QByteArray("breakonsignal"), reply));

    } else {
        VARIANTMAPINIT;
        jsonVal.setProperty(QLatin1String(COMMAND),QJSValue(QLatin1String(SETBREAKPOINT)));

        QJSValue args = parser.call(QJSValueList() << obj);

        args.setProperty(QLatin1String(TYPE),QJSValue(type));
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

//void QJSDebugClient::profile(ProfileCommand command)
//{
////    { "seq"       : <number>,
////      "type"      : "request",
////      "command"   : "profile",
////      "arguments" : { "command"  : "resume" or "pause" }
////    }
//    VARIANTMAPINIT;
//    jsonVal.setProperty(QLatin1String(COMMAND),QJSValue(QLatin1String(PROFILE)));

//    QJSValue args = parser.call(QJSValueList() << obj);

//    if (command == Resume)
//        args.setProperty(QLatin1String(COMMAND),QJSValue(QLatin1String(RESUME)));
//    else
//        args.setProperty(QLatin1String(COMMAND),QJSValue(QLatin1String(PAUSE)));

//    args.setProperty(QLatin1String("modules"),QJSValue(1));
//    if (!args.isUndefined()) {
//        jsonVal.setProperty(QLatin1String(ARGUMENTS),args);
//    }

//    QJSValue json = stringify.call(QJSValueList() << jsonVal);
//    sendMessage(packMessage(V8REQUEST, json.toString().toUtf8()));
//}

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
    QDataStream ds(data);
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
//                    qDebug() << "Error: The test case will fail since no signal is emitted";
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
                //Emit separate signals for scripts ane evaluate
                //as the associated test cases are flaky
                if (debugCommand == "scripts")
                    emit scriptsResult();
                if (debugCommand == "evaluate")
                    emit evaluateResult();

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
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "V8DEBUG";
    rs << cmd << type << message;
    return reply;
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

//    qDebug() << "Time Elapsed:" << t.elapsed();
}

bool tst_QQmlDebugJS::init(const QString &qmlFile, bool blockMode)
{
    connection = new QQmlDebugConnection();
    process = new QQmlDebugProcess(QLibraryInfo::location(QLibraryInfo::BinariesPath) + "/qmlscene", this);
    client = new QJSDebugClient(connection);

    if (blockMode)
        process->start(QStringList() << QLatin1String(BLOCKMODE) << testFile(qmlFile));
    else
        process->start(QStringList() << QLatin1String(NORMALMODE) << testFile(qmlFile));

    if (!process->waitForSessionStart()) {
        qDebug() << "could not launch application, or did not get 'Waiting for connection'.";
        return false;
    }

    const int port = process->debugPort();
    connection->connectToHost("127.0.0.1", port);
    if (!connection->waitForConnected()) {
        qDebug() << "could not connect to host!";
        return false;
    }

    if (client->state() == QQmlDebugClient::Enabled)
        return true;

    return QQmlDebugTest::waitForSignal(client, SIGNAL(enabled()));
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

void tst_QQmlDebugJS::connect()
{
    //void connect()

    QVERIFY(init());
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(connected())));
}

void tst_QQmlDebugJS::interrupt()
{
    //void connect()

    QVERIFY(init());
    client->connect();

    client->interrupt();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(interruptRequested())));
}

void tst_QQmlDebugJS::getVersion()
{
    //void version()

    QVERIFY(init());
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(connected())));

    client->version();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(result())));
}

/* TODO fails because of a race condition when starting up the engine before the view
void tst_QQmlDebugJS::getVersionWhenAttaching()
{
    //void version()

    QVERIFY(init(QLatin1String(TIMER_QMLFILE), false));
    client->connect();

    client->version();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(result())));
}
*/

void tst_QQmlDebugJS::disconnect()
{
    //void disconnect()

    QVERIFY(init());
    client->connect();

    client->disconnect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(result())));
}

void tst_QQmlDebugJS::setBreakpointInScriptOnCompleted()
{
    //void setBreakpoint(QString type, QString target, int line = -1, int column = -1, bool enabled = false, QString condition = QString(), int ignoreCount = -1)

    int sourceLine = 39;
    QVERIFY(init(ONCOMPLETED_QMLFILE));

    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(ONCOMPLETED_QMLFILE), sourceLine, -1, true);
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

    int sourceLine = 39;
    QVERIFY(init(CREATECOMPONENT_QMLFILE));

    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(ONCOMPLETED_QMLFILE), sourceLine, -1, true);
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
    int sourceLine = 40;
    QVERIFY(init(TIMER_QMLFILE));

    client->connect();
    //We can set the breakpoint after connect() here because the timer is repeating and if we miss
    //its first iteration we can still catch the second one.
    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(TIMER_QMLFILE), sourceLine, -1, true);
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

    int sourceLine = 35;
    QVERIFY(init(LOADJSFILE_QMLFILE));

    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(TEST_JSFILE), sourceLine, -1, true);
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

    int sourceLine = 39;
    int actualLine = 41;
    QVERIFY(init(BREAKPOINTRELOCATION_QMLFILE));

    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(BREAKPOINTRELOCATION_QMLFILE), sourceLine, -1, true);
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

    int sourceLine = 40;
    int actualLine = 41;
    QVERIFY(init(BREAKPOINTRELOCATION_QMLFILE));

    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(BREAKPOINTRELOCATION_QMLFILE), sourceLine, -1, true);
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

    int sourceLine = 44;
    QVERIFY(init(BREAKPOINTRELOCATION_QMLFILE));

    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(BREAKPOINTRELOCATION_QMLFILE), sourceLine, -1, true);
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
    int out = 10;
    int sourceLine = 42;
    QVERIFY(init(CONDITION_QMLFILE));

    client->connect();
    //The breakpoint is in a timer loop so we can set it after connect().
    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(CONDITION_QMLFILE), sourceLine, 1, true, QLatin1String("a > 10"));
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
    QVERIFY(init(QUIT_QMLFILE));

    int sourceLine = 41;

    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(QUIT_QMLFILE), sourceLine, -1, true);
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

/* TODO fails because of a race condition when starting up the engine before the view
void tst_QQmlDebugJS::setBreakpointWhenAttaching()
{
    int sourceLine = 49;
    QVERIFY(init(QLatin1String(TIMER_QMLFILE), false));

    client->connect();
    //The breakpoint is in a timer loop so we can set it after connect().
    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(TIMER_QMLFILE), sourceLine);
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));
}
*/

#if 0
void tst_QQmlDebugJS::setBreakpointOnEvent()
{
    QFAIL("Not implemented in V4.");

    //void setBreakpoint(QString type, QString target, int line = -1, int column = -1, bool enabled = false, QString condition = QString(), int ignoreCount = -1)

    QVERIFY(init(TIMER_QMLFILE));

    client->setBreakpoint(QLatin1String(EVENT), QLatin1String("triggered"), -1, -1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    QVariantMap body = value.value("body").toMap();

    QCOMPARE(QFileInfo(body.value("script").toMap().value("name").toString()).fileName(), QLatin1String(TIMER_QMLFILE));
}
#endif

void tst_QQmlDebugJS::clearBreakpoint()
{
    //void clearBreakpoint(int breakpoint);

    int sourceLine1 = 42;
    int sourceLine2 = 43;
    QVERIFY(init(CHANGEBREAKPOINT_QMLFILE));

    client->connect();
    //The breakpoints are in a timer loop so we can set them after connect().
    //Furthermore the breakpoints should be hit in the right order because setting of breakpoints
    //can only occur in the QML event loop. (see QCOMPARE for sourceLine2 below)
    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(CHANGEBREAKPOINT_QMLFILE), sourceLine1, -1, true);
    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(CHANGEBREAKPOINT_QMLFILE), sourceLine2, -1, true);

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

    QVERIFY(init(EXCEPTION_QMLFILE));
    client->setExceptionBreak(QJSDebugClient::All,true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));
}

void tst_QQmlDebugJS::stepNext()
{
    //void continueDebugging(StepAction stepAction, int stepCount = 1);

    int sourceLine = 42;
    QVERIFY(init(STEPACTION_QMLFILE));

    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(STEPACTION_QMLFILE), sourceLine, -1, true);
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

    int sourceLine = 46;
    int actualLine = 42;
    QVERIFY(init(STEPACTION_QMLFILE));

    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(STEPACTION_QMLFILE), sourceLine, 1, true);
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

    int sourceLine = 42;
    int actualLine = 46;
    QVERIFY(init(STEPACTION_QMLFILE));

    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(STEPACTION_QMLFILE), sourceLine, -1, true);
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

    int sourceLine1 = 46;
    int sourceLine2 = 43;
    QVERIFY(init(STEPACTION_QMLFILE));

    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(STEPACTION_QMLFILE), sourceLine1, -1, true);
    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(STEPACTION_QMLFILE), sourceLine2, -1, true);
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

    int sourceLine = 39;
    QVERIFY(init(ONCOMPLETED_QMLFILE));

    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(ONCOMPLETED_QMLFILE), sourceLine, -1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    client->backtrace();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(result())));
}

void tst_QQmlDebugJS::getFrameDetails()
{
    //void frame(int number = -1);

    int sourceLine = 39;
    QVERIFY(init(ONCOMPLETED_QMLFILE));

    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(ONCOMPLETED_QMLFILE), sourceLine, -1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    client->frame();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(result())));
}

void tst_QQmlDebugJS::getScopeDetails()
{
    //void scope(int number = -1, int frameNumber = -1);

    int sourceLine = 39;
    QVERIFY(init(ONCOMPLETED_QMLFILE));

    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(ONCOMPLETED_QMLFILE), sourceLine, -1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    client->scope();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(result())));
}

#if 0
void tst_QQmlDebugJS::evaluateInGlobalScope()
{
    //void evaluate(QString expr, bool global = false, bool disableBreak = false, int frame = -1, const QVariantMap &addContext = QVariantMap());

    QVERIFY(init());

    client->connect();
    client->evaluate(QLatin1String("console.log('Hello World')"), true);
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(evaluateResult())));

    //Verify the value of 'print'
    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    QVariantMap body = value.value("body").toMap();

    QCOMPARE(body.value("text").toString(),QLatin1String("undefined"));
}
#endif

#if 0
void tst_QQmlDebugJS::evaluateInLocalScope()
{
    //void evaluate(QString expr, bool global = false, bool disableBreak = false, int frame = -1, const QVariantMap &addContext = QVariantMap());

    int sourceLine = 47;
    QVERIFY(init(ONCOMPLETED_QMLFILE));

    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QLatin1String(ONCOMPLETED_QMLFILE), sourceLine, -1, true);
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
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(evaluateResult())));

    //Verify the value of 'timer.interval'
    jsonString = client->response;
    value = client->parser.call(QJSValueList() << QJSValue(jsonString)).toVariant().toMap();

    body = value.value("body").toMap();

    QCOMPARE(body.value("value").toInt(),10);
}
#endif

void tst_QQmlDebugJS::getScripts()
{
    //void scripts(int types = -1, QList<int> ids = QList<int>(), bool includeSource = false, QVariant filter = QVariant());

    QVERIFY(init());

    client->setBreakpoint(QLatin1String(SCRIPTREGEXP), QString(TEST_QMLFILE), 40, -1, true);
    client->connect();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(stopped())));

    client->scripts();
    QVERIFY(QQmlDebugTest::waitForSignal(client, SIGNAL(scriptsResult())));
    QString jsonString(client->response);
    QVariantMap value = client->parser.call(QJSValueList()
                                            << QJSValue(jsonString)).toVariant().toMap();

    QList<QVariant> scripts = value.value("body").toList();

    QCOMPARE(scripts.count(), 1);
    QVERIFY(scripts.first().toMap()[QStringLiteral("name")].toString().endsWith(QStringLiteral("data/test.qml")));
}

QTEST_MAIN(tst_QQmlDebugJS)

#include "tst_qqmldebugjs.moc"

