/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Linguist of the Qt Toolkit.
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

#include "translator.h"

#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <QtCore/QTextCodec>
#include <QtCore/QTextStream>

#include <QtCore/QXmlStreamReader>

#include <algorithm>

#define STRINGIFY_INTERNAL(x) #x
#define STRINGIFY(x) STRINGIFY_INTERNAL(x)
#define STRING(s) static QString str##s(QLatin1String(STRINGIFY(s)))

QT_BEGIN_NAMESPACE

QDebug &operator<<(QDebug &d, const QXmlStreamAttribute &attr)
{
    return d << "[" << attr.name().toString() << "," << attr.value().toString() << "]";
}


class TSReader : public QXmlStreamReader
{
public:
    TSReader(QIODevice &dev, ConversionData &cd)
      : QXmlStreamReader(&dev), m_cd(cd)
    {}

    // the "real thing"
    bool read(Translator &translator);

private:
    bool elementStarts(const QString &str) const
    {
        return isStartElement() && name() == str;
    }

    bool isWhiteSpace() const
    {
        return isCharacters() && text().toString().trimmed().isEmpty();
    }

    // needed to expand <byte ... />
    QString readContents();
    // needed to join <lengthvariant>s
    QString readTransContents();

    void handleError();

    ConversionData &m_cd;
};

void TSReader::handleError()
{
    if (isComment())
        return;
    if (hasError() && error() == CustomError) // raised by readContents
        return;

    const QString loc = QString::fromLatin1("at %3:%1:%2")
        .arg(lineNumber()).arg(columnNumber()).arg(m_cd.m_sourceFileName);

    switch (tokenType()) {
    case NoToken: // Cannot happen
    default: // likewise
    case Invalid:
        raiseError(QString::fromLatin1("Parse error %1: %2").arg(loc, errorString()));
        break;
    case StartElement:
        raiseError(QString::fromLatin1("Unexpected tag <%1> %2").arg(name().toString(), loc));
        break;
    case Characters:
        {
            QString tok = text().toString();
            if (tok.length() > 30)
                tok = tok.left(30) + QLatin1String("[...]");
            raiseError(QString::fromLatin1("Unexpected characters '%1' %2").arg(tok, loc));
        }
        break;
    case EntityReference:
        raiseError(QString::fromLatin1("Unexpected entity '&%1;' %2").arg(name().toString(), loc));
        break;
    case ProcessingInstruction:
        raiseError(QString::fromLatin1("Unexpected processing instruction %1").arg(loc));
        break;
    }
}

static QString byteValue(QString value)
{
    int base = 10;
    if (value.startsWith(QLatin1String("x"))) {
        base = 16;
        value.remove(0, 1);
    }
    int n = value.toUInt(0, base);
    return (n != 0) ? QString(QChar(n)) : QString();
}

QString TSReader::readContents()
{
    STRING(byte);
    STRING(value);

    QString result;
    while (!atEnd()) {
        readNext();
        if (isEndElement()) {
            break;
        } else if (isCharacters()) {
            result += text();
        } else if (elementStarts(strbyte)) {
            // <byte value="...">
            result += byteValue(attributes().value(strvalue).toString());
            readNext();
            if (!isEndElement()) {
                handleError();
                break;
            }
        } else {
            handleError();
            break;
        }
    }
    //qDebug() << "TEXT: " << result;
    return result;
}

QString TSReader::readTransContents()
{
    STRING(lengthvariant);
    STRING(variants);
    STRING(yes);

    if (attributes().value(strvariants) == stryes) {
        QString result;
        while (!atEnd()) {
            readNext();
            if (isEndElement()) {
                break;
            } else if (isWhiteSpace()) {
                // ignore these, just whitespace
            } else if (elementStarts(strlengthvariant)) {
                if (!result.isEmpty())
                    result += QChar(Translator::BinaryVariantSeparator);
                result += readContents();
            } else {
                handleError();
                break;
            }
        }
        return result;
    } else {
        return readContents();
    }
}

bool TSReader::read(Translator &translator)
{
    STRING(byte);
    STRING(catalog);
    STRING(comment);
    STRING(context);
    STRING(defaultcodec);
    STRING(dependencies);
    STRING(dependency);
    STRING(extracomment);
    STRING(filename);
    STRING(id);
    STRING(language);
    STRING(line);
    STRING(location);
    STRING(message);
    STRING(name);
    STRING(numerus);
    STRING(numerusform);
    STRING(obsolete);
    STRING(oldcomment);
    STRING(oldsource);
    STRING(source);
    STRING(sourcelanguage);
    STRING(translation);
    STRING(translatorcomment);
    STRING(TS);
    STRING(type);
    STRING(unfinished);
    STRING(userdata);
    STRING(value);
    STRING(vanished);
    //STRING(version);
    STRING(yes);

    static const QString strextrans(QLatin1String("extra-"));
    static const QString strUtf8(QLatin1String("UTF-8"));

    while (!atEnd()) {
        readNext();
        if (isStartDocument()) {
            // <!DOCTYPE TS>
            //qDebug() << attributes();
        } else if (isEndDocument()) {
            // <!DOCTYPE TS>
            //qDebug() << attributes();
        } else if (isDTD()) {
            // <!DOCTYPE TS>
            //qDebug() << tokenString();
        } else if (elementStarts(strTS)) {
            // <TS>
            //qDebug() << "TS " << attributes();
            QHash<QString, int> currentLine;
            QString currentFile;
            bool maybeRelative = false, maybeAbsolute = false;

            QXmlStreamAttributes atts = attributes();
            //QString version = atts.value(strversion).toString();
            translator.setLanguageCode(atts.value(strlanguage).toString());
            translator.setSourceLanguageCode(atts.value(strsourcelanguage).toString());
            while (!atEnd()) {
                readNext();
                if (isEndElement()) {
                    // </TS> found, finish local loop
                    break;
                } else if (isWhiteSpace()) {
                    // ignore these, just whitespace
                } else if (elementStarts(strdefaultcodec)) {
                    // <defaultcodec>
                    readElementText();
                    m_cd.appendError(QString::fromLatin1("Warning: ignoring <defaultcodec> element"));
                    // </defaultcodec>
                } else if (isStartElement()
                        && name().toString().startsWith(strextrans)) {
                    // <extra-...>
                    QString tag = name().toString();
                    translator.setExtra(tag.mid(6), readContents());
                    // </extra-...>
                } else if (elementStarts(strdependencies)) {
                    /*
                     * <dependencies>
                     *   <dependency catalog="qtsystems_no"/>
                     *   <dependency catalog="qtbase_no"/>
                     * </dependencies>
                     **/
                    QStringList dependencies;
                    while (!atEnd()) {
                        readNext();
                        if (isEndElement()) {
                            // </dependencies> found, finish local loop
                            break;
                        } else if (elementStarts(strdependency)) {
                            // <dependency>
                            QXmlStreamAttributes atts = attributes();
                            dependencies.append(atts.value(strcatalog).toString());
                            while (!atEnd()) {
                                readNext();
                                if (isEndElement()) {
                                    // </dependency> found, finish local loop
                                    break;
                                }
                            }
                        }
                    }
                    translator.setDependencies(dependencies);
                } else if (elementStarts(strcontext)) {
                    // <context>
                    QString context;
                    while (!atEnd()) {
                        readNext();
                        if (isEndElement()) {
                            // </context> found, finish local loop
                            break;
                        } else if (isWhiteSpace()) {
                            // ignore these, just whitespace
                        } else if (elementStarts(strname)) {
                            // <name>
                            context = readElementText();
                            // </name>
                        } else if (elementStarts(strmessage)) {
                            // <message>
                            TranslatorMessage::References refs;
                            QString currentMsgFile = currentFile;

                            TranslatorMessage msg;
                            msg.setId(attributes().value(strid).toString());
                            msg.setContext(context);
                            msg.setType(TranslatorMessage::Finished);
                            msg.setPlural(attributes().value(strnumerus) == stryes);
                            while (!atEnd()) {
                                readNext();
                                if (isEndElement()) {
                                    // </message> found, finish local loop
                                    msg.setReferences(refs);
                                    translator.append(msg);
                                    break;
                                } else if (isWhiteSpace()) {
                                    // ignore these, just whitespace
                                } else if (elementStarts(strsource)) {
                                    // <source>...</source>
                                    msg.setSourceText(readContents());
                                } else if (elementStarts(stroldsource)) {
                                    // <oldsource>...</oldsource>
                                    msg.setOldSourceText(readContents());
                                } else if (elementStarts(stroldcomment)) {
                                    // <oldcomment>...</oldcomment>
                                    msg.setOldComment(readContents());
                                } else if (elementStarts(strextracomment)) {
                                    // <extracomment>...</extracomment>
                                    msg.setExtraComment(readContents());
                                } else if (elementStarts(strtranslatorcomment)) {
                                    // <translatorcomment>...</translatorcomment>
                                    msg.setTranslatorComment(readContents());
                                } else if (elementStarts(strlocation)) {
                                    // <location/>
                                    maybeAbsolute = true;
                                    QXmlStreamAttributes atts = attributes();
                                    QString fileName = atts.value(strfilename).toString();
                                    if (fileName.isEmpty()) {
                                        fileName = currentMsgFile;
                                        maybeRelative = true;
                                    } else {
                                        if (refs.isEmpty())
                                            currentFile = fileName;
                                        currentMsgFile = fileName;
                                    }
                                    const QString lin = atts.value(strline).toString();
                                    if (lin.isEmpty()) {
                                        refs.append(TranslatorMessage::Reference(fileName, -1));
                                    } else {
                                        bool bOK;
                                        int lineNo = lin.toInt(&bOK);
                                        if (bOK) {
                                            if (lin.startsWith(QLatin1Char('+')) || lin.startsWith(QLatin1Char('-'))) {
                                                lineNo = (currentLine[fileName] += lineNo);
                                                maybeRelative = true;
                                            }
                                            refs.append(TranslatorMessage::Reference(fileName, lineNo));
                                        }
                                    }
                                    readContents();
                                } else if (elementStarts(strcomment)) {
                                    // <comment>...</comment>
                                    msg.setComment(readContents());
                                } else if (elementStarts(struserdata)) {
                                    // <userdata>...</userdata>
                                    msg.setUserData(readContents());
                                } else if (elementStarts(strtranslation)) {
                                    // <translation>
                                    QXmlStreamAttributes atts = attributes();
                                    QStringRef type = atts.value(strtype);
                                    if (type == strunfinished)
                                        msg.setType(TranslatorMessage::Unfinished);
                                    else if (type == strvanished)
                                        msg.setType(TranslatorMessage::Vanished);
                                    else if (type == strobsolete)
                                        msg.setType(TranslatorMessage::Obsolete);
                                    if (msg.isPlural()) {
                                        QStringList translations;
                                        while (!atEnd()) {
                                            readNext();
                                            if (isEndElement()) {
                                                break;
                                            } else if (isWhiteSpace()) {
                                                // ignore these, just whitespace
                                            } else if (elementStarts(strnumerusform)) {
                                                translations.append(readTransContents());
                                            } else {
                                                handleError();
                                                break;
                                            }
                                        }
                                        msg.setTranslations(translations);
                                    } else {
                                        msg.setTranslation(readTransContents());
                                    }
                                    // </translation>
                                } else if (isStartElement()
                                        && name().toString().startsWith(strextrans)) {
                                    // <extra-...>
                                    QString tag = name().toString();
                                    msg.setExtra(tag.mid(6), readContents());
                                    // </extra-...>
                                } else {
                                    handleError();
                                }
                            }
                            // </message>
                        } else {
                            handleError();
                        }
                    }
                    // </context>
                } else {
                    handleError();
                }
                translator.setLocationsType(maybeRelative ? Translator::RelativeLocations :
                                            maybeAbsolute ? Translator::AbsoluteLocations :
                                                            Translator::NoLocations);
            } // </TS>
        } else {
            handleError();
        }
    }
    if (hasError()) {
        m_cd.appendError(errorString());
        return false;
    }
    return true;
}

static QString numericEntity(int ch)
{
    return QString(ch <= 0x20 ? QLatin1String("<byte value=\"x%1\"/>")
        : QLatin1String("&#x%1;")) .arg(ch, 0, 16);
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
                result += numericEntity(c);
            else // this also covers surrogates
                result += QChar(c);
        }
    }
    return result;
}

static void writeExtras(QTextStream &t, const char *indent,
                        const TranslatorMessage::ExtraData &extras, QRegExp drops)
{
    QStringList outs;
    for (Translator::ExtraData::ConstIterator it = extras.begin(); it != extras.end(); ++it) {
        if (!drops.exactMatch(it.key())) {
            outs << (QStringLiteral("<extra-") + it.key() + QLatin1Char('>')
                     + protect(it.value())
                     + QStringLiteral("</extra-") + it.key() + QLatin1Char('>'));
        }
    }
    outs.sort();
    foreach (const QString &out, outs)
        t << indent << out << endl;
}

static void writeVariants(QTextStream &t, const char *indent, const QString &input)
{
    int offset;
    if ((offset = input.indexOf(QChar(Translator::BinaryVariantSeparator))) >= 0) {
        t << " variants=\"yes\">";
        int start = 0;
        forever {
            t << "\n    " << indent << "<lengthvariant>"
              << protect(input.mid(start, offset - start))
              << "</lengthvariant>";
            if (offset == input.length())
                break;
            start = offset + 1;
            offset = input.indexOf(QChar(Translator::BinaryVariantSeparator), start);
            if (offset < 0)
                offset = input.length();
        }
        t << "\n" << indent;
    } else {
        t << ">" << protect(input);
    }
}

bool saveTS(const Translator &translator, QIODevice &dev, ConversionData &cd)
{
    bool result = true;
    QTextStream t(&dev);
    t.setCodec(QTextCodec::codecForName("UTF-8"));
    //qDebug() << translator.codecName();

    // The xml prolog allows processors to easily detect the correct encoding
    t << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<!DOCTYPE TS>\n";

    t << "<TS version=\"2.1\"";

    QString languageCode = translator.languageCode();
    if (!languageCode.isEmpty() && languageCode != QLatin1String("C"))
        t << " language=\"" << languageCode << "\"";
    languageCode = translator.sourceLanguageCode();
    if (!languageCode.isEmpty() && languageCode != QLatin1String("C"))
        t << " sourcelanguage=\"" << languageCode << "\"";
    t << ">\n";

    QStringList deps = translator.dependencies();
    if (!deps.isEmpty()) {
        t << "<dependencies>\n";
        foreach (const QString &dep, deps)
            t << "<dependency catalog=\"" << dep << "\"/>\n";
        t << "</dependencies>\n";
    }

    QRegExp drops(cd.dropTags().join(QLatin1String("|")));

    writeExtras(t, "    ", translator.extras(), drops);

    QHash<QString, QList<TranslatorMessage> > messageOrder;
    QList<QString> contextOrder;
    foreach (const TranslatorMessage &msg, translator.messages()) {
        // no need for such noise
        if ((msg.type() == TranslatorMessage::Obsolete || msg.type() == TranslatorMessage::Vanished)
            && msg.translation().isEmpty()) {
            continue;
        }

        QList<TranslatorMessage> &context = messageOrder[msg.context()];
        if (context.isEmpty())
            contextOrder.append(msg.context());
        context.append(msg);
    }
    if (cd.sortContexts())
        std::sort(contextOrder.begin(), contextOrder.end());

    QHash<QString, int> currentLine;
    QString currentFile;
    foreach (const QString &context, contextOrder) {
        t << "<context>\n"
             "    <name>"
          << protect(context)
          << "</name>\n";
        foreach (const TranslatorMessage &msg, messageOrder[context]) {
            //msg.dump();

                t << "    <message";
                if (!msg.id().isEmpty())
                    t << " id=\"" << msg.id() << "\"";
                if (msg.isPlural())
                    t << " numerus=\"yes\"";
                t << ">\n";
                if (translator.locationsType() != Translator::NoLocations) {
                    QString cfile = currentFile;
                    bool first = true;
                    foreach (const TranslatorMessage::Reference &ref, msg.allReferences()) {
                        QString fn = cd.m_targetDir.relativeFilePath(ref.fileName())
                                    .replace(QLatin1Char('\\'),QLatin1Char('/'));
                        int ln = ref.lineNumber();
                        QString ld;
                        if (translator.locationsType() == Translator::RelativeLocations) {
                            if (ln != -1) {
                                int dlt = ln - currentLine[fn];
                                if (dlt >= 0)
                                    ld.append(QLatin1Char('+'));
                                ld.append(QString::number(dlt));
                                currentLine[fn] = ln;
                            }

                            if (fn != cfile) {
                                if (first)
                                    currentFile = fn;
                                cfile = fn;
                            } else {
                                fn.clear();
                            }
                            first = false;
                        } else {
                            if (ln != -1)
                                ld = QString::number(ln);
                        }
                        t << "        <location";
                        if (!fn.isEmpty())
                            t << " filename=\"" << fn << "\"";
                        if (!ld.isEmpty())
                            t << " line=\"" << ld << "\"";
                        t << "/>\n";
                    }
                }

                t << "        <source>"
                  << protect(msg.sourceText())
                  << "</source>\n";

                if (!msg.oldSourceText().isEmpty())
                    t << "        <oldsource>" << protect(msg.oldSourceText()) << "</oldsource>\n";

                if (!msg.comment().isEmpty()) {
                    t << "        <comment>"
                      << protect(msg.comment())
                      << "</comment>\n";
                }

                    if (!msg.oldComment().isEmpty())
                        t << "        <oldcomment>" << protect(msg.oldComment()) << "</oldcomment>\n";

                    if (!msg.extraComment().isEmpty())
                        t << "        <extracomment>" << protect(msg.extraComment())
                          << "</extracomment>\n";

                    if (!msg.translatorComment().isEmpty())
                        t << "        <translatorcomment>" << protect(msg.translatorComment())
                          << "</translatorcomment>\n";

                t << "        <translation";
                if (msg.type() == TranslatorMessage::Unfinished)
                    t << " type=\"unfinished\"";
                else if (msg.type() == TranslatorMessage::Vanished)
                    t << " type=\"vanished\"";
                else if (msg.type() == TranslatorMessage::Obsolete)
                    t << " type=\"obsolete\"";
                if (msg.isPlural()) {
                    t << ">";
                    const QStringList &translns = msg.translations();
                    for (int j = 0; j < translns.count(); ++j) {
                        t << "\n            <numerusform";
                        writeVariants(t, "            ", translns[j]);
                        t << "</numerusform>";
                    }
                    t << "\n        ";
                } else {
                    writeVariants(t, "        ", msg.translation());
                }
                t << "</translation>\n";

                writeExtras(t, "        ", msg.extras(), drops);

                if (!msg.userData().isEmpty())
                    t << "        <userdata>" << msg.userData() << "</userdata>\n";
                t << "    </message>\n";
        }
        t << "</context>\n";
    }

    t << "</TS>\n";
    return result;
}

bool loadTS(Translator &translator, QIODevice &dev, ConversionData &cd)
{
    TSReader reader(dev, cd);
    return reader.read(translator);
}

int initTS()
{
    Translator::FileFormat format;

    format.extension = QLatin1String("ts");
    format.fileType = Translator::FileFormat::TranslationSource;
    format.priority = 0;
    format.untranslatedDescription = QT_TRANSLATE_NOOP("FMT", "Qt translation sources");
    format.loader = &loadTS;
    format.saver = &saveTS;
    Translator::registerFileFormat(format);

    return 1;
}

Q_CONSTRUCTOR_FUNCTION(initTS)

QT_END_NAMESPACE
