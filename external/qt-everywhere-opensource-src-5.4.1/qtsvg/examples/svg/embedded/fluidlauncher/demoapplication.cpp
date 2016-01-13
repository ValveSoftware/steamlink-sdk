/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

#include <QDebug>
#include <QDir>

#include "demoapplication.h"



DemoApplication::DemoApplication(QString executableName, QString caption, QString imageName, QStringList args)
{
    imagePath = imageName;
    appCaption = caption;

    if (executableName[0] == QLatin1Char('/'))
        executablePath = executableName;
    else
        executablePath = QDir::cleanPath(QDir::currentPath() + QLatin1Char('/') + executableName);
  
#ifdef WIN32
    if (!executablePath.endsWith(QLatin1String(".exe")))
        executablePath.append(QLatin1String(".exe"));
#endif

    arguments = args;

    process.setProcessChannelMode(QProcess::ForwardedChannels);

    QObject::connect( &process, SIGNAL(finished(int,QProcess::ExitStatus)), 
                      this, SLOT(processFinished(int,QProcess::ExitStatus)));

    QObject::connect( &process, SIGNAL(error(QProcess::ProcessError)), 
                      this, SLOT(processError(QProcess::ProcessError)));

    QObject::connect( &process, SIGNAL(started()), this, SLOT(processStarted()));
}


void DemoApplication::launch()
{
    process.start(executablePath, arguments);
}

QImage DemoApplication::getImage() const
{
    if (imagePath.isEmpty())
        return QImage();

    // in local dir?
    QImage result(imagePath);
    if (!result.isNull())
        return result;

    // provided by qrc
    result = QImage(QString(":/fluidlauncher/%1").arg(imagePath));
    return result;
}

QString DemoApplication::getCaption()
{
    return appCaption;
}

void DemoApplication::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);

    emit demoFinished();

    QObject::disconnect(this, SIGNAL(demoStarted()), 0, 0);
    QObject::disconnect(this, SIGNAL(demoFinished()), 0, 0);
}

void DemoApplication::processError(QProcess::ProcessError err)
{
    qDebug() << "Process error: " << err;
    if (err == QProcess::Crashed)
        emit demoFinished();
}


void DemoApplication::processStarted()
{
    emit demoStarted();
}






