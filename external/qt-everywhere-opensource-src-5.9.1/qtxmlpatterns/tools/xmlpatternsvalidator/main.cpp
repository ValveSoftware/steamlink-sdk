/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
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

#include "main.h"

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtXmlPatterns/QXmlSchema>
#include <QtXmlPatterns/QXmlSchemaValidator>

QT_USE_NAMESPACE

int main(int argc, char **argv)
{
    enum ExitCode
    {
        Valid = 0,
        Invalid,
        ParseError
    };

    enum ExecutionMode
    {
        InvalidMode,
        SchemaOnlyMode,
        InstanceOnlyMode,
        SchemaAndInstanceMode
    };

    const QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QLatin1String("xmlpatternsvalidator"));

    QStringList arguments = QCoreApplication::arguments();
    if (arguments.size() != 2 && arguments.size() != 3) {
        qDebug() << QXmlPatternistCLI::tr("usage: xmlpatternsvalidator (<schema url> | <instance url> <schema url> | <instance url>)");
        return ParseError;
    }

    // parse command line arguments
    ExecutionMode mode = InvalidMode;

    QUrl schemaUri;
    QUrl instanceUri;

    {
        QUrl url = QUrl::fromUserInput(arguments[1]);

        if (arguments.size() == 2) {
            // either it is a schema or instance document

            if (arguments[1].toLower().endsWith(QLatin1String(".xsd"))) {
                schemaUri = url;
                mode = SchemaOnlyMode;
            } else {
                // as we could validate all types of xml documents, don't check the extension here
                instanceUri = url;
                mode = InstanceOnlyMode;
            }
        } else if (arguments.size() == 3) {
            instanceUri = url;
            schemaUri = QUrl::fromUserInput(arguments[2]);

            mode = SchemaAndInstanceMode;
        }
    }

    // Validate schema
    QXmlSchema schema;
    if (InstanceOnlyMode != mode) {
        schema.load(schemaUri);
        if (!schema.isValid())
            return Invalid;
    }

    if (SchemaOnlyMode == mode)
        return Valid;

    // Validate instance
    QXmlSchemaValidator validator(schema);
    if (validator.validate(instanceUri))
        return Valid;

    return Invalid;
}
