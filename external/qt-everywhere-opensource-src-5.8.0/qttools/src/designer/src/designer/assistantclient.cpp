/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#include "assistantclient.h"

#include <QtCore/QString>
#include <QtCore/QProcess>
#include <QtCore/QDir>
#include <QtCore/QLibraryInfo>
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>
#include <QtCore/QObject>
#include <QtCore/QTextStream>
#include <QtCore/QCoreApplication>

QT_BEGIN_NAMESPACE

enum { debugAssistantClient = 0 };

AssistantClient::AssistantClient() :
    m_process(0)
{
}

AssistantClient::~AssistantClient()
{
    if (isRunning()) {
        m_process->terminate();
        m_process->waitForFinished();
    }
    delete m_process;
}

bool AssistantClient::showPage(const QString &path, QString *errorMessage)
{
    QString cmd = QStringLiteral("SetSource ");
    cmd += path;
    return sendCommand(cmd, errorMessage);
}

bool AssistantClient::activateIdentifier(const QString &identifier, QString *errorMessage)
{
    QString cmd = QStringLiteral("ActivateIdentifier ");
    cmd += identifier;
    return sendCommand(cmd, errorMessage);
}

bool AssistantClient::activateKeyword(const QString &keyword, QString *errorMessage)
{
    QString cmd = QStringLiteral("ActivateKeyword ");
    cmd += keyword;
    return sendCommand(cmd, errorMessage);
}

bool AssistantClient::sendCommand(const QString &cmd, QString *errorMessage)
{
    if (debugAssistantClient)
        qDebug() << "sendCommand " << cmd;
    if (!ensureRunning(errorMessage))
        return false;
    if (!m_process->isWritable() || m_process->bytesToWrite() > 0) {
        *errorMessage = QCoreApplication::translate("AssistantClient", "Unable to send request: Assistant is not responding.");
        return false;
    }
    QTextStream str(m_process);
    str << cmd << QLatin1Char('\n') << endl;
    return true;
}

bool AssistantClient::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

QString AssistantClient::binary()
{
    QString app = QLibraryInfo::location(QLibraryInfo::BinariesPath) + QDir::separator();
#if !defined(Q_OS_MACOS)
    app += QStringLiteral("assistant");
#else
    app += QStringLiteral("Assistant.app/Contents/MacOS/Assistant");
#endif

#if defined(Q_OS_WIN)
    app += QStringLiteral(".exe");
#endif

    return app;
}

bool AssistantClient::ensureRunning(QString *errorMessage)
{
    if (isRunning())
        return true;

    if (!m_process)
        m_process = new QProcess;

    const QString app = binary();
    if (!QFileInfo(app).isFile()) {
        *errorMessage = QCoreApplication::translate("AssistantClient", "The binary '%1' does not exist.").arg(app);
        return false;
    }
    if (debugAssistantClient)
        qDebug() << "Running " << app;
    // run
    QStringList args(QStringLiteral("-enableRemoteControl"));
    m_process->start(app, args);
    if (!m_process->waitForStarted()) {
        *errorMessage = QCoreApplication::translate("AssistantClient", "Unable to launch assistant (%1).").arg(app);
        return false;
    }
    return true;
}

QString AssistantClient::documentUrl(const QString &module, int qtVersion)
{
    if (qtVersion == 0)
        qtVersion = QT_VERSION;
    QString rc;
    QTextStream(&rc) << "qthelp://org.qt-project." << module << '.'
                     << (qtVersion >> 16) << ((qtVersion >> 8) & 0xFF) << (qtVersion & 0xFF)
                     << '/' << module << '/';
    return rc;
}

QString AssistantClient::designerManualUrl(int qtVersion)
{
    return documentUrl(QStringLiteral("qtdesigner"), qtVersion);
}

QString AssistantClient::qtReferenceManualUrl(int qtVersion)
{
    return documentUrl(QStringLiteral("qtdoc"), qtVersion);
}

QT_END_NAMESPACE
