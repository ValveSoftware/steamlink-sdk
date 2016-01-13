/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the tools applications of the Qt Toolkit.
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

#ifndef RUNNER_H
#define RUNNER_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtCore/QScopedPointer>
#include <QtCore/QLoggingCategory>

QT_USE_NAMESPACE

class RunnerPrivate;
class Runner
{
public:
    static QMap<QString, QStringList> deviceNames();

    Runner(const QString &app, const QStringList &arguments, const QString &profile = QString(),
           const QString &device = QString());
    ~Runner();

    bool isValid() const;
    QString app() const;
    QStringList arguments() const;
    int deviceIndex() const;

    bool install(bool removeFirst = false);
    bool remove();
    bool start();
    bool enableDebugging(const QString &debuggerExecutable, const QString &debuggerArguments);
    bool disableDebugging();
    bool suspend();
    bool stop();
    bool wait(int maxWaitTime = 0);
    bool setupTest();
    bool collectTest();
    qint64 pid();
    int exitCode();

private:
    QScopedPointer<RunnerPrivate> d_ptr;
    Q_DECLARE_PRIVATE(Runner)
};

Q_DECLARE_LOGGING_CATEGORY(lcWinRtRunner)
Q_DECLARE_LOGGING_CATEGORY(lcWinRtRunnerApp)

#endif // RUNNER_H
