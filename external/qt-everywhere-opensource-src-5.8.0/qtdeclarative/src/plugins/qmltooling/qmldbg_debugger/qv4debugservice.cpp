/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qv4debugservice.h"
#include "qv4debugjob.h"
#include "qqmlengine.h"
#include "qqmldebugpacket.h"

#include <private/qv4engine_p.h>
#include <private/qv4isel_moth_p.h>
#include <private/qv4function_p.h>
#include <private/qqmldebugconnector_p.h>
#include <private/qv8engine_p.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>

const char *const V4_CONNECT = "connect";
const char *const V4_DISCONNECT = "disconnect";
const char *const V4_BREAK_ON_SIGNAL = "breakonsignal";
const char *const V4_PAUSE = "interrupt";

#define NO_PROTOCOL_TRACING
#ifdef NO_PROTOCOL_TRACING
#  define TRACE_PROTOCOL(x)
#else
#include <QtCore/QDebug>
#  define TRACE_PROTOCOL(x) x
#endif

QT_BEGIN_NAMESPACE

class V8CommandHandler;
class UnknownV8CommandHandler;

int QV4DebugServiceImpl::sequence = 0;

class V8CommandHandler
{
public:
    V8CommandHandler(const QString &command)
        : cmd(command)
    {}

    virtual ~V8CommandHandler()
    {}

    QString command() const { return cmd; }

    void handle(const QJsonObject &request, QV4DebugServiceImpl *s)
    {
        TRACE_PROTOCOL(qDebug() << "handling command" << command() << "...");

        req = request;
        seq = req.value(QLatin1String("seq"));
        debugService = s;

        handleRequest();
        if (!response.isEmpty()) {
            response[QLatin1String("type")] = QStringLiteral("response");
            debugService->send(response);
        }

        debugService = 0;
        seq = QJsonValue();
        req = QJsonObject();
        response = QJsonObject();
    }

    virtual void handleRequest() = 0;

protected:
    void addCommand() { response.insert(QStringLiteral("command"), cmd); }
    void addRequestSequence() { response.insert(QStringLiteral("request_seq"), seq); }
    void addSuccess(bool success) { response.insert(QStringLiteral("success"), success); }
    void addBody(const QJsonValue &body)
    {
        response.insert(QStringLiteral("body"), body);
    }

    void addRunning()
    {
        response.insert(QStringLiteral("running"), debugService->debuggerAgent.isRunning());
    }

    void addRefs(const QJsonArray &refs)
    {
        response.insert(QStringLiteral("refs"), refs);
    }

    void createErrorResponse(const QString &msg)
    {
        QJsonValue command = req.value(QLatin1String("command"));
        response.insert(QStringLiteral("command"), command);
        addRequestSequence();
        addSuccess(false);
        addRunning();
        response.insert(QStringLiteral("message"), msg);
    }

    int requestSequenceNr() const
    { return seq.toInt(-1); }

protected:
    QString cmd;
    QJsonObject req;
    QJsonValue seq;
    QV4DebugServiceImpl *debugService;
    QJsonObject response;
};

class UnknownV8CommandHandler: public V8CommandHandler
{
public:
    UnknownV8CommandHandler(): V8CommandHandler(QString()) {}

    virtual void handleRequest()
    {
        QString msg = QLatin1String("unimplemented command \"")
                + req.value(QLatin1String("command")).toString()
                + QLatin1Char('"');
        createErrorResponse(msg);
    }
};

namespace {
class V8VersionRequest: public V8CommandHandler
{
public:
    V8VersionRequest(): V8CommandHandler(QStringLiteral("version")) {}

    virtual void handleRequest()
    {
        addCommand();
        addRequestSequence();
        addSuccess(true);
        addRunning();
        QJsonObject body;
        body.insert(QStringLiteral("V8Version"),
                    QLatin1String("this is not V8, this is V4 in Qt " QT_VERSION_STR));
        body.insert(QStringLiteral("UnpausedEvaluate"), true);
        addBody(body);
    }
};

class V8SetBreakPointRequest: public V8CommandHandler
{
public:
    V8SetBreakPointRequest(): V8CommandHandler(QStringLiteral("setbreakpoint")) {}

    virtual void handleRequest()
    {
        // decypher the payload:
        QJsonObject args = req.value(QLatin1String("arguments")).toObject();
        if (args.isEmpty())
            return;

        QString type = args.value(QLatin1String("type")).toString();
        if (type != QLatin1String("scriptRegExp")) {
            createErrorResponse(QStringLiteral("breakpoint type \"%1\" is not implemented").arg(type));
            return;
        }

        QString fileName = args.value(QLatin1String("target")).toString();
        if (fileName.isEmpty()) {
            createErrorResponse(QStringLiteral("breakpoint has no file name"));
            return;
        }

        int line = args.value(QLatin1String("line")).toInt(-1);
        if (line < 0) {
            createErrorResponse(QStringLiteral("breakpoint has an invalid line number"));
            return;
        }

        bool enabled = args.value(QStringLiteral("enabled")).toBool(true);
        QString condition = args.value(QStringLiteral("condition")).toString();

        // set the break point:
        int id = debugService->debuggerAgent.addBreakPoint(fileName, line + 1, enabled, condition);

        // response:
        addCommand();
        addRequestSequence();
        addSuccess(true);
        addRunning();
        QJsonObject body;
        body.insert(QStringLiteral("type"), type);
        body.insert(QStringLiteral("breakpoint"), id);
        // It's undocumented, but V8 sends back an actual_locations array too. However, our
        // Debugger currently doesn't tell us when it resolved a breakpoint, so we'll leave them
        // pending until the breakpoint is hit for the first time.
        addBody(body);
    }
};

class V8ClearBreakPointRequest: public V8CommandHandler
{
public:
    V8ClearBreakPointRequest(): V8CommandHandler(QStringLiteral("clearbreakpoint")) {}

    virtual void handleRequest()
    {
        // decypher the payload:
        QJsonObject args = req.value(QLatin1String("arguments")).toObject();
        if (args.isEmpty())
            return;

        int id = args.value(QLatin1String("breakpoint")).toInt(-1);
        if (id < 0) {
            createErrorResponse(QStringLiteral("breakpoint has an invalid number"));
            return;
        }

        // remove the break point:
        debugService->debuggerAgent.removeBreakPoint(id);

        // response:
        addCommand();
        addRequestSequence();
        addSuccess(true);
        addRunning();
        QJsonObject body;
        body.insert(QStringLiteral("type"), QStringLiteral("scriptRegExp"));
        body.insert(QStringLiteral("breakpoint"), id);
        addBody(body);
    }
};

class V8BacktraceRequest: public V8CommandHandler
{
public:
    V8BacktraceRequest(): V8CommandHandler(QStringLiteral("backtrace")) {}

    virtual void handleRequest()
    {
        // decypher the payload:

        QJsonObject arguments = req.value(QLatin1String("arguments")).toObject();
        int fromFrame = arguments.value(QLatin1String("fromFrame")).toInt(0);
        int toFrame = arguments.value(QLatin1String("toFrame")).toInt(fromFrame + 10);
        // no idea what the bottom property is for, so we'll ignore it.

        QV4Debugger *debugger = debugService->debuggerAgent.pausedDebugger();
        if (!debugger) {
            createErrorResponse(QStringLiteral("Debugger has to be paused to retrieve backtraces."));
            return;
        }

        BacktraceJob job(debugger->collector(), fromFrame, toFrame);
        debugger->runInEngine(&job);

        // response:
        addCommand();
        addRequestSequence();
        addSuccess(true);
        addRunning();
        addBody(job.returnValue());
        addRefs(job.refs());
    }
};

class V8FrameRequest: public V8CommandHandler
{
public:
    V8FrameRequest(): V8CommandHandler(QStringLiteral("frame")) {}

    virtual void handleRequest()
    {
        // decypher the payload:
        QJsonObject arguments = req.value(QLatin1String("arguments")).toObject();
        const int frameNr = arguments.value(QLatin1String("number")).toInt(
                    debugService->selectedFrame());

        QV4Debugger *debugger = debugService->debuggerAgent.pausedDebugger();
        if (!debugger) {
            createErrorResponse(QStringLiteral("Debugger has to be paused to retrieve frames."));
            return;
        }

        if (frameNr < 0) {
            createErrorResponse(QStringLiteral("frame command has invalid frame number"));
            return;
        }

        FrameJob job(debugger->collector(), frameNr);
        debugger->runInEngine(&job);
        if (!job.wasSuccessful()) {
            createErrorResponse(QStringLiteral("frame retrieval failed"));
            return;
        }

        debugService->selectFrame(frameNr);

        // response:
        addCommand();
        addRequestSequence();
        addSuccess(true);
        addRunning();
        addBody(job.returnValue());
        addRefs(job.refs());
    }
};

class V8ScopeRequest: public V8CommandHandler
{
public:
    V8ScopeRequest(): V8CommandHandler(QStringLiteral("scope")) {}

    virtual void handleRequest()
    {
        // decypher the payload:
        QJsonObject arguments = req.value(QLatin1String("arguments")).toObject();
        const int frameNr = arguments.value(QLatin1String("frameNumber")).toInt(
                    debugService->selectedFrame());
        const int scopeNr = arguments.value(QLatin1String("number")).toInt(0);

        QV4Debugger *debugger = debugService->debuggerAgent.pausedDebugger();
        if (!debugger) {
            createErrorResponse(QStringLiteral("Debugger has to be paused to retrieve scope."));
            return;
        }

        if (frameNr < 0) {
            createErrorResponse(QStringLiteral("scope command has invalid frame number"));
            return;
        }
        if (scopeNr < 0) {
            createErrorResponse(QStringLiteral("scope command has invalid scope number"));
            return;
        }

        ScopeJob job(debugger->collector(), frameNr, scopeNr);
        debugger->runInEngine(&job);
        if (!job.wasSuccessful()) {
            createErrorResponse(QStringLiteral("scope retrieval failed"));
            return;
        }

        // response:
        addCommand();
        addRequestSequence();
        addSuccess(true);
        addRunning();
        addBody(job.returnValue());
        addRefs(job.refs());
    }
};

class V8LookupRequest: public V8CommandHandler
{
public:
    V8LookupRequest(): V8CommandHandler(QStringLiteral("lookup")) {}

    virtual void handleRequest()
    {
        // decypher the payload:
        QJsonObject arguments = req.value(QLatin1String("arguments")).toObject();
        QJsonArray handles = arguments.value(QLatin1String("handles")).toArray();

        QV4Debugger *debugger = debugService->debuggerAgent.pausedDebugger();
        if (!debugger) {
            const QList<QV4Debugger *> &debuggers = debugService->debuggerAgent.debuggers();
            if (debuggers.count() > 1) {
                createErrorResponse(QStringLiteral("Cannot lookup values if multiple debuggers are running and none is paused"));
                return;
            } else if (debuggers.count() == 0) {
                createErrorResponse(QStringLiteral("No debuggers available to lookup values"));
                return;
            }
            debugger = debuggers.first();
        }

        ValueLookupJob job(handles, debugger->collector());
        debugger->runInEngine(&job);
        if (!job.exceptionMessage().isEmpty()) {
            createErrorResponse(job.exceptionMessage());
        } else {
            // response:
            addCommand();
            addRequestSequence();
            addSuccess(true);
            addRunning();
            addBody(job.returnValue());
            addRefs(job.refs());
        }
    }
};

class V8ContinueRequest: public V8CommandHandler
{
public:
    V8ContinueRequest(): V8CommandHandler(QStringLiteral("continue")) {}

    virtual void handleRequest()
    {
        // decypher the payload:
        QJsonObject arguments = req.value(QLatin1String("arguments")).toObject();

        QV4Debugger *debugger = debugService->debuggerAgent.pausedDebugger();
        if (!debugger) {
            createErrorResponse(QStringLiteral("Debugger has to be paused in order to continue."));
            return;
        }
        debugService->debuggerAgent.clearAllPauseRequests();

        if (arguments.empty()) {
            debugger->resume(QV4Debugger::FullThrottle);
        } else {
            QJsonObject arguments = req.value(QLatin1String("arguments")).toObject();
            QString stepAction = arguments.value(QLatin1String("stepaction")).toString();
            const int stepcount = arguments.value(QLatin1String("stepcount")).toInt(1);
            if (stepcount != 1)
                qWarning() << "Step count other than 1 is not supported.";

            if (stepAction == QLatin1String("in")) {
                debugger->resume(QV4Debugger::StepIn);
            } else if (stepAction == QLatin1String("out")) {
                debugger->resume(QV4Debugger::StepOut);
            } else if (stepAction == QLatin1String("next")) {
                debugger->resume(QV4Debugger::StepOver);
            } else {
                createErrorResponse(QStringLiteral("continue command has invalid stepaction"));
                return;
            }
        }

        // response:
        addCommand();
        addRequestSequence();
        addSuccess(true);
        addRunning();
    }
};

class V8DisconnectRequest: public V8CommandHandler
{
public:
    V8DisconnectRequest(): V8CommandHandler(QStringLiteral("disconnect")) {}

    virtual void handleRequest()
    {
        debugService->debuggerAgent.removeAllBreakPoints();
        debugService->debuggerAgent.resumeAll();

        // response:
        addCommand();
        addRequestSequence();
        addSuccess(true);
        addRunning();
    }
};

class V8SetExceptionBreakRequest: public V8CommandHandler
{
public:
    V8SetExceptionBreakRequest(): V8CommandHandler(QStringLiteral("setexceptionbreak")) {}

    virtual void handleRequest()
    {
        bool wasEnabled = debugService->debuggerAgent.breakOnThrow();

        //decypher the payload:
        QJsonObject arguments = req.value(QLatin1String("arguments")).toObject();
        QString type = arguments.value(QLatin1String("type")).toString();
        bool enabled = arguments.value(QLatin1String("number")).toBool(!wasEnabled);

        if (type == QLatin1String("all")) {
            // that's fine
        } else if (type == QLatin1String("uncaught")) {
            createErrorResponse(QStringLiteral("breaking only on uncaught exceptions is not supported yet"));
            return;
        } else {
            createErrorResponse(QStringLiteral("invalid type for break on exception"));
            return;
        }

        // do it:
        debugService->debuggerAgent.setBreakOnThrow(enabled);

        QJsonObject body;
        body[QLatin1String("type")] = type;
        body[QLatin1String("enabled")] = debugService->debuggerAgent.breakOnThrow();

        // response:
        addBody(body);
        addRunning();
        addSuccess(true);
        addRequestSequence();
        addCommand();
    }
};

class V8ScriptsRequest: public V8CommandHandler
{
public:
    V8ScriptsRequest(): V8CommandHandler(QStringLiteral("scripts")) {}

    virtual void handleRequest()
    {
        //decypher the payload:
        QJsonObject arguments = req.value(QLatin1String("arguments")).toObject();
        int types = arguments.value(QLatin1String("types")).toInt(-1);
        if (types < 0 || types > 7) {
            createErrorResponse(QStringLiteral("invalid types value in scripts command"));
            return;
        } else if (types != 4) {
            createErrorResponse(QStringLiteral("unsupported types value in scripts command"));
            return;
        }

        // do it:
        QV4Debugger *debugger = debugService->debuggerAgent.pausedDebugger();
        if (!debugger) {
            createErrorResponse(QStringLiteral("Debugger has to be paused to retrieve scripts."));
            return;
        }

        GatherSourcesJob job(debugger->engine());
        debugger->runInEngine(&job);

        QJsonArray body;
        foreach (const QString &source, job.result()) {
            QJsonObject src;
            src[QLatin1String("name")] = source;
            src[QLatin1String("scriptType")] = 4;
            body.append(src);
        }

        addSuccess(true);
        addRunning();
        addBody(body);
        addCommand();
        addRequestSequence();
    }
};

// Request:
// {
//   "seq": 4,
//   "type": "request",
//   "command": "evaluate",
//   "arguments": {
//     "expression": "a",
//     "frame": 0
//   }
// }
//
// Response:
// {
//   "body": {
//     "handle": 3,
//     "type": "number",
//     "value": 1
//   },
//   "command": "evaluate",
//   "refs": [],
//   "request_seq": 4,
//   "running": false,
//   "seq": 5,
//   "success": true,
//   "type": "response"
// }
//
// The "value" key in "body" is the result of evaluating the expression in the request.
class V8EvaluateRequest: public V8CommandHandler
{
public:
    V8EvaluateRequest(): V8CommandHandler(QStringLiteral("evaluate")) {}

    virtual void handleRequest()
    {
        QJsonObject arguments = req.value(QLatin1String("arguments")).toObject();
        QString expression = arguments.value(QLatin1String("expression")).toString();
        int frame = -1;

        QV4Debugger *debugger = debugService->debuggerAgent.pausedDebugger();
        if (!debugger) {
            const QList<QV4Debugger *> &debuggers = debugService->debuggerAgent.debuggers();
            if (debuggers.count() > 1) {
                createErrorResponse(QStringLiteral("Cannot evaluate expressions if multiple debuggers are running and none is paused"));
                return;
            } else if (debuggers.count() == 0) {
                createErrorResponse(QStringLiteral("No debuggers available to evaluate expressions"));
                return;
            }
            debugger = debuggers.first();
        } else {
            frame = arguments.value(QLatin1String("frame")).toInt(0);
        }

        ExpressionEvalJob job(debugger->engine(), frame, expression, debugger->collector());
        debugger->runInEngine(&job);
        if (job.hasExeption()) {
            createErrorResponse(job.exceptionMessage());
        } else {
            addCommand();
            addRequestSequence();
            addSuccess(true);
            addRunning();
            addBody(job.returnValue());
            addRefs(job.refs());
        }
    }
};
} // anonymous namespace

void QV4DebugServiceImpl::addHandler(V8CommandHandler* handler)
{
    handlers[handler->command()] = handler;
}

V8CommandHandler *QV4DebugServiceImpl::v8CommandHandler(const QString &command) const
{
    V8CommandHandler *handler = handlers.value(command, 0);
    if (handler)
        return handler;
    else
        return unknownV8CommandHandler.data();
}

QV4DebugServiceImpl::QV4DebugServiceImpl(QObject *parent) :
    QQmlConfigurableDebugService<QV4DebugService>(1, parent),
    debuggerAgent(this), theSelectedFrame(0),
    unknownV8CommandHandler(new UnknownV8CommandHandler)
{
    addHandler(new V8VersionRequest);
    addHandler(new V8SetBreakPointRequest);
    addHandler(new V8ClearBreakPointRequest);
    addHandler(new V8BacktraceRequest);
    addHandler(new V8FrameRequest);
    addHandler(new V8ScopeRequest);
    addHandler(new V8LookupRequest);
    addHandler(new V8ContinueRequest);
    addHandler(new V8DisconnectRequest);
    addHandler(new V8SetExceptionBreakRequest);
    addHandler(new V8ScriptsRequest);
    addHandler(new V8EvaluateRequest);
}

QV4DebugServiceImpl::~QV4DebugServiceImpl()
{
    qDeleteAll(handlers);
}

void QV4DebugServiceImpl::engineAdded(QJSEngine *engine)
{
    QMutexLocker lock(&m_configMutex);
    if (engine) {
        QV4::ExecutionEngine *ee = QV8Engine::getV4(engine->handle());
        if (QQmlDebugConnector *server = QQmlDebugConnector::instance()) {
            if (ee) {
                ee->iselFactory.reset(new QV4::Moth::ISelFactory);
                QV4Debugger *debugger = new QV4Debugger(ee);
                if (state() == Enabled)
                    ee->setDebugger(debugger);
                debuggerAgent.addDebugger(debugger);
                debuggerAgent.moveToThread(server->thread());
            }
        }
    }
    QQmlConfigurableDebugService<QV4DebugService>::engineAdded(engine);
}

void QV4DebugServiceImpl::engineAboutToBeRemoved(QJSEngine *engine)
{
    QMutexLocker lock(&m_configMutex);
    if (engine){
        const QV4::ExecutionEngine *ee = QV8Engine::getV4(engine->handle());
        if (ee) {
            QV4Debugger *debugger = qobject_cast<QV4Debugger *>(ee->debugger());
            if (debugger)
                debuggerAgent.removeDebugger(debugger);
        }
    }
    QQmlConfigurableDebugService<QV4DebugService>::engineAboutToBeRemoved(engine);
}

void QV4DebugServiceImpl::stateAboutToBeChanged(State state)
{
    QMutexLocker lock(&m_configMutex);
    if (state == Enabled) {
        foreach (QV4Debugger *debugger, debuggerAgent.debuggers()) {
            QV4::ExecutionEngine *ee = debugger->engine();
            if (!ee->debugger())
                ee->setDebugger(debugger);
        }
    }
    QQmlConfigurableDebugService<QV4DebugService>::stateAboutToBeChanged(state);
}

void QV4DebugServiceImpl::signalEmitted(const QString &signal)
{
    //This function is only called by QQmlBoundSignal
    //only if there is a slot connected to the signal. Hence, there
    //is no need for additional check.

    //Parse just the name and remove the class info
    //Normalize to Lower case.
    QString signalName = signal.left(signal.indexOf(QLatin1Char('('))).toLower();

    foreach (const QString &signal, breakOnSignals) {
        if (signal == signalName) {
            // TODO: pause debugger
            break;
        }
    }
}

void QV4DebugServiceImpl::messageReceived(const QByteArray &message)
{
    QMutexLocker lock(&m_configMutex);

    QQmlDebugPacket ms(message);
    QByteArray header;
    ms >> header;

    TRACE_PROTOCOL(qDebug() << "received message with header" << header);

    if (header == "V8DEBUG") {
        QByteArray type;
        QByteArray payload;
        ms >> type >> payload;
        TRACE_PROTOCOL(qDebug() << "... type:" << type);

        if (type == V4_CONNECT) {
            emit messageToClient(name(), packMessage(type));
            stopWaiting();
        } else if (type == V4_PAUSE) {
            debuggerAgent.pauseAll();
            sendSomethingToSomebody(type);
        } else if (type == V4_BREAK_ON_SIGNAL) {
            QByteArray signal;
            bool enabled;
            ms >> signal >> enabled;
             //Normalize to lower case.
            QString signalName(QString::fromUtf8(signal).toLower());
            if (enabled)
                breakOnSignals.append(signalName);
            else
                breakOnSignals.removeOne(signalName);
        } else if (type == "v8request") {
            handleV8Request(payload);
        } else if (type == V4_DISCONNECT) {
            TRACE_PROTOCOL(qDebug() << "... payload:" << payload.constData());
            handleV8Request(payload);
        } else {
            sendSomethingToSomebody(type, 0);
        }
    }
}

void QV4DebugServiceImpl::sendSomethingToSomebody(const char *type, int magicNumber)
{
    QQmlDebugPacket rs;
    rs << QByteArray(type)
       << QByteArray::number(int(version())) << QByteArray::number(magicNumber);
    emit messageToClient(name(), packMessage(type, rs.data()));
}

void QV4DebugServiceImpl::handleV8Request(const QByteArray &payload)
{
    TRACE_PROTOCOL(qDebug() << "v8request, payload:" << payload.constData());

    QJsonDocument request = QJsonDocument::fromJson(payload);
    QJsonObject o = request.object();
    QJsonValue type = o.value(QLatin1String("type"));
    if (type.toString() == QLatin1String("request")) {
        QJsonValue command = o.value(QLatin1String("command"));
        V8CommandHandler *h = v8CommandHandler(command.toString());
        if (h)
            h->handle(o, this);
    }
}

QByteArray QV4DebugServiceImpl::packMessage(const QByteArray &command, const QByteArray &message)
{
    QQmlDebugPacket rs;
    static const QByteArray cmd("V8DEBUG");
    rs << cmd << command << message;
    return rs.data();
}

void QV4DebugServiceImpl::send(QJsonObject v8Payload)
{
    v8Payload[QLatin1String("seq")] = sequence++;
    QJsonDocument doc;
    doc.setObject(v8Payload);
#ifdef NO_PROTOCOL_TRACING
    QByteArray responseData = doc.toJson(QJsonDocument::Compact);
#else
    QByteArray responseData = doc.toJson(QJsonDocument::Indented);
#endif

    TRACE_PROTOCOL(qDebug() << "sending response for:" << responseData.constData() << endl);

    emit messageToClient(name(), packMessage("v8message", responseData));
}

void QV4DebugServiceImpl::selectFrame(int frameNr)
{
    theSelectedFrame = frameNr;
}

int QV4DebugServiceImpl::selectedFrame() const
{
    return theSelectedFrame;
}

QT_END_NAMESPACE
