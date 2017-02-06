/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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

#include <QtCore/QFile>

#include "qhcpwriter.h"

QT_BEGIN_NAMESPACE

QhcpWriter::QhcpWriter()
{
    setAutoFormatting(true);
}

void QhcpWriter::setHelpProjectFile(const QString &qhpFile)
{
    m_qhpFile = qhpFile;
}

void QhcpWriter::setProperties(const QMap<QString, QString> props)
{
    m_properties = props;
}

void QhcpWriter::setTitlePath(const QString &path)
{
    m_titlePath = path;
}

bool QhcpWriter::writeFile(const QString &fileName)
{
    QFile out(fileName);
    if (!out.open(QIODevice::WriteOnly))
        return false;

    setDevice(&out);
    writeStartDocument();
    writeStartElement(QLatin1String("QHelpCollectionProject"));
    writeAttribute(QLatin1String("version"), QLatin1String("1.0"));
    writeAssistantSettings();
    writeDocuments();
    writeEndDocument();
    return true;
}

void QhcpWriter::writeAssistantSettings()
{
    if (m_properties.isEmpty())
        return;

    writeStartElement(QLatin1String("assistant"));

    if (m_properties.contains(QLatin1String("title")))
        writeTextElement(QLatin1String("title"), m_properties.value(QLatin1String("title")));
    if (m_properties.contains(QLatin1String("applicationicon")))
        writeTextElement(QLatin1String("applicationIcon"),
            m_properties.value(QLatin1String("applicationicon")));
    if (m_properties.contains(QLatin1String("startpage")))
        writeTextElement(QLatin1String("startPage"), m_titlePath + QLatin1String("/")
            + m_properties.value(QLatin1String("startpage")));
    if (m_properties.contains(QLatin1String("aboutmenutext"))) {
        writeStartElement(QLatin1String("aboutMenuText"));
        writeTextElement(QLatin1String("text"),
            m_properties.value(QLatin1String("aboutmenutext")));
        writeEndElement();
    }
    if (m_properties.contains(QLatin1String("abouturl"))) {
        writeStartElement(QLatin1String("aboutDialog"));
        writeTextElement(QLatin1String("file"), m_properties.value(QLatin1String("abouturl")));
        writeEndElement();
    }
    if (m_properties.contains(QLatin1String("name"))) {
        writeTextElement(QLatin1String("cacheDirectory"),
            QLatin1String(".") + m_properties.value(QLatin1String("name")));
    }

    writeEndElement();
}

void QhcpWriter::writeDocuments()
{
    if (m_qhpFile.isEmpty())
        return;

    QString out = m_qhpFile;
    int i = out.indexOf(QLatin1Char('.'));
    if (i > -1)
        out = out.left(i);
    out.append(QLatin1String(".qch"));

    writeStartElement(QLatin1String("docFiles"));

    writeStartElement(QLatin1String("generate"));
    writeStartElement(QLatin1String("file"));
    writeTextElement(QLatin1String("input"), m_qhpFile);
    writeTextElement(QLatin1String("output"), out);
    writeEndElement();
    writeEndElement();

    writeStartElement(QLatin1String("register"));
    writeTextElement(QLatin1String("file"), out);
    writeEndElement();

    writeEndElement();
}

QT_END_NAMESPACE
