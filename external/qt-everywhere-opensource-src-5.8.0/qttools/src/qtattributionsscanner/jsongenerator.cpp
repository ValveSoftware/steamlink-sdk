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

#include "jsongenerator.h"

#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>

#include <iostream>

namespace JsonGenerator {

static QJsonObject generate(Package package)
{
    QJsonObject obj;

    obj.insert(QStringLiteral("Id"), package.id);
    obj.insert(QStringLiteral("Path"), package.path);
    obj.insert(QStringLiteral("Files"), package.files.join(QLatin1Char(' ')));
    obj.insert(QStringLiteral("QDocModule"), package.qdocModule);
    obj.insert(QStringLiteral("Name"), package.name);
    obj.insert(QStringLiteral("QtUsage"), package.qtUsage);
    obj.insert(QStringLiteral("QtParts"), QJsonArray::fromStringList(package.qtParts));

    obj.insert(QStringLiteral("Description"), package.description);
    obj.insert(QStringLiteral("Homepage"), package.homepage);
    obj.insert(QStringLiteral("Version"), package.version);
    obj.insert(QStringLiteral("DownloadLocation"), package.downloadLocation);

    obj.insert(QStringLiteral("License"), package.license);
    obj.insert(QStringLiteral("LicenseId"), package.licenseId);
    obj.insert(QStringLiteral("LicenseFile"), package.licenseFile);

    obj.insert(QStringLiteral("Copyright"), package.copyright);

    return obj;
}

void generate(QTextStream &out, const QVector<Package> &packages, LogLevel logLevel)
{
    if (logLevel == VerboseLog)
        std::cerr << qPrintable(tr("Generating json...\n"));

    QJsonDocument document;
    QJsonArray array;
    foreach (const Package &package, packages)
        array.append(generate(package));
    document.setArray(array);

    out << document.toJson();
}

} // namespace JsonGenerator
