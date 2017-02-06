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

#include "qdocgenerator.h"

#include <QtCore/qdir.h>

#include <iostream>

namespace QDocGenerator {

// See definition of idstring and licenseid in https://spdx.org/spdx-specification-21-web-version
static bool isSpdxLicenseId(const QString &str) {
    if (str.isEmpty())
        return false;
    for (auto iter(str.cbegin()); iter != str.cend(); ++iter) {
        const QChar c = *iter;
        if (!((c >= QLatin1Char('A') && c <= QLatin1Char('Z'))
              || (c >= QLatin1Char('a') && c <= QLatin1Char('z'))
              || (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
              || (c == QLatin1Char('-')) || (c == QLatin1Char('.'))))
            return false;
    }
    return true;
}

static QString languageJoin(const QStringList &list)
{
    QString result;
    for (int i = 0; i < list.size(); ++i) {
        QString delimiter = QStringLiteral(", ");
        if (i == list.size() - 1) // last item
            delimiter.clear();
        else if (list.size() == 2)
            delimiter = QStringLiteral(" and ");
        else if (list.size() > 2 && i == list.size() - 2)
            delimiter = QStringLiteral(", and "); // oxford comma
        result += list[i] + delimiter;
    }

    return result;
}

static void generate(QTextStream &out, const Package &package, const QDir &baseDir,
                     LogLevel logLevel)
{
    out << "/*!\n\n";
    out << "\\contentspage attributions.html\n";
    for (const QString &part: package.qtParts)
        out << "\\ingroup attributions-" << part << "\n";

    if (package.qtParts.contains(QLatin1String("libs"))) {
        // show up in xxx-index.html page of module
        out << "\\ingroup attributions-" << package.qdocModule << "\n";
        // include in '\generatelist annotatedattributions'
        out << "\\page " << package.qdocModule << "-attribution-" << package.id
            << ".html attribution\n";
    } else {
        out << "\\page " << package.qdocModule << "-attribution-" << package.id
            << ".html \n";
    }

    out << "\\target " << package.id << "\n\n";
    out << "\\title " << package.name << "\n";
    out << "\\brief " << package.license << "\n\n";

    if (!package.description.isEmpty())
        out << package.description << "\n\n";

    if (!package.qtUsage.isEmpty())
        out << package.qtUsage << "\n\n";

    QStringList sourcePaths;
    if (package.files.isEmpty()) {
        sourcePaths << baseDir.relativeFilePath(package.path);
    } else {
        const QDir packageDir(package.path);
        for (const QString &filePath: package.files) {
            const QString absolutePath = packageDir.absoluteFilePath(filePath);
            sourcePaths << baseDir.relativeFilePath(absolutePath);
        }
    }

    out << "The sources can be found in " << languageJoin(sourcePaths) << ".\n\n";

    const bool hasPackageVersion = !package.version.isEmpty();
    const bool hasPackageDownloadLocation = !package.downloadLocation.isEmpty();
    if (!package.homepage.isEmpty()) {
        out << "\\l{" << package.homepage << "}{Project Homepage}";
        if (hasPackageVersion)
            out << ", ";
    }
    if (hasPackageVersion) {
        out << "upstream version: ";
        if (hasPackageDownloadLocation)
            out << "\\l{" << package.downloadLocation << "}{";
        out << package.version;
        if (hasPackageDownloadLocation)
            out << "}";
    }

    out << "\n\n";

    if (!package.copyright.isEmpty())
        out << "\n\\badcode\n" << package.copyright << "\n\\endcode\n\n";

    if (isSpdxLicenseId(package.licenseId) && package.licenseId != QLatin1String("NONE"))
        out << "\\l{https://spdx.org/licenses/" << package.licenseId << ".html}"
            << "{" << package.license << "}.\n\n";
    else
        out << package.license << ".\n\n";

    if (!package.licenseFile.isEmpty()) {
        QFile file(package.licenseFile);
        if (!file.open(QIODevice::ReadOnly)) {
            if (logLevel != SilentLog)
                std::cerr << qPrintable(tr("Cannot open file %1.").arg(
                                            QDir::toNativeSeparators(package.licenseFile))) << "\n";
            return;
        }
        out << "\\badcode\n";
        out << QString::fromUtf8(file.readAll()).trimmed();
        out << "\n\\endcode\n";
    }
    out << "*/\n";
}

void generate(QTextStream &out, const QVector<Package> &packages, const QString &baseDirectory,
              LogLevel logLevel)
{
    if (logLevel == VerboseLog)
        std::cerr << qPrintable(tr("Generating qdoc file...")) << std::endl;

    QDir baseDir(baseDirectory);
    for (const Package &package : packages)
        generate(out, package, baseDir, logLevel);
}

} // namespace QDocGenerator
