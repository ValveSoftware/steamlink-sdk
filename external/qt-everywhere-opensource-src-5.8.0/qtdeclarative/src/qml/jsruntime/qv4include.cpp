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

#include "qv4include_p.h"
#include "qv4scopedvalue_p.h"

#include <QtQml/qjsengine.h>
#if QT_CONFIG(qml_network)
#include <QtNetwork/qnetworkrequest.h>
#include <QtNetwork/qnetworkreply.h>
#endif
#include <QtCore/qfile.h>
#include <QtQml/qqmlfile.h>

#include <private/qqmlengine_p.h>
#include <private/qv4engine_p.h>
#include <private/qv4functionobject_p.h>
#include <private/qv4script_p.h>
#include <private/qv4context_p.h>
#include <private/qqmlcontextwrapper_p.h>

QT_BEGIN_NAMESPACE

QV4Include::QV4Include(const QUrl &url, QV4::ExecutionEngine *engine,
                       QV4::QmlContext *qmlContext, const QV4::Value &callback)
    : v4(engine), m_url(url)
#if QT_CONFIG(qml_network)
    , m_redirectCount(0), m_network(0) , m_reply(0)
#endif
{
    if (qmlContext)
        m_qmlContext.set(engine, *qmlContext);
    if (callback.as<QV4::FunctionObject>())
        m_callbackFunction.set(engine, callback);

    m_resultObject.set(v4, resultValue(v4));

#if QT_CONFIG(qml_network)
    m_network = engine->v8Engine->networkAccessManager();

    QNetworkRequest request;
    request.setUrl(url);

    m_reply = m_network->get(request);
    QObject::connect(m_reply, SIGNAL(finished()), this, SLOT(finished()));
#else
    finished();
#endif
}

QV4Include::~QV4Include()
{
#if QT_CONFIG(qml_network)
    delete m_reply;
    m_reply = 0;
#endif
}

QV4::ReturnedValue QV4Include::resultValue(QV4::ExecutionEngine *v4, Status status)
{
    QV4::Scope scope(v4);

    // XXX It seems inefficient to create this object from scratch each time.
    QV4::ScopedObject o(scope, v4->newObject());
    QV4::ScopedString s(scope);
    QV4::ScopedValue v(scope);
    o->put((s = v4->newString(QStringLiteral("OK"))), (v = QV4::Primitive::fromInt32(Ok)));
    o->put((s = v4->newString(QStringLiteral("LOADING"))), (v = QV4::Primitive::fromInt32(Loading)));
    o->put((s = v4->newString(QStringLiteral("NETWORK_ERROR"))), (v = QV4::Primitive::fromInt32(NetworkError)));
    o->put((s = v4->newString(QStringLiteral("EXCEPTION"))), (v = QV4::Primitive::fromInt32(Exception)));
    o->put((s = v4->newString(QStringLiteral("status"))), (v = QV4::Primitive::fromInt32(status)));

    return o.asReturnedValue();
}

void QV4Include::callback(const QV4::Value &callback, const QV4::Value &status)
{
    if (!callback.isObject())
        return;
    QV4::ExecutionEngine *v4 = callback.as<QV4::Object>()->engine();
    QV4::Scope scope(v4);
    QV4::ScopedFunctionObject f(scope, callback);
    if (!f)
        return;

    QV4::ScopedCallData callData(scope, 1);
    callData->thisObject = v4->globalObject->asReturnedValue();
    callData->args[0] = status;
    f->call(scope, callData);
    if (scope.hasException())
        scope.engine->catchException();
}

QV4::ReturnedValue QV4Include::result()
{
    return m_resultObject.value();
}

#define INCLUDE_MAXIMUM_REDIRECT_RECURSION 15
void QV4Include::finished()
{
#if QT_CONFIG(qml_network)
    m_redirectCount++;

    if (m_redirectCount < INCLUDE_MAXIMUM_REDIRECT_RECURSION) {
        QVariant redirect = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (redirect.isValid()) {
            m_url = m_url.resolved(redirect.toUrl());
            delete m_reply;

            QNetworkRequest request;
            request.setUrl(m_url);

            m_reply = m_network->get(request);
            QObject::connect(m_reply, SIGNAL(finished()), this, SLOT(finished()));
            return;
        }
    }

    QV4::Scope scope(v4);
    QV4::ScopedObject resultObj(scope, m_resultObject.value());
    QV4::ScopedString status(scope, v4->newString(QStringLiteral("status")));
    if (m_reply->error() == QNetworkReply::NoError) {
        QByteArray data = m_reply->readAll();

        QString code = QString::fromUtf8(data);
        QmlIR::Document::removeScriptPragmas(code);

        QV4::Scoped<QV4::QmlContext> qml(scope, m_qmlContext.value());
        QV4::Script script(v4, qml, code, m_url.toString());

        script.parse();
        if (!scope.engine->hasException)
            script.run();
        if (scope.engine->hasException) {
            QV4::ScopedValue ex(scope, scope.engine->catchException());
            resultObj->put(status, QV4::ScopedValue(scope, QV4::Primitive::fromInt32(Exception)));
            QV4::ScopedString exception(scope, v4->newString(QStringLiteral("exception")));
            resultObj->put(exception, ex);
        } else {
            resultObj->put(status, QV4::ScopedValue(scope, QV4::Primitive::fromInt32(Ok)));
        }
    } else {
        resultObj->put(status, QV4::ScopedValue(scope, QV4::Primitive::fromInt32(NetworkError)));
    }
#else
    QV4::Scope scope(v4);
    QV4::ScopedObject resultObj(scope, m_resultObject.value());
    QV4::ScopedString status(scope, v4->newString(QStringLiteral("status")));
    resultObj->put(status, QV4::ScopedValue(scope, QV4::Primitive::fromInt32(NetworkError)));
#endif // qml_network

    QV4::ScopedValue cb(scope, m_callbackFunction.value());
    callback(cb, resultObj);

    disconnect();
    deleteLater();
}

/*
    Documented in qv8engine.cpp
*/
QV4::ReturnedValue QV4Include::method_include(QV4::CallContext *ctx)
{
    if (!ctx->argc())
        return QV4::Encode::undefined();

    QV4::Scope scope(ctx->engine());
    QQmlContextData *context = scope.engine->callingQmlContext();

    if (!context || !context->isJSContext)
        V4THROW_ERROR("Qt.include(): Can only be called from JavaScript files");

    QV4::ScopedValue callbackFunction(scope, QV4::Primitive::undefinedValue());
    if (ctx->argc() >= 2 && ctx->args()[1].as<QV4::FunctionObject>())
        callbackFunction = ctx->args()[1];

#if QT_CONFIG(qml_network)
    QUrl url(scope.engine->resolvedUrl(ctx->args()[0].toQStringNoThrow()));
    if (scope.engine->qmlEngine() && scope.engine->qmlEngine()->urlInterceptor())
        url = scope.engine->qmlEngine()->urlInterceptor()->intercept(url, QQmlAbstractUrlInterceptor::JavaScriptFile);

    QString localFile = QQmlFile::urlToLocalFileOrQrc(url);

    QV4::ScopedValue result(scope);
    QV4::Scoped<QV4::QmlContext> qmlcontext(scope, scope.engine->qmlContext());

    if (localFile.isEmpty()) {
        QV4Include *i = new QV4Include(url, scope.engine, qmlcontext, callbackFunction);
        result = i->result();

    } else {
        QScopedPointer<QV4::Script> script;

        if (const QQmlPrivate::CachedQmlUnit *cachedUnit = QQmlMetaType::findCachedCompilationUnit(url)) {
            QV4::CompiledData::CompilationUnit *jsUnit = cachedUnit->createCompilationUnit();
            script.reset(new QV4::Script(scope.engine, qmlcontext, jsUnit));
        } else {
            QFile f(localFile);

            if (f.open(QIODevice::ReadOnly)) {
                QByteArray data = f.readAll();
                QString code = QString::fromUtf8(data);
                QmlIR::Document::removeScriptPragmas(code);

                script.reset(new QV4::Script(scope.engine, qmlcontext, code, url.toString()));
            }
        }

        if (!script.isNull()) {
            script->parse();
            if (!scope.engine->hasException)
                script->run();
            if (scope.engine->hasException) {
                QV4::ScopedValue ex(scope, scope.engine->catchException());
                result = resultValue(scope.engine, Exception);
                QV4::ScopedString exception(scope, scope.engine->newString(QStringLiteral("exception")));
                result->as<QV4::Object>()->put(exception, ex);
            } else {
                result = resultValue(scope.engine, Ok);
            }
        } else {
            result = resultValue(scope.engine, NetworkError);
        }

        callback(callbackFunction, result);
    }

    return result->asReturnedValue();
#else
    QV4::ScopedValue result(scope);
    result = resultValue(scope.engine, NetworkError);
    callback(callbackFunction, result);
    return result->asReturnedValue();
#endif
}

QT_END_NAMESPACE
