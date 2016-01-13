/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include <QtQuickTest/quicktest.h>
#include <QtTest/QtTest>
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include "../common/common.h"

int main(int argc, char** argv)
{
    if (EnginioTests::TESTAPP_URL.isEmpty()) {
        qFatal("Needed environment variable ENGINIO_API_URL is not set!");
        return EXIT_FAILURE;
    }

    QGuiApplication app(argc, argv);
    const QString appPath = QGuiApplication::applicationDirPath();
    // This allows starting the test without previously defining QML2_IMPORT_PATH.
    QDir qmlImportDir(appPath);
#if defined(Q_OS_WIN)
    qmlImportDir.cd("../../../../qml");
#else
    qmlImportDir.cd("../../../qml");
#endif
    QByteArray canonicalImportPath = qmlImportDir.canonicalPath().toUtf8();
    qputenv("QML2_IMPORT_PATH", canonicalImportPath);

    qDebug("QML2_IMPORT_PATH=%s",canonicalImportPath.data());

    QString qmlFilePath(QUICK_TEST_SOURCE_DIR);
    QFile qmltestConfig(qmlFilePath + QDir::separator() + "config.js");

    // Since we create and remove the backend each time the QML tests are run
    // we always need to recreate the configuration file with the new backend API keys.
    qmltestConfig.remove();

    if (!qmltestConfig.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qFatal("Could not open configuration file for writing: %s", qmltestConfig.fileName().toLocal8Bit().data());
        return EXIT_FAILURE;
    }

    EnginioTests::EnginioBackendManager backendManager;
    QString backendName = QStringLiteral("EnginioClient_QML") + QString::number(QDateTime::currentMSecsSinceEpoch());

    if (!backendManager.createBackend(backendName))
        return EXIT_FAILURE;

    if (!EnginioTests::prepareTestObjectType(backendName))
        return EXIT_FAILURE;

    QJsonObject apiKeys = backendManager.backendApiKeys(backendName, EnginioTests::TESTAPP_ENV);
    QByteArray backendId = apiKeys["backendId"].toString().toUtf8();

    EnginioTests::prepareTestUsersAndUserGroups(backendId);

    QTextStream out(&qmltestConfig);
    out << "var backendData = {\n" \
        << "    id: \"" << backendId << "\",\n" \
        << "    serviceUrl: \"" << EnginioTests::TESTAPP_URL << "\"\n" \
        << "}\n"
        << "var testSourcePath = \"" QUICK_TEST_SOURCE_DIR "\"\n" \
        << "var testObjectType = \"objects." + EnginioTests::CUSTOM_OBJECT1 + "\"";

    out.flush();
    qmltestConfig.flush();

    int exitStatus = quick_test_main(argc, argv, "qmltests", QUICK_TEST_SOURCE_DIR);

    qmltestConfig.remove();

    backendManager.removeBackend(backendName);

    return exitStatus;
}

