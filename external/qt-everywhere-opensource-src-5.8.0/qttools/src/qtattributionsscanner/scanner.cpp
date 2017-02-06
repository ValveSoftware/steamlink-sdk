/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#include "scanner.h"
#include "logging.h"

#include <QtCore/qdir.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qvariant.h>
#include <iostream>

namespace Scanner {

static void missingPropertyWarning(const QString &filePath, const QString &property)
{
    std::cerr << qPrintable(tr("File %1: Missing mandatory property '%2'.").arg(
                                QDir::toNativeSeparators(filePath), property)) << std::endl;
}

static Package readPackage(const QJsonObject &object, const QString &filePath, LogLevel logLevel)
{
    Package p;
    const QString directory = QFileInfo(filePath).absolutePath();
    p.path = directory;

    for (auto iter = object.constBegin(); iter != object.constEnd(); ++iter) {
        const QString key = iter.key();

        if (!iter.value().isString() && key != QLatin1String("QtParts")) {
            if (logLevel != SilentLog)
                std::cerr << qPrintable(tr("File %1: Expected JSON string as value of %2.").arg(
                                            QDir::toNativeSeparators(filePath), key)) << std::endl;
            continue;
        }
        const QString value = iter.value().toString();
        if (key == QLatin1String("Name")) {
            p.name = value;
        } else if (key == QLatin1String("Path")) {
            p.path = QDir(directory).absoluteFilePath(value);
        } else if (key == QLatin1String("Files")) {
            p.files = value.split(QRegExp(QStringLiteral("\\s")), QString::SkipEmptyParts);
        } else if (key == QLatin1String("Id")) {
            p.id = value;
        } else if (key == QLatin1String("Homepage")) {
            p.homepage = value;
        } else if (key == QLatin1String("Version")) {
            p.version = value;
        } else if (key == QLatin1String("DownloadLocation")) {
            p.downloadLocation = value;
        } else if (key == QLatin1String("License")) {
            p.license = value;
        } else if (key == QLatin1String("LicenseId")) {
            p.licenseId = value;
        } else if (key == QLatin1String("LicenseFile")) {
            p.licenseFile = QDir(directory).absoluteFilePath(value);
        } else if (key == QLatin1String("Copyright")) {
            p.copyright = value;
        } else if (key == QLatin1String("QDocModule")) {
            p.qdocModule = value;
        } else if (key == QLatin1String("Description")) {
            p.description = value;
        } else if (key == QLatin1String("QtUsage")) {
            p.qtUsage = value;
        } else if (key == QLatin1String("QtParts")) {
            const QVariantList variantList = iter.value().toArray().toVariantList();
            for (const QVariant &v: variantList) {
                if (v.type() != QMetaType::QString && logLevel != SilentLog) {
                    std::cerr << qPrintable(tr("File %1: Expected JSON string in array of %2.").arg(
                                                QDir::toNativeSeparators(filePath), key))
                              << std::endl;
                }
                p.qtParts.append(v.toString());
            }
        } else {
            if (logLevel != SilentLog)
                std::cerr << qPrintable(tr("File %1: Unknown key %2.").arg(
                                            QDir::toNativeSeparators(filePath), key)) << std::endl;
        }
    }

    // Validate
    if (logLevel != SilentLog) {
        if (p.name.isEmpty())
            missingPropertyWarning(filePath, QStringLiteral("Name"));
        if (p.id.isEmpty())
            missingPropertyWarning(filePath, QStringLiteral("Id"));
        if (p.qdocModule.isEmpty())
            missingPropertyWarning(filePath, QStringLiteral("QDocModule"));
        if (p.qtUsage.isEmpty())
            missingPropertyWarning(filePath, QStringLiteral("QtUsage"));
        if (p.license.isEmpty())
            missingPropertyWarning(filePath, QStringLiteral("License"));
        if (p.copyright.isEmpty())
            missingPropertyWarning(filePath, QStringLiteral("Copyright"));
        if (p.qtParts.isEmpty())
            p.qtParts << QStringLiteral("libs");

        foreach (const QString &part, p.qtParts) {
            if (part != QLatin1String("examples")
                    && part != QLatin1String("tests")
                    && part != QLatin1String("tools")
                    && part != QLatin1String("libs")
                    && logLevel != SilentLog) {
                std::cerr << qPrintable(tr("File %1: Property 'QtPart' contains unknown element "
                                           "'%2'. Valid entries are 'examples', 'tests', 'tools' "
                                           "and 'libs'.").arg(
                                            QDir::toNativeSeparators(filePath), part))
                          << std::endl;
            }
        }
    }
    return p;
}

static QVector<Package> readFile(const QString &filePath, LogLevel logLevel)
{
    if (logLevel == VerboseLog) {
        std::cerr << qPrintable(tr("Reading file %1...").arg(
                                    QDir::toNativeSeparators(filePath))) << std::endl;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (logLevel != SilentLog)
            std::cerr << qPrintable(tr("Could not open file %1.").arg(
                                        QDir::toNativeSeparators(file.fileName()))) << std::endl;
        return QVector<Package>();
    }

    QJsonParseError jsonParseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &jsonParseError);
    if (document.isNull()) {
        if (logLevel != SilentLog)
            std::cerr << qPrintable(tr("Could not parse file %1: %2").arg(
                                        QDir::toNativeSeparators(file.fileName()),
                                        jsonParseError.errorString()))
                      << std::endl;
        return QVector<Package>();
    }

    QVector<Package> packages;
    if (document.isObject()) {
        packages << readPackage(document.object(), file.fileName(), logLevel);
    } else if (document.isArray()) {
        QJsonArray array = document.array();
        for (int i = 0, size = array.size(); i < size; ++i) {
            QJsonValue value = array.at(i);
            if (value.isObject()) {
                packages << readPackage(value.toObject(), file.fileName(), logLevel);
            } else {
                if (logLevel != SilentLog)
                    std::cerr << qPrintable(tr("File %1: Expecting JSON object in array.").arg(
                                                QDir::toNativeSeparators(file.fileName())))
                              << std::endl;
            }
        }
    } else {
        if (logLevel != SilentLog)
            std::cerr << qPrintable(tr("File %1: Expecting JSON object in array.").arg(
                                        QDir::toNativeSeparators(file.fileName()))) << std::endl;
    }
    return packages;
}

QVector<Package> scanDirectory(const QString &directory, LogLevel logLevel)
{
    QDir dir(directory);
    QVector<Package> packages;

    dir.setNameFilters(QStringList() << QStringLiteral("qt_attribution.json"));
    dir.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Files);

    foreach (const QFileInfo &info, dir.entryInfoList()) {
        if (info.isDir()) {
            packages += scanDirectory(info.filePath(), logLevel);
        } else {
            packages += readFile(info.filePath(), logLevel);
        }
    }

    return packages;
}

} // namespace Scanner
