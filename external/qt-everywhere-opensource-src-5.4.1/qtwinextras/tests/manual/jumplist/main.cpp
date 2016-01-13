/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
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

#include "testwidget.h"

#include <QApplication>
#include <QSettings>
#include <QDir>

void associateFileType()
{
    QString exeFileName = QCoreApplication::applicationFilePath();
    exeFileName = exeFileName.right(exeFileName.length() - exeFileName.lastIndexOf("/") - 1);
    QString appName = "QtWinExtras JumpList Test";

    QSettings regApplications("HKEY_CURRENT_USER\\Software\\Classes\\Applications\\" + exeFileName, QSettings::NativeFormat);
    regApplications.setValue("FriendlyAppName", appName);

    regApplications.beginGroup("SupportedTypes");
    regApplications.setValue(".txt", QString());
    regApplications.endGroup();

    regApplications.beginGroup("shell");
    regApplications.beginGroup("open");
    regApplications.setValue("FriendlyAppName", appName);
    regApplications.beginGroup("command");
    regApplications.setValue(".", '"' + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + "\" \"%1\"");
    regApplications.endGroup();
    regApplications.endGroup();
    regApplications.endGroup();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    associateFileType();

    TestWidget w;
    if (QCoreApplication::arguments().contains("-fullscreen"))
        w.showFullScreen();
    else
        w.show();

    return a.exec();
}
