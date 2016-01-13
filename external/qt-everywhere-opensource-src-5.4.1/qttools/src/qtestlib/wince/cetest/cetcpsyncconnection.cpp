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

#include "CeTcpSyncConnection.h"
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo>

static const QString ceTcpSyncProgram = "cetcpsync";
extern void debugOutput(const QString& text, int level);

CeTcpSyncConnection::CeTcpSyncConnection()
        : AbstractRemoteConnection()
        , connected(false)
{
}

CeTcpSyncConnection::~CeTcpSyncConnection()
{
    if (isConnected())
        disconnect();
}

bool CeTcpSyncConnection::connect(QVariantList&)
{
    // We connect with each command, so this is always true
    // The command itself will fail then
    const QString cmd = ceTcpSyncProgram + " noop";
    if (system(qPrintable(cmd)) != 0)
        return false;
    connected = true;
    return true;
}

void CeTcpSyncConnection::disconnect()
{
    connected = false;
}

bool CeTcpSyncConnection::isConnected() const
{
    return connected;
}

inline QString boolToString(bool b)
{
    return b ? "true" : "false";
}

static bool fileTimeFromString(FILETIME& ft, const QString& str)
{
    int idx = str.indexOf("*");
    if (idx <= 0)
        return false;
    bool ok;
    ft.dwLowDateTime = str.left(idx).toULong(&ok);
    if (!ok)
        return false;
    ft.dwHighDateTime = str.mid(idx+1).toULong(&ok);
    return ok;
}

static QString fileTimeToString(FILETIME& ft)
{
    return QString::number(ft.dwLowDateTime) + "*" + QString::number(ft.dwHighDateTime);
}

bool CeTcpSyncConnection::copyFileToDevice(const QString &localSource, const QString &deviceDest, bool failIfExists)
{
    QString cmd = ceTcpSyncProgram + " copyFileToDevice \"" + localSource + "\" \"" + deviceDest + "\" " + boolToString(failIfExists);
    return system(qPrintable(cmd)) == 0;
}

bool CeTcpSyncConnection::copyDirectoryToDevice(const QString &localSource, const QString &deviceDest, bool recursive)
{
    QString cmd = ceTcpSyncProgram + " copyDirectoryToDevice \"" + localSource + "\" \"" + deviceDest + "\" " + boolToString(recursive);
    return system(qPrintable(cmd)) == 0;
}

bool CeTcpSyncConnection::copyFileFromDevice(const QString &deviceSource, const QString &localDest, bool failIfExists)
{
    QString cmd = ceTcpSyncProgram + " copyFileFromDevice \"" + deviceSource + "\" \"" + localDest + "\" " + boolToString(failIfExists);
    return system(qPrintable(cmd)) == 0;
}

bool CeTcpSyncConnection::copyDirectoryFromDevice(const QString &deviceSource, const QString &localDest, bool recursive)
{
    QString cmd = ceTcpSyncProgram + " copyDirectoryFromDevice \"" + deviceSource + "\" \"" + localDest + "\" " + boolToString(recursive);
    return system(qPrintable(cmd)) == 0;
}

bool CeTcpSyncConnection::copyFile(const QString &srcFile, const QString &destFile, bool failIfExists)
{
    QString cmd = ceTcpSyncProgram + " copyFile \"" + srcFile + "\" \"" + destFile + "\" " + boolToString(failIfExists);
    return system(qPrintable(cmd)) == 0;
}

bool CeTcpSyncConnection::copyDirectory(const QString &srcDirectory, const QString &destDirectory,
                                        bool recursive)
{
    QString cmd = ceTcpSyncProgram + " copyDirectory \"" + srcDirectory + "\" \"" + destDirectory + "\" " + boolToString(recursive);
    return system(qPrintable(cmd)) == 0;
}

bool CeTcpSyncConnection::deleteFile(const QString &fileName)
{
    QString cmd = ceTcpSyncProgram + " deleteFile \"" + fileName + "\"";
    return system(qPrintable(cmd)) == 0;
}

bool CeTcpSyncConnection::deleteDirectory(const QString &directory, bool recursive, bool failIfContentExists)
{
    QString cmd = ceTcpSyncProgram + " deleteDirectory \"" + directory + "\" " + boolToString(recursive) + " " + boolToString(failIfContentExists);
    return system(qPrintable(cmd)) == 0;
}

bool CeTcpSyncConnection::execute(QString program, QString arguments, int timeout, int *returnValue)
{
    QString cmd = ceTcpSyncProgram + " execute \"" + program + "\" \"" + arguments + "\" " + QString::number(timeout);
    int exitCode = system(qPrintable(cmd));
    if (returnValue)
        *returnValue = exitCode;
    return true;
}

bool CeTcpSyncConnection::createDirectory(const QString &path, bool deleteBefore)
{
    QString cmd = ceTcpSyncProgram + " createDirectory \"" + path + "\" " + boolToString(deleteBefore);
    return system(qPrintable(cmd)) == 0;
}

bool CeTcpSyncConnection::timeStampForLocalFileTime(FILETIME* fTime) const
{
    QString cmd = ceTcpSyncProgram + " timeStampForLocalFileTime " + fileTimeToString(*fTime) + " >qt_cetcpsyncdata.txt";
    if (system(qPrintable(cmd)) != 0)
        return false;

    QFile file("qt_cetcpsyncdata.txt");
    if (!file.open(QIODevice::ReadOnly))
        return false;

    bool result = fileTimeFromString(*fTime, file.readLine());
    file.close();
    file.remove();
    return result;
}

bool CeTcpSyncConnection::fileCreationTime(const QString &fileName, FILETIME* deviceCreationTime) const
{
    QString cmd = ceTcpSyncProgram + " fileCreationTime \"" + fileName + "\" >qt_cetcpsyncdata.txt";
    if (system(qPrintable(cmd)) != 0)
        return false;

    QFile file("qt_cetcpsyncdata.txt");
    if (!file.open(QIODevice::ReadOnly))
        return false;

    bool result = fileTimeFromString(*deviceCreationTime, file.readLine());
    file.close();
    file.remove();
    return result;
}

bool CeTcpSyncConnection::resetDevice()
{
    qWarning("CeTcpSyncConnection::resetDevice not implemented");
    return false;
}

bool CeTcpSyncConnection::toggleDevicePower(int *returnValue)
{
    Q_UNUSED(returnValue);
    qWarning("CeTcpSyncConnection::toggleDevicePower not implemented");
    return false;
}

bool CeTcpSyncConnection::setDeviceAwake(bool activate, int *returnValue)
{
    Q_UNUSED(activate);
    Q_UNUSED(returnValue);
    qWarning("CeTcpSyncConnection::setDeviceAwake not implemented");
    return false;
}
