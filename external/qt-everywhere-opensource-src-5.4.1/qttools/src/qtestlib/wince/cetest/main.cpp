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

#ifdef QT_CETEST_NO_ACTIVESYNC
#   include "cetcpsyncconnection.h"
#else
#   include "activesyncconnection.h"
#endif

const int SLEEP_AFTER_RESET = 60000; // sleep for 1 minute
const int SLEEP_RECONNECT   = 2000;  // sleep for 2 seconds before trying another reconnect

#include "deployment.h"
#include <option.h>
#include <project.h>
#include <property.h>
#include <qstringlist.h>
#include <qfileinfo.h>
#include <qdir.h>
#include <iostream>
using namespace std;

const int debugLevel = 0;
void debugOutput(const QString& text, int level)
{
    if (level <= debugLevel)
        cout << qPrintable(text) << endl;
}

// needed for QMake sources to compile
QString project_builtin_regx() { return QString();}
static QString pwd;
QString qmake_getpwd()
{
    if(pwd.isNull())
        pwd = QDir::currentPath();
    return pwd;
}
bool qmake_setpwd(const QString &p)
{
    if(QDir::setCurrent(p)) {
        pwd = QDir::currentPath();
        return true;
    }
    return false;
}

namespace TestConfiguration {
    QString localExecutable;
    QString localQtConf;
    QString remoteTestPath;
    QString remoteLibraryPath;
    QString remoteExecutable;
    QString remoteResultFile;

    bool testDebug;
    void init()
    {
        testDebug = true;
        localQtConf = QLatin1String("no");
        remoteTestPath = QLatin1String("\\Program Files\\qt_test");
        remoteLibraryPath = remoteTestPath;
        remoteResultFile = QLatin1String("\\qt_test_results.txt");
    }
}

void usage()
{
    cout <<
        "QTestLib options\n"
        "  All valid QTestLib command-line options are accepted.\n"
        "  For details of QTestLib options, refer to the QTestLib Manual.\n"
        "\n"
        "cetest specific options\n"
        " -debug            : Test debug version[default]\n"
        " -release          : Test release version\n"
        " -libpath <path>   : Remote path to deploy Qt libraries to\n"
        " -reset            : Reset device before starting a test\n"
        " -awake            : Device does not go sleep mode\n"
        " -qt-delete        : Delete the Qt libraries after execution\n"
        " -project-delete   : Delete the project file(s) after execution\n"
        " -delete           : Delete everything deployed after execution\n"
        " -conf             : Specify location of qt.conf file\n"
        " -f <file>         : Specify project file\n"
        " -cache <file>     : Specify .qmake.cache file to use\n"
        " -d                : Increase qmake debugging \n"
        " -timeout <value>  : Specify a timeout value after which the test will be terminated\n"
        "                     -1 specifies waiting forever (default)\n"
        "                      0 specifies starting the process detached\n"
        "                     >0 wait <value> seconds\n"
        " -help             : This help\n"
        "\n";
}

int main(int argc, char **argv)
{
    QStringList arguments;
    for (int i=0; i<argc; ++i)
        arguments.append(QString::fromLatin1(argv[i]));

    TestConfiguration::init();

    QStringList launchArguments;
    QString resultFile;
    QString proFile;
    QString cacheFile;
    int timeout = -1;
    bool cleanupQt = false;
    bool cleanupProject = false;
    bool deviceReset = false;
    bool keepAwake = false;

    for (int i=1; i<arguments.size(); ++i) {
        if (arguments.at(i).toLower() == QLatin1String("-help")
                    || arguments.at(i).toLower() == QLatin1String("--help")
                    || arguments.at(i).toLower() == QLatin1String("/?")) {
            usage();
            return 0;
        } else if (arguments.at(i).toLower() == QLatin1String("-o")) {
            if (++i == arguments.size()) {
                cout << "Error: No output file specified!" << endl;
                return -1;
            }
            resultFile = arguments.at(i);
        } else if (arguments.at(i).toLower() == QLatin1String("-eventdelay")
                    || arguments.at(i).toLower() == QLatin1String("-keydelay")
                    || arguments.at(i).toLower() == QLatin1String("-mousedelay")
                    || arguments.at(i).toLower() == QLatin1String("-maxwarnings")) {
            launchArguments.append(arguments.at(i++));
            if (i == arguments.size()) {
                cout << "Please specify value for:" << qPrintable(arguments.at(i-1).mid(1)) << endl;
                return -1;
            }
            launchArguments.append(arguments.at(i));
        } else if (arguments.at(i).toLower() == QLatin1String("-debug")) {
            TestConfiguration::testDebug = true;
            Option::before_user_vars.append("CONFIG-=release");
            Option::before_user_vars.append("CONFIG+=debug");
        } else if (arguments.at(i).toLower() == QLatin1String("-release")) {
            TestConfiguration::testDebug = false;
            Option::before_user_vars.append("CONFIG-=debug");
            Option::before_user_vars.append("CONFIG+=release");
        } else if (arguments.at(i).toLower() == QLatin1String("-libpath")) {
            if (++i == arguments.size()) {
                cout << "Error: No library path specified!" << endl;
                return -1;
            }
            TestConfiguration::remoteLibraryPath = arguments.at(i);
        } else if (arguments.at(i).toLower() == QLatin1String("-qt-delete")) {
            cleanupQt = true;
        } else if (arguments.at(i).toLower() == QLatin1String("-project-delete")) {
            cleanupProject = true;
        } else if (arguments.at(i).toLower() == QLatin1String("-delete")) {
            cleanupQt = true;
            cleanupProject = true;
        } else if (arguments.at(i).toLower() == QLatin1String("-reset")) {
            deviceReset = true;
        } else if (arguments.at(i).toLower() == QLatin1String("-awake")) {
            keepAwake = true;
        } else if (arguments.at(i).toLower() == QLatin1String("-conf")) {
            if (++i == arguments.size()) {
                cout << "Error: No qt.conf file specified!" << endl;
                return -1;
            }
            if (!QFileInfo(arguments.at(i)).exists())
                cout << "Warning: could not find qt.conf file at:" << qPrintable(arguments.at(i)) << endl;
            else
                TestConfiguration::localQtConf = arguments.at(i);
        } else if (arguments.at(i).toLower() == QLatin1String("-f")) {
            if (++i == arguments.size()) {
                cout << "Error: No output file specified!" << endl;
                return -1;
            }
            proFile = arguments.at(i);
        } else if (arguments.at(i).toLower() == QLatin1String("-cache")) {
            if (++i == arguments.size()) {
                cout << "Error: No cache file specified!" << endl;
                return -1;
            }
            cacheFile = arguments.at(i);
        } else if (arguments.at(i).toLower() == QLatin1String("-d")) {
            Option::debug_level++;
        } else if (arguments.at(i).toLower() == QLatin1String("-timeout")) {
            if (++i == arguments.size()) {
                cout << "Error: No timeout value specified!" << endl;
                return -1;
            }
            timeout = QString(arguments.at(i)).toInt();
        } else {
            launchArguments.append(arguments.at(i));
        }
    }

    // check for .pro file
    if (proFile.isEmpty()) {
        proFile = QDir::current().dirName() + QLatin1String(".pro");
        if (!QFileInfo(proFile).exists()) {
            cout << "Error: Could not find project file in current directory." << endl;
            return -1;
        }
        debugOutput(QString::fromLatin1("Using Project File:").append(proFile),1);
    }else {
        if (!QFileInfo(proFile).exists()) {
            cout << "Error: Project file does not exist " << qPrintable(proFile) << endl;
            return -1;
        }
    }

    Option::before_user_vars.append("CONFIG+=build_pass");

    // read target and deployment rules passing the .pro to use instead of
    //      relying on qmake guessing the .pro to use
    int qmakeArgc = 2;
    QByteArray ba(QFile::encodeName(proFile));
    char* proFileEncodedName =  ba.data();
    char* qmakeArgv[2] = { "qmake.exe", proFileEncodedName };

    Option::qmake_mode = Option::QMAKE_GENERATE_NOTHING;
    Option::output_dir = qmake_getpwd();
    if (!cacheFile.isEmpty())
        Option::mkfile::cachefile = cacheFile;
    int ret = Option::init(qmakeArgc, qmakeArgv);
    if(ret != Option::QMAKE_CMDLINE_SUCCESS) {
        cout << "Error: could not parse " << qPrintable(proFile) << endl;
        return -1;
    }

    QMakeProperty prop;
    QMakeProject project(&prop);

    project.read(proFile);
    if (project.values("TEMPLATE").join(" ").toLower() != QString("app")) {
        cout << "Error: Can only test executables!" << endl;
        return -1;
    }
    // Check whether the project is still in debug/release mode after reading
    // If .pro specifies to be one mode only, we need to accept this
    if (project.isActiveConfig("debug") && !project.isActiveConfig("release")) {
        TestConfiguration::testDebug = true;
        debugOutput("ActiveConfig: debug only in .pro.", 1);
    }
    if (!project.isActiveConfig("debug") && project.isActiveConfig("release")) {
        TestConfiguration::testDebug = false;
        debugOutput("ActiveConfig: release only in .pro.", 1);
    }

    // determine what is the real mkspec to use if the default mkspec is being used
    if (Option::mkfile::qmakespec.endsWith("/default"))
        project.values("QMAKESPEC") = project.values("QMAKESPEC_ORIGINAL");
    else
        project.values("QMAKESPEC") = QStringList() << Option::mkfile::qmakespec;

   // ensure that QMAKESPEC is non-empty .. to meet requirements of QList::at()
   if (project.values("QMAKESPEC").isEmpty()){
       cout << "Error: QMAKESPEC not set after parsing " << qPrintable(proFile) << endl;
       return -1;
   }

   // ensure that QT_CE_C_RUNTIME is non-empty .. to meet requirements of QList::at()
   if (project.values("QT_CE_C_RUNTIME").isEmpty()){
       cout << "Error: QT_CE_C_RUNTIME not defined in mkspec/qconfig.pri " << qPrintable(project.values("QMAKESPEC").join(" "));
       return -1;
   }

    QString destDir = project.values("DESTDIR").join(" ");
    if (!destDir.isEmpty()) {
        if (QDir::isRelativePath(destDir)) {
            QFileInfo fi(proFile);
            if (destDir == QLatin1String("."))
                destDir = fi.absolutePath() + "/" + destDir + "/" + (TestConfiguration::testDebug ? "debug" : "release");
            else
                destDir = fi.absolutePath() + QDir::separator() + destDir;
        }
    } else {
        QFileInfo fi(proFile);
        destDir = fi.absolutePath();
        destDir += QDir::separator() + QLatin1String(TestConfiguration::testDebug ? "debug" : "release");
    }

    DeploymentList qtDeploymentList;
    DeploymentList projectDeploymentList;

    TestConfiguration::localExecutable = Option::fixPathToLocalOS(destDir + QDir::separator() + project.values("TARGET").join(" ") + QLatin1String(".exe"));
    TestConfiguration::remoteTestPath = QLatin1String("\\Program Files\\") + Option::fixPathToLocalOS(project.values("TARGET").join(QLatin1String(" ")));
    if (!arguments.contains(QLatin1String("-libpath"), Qt::CaseInsensitive))
        TestConfiguration::remoteLibraryPath = TestConfiguration::remoteTestPath;

    QString targetExecutable = Option::fixPathToLocalOS(project.values("TARGET").join(QLatin1String(" ")));
    int last = targetExecutable.lastIndexOf(QLatin1Char('\\'));
    targetExecutable = targetExecutable.mid( last == -1 ? 0 : last+1 );
    TestConfiguration::remoteExecutable = TestConfiguration::remoteTestPath + QDir::separator() + targetExecutable + QLatin1String(".exe");
    projectDeploymentList.append(CopyItem(TestConfiguration::localExecutable , TestConfiguration::remoteExecutable));

    // deploy
#ifdef QT_CETEST_NO_ACTIVESYNC
    CeTcpSyncConnection connection;
#else
    ActiveSyncConnection connection;
#endif
    if (!connection.connect()) {
        cout << "Error: Could not connect to device!" << endl;
        return -1;
    }
    DeploymentHandler deployment;
    deployment.setConnection(&connection);

    deployment.initQtDeploy(&project, qtDeploymentList, TestConfiguration::remoteLibraryPath);
    deployment.initProjectDeploy(&project , projectDeploymentList, TestConfiguration::remoteTestPath);

    // add qt.conf
    if (TestConfiguration::localQtConf != QLatin1String("no")) {
        QString qtConfOrigin = QFileInfo(TestConfiguration::localQtConf).absoluteFilePath();
        QString qtConfTarget = Option::fixPathToLocalOS(TestConfiguration::remoteTestPath + QDir::separator() + QLatin1String("qt.conf"));
        projectDeploymentList.append(CopyItem(qtConfOrigin, qtConfTarget));
    }

    if (!deployment.deviceDeploy(qtDeploymentList) || !deployment.deviceDeploy(projectDeploymentList)) {
        cout << "Error: Could not copy file(s) to device" << endl;
        return -1;
    }
    // device power mode
    if (keepAwake)
    {
        int retVal = 0;
        if (!connection.setDeviceAwake(true, &retVal)) {
            cout << "Error: Could not set unattended mode on device" << endl;
            return -1;
        }
    }

    // reset device
    if (deviceReset)
    {
        if (!connection.resetDevice()) {
        //if (!connection.toggleDevicePower( &retVal)) {
            cout << "Error: Could not reset the device" << endl;
            return -1;
        }
        cout << " Entering sleep after reset for " << SLEEP_AFTER_RESET / 1000 << " seconds ... " << endl;
        Sleep(SLEEP_AFTER_RESET);
        cout << " ... woke up. " << endl;
        connection.disconnect();
        // reconnect after reset
        int retryCount = 21;
        while (--retryCount)
        {
            if (!connection.connect())
                Sleep(SLEEP_RECONNECT);
            else
                break;
        }
        if (!connection.isConnected())
        {
            cout << "Error: Could not connect to device!" << endl;
            return -1;
        }
    }

    // launch
    launchArguments.append("-o");
    launchArguments.append(TestConfiguration::remoteResultFile);

    cout << endl << "Remote Launch:" << qPrintable(TestConfiguration::remoteExecutable) << " " << qPrintable(launchArguments.join(" ")) << endl;
    if (!connection.execute(TestConfiguration::remoteExecutable, launchArguments.join(" "), timeout)) {
        cout << "Error: Could not execute target file" << endl;
        return -1;
    }


    // copy result file
    // show results
    if (resultFile.isEmpty()) {
        QString tempResultFile = Option::fixPathToLocalOS(QDir::tempPath() + "/qt_ce_temp_result_file.txt");
        if (connection.copyFileFromDevice(TestConfiguration::remoteResultFile, tempResultFile)) {
            QFile file(tempResultFile);
            QByteArray arr;
            if (file.open(QIODevice::ReadOnly)) {
                arr = file.readAll();
                cout << arr.constData() << endl;
            }
            file.close();
            file.remove();
        }
    } else {
        connection.copyFileFromDevice(TestConfiguration::remoteResultFile, resultFile);
    }

    // delete
    connection.deleteFile(TestConfiguration::remoteResultFile);
    if (cleanupQt)
        deployment.cleanup(qtDeploymentList);
    if (cleanupProject)
        deployment.cleanup(projectDeploymentList);
    return 0;
}
