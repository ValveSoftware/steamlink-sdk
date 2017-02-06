/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "runner.h"

#include "runnerengine.h"

#ifndef RTRUNNER_NO_APPXPHONE
#include "appxphoneengine.h"
#endif
#ifndef RTRUNNER_NO_APPXLOCAL
#include "appxlocalengine.h"
#endif

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QLoggingCategory>

QT_USE_NAMESPACE

Q_LOGGING_CATEGORY(lcWinRtRunner, "qt.winrtrunner")
Q_LOGGING_CATEGORY(lcWinRtRunnerApp, "qt.winrtrunner.app")

class RunnerPrivate
{
public:
    bool isValid;
    QString app;
    QString manifest;
    QStringList arguments;
    int deviceIndex;
    QString deviceOutputFile;
    QString localOutputFile;

    QString profile;
    QScopedPointer<RunnerEngine> engine;
};

QMap<QString, QStringList> Runner::deviceNames()
{
    QMap<QString, QStringList> deviceNames;
#ifndef RTRUNNER_NO_APPXLOCAL
    deviceNames.insert(QStringLiteral("Appx"), AppxLocalEngine::deviceNames());
#endif
#ifndef RTRUNNER_NO_APPXPHONE
    deviceNames.insert(QStringLiteral("Phone"), AppxPhoneEngine::deviceNames());
#endif
    return deviceNames;
}

Runner::Runner(const QString &app, const QStringList &arguments,
               const QString &profile, const QString &deviceName)
    : d_ptr(new RunnerPrivate)
{
    Q_D(Runner);
    d->isValid = false;
    d->app = app;
    d->arguments = arguments;
    d->profile = profile;

    bool deviceIndexKnown;
    d->deviceIndex = deviceName.toInt(&deviceIndexKnown);
#ifndef RTRUNNER_NO_APPXPHONE
    if (!deviceIndexKnown) {
        d->deviceIndex = AppxPhoneEngine::deviceNames().indexOf(deviceName);
        if (d->deviceIndex < 0)
            d->deviceIndex = 0;
    }
    if ((d->profile.isEmpty() || d->profile.toLower() == QStringLiteral("appxphone"))
            && AppxPhoneEngine::canHandle(this)) {
        if (RunnerEngine *engine = AppxPhoneEngine::create(this)) {
            d->engine.reset(engine);
            d->isValid = true;
            qCWarning(lcWinRtRunner) << "Using the AppxPhone profile.";
            return;
        }
    }
#endif
#ifndef RTRUNNER_NO_APPXLOCAL
    if (!deviceIndexKnown) {
        d->deviceIndex = AppxLocalEngine::deviceNames().indexOf(deviceName);
        if (d->deviceIndex < 0)
            d->deviceIndex = 0;
    }
    if ((d->profile.isEmpty() || d->profile.toLower() == QStringLiteral("appx"))
            && AppxLocalEngine::canHandle(this)) {
        if (RunnerEngine *engine = AppxLocalEngine::create(this)) {
            d->engine.reset(engine);
            d->isValid = true;
            qCWarning(lcWinRtRunner) << "Using the Appx profile.";
            return;
        }
    }
#endif
    // Place other engines here

    qCWarning(lcWinRtRunner) << "Unable to find a run profile for" << app << ".";
}

Runner::~Runner()
{
}

bool Runner::isValid() const
{
    Q_D(const Runner);
    return d->isValid;
}

QString Runner::app() const
{
    Q_D(const Runner);
    return d->app;
}

QStringList Runner::arguments() const
{
    Q_D(const Runner);
    return d->arguments;
}

int Runner::deviceIndex() const
{
    Q_D(const Runner);
    return d->deviceIndex;
}

bool Runner::install(bool removeFirst)
{
    Q_D(Runner);
    Q_ASSERT(d->engine);

    return d->engine->install(removeFirst);
}

bool Runner::remove()
{
    Q_D(Runner);
    Q_ASSERT(d->engine);

    return d->engine->remove();
}

bool Runner::start()
{
    Q_D(Runner);
    Q_ASSERT(d->engine);

    return d->engine->start();
}

bool Runner::enableDebugging(const QString &debuggerExecutable, const QString &debuggerArguments)
{
    Q_D(Runner);
    Q_ASSERT(d->engine);

    return d->engine->enableDebugging(debuggerExecutable, debuggerArguments);
}

bool Runner::disableDebugging()
{
    Q_D(Runner);
    Q_ASSERT(d->engine);

    return d->engine->disableDebugging();
}

bool Runner::suspend()
{
    Q_D(Runner);
    Q_ASSERT(d->engine);

    return d->engine->suspend();
}

bool Runner::stop()
{
    Q_D(Runner);
    Q_ASSERT(d->engine);

    return d->engine->stop();
}

bool Runner::wait(int maxWaitTime)
{
    Q_D(Runner);
    Q_ASSERT(d->engine);

    return d->engine->waitForFinished(maxWaitTime);
}

bool Runner::setupTest()
{
    Q_D(Runner);
    Q_ASSERT(d->engine);

    // Fix-up output path
    int outputIndex = d->arguments.indexOf(QStringLiteral("-o")) + 1;
    if (outputIndex > 0 && d->arguments.size() > outputIndex) {
        d->localOutputFile = d->arguments.at(outputIndex);
    } else {
        if (outputIndex > 0)
            d->arguments.removeAt(outputIndex);
        d->localOutputFile = QFileInfo(d->engine->executable()).baseName() + QStringLiteral("_output.txt");
        d->arguments.append(QStringLiteral("-o"));
        d->arguments.append(QString());
        outputIndex = d->arguments.length() - 1;
    }
    d->deviceOutputFile = d->engine->devicePath(d->localOutputFile);
    d->arguments[outputIndex] = d->deviceOutputFile;

    // Write a qt.conf to the executable directory
    QDir executableDir = QFileInfo(d->engine->executable()).absoluteDir();
    QFile qtConf(executableDir.absoluteFilePath(QStringLiteral("qt.conf")));
    if (!qtConf.exists()) {
        if (!qtConf.open(QFile::WriteOnly)) {
            qCWarning(lcWinRtRunner) << "Could not open qt.conf for writing.";
            return false;
        }
        qtConf.write(QByteArrayLiteral("[Paths]\nPlugins=/"));
    }

    return true;
}

bool Runner::collectTest()
{
    Q_D(Runner);
    Q_ASSERT(d->engine);

    // Fetch test output
    if (!d->engine->receiveFile(d->deviceOutputFile, d->localOutputFile)) {
        qCWarning(lcWinRtRunner).nospace().noquote()
                << "Unable to copy test output file \""
                << QDir::toNativeSeparators(d->deviceOutputFile)
                << "\" to local file \"" << QDir::toNativeSeparators(d->localOutputFile) << "\".";
        return false;
    }

    QFile testResults(d->localOutputFile);
    if (!testResults.open(QFile::ReadOnly)) {
        qCWarning(lcWinRtRunner) << "Unable to read test results:" << testResults.errorString();
        return false;
    }

    const QByteArray contents = testResults.readAll();
    std::fputs(contents.constData(), stdout);
    return true;
}

qint64 Runner::pid()
{
    Q_D(Runner);
    if (!d->engine)
        return -1;

    return d->engine->pid();
}

int Runner::exitCode()
{
    Q_D(Runner);
    if (!d->engine)
        return -1;

    return d->engine->exitCode();
}
