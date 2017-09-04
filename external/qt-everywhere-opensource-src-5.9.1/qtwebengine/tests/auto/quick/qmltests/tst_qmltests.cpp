/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include <QtCore/QScopedPointer>
#include <QtQuickTest/quicktest.h>
#include <QtWebEngine/QQuickWebEngineProfile>
#include "qt_webengine_quicktest.h"

#if defined(Q_OS_LINUX) && defined(QT_DEBUG)
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#endif

#if defined(Q_OS_LINUX) && defined(QT_DEBUG)
static bool debuggerPresent()
{
    int fd = open("/proc/self/status", O_RDONLY);
    if (fd == -1)
      return false;
    char buffer[2048];
    ssize_t size = read(fd, buffer, sizeof(buffer) - 1);
    if (size == -1) {
      close(fd);
      return false;
    }
    buffer[size] = 0;
    const char tracerPidToken[] = "\nTracerPid:";
    char *tracerPid = strstr(buffer, tracerPidToken);
    if (!tracerPid) {
      close(fd);
      return false;
    }
    tracerPid += sizeof(tracerPidToken);
    long int pid = strtol(tracerPid, &tracerPid, 10);
    close(fd);
    return pid != 0;
}

static void stackTrace()
{
    bool ok = false;
    const int disableStackDump = qEnvironmentVariableIntValue("QTEST_DISABLE_STACK_DUMP", &ok);
    if (ok && disableStackDump == 1)
        return;

    if (debuggerPresent())
        return;

    fprintf(stderr, "\n========= Received signal, dumping stack ==============\n");
    char cmd[512];
    qsnprintf(cmd, 512, "gdb --pid %d 2>/dev/null <<EOF\n"
                        "set prompt\n"
                        "set height 0\n"
                        "thread apply all where full\n"
                        "detach\n"
                        "quit\n"
                        "EOF\n",
                        (int)getpid());

    if (system(cmd) == -1)
        fprintf(stderr, "calling gdb failed\n");
    fprintf(stderr, "========= End of stack trace ==============\n");
}

static void sigSegvHandler(int signum)
{
    stackTrace();
    qFatal("Received signal %d", signum);
}
#endif

int main(int argc, char **argv)
{
#if defined(Q_OS_LINUX) && defined(QT_DEBUG)
    struct sigaction sigAction;

    sigemptyset(&sigAction.sa_mask);
    sigAction.sa_handler = &sigSegvHandler;
    sigAction.sa_flags = 0;

    sigaction(SIGSEGV, &sigAction, 0);
#endif

    // Inject the mock ui delegates module
    qputenv("QML2_IMPORT_PATH", QByteArray(TESTS_SOURCE_DIR "qmltests/mock-delegates"));
    QScopedPointer<Application> app;

    // Force to use English language for testing due to error message checks
    QLocale::setDefault(QLocale("en"));

    if (!QCoreApplication::instance())
        app.reset(new Application(argc, argv));
    QtWebEngine::initialize();
    QQuickWebEngineProfile::defaultProfile()->setOffTheRecord(true);

    QTEST_ADD_GPU_BLACKLIST_SUPPORT_DEFS
    QTEST_SET_MAIN_SOURCE_PATH

    int i = quick_test_main(argc, argv, "qmltests", QUICK_TEST_SOURCE_DIR);
    return i;
}
