/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QStack>
#include <QVector>
#include <QPainter>
#include <QTextLayout>
#include <QDebug>
#include <qmath.h>
#include "qquickstyledtext_p.h"
#include <QQmlContext>

/*
    QQuickStyledText supports few tags:

    <b></b> - bold
    <strong></strong> - bold
    <i></i> - italic
    <br> - new line
    <p> - paragraph
    <u> - underlined text
    <font color="color_name" size="1-7"></font>
    <h1> to <h6> - headers
    <a href=""> - anchor
    <ol type="">, <ul type=""> and <li> - ordered and unordered lists
    <pre></pre> - preformated
    <img src=""> - images

    The opening and closing tags must be correctly nested.
*/

QT_BEGIN_NAMESPACE

Q_GUI_EXPORT int qt_defaultDpi();

class QQuickStyledTextPrivate
{
public:
    enum ListType { Ordered, Unordered };
    enum ListFormat { Bullet, Disc, Square, Decimal, LowerAlpha, UpperAlpha, LowerRoman, UpperRoman };

    struct List {
        int level;
        ListType type;
        ListFormat format;
    };

    QQuickStyledTextPrivate(const QString &t, QTextLayout &l,
                                  QList<QQuickStyledTextImgTag*> &imgTags,
                                  const QUrl &baseUrl,
                                  QQmlContext *context,
                                  bool preloadImages,
                                  bool *fontSizeModified)
        : text(t), layout(l), imgTags(&imgTags), baseFont(layout.font()), baseUrl(baseUrl), hasNewLine(true), nbImages(0), updateImagePositions(false)
        , preFormat(false), prependSpace(false), hasSpace(true), preloadImages(preloadImages), fontSizeModified(fontSizeModified), context(context)
    {
    }

    void parse();
    void appendText(const QString &textIn, int start, int length, QString &textOut);
    bool parseTag(const QChar *&ch, const QString &textIn, QString &textOut, QTextCharFormat &format);
    bool parseCloseTag(const QChar *&ch, const QString &textIn, QString &textOut);
    void parseEntity(const QChar *&ch, const QString &textIn, QString &textOut);
    bool parseFontAttributes(const QChar *&ch, const QString &textIn, QTextCharFormat &format);
    bool parseOrderedListAttributes(const QChar *&ch, const QString &textIn);
    bool parseUnorderedListAttributes(const QChar *&ch, const QString &textIn);
    bool parseAnchorAttributes(const QChar *&ch, const QString &textIn, QTextCharFormat &format);
    void parseImageAttributes(const QChar *&ch, const QString &textIn, QString &textOut);
    QPair<QStringRef,QStringRef> parseAttribute(const QChar *&ch, const QString &textIn);
    QStringRef parseValue(const QChar *&ch, const QString &textIn);
    void setFontSize(int size, QTextCharFormat &format);

    inline void skipSpace(const QChar *&ch) {
        while (ch->isSpace() && !ch->isNull())
            ++ch;
    }

    static QString toAlpha(int value, bool upper);
    static QString toRoman(int value, bool upper);

    QString text;
    QTextLayout &layout;
    QList<QQuickStyledTextImgTag*> *imgTags;
    QFont baseFont;
    QStack<List> listStack;
    QUrl baseUrl;
    bool hasNewLine;
    int nbImages;
    bool updateImagePositions;
    bool preFormat;
    bool prependSpace;
    bool hasSpace;
    bool preloadImages;
    bool *fontSizeModified;
    QQmlContext *context;

    static const QChar lessThan;
    static const QChar greaterThan;
    static const QChar equals;
    static const QChar singleQuote;
    static const QChar doubleQuote;
    static const QChar slash;
    static const QChar ampersand;
    static const QChar bullet;
    static const QChar disc;
    static const QChar square;
    static const QChar lineFeed;
    static const QChar space;
    static const int tabsize = 6;
};

const QChar QQuickStyledTextPrivate::lessThan(QLatin1Char('<'));
const QChar QQuickStyledTextPrivate::greaterThan(QLatin1Char('>'));
const QChar QQuickStyledTextPrivate::equals(QLatin1Char('='));
const QChar QQuickStyledTextPrivate::singleQuote(QLatin1Char('\''));
const QChar QQuickStyledTextPrivate::doubleQuote(QLatin1Char('\"'));
const QChar QQuickStyledTextPrivate::slash(QLatin1Char('/'));
const QChar QQuickStyledTextPrivate::ampersand(QLatin1Char('&'));
const QChar QQuickStyledTextPrivate::bullet(0x2022);
const QChar QQuickStyledTextPrivate::disc(0x25e6);
const QChar QQuickStyledTextPrivate::square(0x25a1);
const QChar QQuickStyledTextPrivate::lineFeed(QLatin1Char('\n'));
const QChar QQuickStyledTextPrivate::space(QLatin1Char(' '));

QQuickStyledText::QQuickStyledText(const QString &string, QTextLayout &layout,
                                               QList<QQuickStyledTextImgTag*> &imgTags,
                                               const QUrl &baseUrl,
                                               QQmlContext *context,
                                               bool preloadImages,
                                               bool *fontSizeModified)
    : d(new QQuickStyledTextPrivate(string, layout, imgTags, baseUrl, context, preloadImages, fontSizeModified))
{
}

QQuickStyledText::~QQuickStyledText()
{
    delete d;
}

void QQuickStyledText::parse(const QString &string, QTextLayout &layout,
                                   QList<QQuickStyledTextImgTag*> &imgTags,
                                   const QUrl &baseUrl,
                                   QQmlContext *context,
                                   bool preloadImages,
                                   bool *fontSizeModified)
{
    if (string.isEmpty())
        return;
    QQuickStyledText styledText(string, layout, imgTags, baseUrl, context, preloadImages, fontSizeModified);
    styledText.d->parse();
}

void QQuickStyledTextPrivate::parse()
{
    QVector<QTextLayout::FormatRange> ranges;
    QStack<QTextCharFormat> formatStack;

    QString drawText;
    drawText.reserve(text.count());

    updateImagePositions = !imgTags->isEmpty();

    int textStart = 0;
    int textLength = 0;
    int rangeStart = 0;
    bool formatChanged = false;

    const QChar *ch = text.constData();
    while (!ch->isNull()) {
        if (*ch == lessThan) {
            if (textLength) {
                appendText(text, textStart, textLength, drawText);
            } else if (prependSpace) {
                drawText.append(space);
                prependSpace = false;
                hasSpace = true;
            }

            if (rangeStart != drawText.length() && formatStack.count()) {
                if (formatChanged) {
                    QTextLayout::FormatRange formatRange;
                    formatRange.format = formatStack.top();
                    formatRange.start = rangeStart;
                    formatRange.length = drawText.length() - rangeStart;
                    ranges.append(formatRange);
                    formatChanged = false;
                } else if (ranges.count()) {
                    ranges.last().length += drawText.length() - rangeStart;
                }
            }
            rangeStart = drawText.length();
            ++ch;
            if (*ch == slash) {
                ++ch;
                if (parseCloseTag(ch, text, drawText)) {
                    if (formatStack.count()) {
                        formatChanged = true;
                        formatStack.pop();
                    }
                }
            } else {
                QTextCharFormat format;
                if (formatStack.count())
                    format = formatStack.top();
                if (parseTag(ch, text, drawText, format)) {
                    formatChanged = true;
                    formatStack.push(format);
                }
            }
            textStart = ch - text.constData() + 1;
            textLength = 0;
        } else if (*ch == ampersand) {
            ++ch;
            appendText(text, textStart, textLength, drawText);
            parseEntity(ch, text, drawText);
            textStart = ch - text.constData() + 1;
            textLength = 0;
        } else if (ch->isSpace()) {
            if (textLength)
                appendText(text, textStart, textLength, drawText);
            if (!preFormat) {
                prependSpace = !hasSpace;
                for (const QChar *n = ch + 1; !n->isNull() && n->isSpace(); ++n)
                    ch = n;
                hasNewLine = false;
            } else  if (*ch == lineFeed) {
                drawText.append(QChar(QChar::LineSeparator));
                hasNewLine = true;
            } else {
                drawText.append(QChar(QChar::Nbsp));
                hasNewLine = false;
            }
            textStart = ch - text.constData() + 1;
            textLength = 0;
        } else {
            ++textLength;
        }
        if (!ch->isNull())
            ++ch;
    }
    if (textLength)
        appendText(text, textStart, textLength, drawText);
    if (rangeStart != drawText.length() && formatStack.count()) {
        if (formatChanged) {
            QTextLayout::FormatRange formatRange;
            formatRange.format = formatStack.top();
            formatRange.start = rangeStart;
            formatRange.length = drawText.length() - rangeStart;
            ranges.append(formatRange);
        } else if (ranges.count()) {
            ranges.last().length += drawText.length() - rangeStart;
        }
    }

    layout.setText(drawText);
    layout.setFormats(ranges);
}

void QQuickStyledTextPrivate::appendText(const QString &textIn, int start, int length, QString &textOut)
{
    if (prependSpace)
        textOut.append(space);
    textOut.append(QStringRef(&textIn, start, length));
    prependSpace = false;
    hasSpace = false;
    hasNewLine = false;
}

//
// Calculates and sets the correct font size in points
// depending on the size multiplier and base font.
//
void QQuickStyledTextPrivate::setFontSize(int size, QTextCharFormat &format)
{
    static const qreal scaling[] = { 0.7, 0.8, 1.0, 1.2, 1.5, 2.0, 2.4 };
    if (baseFont.pointSizeF() != -1)
        format.setFontPointSize(baseFont.pointSize() * scaling[size - 1]);
    else
        format.setFontPointSize(baseFont.pixelSize() * qreal(72.) / qreal(qt_defaultDpi()) * scaling[size - 1]);
    *fontSizeModified = true;
}

bool QQuickStyledTextPrivate::parseTag(const QChar *&ch, const QString &textIn, QString &textOut, QTextCharFormat &format)
{
    skipSpace(ch);

    int tagStart = ch - textIn.constData();
    int tagLength = 0;
    while (!ch->isNull()) {
        if (*ch == greaterThan) {
            if (tagLength == 0)
                return false;
            QStringRef tag(&textIn, tagStart, tagLength);
            const QChar char0 = tag.at(0);
            if (char0 == QLatin1Char('b')) {
                if (tagLength == 1) {
                    format.setFontWeight(QFont::Bold);
                    return true;
                } else if (tagLength == 2 && tag.at(1) == QLatin1Char('r')) {
                    textOut.append(QChar(QChar::LineSeparator));
                    hasSpace = true;
                    prependSpace = false;
                    return false;
                }
            } else if (char0 == QLatin1Char('i')) {
                if (tagLength == 1) {
                    format.setFontItalic(true);
                    return true;
                }
            } else if (char0 == QLatin1Char('p')) {
                if (tagLength == 1) {
                    if (!hasNewLine)
                        textOut.append(QChar::LineSeparator);
                    hasSpace = true;
                    prependSpace = false;
                } else if (tag == QLatin1String("pre")) {
                    preFormat = true;
                    if (!hasNewLine)
                        textOut.append(QChar::LineSeparator);
                    format.setFontFamily(QString::fromLatin1("Courier New,courier"));
                    format.setFontFixedPitch(true);
                    return true;
                }
            } else if (char0 == QLatin1Char('u')) {
                if (tagLength == 1) {
                    format.setFontUnderline(true);
                    return true;
                } else if (tag == QLatin1String("ul")) {
                    List listItem;
                    listItem.level = 0;
                    listItem.type = Unordered;
                    listItem.format = Bullet;
                    listStack.push(listItem);
                }
            } else if (char0 == QLatin1Char('h') && tagLength == 2) {
                int level = tag.at(1).digitValue();
                if (level >= 1 && level <= 6) {
                    if (!hasNewLine)
                        textOut.append(QChar::LineSeparator);
                    hasSpace = true;
                    prependSpace = false;
                    setFontSize(7 - level, format);
                    format.setFontWeight(QFont::Bold);
                    return true;
                }
            } else if (tag == QLatin1String("strong")) {
                format.setFontWeight(QFont::Bold);
                return true;
            } else if (tag == QLatin1String("ol")) {
                List listItem;
                listItem.level = 0;
                listItem.type = Ordered;
                listItem.format = Decimal;
                listStack.push(listItem);
            } else if (tag == QLatin1String("li")) {
                if (!hasNewLine)
                    textOut.append(QChar(QChar::LineSeparator));
                if (!listStack.isEmpty()) {
                    int count = ++listStack.top().level;
                    for (int i = 0; i < listStack.size(); ++i)
                        textOut += QString(tabsize, QChar::Nbsp);
                    switch (listStack.top().format) {
                    case Decimal:
                        textOut += QString::number(count) % QLatin1Char('.');
                        break;
                    case LowerAlpha:
                        textOut += toAlpha(count, false) % QLatin1Char('.');
                        break;
                    case UpperAlpha:
                        textOut += toAlpha(count, true) % QLatin1Char('.');
                        break;
                    case LowerRoman:
                        textOut += toRoman(count, false) % QLatin1Char('.');
                        break;
                    case UpperRoman:
                        textOut += toRoman(count, true) % QLatin1Char('.');
                        break;
                    case Bullet:
                        textOut += bullet;
                        break;
                    case Disc:
                        textOut += disc;
                        break;
                    case Square:
                        textOut += square;
                        break;
                    }
                    textOut += QString(2, QChar::Nbsp);
                }
            }
            return false;
        } else if (ch->isSpace()) {
            // may have params.
            QStringRef tag(&textIn, tagStart, tagLength);
            if (tag == QLatin1String("font"))
                return parseFontAttributes(ch, textIn, format);
            if (tag == QLatin1String("ol")) {
                parseOrderedListAttributes(ch, textIn);
                return false; // doesn't modify format
            }
            if (tag == QLatin1String("ul")) {
                parseUnorderedListAttributes(ch, textIn);
                return false; // doesn't modify format
            }
            if (tag == QLatin1String("a")) {
                return parseAnchorAttributes(ch, textIn, format);
            }
            if (tag == QLatin1String("img")) {
                parseImageAttributes(ch, textIn, textOut);
                return false;
            }
            if (*ch == greaterThan || ch->isNull())
                continue;
        } else if (*ch != slash) {
            tagLength++;
        }
        ++ch;
    }
    return false;
}

bool QQuickStyledTextPrivate::parseCloseTag(const QChar *&ch, const QString &textIn, QString &textOut)
{
    skipSpace(ch);

    int tagStart = ch - textIn.constData();
    int tagLength = 0;
    while (!ch->isNull()) {
        if (*ch == greaterThan) {
            if (tagLength == 0)
                return false;
            QStringRef tag(&textIn, tagStart, tagLength);
            const QChar char0 = tag.at(0);
            hasNewLine = false;
            if (char0 == QLatin1Char('b')) {
                if (tagLength == 1)
                    return true;
                else if (tag.at(1) == QLatin1Char('r') && tagLength == 2)
                    return false;
            } else if (char0 == QLatin1Char('i')) {
                if (tagLength == 1)
                    return true;
            } else if (char0 == QLatin1Char('a')) {
                if (tagLength == 1)
                    return true;
            } else if (char0 == QLatin1Char('p')) {
                if (tagLength == 1) {
                    textOut.append(QChar::LineSeparator);
                    hasNewLine = true;
                    hasSpace = true;
                    return false;
                } else if (tag == QLatin1String("pre")) {
                    preFormat = false;
                    if (!hasNewLine)
                        textOut.append(QChar::LineSeparator);
                    hasNewLine = true;
                    hasSpace = true;
                    return true;
                }
            } else if (char0 == QLatin1Char('u')) {
                if (tagLength == 1)
                    return true;
                else if (tag == QLatin1String("ul")) {
                    if (!listStack.isEmpty()) {
                        listStack.pop();
                        if (!listStack.count())
                            textOut.append(QChar::LineSeparator);
                    }
                    return false;
                }
            } else if (char0 == QLatin1Char('h') && tagLength == 2) {
                textOut.append(QChar::LineSeparator);
                hasNewLine = true;
                hasSpace = true;
                return true;
            } else if (tag == QLatin1String("font")) {
                return true;
            } else if (tag == QLatin1String("strong")) {
                return true;
            } else if (tag == QLatin1String("ol")) {
                if (!listStack.isEmpty()) {
                    listStack.pop();
                    if (!listStack.count())
                        textOut.append(QChar::LineSeparator);
                }
                return false;
            } else if (tag == QLatin1String("li")) {
                return false;
            }
            return false;
        } else if (!ch->isSpace()){
            tagLength++;
        }
        ++ch;
    }

    return false;
}

void QQuickStyledTextPrivate::parseEntity(const QChar *&ch, const QString &textIn, QString &textOut)
{
    int entityStart = ch - textIn.constData();
    int entityLength = 0;
    while (!ch->isNull()) {
        if (*ch == QLatin1Char(';')) {
            QStringRef entity(&textIn, entityStart, entityLength);
            if (entity == QLatin1String("gt"))
                textOut += QChar(62);
            else if (entity == QLatin1String("lt"))
                textOut += QChar(60);
            else if (entity == QLatin1String("amp"))
                textOut += QChar(38);
            else if (entity == QLatin1String("quot"))
                textOut += QChar(34);
            else if (entity == QLatin1String("nbsp"))
                textOut += QChar(QChar::Nbsp);
            return;
        } else if (*ch == QLatin1Char(' ')) {
            QStringRef entity(&textIn, entityStart - 1, entityLength + 1);
            textOut += entity + *ch;
            return;
        }
        ++entityLength;
        ++ch;
    }
}

bool QQuickStyledTextPrivate::parseFontAttributes(const QChar *&ch, const QString &textIn, QTextCharFormat &format)
{
    bool valid = false;
    QPair<QStringRef,QStringRef> attr;
    do {
        attr = parseAttribute(ch, textIn);
        if (attr.first == QLatin1String("color")) {
            valid = true;
            format.setForeground(QColor(attr.second.toString()));
        } else if (attr.first == QLatin1String("size")) {
            valid = true;
            int size = attr.second.toString().toInt();
            if (attr.second.at(0) == QLatin1Char('-') || attr.second.at(0) == QLatin1Char('+'))
                size += 3;
            if (size >= 1 && size <= 7)
                setFontSize(size, format);
        }
    } while (!ch->isNull() && !attr.first.isEmpty());

    return valid;
}

bool QQuickStyledTextPrivate::parseOrderedListAttributes(const QChar *&ch, const QString &textIn)
{
    bool valid = false;

    List listItem;
    listItem.level = 0;
    listItem.type = Ordered;
    listItem.format = Decimal;

    QPair<QStringRef,QStringRef> attr;
    do {
        attr = parseAttribute(ch, textIn);
        if (attr.first == QLatin1String("type")) {
            valid = true;
            if (attr.second == QLatin1String("a"))
                listItem.format = LowerAlpha;
            else if (attr.second == QLatin1String("A"))
                listItem.format = UpperAlpha;
            else if (attr.second == QLatin1String("i"))
                listItem.format = LowerRoman;
            else if (attr.second == QLatin1String("I"))
                listItem.format = UpperRoman;
        }
    } while (!ch->isNull() && !attr.first.isEmpty());

    listStack.push(listItem);
    return valid;
}

bool QQuickStyledTextPrivate::parseUnorderedListAttributes(const QChar *&ch, const QString &textIn)
{
    bool valid = false;

    List listItem;
    listItem.level = 0;
    listItem.type = Unordered;
    listItem.format = Bullet;

    QPair<QStringRef,QStringRef> attr;
    do {
        attr = parseAttribute(ch, textIn);
        if (attr.first == QLatin1String("type")) {
            valid = true;
            if (attr.second == QLatin1String("disc"))
                listItem.format = Disc;
            else if (attr.second == QLatin1String("square"))
                listItem.format = Square;
        }
    } while (!ch->isNull() && !attr.first.isEmpty());

    listStack.push(listItem);
    return valid;
}

bool QQuickStyledTextPrivate::parseAnchorAttributes(const QChar *&ch, const QString &textIn, QTextCharFormat &format)
{
    bool valid = false;

    QPair<QStringRef,QStringRef> attr;
    do {
        attr = parseAttribute(ch, textIn);
        if (attr.first == QLatin1String("href")) {
            format.setAnchorHref(attr.second.toString());
            format.setAnchor(true);
            format.setFontUnderline(true);
            valid = true;
        }
    } while (!ch->isNull() && !attr.first.isEmpty());

    return valid;
}

void QQuickStyledTextPrivate::parseImageAttributes(const QChar *&ch, const QString &textIn, QString &textOut)
{
    qreal imgWidth = 0.0;
    QFontMetricsF fm(layout.font());
    const qreal spaceWidth = fm.width(QChar::Nbsp);
    const bool trailingSpace = textOut.endsWith(space);

    if (!updateImagePositions) {
        QQuickStyledTextImgTag *image = new QQuickStyledTextImgTag;
        image->position = textOut.length() + (trailingSpace ? 0 : 1);

        QPair<QStringRef,QStringRef> attr;
        do {
            attr = parseAttribute(ch, textIn);
            if (attr.first == QLatin1String("src")) {
                image->url =  QUrl(attr.second.toString());
            } else if (attr.first == QLatin1String("width")) {
                image->size.setWidth(attr.second.toString().toInt());
            } else if (attr.first == QLatin1String("height")) {
                image->size.setHeight(attr.second.toString().toInt());
            } else if (attr.first == QLatin1String("align")) {
                if (attr.second.toString() == QLatin1String("top")) {
                    image->align = QQuickStyledTextImgTag::Top;
                } else if (attr.second.toString() == QLatin1String("middle")) {
                    image->align = QQuickStyledTextImgTag::Middle;
                }
            }
        } while (!ch->isNull() && !attr.first.isEmpty());

        if (preloadImages && !image->size.isValid()) {
            // if we don't know its size but the image is a local image,
            // we load it in the pixmap cache and save its implicit size
            // to avoid a relayout later on.
            QUrl url = baseUrl.resolved(image->url);
            if (url.isLocalFile()) {
                image->pix = new QQuickPixmap(context->engine(), url, image->size);
                if (image->pix && image->pix->isReady()) {
                    image->size = image->pix->implicitSize();
                } else {
                    delete image->pix;
                    image->pix = 0;
                }
            }
        }

        imgWidth = image->size.width();
        image->offset = -std::fmod(imgWidth, spaceWidth) / 2.0;
        imgTags->append(image);

    } else {
        // if we already have a list of img tags for this text
        // we only want to update the positions of these tags.
        QQuickStyledTextImgTag *image = imgTags->value(nbImages);
        image->position = textOut.length() + (trailingSpace ? 0 : 1);
        imgWidth = image->size.width();
        image->offset = -std::fmod(imgWidth, spaceWidth) / 2.0;
        QPair<QStringRef,QStringRef> attr;
        do {
            attr = parseAttribute(ch, textIn);
        } while (!ch->isNull() && !attr.first.isEmpty());
        nbImages++;
    }

    QString padding(qFloor(imgWidth / spaceWidth), QChar::Nbsp);
    if (!trailingSpace)
        textOut += QLatin1Char(' ');
    textOut += padding;
    textOut += QLatin1Char(' ');
}

QPair<QStringRef,QStringRef> QQuickStyledTextPrivate::parseAttribute(const QChar *&ch, const QString &textIn)
{
    skipSpace(ch);

    int attrStart = ch - textIn.constData();
    int attrLength = 0;
    while (!ch->isNull()) {
        if (*ch == greaterThan) {
            break;
        } else if (*ch == equals) {
            ++ch;
            if (*ch != singleQuote && *ch != doubleQuote) {
                while (*ch != greaterThan && !ch->isNull())
                    ++ch;
                break;
            }
            ++ch;
            if (!attrLength)
                break;
            QStringRef attr(&textIn, attrStart, attrLength);
            QStringRef val = parseValue(ch, textIn);
            if (!val.isEmpty())
                return QPair<QStringRef,QStringRef>(attr,val);
            break;
        } else {
            ++attrLength;
        }
        ++ch;
    }

    return QPair<QStringRef,QStringRef>();
}

QStringRef QQuickStyledTextPrivate::parseValue(const QChar *&ch, const QString &textIn)
{
    int valStart = ch - textIn.constData();
    int valLength = 0;
    while (*ch != singleQuote && *ch != doubleQuote && !ch->isNull()) {
        ++valLength;
        ++ch;
    }
    if (ch->isNull())
        return QStringRef();
    ++ch; // skip quote

    return QStringRef(&textIn, valStart, valLength);
}

QString QQuickStyledTextPrivate::toAlpha(int value, bool upper)
{
    const char baseChar = upper ? 'A' : 'a';

    QString result;
    int c = value;
    while (c > 0) {
        c--;
        result.prepend(QChar(baseChar + (c % 26)));
        c /= 26;
    }
    return result;
}

QString QQuickStyledTextPrivate::toRoman(int value, bool upper)
{
    QString result = QLatin1String("?");
    // works for up to 4999 items
    if (value < 5000) {
        QByteArray romanNumeral;

        static const char romanSymbolsLower[] = "iiivixxxlxcccdcmmmm";
        static const char romanSymbolsUpper[] = "IIIVIXXXLXCCCDCMMMM";
        QByteArray romanSymbols;
        if (!upper)
            romanSymbols = QByteArray::fromRawData(romanSymbolsLower, sizeof(romanSymbolsLower));
        else
            romanSymbols = QByteArray::fromRawData(romanSymbolsUpper, sizeof(romanSymbolsUpper));

        int c[] = { 1, 4, 5, 9, 10, 40, 50, 90, 100, 400, 500, 900, 1000 };
        int n = value;
        for (int i = 12; i >= 0; n %= c[i], i--) {
            int q = n / c[i];
            if (q > 0) {
                int startDigit = i + (i + 3) / 4;
                int numDigits;
                if (i % 4) {
                    if ((i - 2) % 4)
                        numDigits = 2;
                    else
                        numDigits = 1;
                }
                else
                    numDigits = q;
                romanNumeral.append(romanSymbols.mid(startDigit, numDigits));
            }
        }
        result = QString::fromLatin1(romanNumeral);
    }
    return result;
}

QT_END_NAMESPACE
