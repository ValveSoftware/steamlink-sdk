/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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


#include "qbluetoothtransferreply_bluez_p.h"
#include "qbluetoothaddress.h"

#include "bluez/obex_client_p.h"
#include "bluez/obex_agent_p.h"
#include "bluez/obex_transfer_p.h"
#include "bluez/bluez5_helper_p.h"
#include "bluez/obex_client1_bluez5_p.h"
#include "bluez/obex_objectpush1_bluez5_p.h"
#include "bluez/obex_transfer1_bluez5_p.h"
#include "bluez/properties_p.h"
#include "qbluetoothtransferreply.h"

#include <QtCore/QAtomicInt>
#include <QtCore/QLoggingCategory>
#include <QtCore/QVector>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrentRun>

static const QLatin1String agentPath("/qt/agent");
static QAtomicInt agentPathCounter;

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_BLUEZ)

QBluetoothTransferReplyBluez::QBluetoothTransferReplyBluez(QIODevice *input, const QBluetoothTransferRequest &request,
                                                           QBluetoothTransferManager *parent)
:   QBluetoothTransferReply(parent),
    m_client(0), m_agent(0), m_clientBluez(0), m_objectPushBluez(0),
    m_tempfile(0), m_source(input),
    m_running(false), m_finished(false), m_size(0),
    m_error(QBluetoothTransferReply::NoError), m_errorStr(), m_transfer_path()
{
    setRequest(request);
    setManager(parent);

    if (!input) {
        qCWarning(QT_BT_BLUEZ) << "Invalid input device (null)";
        m_errorStr = QBluetoothTransferReply::tr("Invalid input device (null)");
        m_error = QBluetoothTransferReply::FileNotFoundError;
        m_finished = true;
        return;
    }

    if (isBluez5()) {
        m_clientBluez = new OrgBluezObexClient1Interface(QStringLiteral("org.bluez.obex"),
                                                        QStringLiteral("/org/bluez/obex"),
                                                        QDBusConnection::sessionBus(), this);


    } else {
        m_client = new OrgOpenobexClientInterface(QStringLiteral("org.openobex.client"),
                                                  QStringLiteral("/"),
                                                  QDBusConnection::sessionBus());

        m_agent_path = agentPath;
        m_agent_path.append(QStringLiteral("/%1%2/%3").
                            arg(sanitizeNameForDBus(QCoreApplication::applicationName())).
                            arg(QCoreApplication::applicationPid()).
                            arg(agentPathCounter.fetchAndAddOrdered(1)));

        m_agent = new AgentAdaptor(this);

        if (!QDBusConnection::sessionBus().registerObject(m_agent_path, this))
            qCWarning(QT_BT_BLUEZ) << "Failed creating obex agent dbus objects";
    }

    QMetaObject::invokeMethod(this, "start", Qt::QueuedConnection);
    m_running = true;
}

/*!
    Destroys the QBluetoothTransferReply object.
*/
QBluetoothTransferReplyBluez::~QBluetoothTransferReplyBluez()
{
    QDBusConnection::sessionBus().unregisterObject(m_agent_path);
    delete m_client;
}

bool QBluetoothTransferReplyBluez::start()
{
    QFile *file = qobject_cast<QFile *>(m_source);

    if(!file){
        m_tempfile = new QTemporaryFile(this );
        m_tempfile->open();
        qCDebug(QT_BT_BLUEZ) << "Not a QFile, making a copy" << m_tempfile->fileName();
        if (!m_source->isReadable()) {
            m_errorStr = QBluetoothTransferReply::tr("QIODevice cannot be read. "
                                                     "Make sure it is open for reading.");
            m_error = QBluetoothTransferReply::IODeviceNotReadableError;
            m_finished = true;
            m_running = false;

            emit QBluetoothTransferReply::error(m_error);
            emit finished(this);
            return false;
        }

        QFutureWatcher<bool> *watcher = new QFutureWatcher<bool>();
        QObject::connect(watcher, SIGNAL(finished()), this, SLOT(copyDone()));

        QFuture<bool> results = QtConcurrent::run(QBluetoothTransferReplyBluez::copyToTempFile, m_tempfile, m_source);
        watcher->setFuture(results);
    }
    else {
        if (!file->exists()) {
            m_errorStr = QBluetoothTransferReply::tr("Source file does not exist");
            m_error = QBluetoothTransferReply::FileNotFoundError;
            m_finished = true;
            m_running = false;

            emit QBluetoothTransferReply::error(m_error);
            emit finished(this);
            return false;
        }
        if (request().address().isNull()) {
            m_errorStr = QBluetoothTransferReply::tr("Invalid target address");
            m_error = QBluetoothTransferReply::HostNotFoundError;
            m_finished = true;
            m_running = false;

            emit QBluetoothTransferReply::error(m_error);
            emit finished(this);
            return false;
        }
        m_size = file->size();
        startOPP(file->fileName());
    }
    return true;
}

bool QBluetoothTransferReplyBluez::copyToTempFile(QIODevice *to, QIODevice *from)
{
    QVector<char> block(4096);
    int size;

    while ((size = from->read(block.data(), block.size())) > 0) {
        if (size != to->write(block.data(), size)) {
            return false;
        }
    }

    return true;
}

void QBluetoothTransferReplyBluez::cleanupSession()
{
    if (!m_objectPushBluez)
        return;

    QDBusPendingReply<> reply = m_clientBluez->RemoveSession(QDBusObjectPath(m_objectPushBluez->path()));
    reply.waitForFinished();
    if (reply.isError())
        qCWarning(QT_BT_BLUEZ) << "Abort: Cannot remove obex session";

    delete m_objectPushBluez;
    m_objectPushBluez = 0;
}

void QBluetoothTransferReplyBluez::copyDone()
{
    m_size = m_tempfile->size();
    startOPP(m_tempfile->fileName());
    QObject::sender()->deleteLater();
}

void QBluetoothTransferReplyBluez::sessionCreated(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<QDBusObjectPath> reply = *watcher;
    if (reply.isError()) {
        qCWarning(QT_BT_BLUEZ) << "Failed to create obex session:"
                               << reply.error().name() << reply.reply().errorMessage();

        m_errorStr = QBluetoothTransferReply::tr("Invalid target address");
        m_error = QBluetoothTransferReply::HostNotFoundError;
        m_finished = true;
        m_running = false;

        emit QBluetoothTransferReply::error(m_error);
        emit finished(this);

        watcher->deleteLater();
        return;
    }

    m_objectPushBluez = new OrgBluezObexObjectPush1Interface(QStringLiteral("org.bluez.obex"),
                                                           reply.value().path(),
                                                           QDBusConnection::sessionBus(), this);
    QDBusPendingReply<QDBusObjectPath, QVariantMap> newReply = m_objectPushBluez->SendFile(fileToTranser);
    QDBusPendingCallWatcher *newWatcher = new QDBusPendingCallWatcher(newReply, this);
    connect(newWatcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            SLOT(sessionStarted(QDBusPendingCallWatcher*)));
    watcher->deleteLater();
}

void QBluetoothTransferReplyBluez::sessionStarted(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<QDBusObjectPath, QVariantMap> reply = *watcher;
    if (reply.isError()) {
        qCWarning(QT_BT_BLUEZ) << "Failed to start obex session:"
                               << reply.error().name() << reply.reply().errorMessage();

        m_errorStr = QBluetoothTransferReply::tr("Push session cannot be started");
        m_error = QBluetoothTransferReply::SessionError;
        m_finished = true;
        m_running = false;

        cleanupSession();

        emit QBluetoothTransferReply::error(m_error);
        emit finished(this);

        watcher->deleteLater();
        return;
    }

    const QDBusObjectPath path = reply.argumentAt<0>();
    const QVariantMap map = reply.argumentAt<1>();
    m_transfer_path = path.path();

    //watch the transfer
    OrgFreedesktopDBusPropertiesInterface *properties = new OrgFreedesktopDBusPropertiesInterface(
                            QStringLiteral("org.bluez.obex"), path.path(),
                            QDBusConnection::sessionBus(), this);
    connect(properties, SIGNAL(PropertiesChanged(QString,QVariantMap,QStringList)),
            SLOT(sessionChanged(QString,QVariantMap,QStringList)));

    watcher->deleteLater();
}

void QBluetoothTransferReplyBluez::sessionChanged(const QString &interface,
                                                  const QVariantMap &changed_properties,
                                                  const QStringList &)
{
    if (changed_properties.contains(QStringLiteral("Transferred"))) {
        emit transferProgress(
            changed_properties.value(QStringLiteral("Transferred")).toULongLong(),
            m_size);
    }

    if (changed_properties.contains(QStringLiteral("Status"))) {
        const QString s = changed_properties.
                value(QStringLiteral("Status")).toString();
        if (s == QStringLiteral("complete")
            || s == QStringLiteral("error")) {

            m_transfer_path.clear();
            m_finished = true;
            m_running = false;

            if (s == QStringLiteral("error")) {
                m_error = QBluetoothTransferReply::UnknownError;
                m_errorStr = tr("Unknown Error");

                emit QBluetoothTransferReply::error(m_error);
            } else { // complete
                // allow progress bar to complete
                emit transferProgress(m_size, m_size);
            }

            cleanupSession();

            emit finished(this);
        } // ignore "active", "queued" & "suspended" status
    }
    qCDebug(QT_BT_BLUEZ) << "Transfer update:" << interface << changed_properties;
}

void QBluetoothTransferReplyBluez::startOPP(const QString &filename)
{
    if (m_client) { // Bluez 4
        QVariantMap device;
        QStringList files;

        device.insert(QStringLiteral("Destination"), request().address().toString());
        files << filename;

        QDBusObjectPath path(m_agent_path);
        QDBusPendingReply<> sendReply = m_client->SendFiles(device, files, path);

        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(sendReply, this);
        connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
                         this, SLOT(sendReturned(QDBusPendingCallWatcher*)));
    } else { //Bluez 5
        fileToTranser = filename;
        QVariantMap mapping;
        mapping.insert(QStringLiteral("Target"), QStringLiteral("opp"));

        QDBusPendingReply<QDBusObjectPath> reply = m_clientBluez->CreateSession(
                                        request().address().toString(), mapping);

        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
        connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
                SLOT(sessionCreated(QDBusPendingCallWatcher*)));
    }
}

void QBluetoothTransferReplyBluez::sendReturned(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<> sendReply = *watcher;
    if(sendReply.isError()){
        m_finished = true;
        m_running = false;
        m_errorStr = sendReply.error().message();
        if (m_errorStr == QStringLiteral("Could not open file for sending")) {
            m_error = QBluetoothTransferReply::FileNotFoundError;
            m_errorStr = tr("Could not open file for sending");
        } else if (m_errorStr == QStringLiteral("The transfer was canceled")) {
            m_error = QBluetoothTransferReply::UserCanceledTransferError;
            m_errorStr = tr("The transfer was canceled");
        } else {
            m_error = QBluetoothTransferReply::UnknownError;
        }

        emit QBluetoothTransferReply::error(m_error);
        emit finished(this);
    }
}

QBluetoothTransferReply::TransferError QBluetoothTransferReplyBluez::error() const
{
    return m_error;
}

QString QBluetoothTransferReplyBluez::errorString() const
{
    return m_errorStr;
}

void QBluetoothTransferReplyBluez::Complete(const QDBusObjectPath &in0)
{
    Q_UNUSED(in0);
    m_transfer_path.clear();
    m_finished = true;
    m_running = false;
}

void QBluetoothTransferReplyBluez::Error(const QDBusObjectPath &in0, const QString &in1)
{
    Q_UNUSED(in0);
    m_transfer_path.clear();
    m_finished = true;
    m_running = false;
    m_errorStr = in1;
    if (in1 == QStringLiteral("Could not open file for sending")) {
        m_error = QBluetoothTransferReply::FileNotFoundError;
        m_errorStr = tr("Could not open file for sending");
    } else if (in1 == QStringLiteral("Operation canceled")) {
        m_error = QBluetoothTransferReply::UserCanceledTransferError;
        m_errorStr = QBluetoothTransferReply::tr("Operation canceled");
    } else {
        m_error = QBluetoothTransferReply::UnknownError;
    }

    emit QBluetoothTransferReply::error(m_error);
    emit finished(this);
}

void QBluetoothTransferReplyBluez::Progress(const QDBusObjectPath &in0, qulonglong in1)
{
    Q_UNUSED(in0);
    emit transferProgress(in1, m_size);
}

void QBluetoothTransferReplyBluez::Release()
{
    if(m_errorStr.isEmpty())
        emit finished(this);
}

QString QBluetoothTransferReplyBluez::Request(const QDBusObjectPath &in0)
{
    m_transfer_path = in0.path();

    return QString();
}

/*!
    Returns true if this reply has finished; otherwise returns false.
*/
bool QBluetoothTransferReplyBluez::isFinished() const
{
    return m_finished;
}

/*!
    Returns true if this reply is running; otherwise returns false.
*/
bool QBluetoothTransferReplyBluez::isRunning() const
{
    return m_running;
}

void QBluetoothTransferReplyBluez::abort()
{
    if (m_transfer_path.isEmpty())
        return;

    if (m_client) {
        OrgOpenobexTransferInterface xfer(QStringLiteral("org.openobex.client"),
                                          m_transfer_path,
                                          QDBusConnection::sessionBus());

        QDBusPendingReply<> reply = xfer.Cancel();
        reply.waitForFinished();
        if (reply.isError())
            qCWarning(QT_BT_BLUEZ) << "Failed to abort transfer" << reply.error().message();

    } else if (m_clientBluez) {
        OrgBluezObexTransfer1Interface iface(QStringLiteral("org.bluez.obex"),
                                             m_transfer_path,
                                             QDBusConnection::sessionBus());

        QDBusPendingReply<> reply = iface.Cancel();
        reply.waitForFinished();
        if (reply.isError())
            qCDebug(QT_BT_BLUEZ) << "Failed to abort transfer" << reply.error().message();

        m_error = QBluetoothTransferReply::UserCanceledTransferError;
        m_errorStr = tr("Operation canceled");

        cleanupSession();

        emit QBluetoothTransferReply::error(m_error);
        emit finished(this);
    }
}

#include "moc_qbluetoothtransferreply_bluez_p.cpp"

QT_END_NAMESPACE
