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

#include <QtCore/QCommandLineParser>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtCore/QLoggingCategory>

#include <iostream>

#include "runner.h"

QT_USE_NAMESPACE

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription(QLatin1String("winrtrunner installs, runs, and collects test "
                                                   "results for packages made with Qt."));
    parser.addPositionalArgument(QStringLiteral("package [arguments]"),
                                 QLatin1String("The executable or package manifest to act upon. "
                                               "Arguments after the package name will be passed "
                                               "to the application when it starts."));

    QCommandLineOption testOption(QStringLiteral("test"),
                                  QLatin1String("Install, start, collect output, stop (if needed), "
                                                "and uninstall the package. This is the "
                                                "default action of winrtrunner."));
    parser.addOption(testOption);

    QCommandLineOption startOption(QStringLiteral("start"),
                                   QLatin1String("Start the package. The package is installed if "
                                                 "it is not already installed. Pass --install to "
                                                 "force reinstallation."));
    parser.addOption(startOption);

    QCommandLineOption debugOption(QStringLiteral("debug"),
                                   QLatin1String("Start the package with the debugger attached. "
                                                 "The package is installed if it is not already "
                                                 "installed. Pass --install to force "
                                                 "reinstallation."),
                                   QLatin1Literal("debugger"));
    parser.addOption(debugOption);

    QCommandLineOption debuggerArgumentsOption(QStringLiteral("debugger-arguments"),
                                               QLatin1String("Arguments that are passed to the "
                                                             "debugger when --debug is used. If no "
                                                             "debugger was provided this option is "
                                                             "ignored."),
                                               QLatin1String("arguments"));
    parser.addOption(debuggerArgumentsOption);

    QCommandLineOption suspendOption(QStringLiteral("suspend"),
                                     QLatin1String("Suspend a running package. When combined "
                                                   "with --stop or --test, the app will be "
                                                   "suspended before being terminated."));
    parser.addOption(suspendOption);

    QCommandLineOption stopOption(QStringLiteral("stop"),
                                  QLatin1String("Terminate a running package. Can be be "
                                                "combined with --start and --suspend."));
    parser.addOption(stopOption);

    QCommandLineOption waitOption(QStringLiteral("wait"),
                                  QLatin1String("If the package is running, waits the given "
                                                "number of seconds before continuing to the next "
                                                "task. Passing 0 causes the runner to wait "
                                                "indefinitely."),
                                  QStringLiteral("seconds"));
    parser.addOption(waitOption);

    QCommandLineOption installOption(QStringLiteral("install"),
                                     QStringLiteral("(Re)installs the package."));
    parser.addOption(installOption);

    QCommandLineOption removeOption(QStringLiteral("remove"),
                                    QStringLiteral("Uninstalls the package."));
    parser.addOption(removeOption);

    QCommandLineOption deviceOption(QStringLiteral("device"),
                                    QLatin1String("Specifies the device to target as a device name "
                                                  " or index. Use --list-devices to find available "
                                                  "devices. The default device is the first device "
                                                  "found for the active run profile."),
                                    QStringLiteral("name|index"));
    parser.addOption(deviceOption);

    QCommandLineOption profileOption(QStringLiteral("profile"),
                                     QStringLiteral("Force a particular run profile."),
                                     QStringLiteral("name"));
    parser.addOption(profileOption);

    QCommandLineOption listDevicesOption(QStringLiteral("list-devices"),
                                         QLatin1String("List the available devices "
                                                       "(for use with --device)."));
    parser.addOption(listDevicesOption);

    QCommandLineOption verbosityOption(QStringLiteral("verbose"),
                                       QLatin1String("The verbosity level of the message output "
                                                     "(0 - silent, 1 - info, 2 - debug). Defaults to 1."),
                                       QStringLiteral("level"), QStringLiteral("1"));
    parser.addOption(verbosityOption);

    QCommandLineOption ignoreErrorsOption(QStringLiteral("ignore-errors"),
                                          QStringLiteral("Always exit with code 0, regardless of the error state."));
    parser.addOption(ignoreErrorsOption);

    parser.addHelpOption();
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    QStringList arguments = QCoreApplication::arguments();
    parser.parse(arguments);

    QStringList filterRules = QStringList() // Default logging rules
            << QStringLiteral("qt.winrtrunner.warning=true")
            << QStringLiteral("qt.winrtrunner.critical=true")
            << QStringLiteral("qt.winrtrunner.app=true");
    if (parser.isSet(verbosityOption)) {
        bool ok;
        uint verbosity = parser.value(verbosityOption).toUInt(&ok);
        if (!ok || verbosity > 2) {
            qCCritical(lcWinRtRunner) << "Incorrect value specified for verbosity.";
            parser.showHelp(1);
        }
        switch (verbosity) {
        case 2: // Enable debug print
            filterRules.append(QStringLiteral("qt.winrtrunner.debug=true"));
            break;
        case 1: // Remove warnings
            filterRules.removeFirst();
            // fall through
        case 0: // Silent
            filterRules.removeFirst();
            // fall through
        default: // Impossible
            break;
        }
    }
    QLoggingCategory::setFilterRules(filterRules.join(QLatin1Char('\n')));

    if (parser.isSet(listDevicesOption)) {
        std::wcout << "Available devices:\n";
        const QMap<QString, QStringList> deviceNames = Runner::deviceNames();
        foreach (const QString &profile, deviceNames.keys()) {
            std::wcout << reinterpret_cast<const wchar_t *>(profile.utf16()) << ":\n";
            int index = 0;
            foreach (const QString &device, deviceNames.value(profile)) {
                std::wcout << "  " << index++ << ' '
                           << reinterpret_cast<const wchar_t *>(device.utf16()) << '\n';
            }
        }
        std::wcout << std::endl;
        return 0;
    }

    // Process front-end args
    if (parser.positionalArguments().count() < 1)
        parser.showHelp(parser.isSet(QStringLiteral("help")) ? 0 : 1);
    const QString app = parser.positionalArguments().first();
    const int appArgsPos = arguments.indexOf(app) + 1;
    const QStringList mainArgs = arguments.mid(0, appArgsPos);
    QStringList appArgs = arguments.mid(appArgsPos);
    parser.process(mainArgs);

    // Exit codes:
    // 1 - Bad arguments
    // 2 - Bad package or no backend available
    // 3 - Installation failed
    // 4 - Removal failed
    // 5 - Start failed
    // 6 - Suspend failed
    // 7 - Stop failed
    // 8 - Test setup failed
    // 9 - Test results retrieval failed
    // 10 - Enabling debugging failed
    // In "test" mode, the exit code of the app is returned

    bool ignoreErrors = parser.isSet(ignoreErrorsOption);
    bool testEnabled = parser.isSet(testOption);
    bool startEnabled = testEnabled || parser.isSet(startOption) || parser.isSet(debugOption);
    bool suspendEnabled = parser.isSet(suspendOption);
    bool waitEnabled = testEnabled || parser.isSet(waitOption);
    bool stopEnabled = !testEnabled && parser.isSet(stopOption); // test and stop are mutually exclusive
    bool installEnabled = testEnabled || startEnabled || parser.isSet(installOption);
    bool removeBeforeInstall = testEnabled || parser.isSet(installOption);
    bool removeEnabled = testEnabled || parser.isSet(removeOption);
    // Default to test mode if no conflicting arguments were passed
    if (!testEnabled && !installEnabled && !startEnabled && !stopEnabled && !suspendEnabled && !removeEnabled)
        testEnabled = installEnabled = removeBeforeInstall = startEnabled = waitEnabled = stopEnabled = removeEnabled = true;

    int waitTime = parser.value(waitOption).toInt();
    if (!waitTime && testEnabled)
        waitTime = 300; // The maximum wait period for test cases is 300 seconds (5 minutes)

    // Set up runner
    Runner runner(app, appArgs, parser.value(profileOption), parser.value(deviceOption));
    if (!runner.isValid())
        return ignoreErrors ? 0 : 2;

    if (testEnabled && !runner.setupTest()) {
        qCDebug(lcWinRtRunner) << "Test setup failed, exiting with code 8.";
        return ignoreErrors ? 0 : 8;
    }

    if (installEnabled && !runner.install(removeBeforeInstall)) {
        qCDebug(lcWinRtRunner) << "Installation failed, exiting with code 3.";
        return ignoreErrors ? 0 : 3;
    }

    if (parser.isSet(debugOption)) {
        const QString &debuggerExecutable = parser.value(debugOption);
        const QString &debuggerArguments = parser.value(debuggerArgumentsOption);
        qCDebug(lcWinRtRunner) << "Debugger:         " << debuggerExecutable;
        qCDebug(lcWinRtRunner) << "Debugger Options: " << debuggerArguments;
        if (debuggerExecutable.isEmpty()
                || !runner.enableDebugging(debuggerExecutable, debuggerArguments)) {
            qCDebug(lcWinRtRunner) << "Failed to enable debugging, exiting with code 10.";
            return ignoreErrors ? 0 : 10;
        }
    }

    bool startFailed = startEnabled && !runner.start();

    if (parser.isSet(debugOption) && !runner.disableDebugging())
        qCDebug(lcWinRtRunner) << "Failed to disable debugging";

    if (startFailed) {
        qCDebug(lcWinRtRunner) << "Start failed, exiting with code 5.";
        return ignoreErrors ? 0 : 5;
    }

    qint64 pid = runner.pid();
    if (pid != -1)
        qCWarning(lcWinRtRunner) << "App started with process ID" << pid;

    if (waitEnabled)
        runner.wait(waitTime);

    if (suspendEnabled && !runner.suspend()) {
        qCDebug(lcWinRtRunner) << "Suspend failed, exiting with code 6.";
        return ignoreErrors ? 0 : 6;
    }

    if (stopEnabled && !runner.stop()) {
        qCDebug(lcWinRtRunner) << "Stop failed, exiting with code 7.";
        return ignoreErrors ? 0 : 7;
    }

    if (testEnabled && !runner.collectTest()) {
        qCDebug(lcWinRtRunner) << "Collect test failed, exiting with code 9.";
        return ignoreErrors ? 0 : 9;
    }

    if (removeEnabled && !runner.remove()) {
        qCDebug(lcWinRtRunner) << "Remove failed, exiting with code 4.";
        return ignoreErrors ? 0 : 4;
    }

    if (stopEnabled) {
        int exitCode = runner.exitCode();
        if (exitCode == -1)
            return 0; // Exit code unknown; not necessarily an error
        qCWarning(lcWinRtRunner) <<  "App exited with code" << exitCode;
        return ignoreErrors ? 0 : exitCode;
    }

    return 0;
}
