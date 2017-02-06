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

#include "qqmlnativedebugconnector.h"
#include "qqmldebugpacket.h"

#include <private/qhooks_p.h>

#include <QtQml/qjsengine.h>
#include <QtCore/qdebug.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qpointer.h>
#include <QtCore/qvector.h>

//#define TRACE_PROTOCOL(s) qDebug() << s
#define TRACE_PROTOCOL(s)

QT_USE_NAMESPACE

static bool expectSyncronousResponse = false;
Q_GLOBAL_STATIC(QByteArray, responseBuffer)

extern "C" {

Q_DECL_EXPORT const char *qt_qmlDebugMessageBuffer;
Q_DECL_EXPORT int qt_qmlDebugMessageLength;
Q_DECL_EXPORT bool qt_qmlDebugConnectionBlocker;

// In blocking mode, this will busy wait until the debugger sets block to false.
Q_DECL_EXPORT void qt_qmlDebugConnectorOpen();

// First thing, set the debug stream version. Please use this function as we might move the version
// member to some other place.
Q_DECL_EXPORT void qt_qmlDebugSetStreamVersion(int version)
{
    QQmlNativeDebugConnector::setDataStreamVersion(version);
}


// Break in this one to process output from an asynchronous message/
Q_DECL_EXPORT void qt_qmlDebugMessageAvailable()
{
}


// Break in this one to get notified about construction and destruction of
// interesting objects, such as QmlEngines.
Q_DECL_EXPORT void qt_qmlDebugObjectAvailable()
{
}

Q_DECL_EXPORT void qt_qmlDebugClearBuffer()
{
    responseBuffer->clear();
}

// Send a message to a service.
Q_DECL_EXPORT bool qt_qmlDebugSendDataToService(const char *serviceName, const char *hexData)
{
    QByteArray msg = QByteArray::fromHex(hexData);

    QQmlDebugConnector *instance = QQmlDebugConnector::instance();
    if (!instance)
        return false;

    QQmlDebugService *recipient = instance->service(serviceName);
    if (!recipient)
        return false;

    TRACE_PROTOCOL("Recipient: " << recipient << " got message: " << msg);
    expectSyncronousResponse = true;
    recipient->messageReceived(msg);
    expectSyncronousResponse = false;

    return true;
}

// Enable a service.
Q_DECL_EXPORT bool qt_qmlDebugEnableService(const char *data)
{
    QQmlDebugConnector *instance = QQmlDebugConnector::instance();
    if (!instance)
        return false;

    QString name = QString::fromLatin1(data);
    QQmlDebugService *service = instance->service(name);
    if (!service || service->state() == QQmlDebugService::Enabled)
        return false;

    service->stateAboutToBeChanged(QQmlDebugService::Enabled);
    service->setState(QQmlDebugService::Enabled);
    service->stateChanged(QQmlDebugService::Enabled);
    return true;
}

Q_DECL_EXPORT bool qt_qmlDebugDisableService(const char *data)
{
    QQmlDebugConnector *instance = QQmlDebugConnector::instance();
    if (!instance)
        return false;

    QString name = QString::fromLatin1(data);
    QQmlDebugService *service = instance->service(name);
    if (!service || service->state() == QQmlDebugService::Unavailable)
        return false;

    service->stateAboutToBeChanged(QQmlDebugService::Unavailable);
    service->setState(QQmlDebugService::Unavailable);
    service->stateChanged(QQmlDebugService::Unavailable);
    return true;
}

quintptr qt_qmlDebugTestHooks[] = {
    quintptr(1), // Internal Version
    quintptr(6), // Number of entries following
    quintptr(&qt_qmlDebugMessageBuffer),
    quintptr(&qt_qmlDebugMessageLength),
    quintptr(&qt_qmlDebugSendDataToService),
    quintptr(&qt_qmlDebugEnableService),
    quintptr(&qt_qmlDebugDisableService),
    quintptr(&qt_qmlDebugObjectAvailable)
};

// In blocking mode, this will busy wait until the debugger sets block to false.
Q_DECL_EXPORT void qt_qmlDebugConnectorOpen()
{
    TRACE_PROTOCOL("Opening native debug connector");

    // FIXME: Use a dedicated hook. Startup is a safe workaround, though,
    // as we are already beyond its only use.
    qtHookData[QHooks::Startup] = quintptr(&qt_qmlDebugTestHooks);

    while (qt_qmlDebugConnectionBlocker)
        ;

    TRACE_PROTOCOL("Opened native debug connector");
}

} // extern "C"

QT_BEGIN_NAMESPACE

QQmlNativeDebugConnector::QQmlNativeDebugConnector()
    : m_blockingMode(false)
{
    const QString args = commandLineArguments();
    const auto lstjsDebugArguments = args.splitRef(QLatin1Char(','));
    QStringList services;
    for (const QStringRef &strArgument : lstjsDebugArguments) {
        if (strArgument == QLatin1String("block")) {
            m_blockingMode = true;
        } else if (strArgument == QLatin1String("native")) {
            // Ignore. This is used to signal that this connector
            // should be loaded and that has already happened.
        } else if (strArgument.startsWith(QLatin1String("services:"))) {
            services.append(strArgument.mid(9).toString());
        } else if (!services.isEmpty()) {
            services.append(strArgument.toString());
        } else {
            qWarning("QML Debugger: Invalid argument \"%s\" detected. Ignoring the same.",
                     strArgument.toUtf8().constData());
        }
    }
    setServices(services);
}

QQmlNativeDebugConnector::~QQmlNativeDebugConnector()
{
    foreach (QQmlDebugService *service, m_services) {
        service->stateAboutToBeChanged(QQmlDebugService::NotConnected);
        service->setState(QQmlDebugService::NotConnected);
        service->stateChanged(QQmlDebugService::NotConnected);
    }
}

bool QQmlNativeDebugConnector::blockingMode() const
{
    return m_blockingMode;
}

QQmlDebugService *QQmlNativeDebugConnector::service(const QString &name) const
{
    for (QVector<QQmlDebugService *>::ConstIterator i = m_services.begin(); i != m_services.end();
         ++i) {
        if ((*i)->name() == name)
            return *i;
    }
    return 0;
}

void QQmlNativeDebugConnector::addEngine(QJSEngine *engine)
{
    Q_ASSERT(!m_engines.contains(engine));

    TRACE_PROTOCOL("Add engine to connector:" << engine);
    foreach (QQmlDebugService *service, m_services)
        service->engineAboutToBeAdded(engine);

    announceObjectAvailability(QLatin1String("qmlengine"), engine, true);

    foreach (QQmlDebugService *service, m_services)
        service->engineAdded(engine);

    m_engines.append(engine);
}

void QQmlNativeDebugConnector::removeEngine(QJSEngine *engine)
{
    Q_ASSERT(m_engines.contains(engine));

    TRACE_PROTOCOL("Remove engine from connector:" << engine);
    foreach (QQmlDebugService *service, m_services)
        service->engineAboutToBeRemoved(engine);

    announceObjectAvailability(QLatin1String("qmlengine"), engine, false);

    foreach (QQmlDebugService *service, m_services)
        service->engineRemoved(engine);

    m_engines.removeOne(engine);
}

bool QQmlNativeDebugConnector::hasEngine(QJSEngine *engine) const
{
    return m_engines.contains(engine);
}

void QQmlNativeDebugConnector::announceObjectAvailability(const QString &objectType,
                                                          QObject *object, bool available)
{
    QJsonObject ob;
    ob.insert(QLatin1String("objecttype"), objectType);
    ob.insert(QLatin1String("object"), QString::number(quintptr(object)));
    ob.insert(QLatin1String("available"), available);
    QJsonDocument doc;
    doc.setObject(ob);

    QByteArray ba = doc.toJson(QJsonDocument::Compact);
    qt_qmlDebugMessageBuffer = ba.constData();
    qt_qmlDebugMessageLength = ba.size();
    TRACE_PROTOCOL("Reporting engine availabilty");
    qt_qmlDebugObjectAvailable(); // Trigger native breakpoint.
}

bool QQmlNativeDebugConnector::addService(const QString &name, QQmlDebugService *service)
{
    TRACE_PROTOCOL("Add service to connector: " << qPrintable(name) << service);
    for (auto it = m_services.cbegin(), end = m_services.cend(); it != end; ++it) {
        if ((*it)->name() == name)
            return false;
    }

    connect(service, &QQmlDebugService::messageToClient,
            this, &QQmlNativeDebugConnector::sendMessage);
    connect(service, &QQmlDebugService::messagesToClient,
            this, &QQmlNativeDebugConnector::sendMessages);

    service->setState(QQmlDebugService::Unavailable);

    m_services << service;
    return true;
}

bool QQmlNativeDebugConnector::removeService(const QString &name)
{
    for (QVector<QQmlDebugService *>::Iterator i = m_services.begin(); i != m_services.end(); ++i) {
        if ((*i)->name() == name) {
            QQmlDebugService *service = *i;
            m_services.erase(i);
            service->setState(QQmlDebugService::NotConnected);

            disconnect(service, &QQmlDebugService::messagesToClient,
                       this, &QQmlNativeDebugConnector::sendMessages);
            disconnect(service, &QQmlDebugService::messageToClient,
                       this, &QQmlNativeDebugConnector::sendMessage);

            return true;
        }
    }
    return false;
}

bool QQmlNativeDebugConnector::open(const QVariantHash &configuration)
{
    m_blockingMode = configuration.value(QStringLiteral("block"), m_blockingMode).toBool();
    qt_qmlDebugConnectionBlocker = m_blockingMode;
    qt_qmlDebugConnectorOpen();
    return true;
}

void QQmlNativeDebugConnector::setDataStreamVersion(int version)
{
    Q_ASSERT(version <= QDataStream::Qt_DefaultCompiledVersion);
    s_dataStreamVersion = version;
}

void QQmlNativeDebugConnector::sendMessage(const QString &name, const QByteArray &message)
{
    (*responseBuffer) += name.toUtf8() + ' ' + QByteArray::number(message.size()) + ' ' + message;
    qt_qmlDebugMessageBuffer = responseBuffer->constData();
    qt_qmlDebugMessageLength = responseBuffer->size();
    // Responses are allowed to accumulate, the buffer will be cleared by
    // separate calls to qt_qmlDebugClearBuffer() once the synchronous
    // function return ('if' branch below) or in the native breakpoint handler
    // ('else' branch below).
    if (expectSyncronousResponse) {
        TRACE_PROTOCOL("Expected synchronous response in " << message);
        // Do not trigger the native breakpoint on qt_qmlDebugMessageFromService.
    } else {
        TRACE_PROTOCOL("Found asynchronous message in " << message);
        // Trigger native breakpoint.
        qt_qmlDebugMessageAvailable();
    }
}

void QQmlNativeDebugConnector::sendMessages(const QString &name, const QList<QByteArray> &messages)
{
    for (int i = 0; i != messages.size(); ++i)
        sendMessage(name, messages.at(i));
}

QQmlDebugConnector *QQmlNativeDebugConnectorFactory::create(const QString &key)
{
    return key == QLatin1String("QQmlNativeDebugConnector") ? new QQmlNativeDebugConnector : 0;
}

QT_END_NAMESPACE
