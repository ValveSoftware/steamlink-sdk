/****************************************************************************
**
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Volker Krause <volker.krause@kdab.com>
** Copyright (C) 2016 Intel Corporation.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit
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

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QPluginLoader>
#include <QStringList>

#include <iostream>

enum PrintOption {
    PrintIID = 0x01,
    PrintClassName = 0x02,
    PrintQtInfo = 0x04,
    PrintUserData = 0x08
};
Q_DECLARE_FLAGS(PrintOptions, PrintOption)
Q_DECLARE_OPERATORS_FOR_FLAGS(PrintOptions)

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qplugininfo"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QT_VERSION_STR));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Qt5 plugin meta-data dumper"));
    QCommandLineOption jsonFormatOption(QStringList() << "f" << "json-format",
                                        QStringLiteral("Print JSON data as: indented, compact"), QStringLiteral("format"));
    QCommandLineOption fullJsonOption("full-json",
                                      QStringLiteral("Print the plugin metadata in JSON format"));
    QCommandLineOption printOption(QStringList() << "p" << QStringLiteral("print"),
                                   QStringLiteral("Print detail (iid, classname, qtinfo, userdata)"), QStringLiteral("detail"));
    jsonFormatOption.setDefaultValue(QStringLiteral("indented"));
    printOption.setDefaultValues(QStringList() << QStringLiteral("iid") << QStringLiteral("qtinfo") << QStringLiteral("userdata"));

    parser.addOption(fullJsonOption);
    parser.addOption(jsonFormatOption);
    parser.addOption(printOption);
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("plugin"), QStringLiteral("Plug-in of which to read the meta data."), QStringLiteral("<plugin>"));
    parser.process(app);

    if (parser.positionalArguments().isEmpty())
        parser.showHelp(1);

    bool fullJson = parser.isSet(fullJsonOption);
    QJsonDocument::JsonFormat jsonFormat = parser.value(jsonFormatOption) == "indented" ? QJsonDocument::Indented : QJsonDocument::Compact;
    QStringList printOptionList = parser.values(printOption);
    PrintOptions print;
    if (printOptionList.contains("iid"))
        print |= PrintIID;
    if (printOptionList.contains("classname"))
        print |= PrintClassName;
    if (printOptionList.contains("qtinfo"))
        print |= PrintQtInfo;
    if (printOptionList.contains("userdata"))
        print |= PrintUserData;

    int retval = 0;
    foreach (const QString &plugin, parser.positionalArguments()) {
        QByteArray pluginNativeName = QFile::encodeName(QDir::toNativeSeparators(plugin));
        if (!QFile::exists(plugin)) {
            std::cerr << "qtplugininfo: " << pluginNativeName.constData() << ": No such file or directory." << std::endl;
            retval = 1;
            continue;
        }
        if (!QLibrary::isLibrary(plugin)) {
            std::cerr << "qtplugininfo: " << pluginNativeName.constData() << ": Not a plug-in." << std::endl;
            retval = 1;
            continue;
        }

        QPluginLoader loader(plugin);
        QJsonObject metaData = loader.metaData();
        if (metaData.isEmpty()) {
            std::cerr << "qtplugininfo: " << pluginNativeName.constData() << ": No plug-in meta-data found: "
                      << qPrintable(loader.errorString()) << std::endl;
            retval = 1;
            continue;
        }

        QString iid = metaData.value("IID").toString();
        QString className = metaData.value("className").toString();
        QJsonValue debug = metaData.value("debug");
        int version = metaData.value("version").toInt();
        QJsonValue userData = metaData.value("MetaData");

        if ((version >> 16) != (QT_VERSION >> 16)) {
            std::cerr << "qtplugininfo: " << pluginNativeName.constData()
                      << ": Qt version mismatch - got major version " << (version >> 16)
                      << ", expected " << (QT_VERSION >> 16) << std::endl;
            retval = 1;
            continue;
        }
        if (iid.isEmpty() || className.isEmpty() || debug.isNull()) {
            std::cerr << "qtplugininfo: " << pluginNativeName.constData() << ": invalid metadata, missing required fields:";
            if (iid.isEmpty())
                std::cerr << " iid";
            if (className.isEmpty())
                std::cerr << " className";
            if (debug.isNull())
                std::cerr << " debug";
            std::cerr << std::endl;
            retval = 1;
            continue;
        }
        if (!userData.isNull() && !userData.isObject()) {
            std::cerr << "qtplugininfo: " << pluginNativeName.constData() << ": invalid metadata, user data is not a JSON object" << std::endl;
            retval = 1;
            continue;
        }

        if (parser.positionalArguments().size() != 1)
            std::cout << pluginNativeName.constData() << ": ";
        if (fullJson) {
            std::cout << QJsonDocument(metaData).toJson(jsonFormat).constData();
            if (jsonFormat == QJsonDocument::Compact)
                std::cout << std::endl;
        } else {
            if (print & PrintIID)
                std::cout << "IID \"" << qPrintable(iid) << "\" ";
            if (print & PrintClassName)
                std::cout << "class " << qPrintable(className) << ' ';
            if (print & PrintQtInfo)
                std::cout << "Qt " << (version >> 16) << '.' << ((version >> 8) & 0xFF) << '.' << (version & 0xFF)
                          << (debug.toBool() ? " (debug)" : " (release)");
            std::cout << std::endl;
            if (print & PrintUserData && userData.isObject())
                std::cout << "User Data: " << QJsonDocument(userData.toObject()).toJson().constData();
        }
    }

    return retval;
}
