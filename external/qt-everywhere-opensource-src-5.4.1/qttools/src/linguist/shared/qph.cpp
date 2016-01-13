/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Linguist of the Qt Toolkit.
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

#include "translator.h"

#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <QtCore/QTextCodec>
#include <QtCore/QTextStream>

#include <QtCore/QXmlStreamReader>

QT_BEGIN_NAMESPACE

class QPHReader : public QXmlStreamReader
{
public:
    QPHReader(QIODevice &dev)
      : QXmlStreamReader(&dev)
    {}

    // the "real thing"
    bool read(Translator &translator);

private:
    bool isWhiteSpace() const
    {
        return isCharacters() && text().toString().trimmed().isEmpty();
    }

    enum DataField { NoField, SourceField, TargetField, DefinitionField };
    DataField m_currentField;
    QString m_currentSource;
    QString m_currentTarget;
    QString m_currentDefinition;
};

bool QPHReader::read(Translator &translator)
{
    m_currentField = NoField;
    QString result;
    while (!atEnd()) {
        readNext();
        if (isStartElement()) {
            if (name() == QLatin1String("source")) {
                m_currentField = SourceField;
            } else if (name() == QLatin1String("target")) {
                m_currentField = TargetField;
            } else if (name() == QLatin1String("definition")) {
                m_currentField = DefinitionField;
            } else {
                m_currentField = NoField;
                if (name() == QLatin1String("QPH")) {
                    QXmlStreamAttributes atts = attributes();
                    translator.setLanguageCode(atts.value(QLatin1String("language")).toString());
                    translator.setSourceLanguageCode(atts.value(QLatin1String("sourcelanguage")).toString());
                }
            }
        } else if (isWhiteSpace()) {
            // ignore these
        } else if (isCharacters()) {
            if (m_currentField == SourceField)
                m_currentSource += text();
            else if (m_currentField == TargetField)
                m_currentTarget += text();
            else if (m_currentField == DefinitionField)
                m_currentDefinition += text();
        } else if (isEndElement() && name() == QLatin1String("phrase")) {
            m_currentTarget.replace(QChar(Translator::TextVariantSeparator),
                                    QChar(Translator::BinaryVariantSeparator));
            TranslatorMessage msg;
            msg.setSourceText(m_currentSource);
            msg.setTranslation(m_currentTarget);
            msg.setComment(m_currentDefinition);
            translator.append(msg);
            m_currentSource.clear();
            m_currentTarget.clear();
            m_currentDefinition.clear();
        }
    }
    return true;
}

static bool loadQPH(Translator &translator, QIODevice &dev, ConversionData &)
{
    translator.setLocationsType(Translator::NoLocations);
    QPHReader reader(dev);
    return reader.read(translator);
}

static QString protect(const QString &str)
{
    QString result;
    result.reserve(str.length() * 12 / 10);
    for (int i = 0; i != str.size(); ++i) {
        uint c = str.at(i).unicode();
        switch (c) {
        case '\"':
            result += QLatin1String("&quot;");
            break;
        case '&':
            result += QLatin1String("&amp;");
            break;
        case '>':
            result += QLatin1String("&gt;");
            break;
        case '<':
            result += QLatin1String("&lt;");
            break;
        case '\'':
            result += QLatin1String("&apos;");
            break;
        default:
            if (c < 0x20 && c != '\r' && c != '\n' && c != '\t')
                result += QString(QLatin1String("&#%1;")).arg(c);
            else // this also covers surrogates
                result += QChar(c);
        }
    }
    return result;
}

static bool saveQPH(const Translator &translator, QIODevice &dev, ConversionData &)
{
    QTextStream t(&dev);
    t.setCodec(QTextCodec::codecForName("UTF-8"));
    t << "<!DOCTYPE QPH>\n<QPH";
    QString languageCode = translator.languageCode();
    if (!languageCode.isEmpty() && languageCode != QLatin1String("C"))
        t << " language=\"" << languageCode << "\"";
    languageCode = translator.sourceLanguageCode();
    if (!languageCode.isEmpty() && languageCode != QLatin1String("C"))
        t << " sourcelanguage=\"" << languageCode << "\"";
    t << ">\n";
    foreach (const TranslatorMessage &msg, translator.messages()) {
        t << "<phrase>\n";
        t << "    <source>" << protect(msg.sourceText()) << "</source>\n";
        QString str = msg.translations().join(QLatin1String("@"));
        str.replace(QChar(Translator::BinaryVariantSeparator),
                    QChar(Translator::TextVariantSeparator));
        t << "    <target>" << protect(str)
            << "</target>\n";
        if (!msg.comment().isEmpty())
            t << "    <definition>" << protect(msg.comment()) << "</definition>\n";
        t << "</phrase>\n";
    }
    t << "</QPH>\n";
    return true;
}

int initQPH()
{
    Translator::FileFormat format;

    format.extension = QLatin1String("qph");
    format.description = FMT::tr("Qt Linguist 'Phrase Book'");
    format.fileType = Translator::FileFormat::TranslationSource;
    format.priority = 0;
    format.loader = &loadQPH;
    format.saver = &saveQPH;
    Translator::registerFileFormat(format);

    return 1;
}

Q_CONSTRUCTOR_FUNCTION(initQPH)

QT_END_NAMESPACE
