/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquicktext_p.h"
#include "qquicktext_p_p.h"

#include <QtQuick/private/qsgcontext_p.h>
#include <private/qqmlglobal_p.h>
#include <private/qsgadaptationlayer_p.h>
#include "qquicktextnode_p.h"
#include "qquickimage_p_p.h"
#include "qquicktextutil_p.h"

#include <QtQuick/private/qsgtexture_p.h>

#include <QtQml/qqmlinfo.h>
#include <QtGui/qevent.h>
#include <QtGui/qabstracttextdocumentlayout.h>
#include <QtGui/qpainter.h>
#include <QtGui/qtextdocument.h>
#include <QtGui/qtextobject.h>
#include <QtGui/qtextcursor.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qinputmethod.h>

#include <private/qtextengine_p.h>
#include <private/qquickstyledtext_p.h>
#include <QtQuick/private/qquickpixmapcache_p.h>

#include <qmath.h>
#include <limits.h>

QT_BEGIN_NAMESPACE


const QChar QQuickTextPrivate::elideChar = QChar(0x2026);

QQuickTextPrivate::QQuickTextPrivate()
    : elideLayout(0), textLine(0), lineWidth(0)
    , color(0xFF000000), linkColor(0xFF0000FF), styleColor(0xFF000000)
    , lineCount(1), multilengthEos(-1)
    , elideMode(QQuickText::ElideNone), hAlign(QQuickText::AlignLeft), vAlign(QQuickText::AlignTop)
    , format(QQuickText::AutoText), wrapMode(QQuickText::NoWrap)
    , style(QQuickText::Normal)
    , renderType(QQuickText::QtRendering)
    , updateType(UpdatePaintNode)
    , maximumLineCountValid(false), updateOnComponentComplete(true), richText(false)
    , styledText(false), widthExceeded(false), heightExceeded(false), internalWidthUpdate(false)
    , requireImplicitSize(false), implicitWidthValid(false), implicitHeightValid(false)
    , truncated(false), hAlignImplicit(true), rightToLeftText(false)
    , layoutTextElided(false), textHasChanged(true), needToUpdateLayout(false), formatModifiesFontSize(false)
    , polishSize(false)
{
    implicitAntialiasing = true;
}

QQuickTextPrivate::ExtraData::ExtraData()
    : lineHeight(1.0)
    , doc(0)
    , minimumPixelSize(12)
    , minimumPointSize(12)
    , nbActiveDownloads(0)
    , maximumLineCount(INT_MAX)
    , lineHeightMode(QQuickText::ProportionalHeight)
    , fontSizeMode(QQuickText::FixedSize)
{
}

void QQuickTextPrivate::init()
{
    Q_Q(QQuickText);
    q->setAcceptedMouseButtons(Qt::LeftButton);
    q->setFlag(QQuickItem::ItemHasContents);
}

QQuickTextDocumentWithImageResources::QQuickTextDocumentWithImageResources(QQuickItem *parent)
: QTextDocument(parent), outstanding(0)
{
    setUndoRedoEnabled(false);
    documentLayout()->registerHandler(QTextFormat::ImageObject, this);
    connect(this, SIGNAL(baseUrlChanged(QUrl)), this, SLOT(reset()));
}

QQuickTextDocumentWithImageResources::~QQuickTextDocumentWithImageResources()
{
    if (!m_resources.isEmpty())
        qDeleteAll(m_resources);
}

QVariant QQuickTextDocumentWithImageResources::loadResource(int type, const QUrl &name)
{
    QQmlContext *context = qmlContext(parent());

    if (type == QTextDocument::ImageResource) {
        QQuickPixmap *p = loadPixmap(context, name);
        return p->image();
    }

    return QTextDocument::loadResource(type, name);
}

void QQuickTextDocumentWithImageResources::requestFinished()
{
    outstanding--;
    if (outstanding == 0) {
        markContentsDirty(0, characterCount());
        emit imagesLoaded();
    }
}

QSizeF QQuickTextDocumentWithImageResources::intrinsicSize(
        QTextDocument *, int, const QTextFormat &format)
{
    if (format.isImageFormat()) {
        QTextImageFormat imageFormat = format.toImageFormat();

        const bool hasWidth = imageFormat.hasProperty(QTextFormat::ImageWidth);
        const int width = qRound(imageFormat.width());
        const bool hasHeight = imageFormat.hasProperty(QTextFormat::ImageHeight);
        const int height = qRound(imageFormat.height());

        QSizeF size(width, height);
        if (!hasWidth || !hasHeight) {
            QQmlContext *context = qmlContext(parent());
            QUrl url = baseUrl().resolved(QUrl(imageFormat.name()));

            QQuickPixmap *p = loadPixmap(context, url);
            if (!p->isReady()) {
                if (!hasWidth)
                    size.setWidth(16);
                if (!hasHeight)
                    size.setHeight(16);
                return size;
            }
            QSize implicitSize = p->implicitSize();

            if (!hasWidth) {
                if (!hasHeight)
                    size.setWidth(implicitSize.width());
                else
                    size.setWidth(qRound(height * (implicitSize.width() / (qreal) implicitSize.height())));
            }
            if (!hasHeight) {
                if (!hasWidth)
                    size.setHeight(implicitSize.height());
                else
                    size.setHeight(qRound(width * (implicitSize.height() / (qreal) implicitSize.width())));
            }
        }
        return size;
    }
    return QSizeF();
}

void QQuickTextDocumentWithImageResources::drawObject(
        QPainter *, const QRectF &, QTextDocument *, int, const QTextFormat &)
{
}

QImage QQuickTextDocumentWithImageResources::image(const QTextImageFormat &format)
{
    QQmlContext *context = qmlContext(parent());
    QUrl url = baseUrl().resolved(QUrl(format.name()));

    QQuickPixmap *p = loadPixmap(context, url);
    return p->image();
}

void QQuickTextDocumentWithImageResources::reset()
{
    clearResources();
    markContentsDirty(0, characterCount());
}

QQuickPixmap *QQuickTextDocumentWithImageResources::loadPixmap(
        QQmlContext *context, const QUrl &url)
{

    QHash<QUrl, QQuickPixmap *>::Iterator iter = m_resources.find(url);

    if (iter == m_resources.end()) {
        QQuickPixmap *p = new QQuickPixmap(context->engine(), url);
        iter = m_resources.insert(url, p);

        if (p->isLoading()) {
            p->connectFinished(this, SLOT(requestFinished()));
            outstanding++;
        }
    }

    QQuickPixmap *p = *iter;
    if (p->isError()) {
        if (!errors.contains(url)) {
            errors.insert(url);
            qmlInfo(parent()) << p->error();
        }
    }
    return p;
}

void QQuickTextDocumentWithImageResources::clearResources()
{
    foreach (QQuickPixmap *pixmap, m_resources)
        pixmap->clear(this);
    qDeleteAll(m_resources);
    m_resources.clear();
    outstanding = 0;
}

void QQuickTextDocumentWithImageResources::setText(const QString &text)
{
    clearResources();

#ifndef QT_NO_TEXTHTMLPARSER
    setHtml(text);
#else
    setPlainText(text);
#endif
}

QSet<QUrl> QQuickTextDocumentWithImageResources::errors;

QQuickTextPrivate::~QQuickTextPrivate()
{
    delete elideLayout;
    delete textLine; textLine = 0;
    qDeleteAll(imgTags);
    imgTags.clear();
}

qreal QQuickTextPrivate::getImplicitWidth() const
{
    if (!requireImplicitSize) {
        // We don't calculate implicitWidth unless it is required.
        // We need to force a size update now to ensure implicitWidth is calculated
        QQuickTextPrivate *me = const_cast<QQuickTextPrivate*>(this);
        me->requireImplicitSize = true;
        me->updateSize();
    }
    return implicitWidth;
}

qreal QQuickTextPrivate::getImplicitHeight() const
{
    if (!requireImplicitSize) {
        QQuickTextPrivate *me = const_cast<QQuickTextPrivate*>(this);
        me->requireImplicitSize = true;
        me->updateSize();
    }
    return implicitHeight;
}

/*!
    \qmlproperty bool QtQuick::Text::antialiasing

    Used to decide if the Text should use antialiasing or not. Only Text
    with renderType of Text.NativeRendering can disable antialiasing.

    The default is true.
*/

void QQuickText::q_imagesLoaded()
{
    Q_D(QQuickText);
    d->updateLayout();
}

void QQuickTextPrivate::updateLayout()
{
    Q_Q(QQuickText);
    if (!q->isComponentComplete()) {
        updateOnComponentComplete = true;
        return;
    }
    updateOnComponentComplete = false;
    layoutTextElided = false;

    if (!visibleImgTags.isEmpty())
        visibleImgTags.clear();
    needToUpdateLayout = false;

    // Setup instance of QTextLayout for all cases other than richtext
    if (!richText) {
        if (textHasChanged) {
            if (styledText && !text.isEmpty()) {
                layout.setFont(font);
                // needs temporary bool because formatModifiesFontSize is in a bit-field
                bool fontSizeModified = false;
                QQuickStyledText::parse(text, layout, imgTags, q->baseUrl(), qmlContext(q), !maximumLineCountValid, &fontSizeModified);
                formatModifiesFontSize = fontSizeModified;
                multilengthEos = -1;
            } else {
                layout.clearAdditionalFormats();
                if (elideLayout)
                    elideLayout->clearAdditionalFormats();
                QString tmp = text;
                multilengthEos = tmp.indexOf(QLatin1Char('\x9c'));
                if (multilengthEos != -1) {
                    tmp = tmp.mid(0, multilengthEos);
                    tmp.replace(QLatin1Char('\n'), QChar::LineSeparator);
                } else if (tmp.contains(QLatin1Char('\n'))) {
                    // Replace always does a detach.  Checking for the new line character first
                    // means iterating over those items again if found but prevents a realloc
                    // otherwise.
                    tmp.replace(QLatin1Char('\n'), QChar::LineSeparator);
                }
                layout.setText(tmp);
            }
            textHasChanged = false;
        }
    } else {
        ensureDoc();
        QTextBlockFormat::LineHeightTypes type;
        type = lineHeightMode() == QQuickText::FixedHeight ? QTextBlockFormat::FixedHeight : QTextBlockFormat::ProportionalHeight;
        QTextBlockFormat blockFormat;
        blockFormat.setLineHeight((lineHeightMode() == QQuickText::FixedHeight ? lineHeight() : lineHeight() * 100), type);
        for (QTextBlock it = extra->doc->begin(); it != extra->doc->end(); it = it.next()) {
            QTextCursor cursor(it);
            cursor.mergeBlockFormat(blockFormat);
        }
    }

    updateSize();

    if (needToUpdateLayout) {
        needToUpdateLayout = false;
        textHasChanged = true;
        updateLayout();
    }

    q->polish();
}

void QQuickText::imageDownloadFinished()
{
    Q_D(QQuickText);

    (d->extra->nbActiveDownloads)--;

    // when all the remote images have been downloaded,
    // if one of the sizes was not specified at parsing time
    // we use the implicit size from pixmapcache and re-layout.

    if (d->extra.isAllocated() && d->extra->nbActiveDownloads == 0) {
        bool needToUpdateLayout = false;
        foreach (QQuickStyledTextImgTag *img, d->visibleImgTags) {
            if (!img->size.isValid()) {
                img->size = img->pix->implicitSize();
                needToUpdateLayout = true;
            }
        }

        if (needToUpdateLayout) {
            d->textHasChanged = true;
            d->updateLayout();
        } else {
            d->updateType = QQuickTextPrivate::UpdatePaintNode;
            update();
        }
    }
}

void QQuickTextPrivate::updateBaseline(qreal baseline, qreal dy)
{
    Q_Q(QQuickText);

    qreal yoff = 0;

    if (q->heightValid()) {
        if (vAlign == QQuickText::AlignBottom)
            yoff = dy;
        else if (vAlign == QQuickText::AlignVCenter)
            yoff = dy/2;
    }

    q->setBaselineOffset(baseline + yoff);
}

void QQuickTextPrivate::updateSize()
{
    Q_Q(QQuickText);

    if (!q->isComponentComplete()) {
        updateOnComponentComplete = true;
        return;
    }

    if (!requireImplicitSize) {
        emit q->implicitWidthChanged();
        emit q->implicitHeightChanged();
        // if the implicitWidth is used, then updateSize() has already been called (recursively)
        if (requireImplicitSize)
            return;
    }

    if (text.isEmpty() && !isLineLaidOutConnected() && fontSizeMode() == QQuickText::FixedSize) {
        // How much more expensive is it to just do a full layout on an empty string here?
        // There may be subtle differences in the height and baseline calculations between
        // QTextLayout and QFontMetrics and the number of variables that can affect the size
        // and position of a line is increasing.
        QFontMetricsF fm(font);
        qreal fontHeight = qCeil(fm.height());  // QScriptLine and therefore QTextLine rounds up
        if (!richText) {                        // line height, so we will as well.
            fontHeight = lineHeightMode() == QQuickText::FixedHeight
                    ? lineHeight()
                    : fontHeight * lineHeight();
        }
        updateBaseline(fm.ascent(), q->height() - fontHeight);
        q->setImplicitSize(0, fontHeight);
        layedOutTextRect = QRectF(0, 0, 0, fontHeight);
        emit q->contentSizeChanged();
        updateType = UpdatePaintNode;
        q->update();
        return;
    }

    QSizeF size(0, 0);
    QSizeF previousSize = layedOutTextRect.size();

    //setup instance of QTextLayout for all cases other than richtext
    if (!richText) {
        qreal baseline = 0;
        QRectF textRect = setupTextLayout(&baseline);

        if (internalWidthUpdate)    // probably the result of a binding loop, but by letting it
            return;      // get this far we'll get a warning to that effect if it is.

        layedOutTextRect = textRect;
        size = textRect.size();
        updateBaseline(baseline, q->height() - size.height());
    } else {
        widthExceeded = true; // always relayout rich text on width changes..
        heightExceeded = false; // rich text layout isn't affected by height changes.
        ensureDoc();
        extra->doc->setDefaultFont(font);
        QQuickText::HAlignment horizontalAlignment = q->effectiveHAlign();
        if (rightToLeftText) {
            if (horizontalAlignment == QQuickText::AlignLeft)
                horizontalAlignment = QQuickText::AlignRight;
            else if (horizontalAlignment == QQuickText::AlignRight)
                horizontalAlignment = QQuickText::AlignLeft;
        }
        QTextOption option;
        option.setAlignment((Qt::Alignment)int(horizontalAlignment | vAlign));
        option.setWrapMode(QTextOption::WrapMode(wrapMode));
        option.setUseDesignMetrics(renderType != QQuickText::NativeRendering);
        extra->doc->setDefaultTextOption(option);
        qreal naturalWidth = 0;
        if (requireImplicitSize && q->widthValid()) {
            extra->doc->setTextWidth(-1);
            naturalWidth = extra->doc->idealWidth();
            const bool wasInLayout = internalWidthUpdate;
            internalWidthUpdate = true;
            q->setImplicitWidth(naturalWidth);
            internalWidthUpdate = wasInLayout;
        }
        if (internalWidthUpdate)
            return;

        extra->doc->setPageSize(QSizeF());
        if (q->widthValid() && (wrapMode != QQuickText::NoWrap || extra->doc->idealWidth() < q->width()))
            extra->doc->setTextWidth(q->width());
        else
            extra->doc->setTextWidth(extra->doc->idealWidth()); // ### Text does not align if width is not set (QTextDoc bug)

        QSizeF dsize = extra->doc->size();
        layedOutTextRect = QRectF(QPointF(0,0), dsize);
        size = QSizeF(extra->doc->idealWidth(),dsize.height());

        QFontMetricsF fm(font);
        updateBaseline(fm.ascent(), q->height() - size.height());

        //### need to confirm cost of always setting these for richText
        internalWidthUpdate = true;
        qreal iWidth = -1;
        if (!q->widthValid())
            iWidth = size.width();
        if (iWidth > -1)
            q->setImplicitSize(iWidth, size.height());
        internalWidthUpdate = false;

        if (iWidth == -1)
            q->setImplicitHeight(size.height());
    }

    if (layedOutTextRect.size() != previousSize)
        emit q->contentSizeChanged();
    updateType = UpdatePaintNode;
    q->update();
}

QQuickTextLine::QQuickTextLine()
    : QObject(), m_line(0), m_height(0)
{
}

void QQuickTextLine::setLine(QTextLine *line)
{
    m_line = line;
}

void QQuickTextLine::setLineOffset(int offset)
{
    m_lineOffset = offset;
}

int QQuickTextLine::number() const
{
    if (m_line)
        return m_line->lineNumber() + m_lineOffset;
    return 0;
}

qreal QQuickTextLine::width() const
{
    if (m_line)
        return m_line->width();
    return 0;
}

void QQuickTextLine::setWidth(qreal width)
{
    if (m_line)
        m_line->setLineWidth(width);
}

qreal QQuickTextLine::height() const
{
    if (m_height)
        return m_height;
    if (m_line)
        return m_line->height();
    return 0;
}

void QQuickTextLine::setHeight(qreal height)
{
    if (m_line)
        m_line->setPosition(QPointF(m_line->x(), m_line->y() - m_line->height() + height));
    m_height = height;
}

qreal QQuickTextLine::x() const
{
    if (m_line)
        return m_line->x();
    return 0;
}

void QQuickTextLine::setX(qreal x)
{
    if (m_line)
        m_line->setPosition(QPointF(x, m_line->y()));
}

qreal QQuickTextLine::y() const
{
    if (m_line)
        return m_line->y();
    return 0;
}

void QQuickTextLine::setY(qreal y)
{
    if (m_line)
        m_line->setPosition(QPointF(m_line->x(), y));
}

bool QQuickTextPrivate::isLineLaidOutConnected()
{
    Q_Q(QQuickText);
    IS_SIGNAL_CONNECTED(q, QQuickText, lineLaidOut, (QQuickTextLine *));
}

void QQuickTextPrivate::setupCustomLineGeometry(QTextLine &line, qreal &height, int lineOffset)
{
    Q_Q(QQuickText);

    if (!textLine)
        textLine = new QQuickTextLine;
    textLine->setLine(&line);
    textLine->setY(height);
    textLine->setHeight(0);
    textLine->setLineOffset(lineOffset);

    // use the text item's width by default if it has one and wrap is on or text must be aligned
    if (q->widthValid() && (q->wrapMode() != QQuickText::NoWrap ||
                            q->effectiveHAlign() != QQuickText::AlignLeft))
        textLine->setWidth(q->width());
    else
        textLine->setWidth(INT_MAX);
    if (lineHeight() != 1.0)
        textLine->setHeight((lineHeightMode() == QQuickText::FixedHeight) ? lineHeight() : line.height() * lineHeight());

    emit q->lineLaidOut(textLine);

    height += textLine->height();
}

void QQuickTextPrivate::elideFormats(
        const int start, const int length, int offset, QList<QTextLayout::FormatRange> *elidedFormats)
{
    const int end = start + length;
    QList<QTextLayout::FormatRange> formats = layout.additionalFormats();
    for (int i = 0; i < formats.count(); ++i) {
        QTextLayout::FormatRange format = formats.at(i);
        const int formatLength = qMin(format.start + format.length, end) - qMax(format.start, start);
        if (formatLength > 0) {
            format.start = qMax(offset, format.start - start + offset);
            format.length = formatLength;
            elidedFormats->append(format);
        }
    }
}

QString QQuickTextPrivate::elidedText(qreal lineWidth, const QTextLine &line, QTextLine *nextLine) const
{
    if (nextLine) {
        return layout.engine()->elidedText(
                Qt::TextElideMode(elideMode),
                QFixed::fromReal(lineWidth),
                0,
                line.textStart(),
                line.textLength() + nextLine->textLength());
    } else {
        QString elideText = layout.text().mid(line.textStart(), line.textLength());
        if (!styledText) {
            // QFontMetrics won't help eliding styled text.
            elideText[elideText.length() - 1] = elideChar;
            // Appending the elide character may push the line over the maximum width
            // in which case the elided text will need to be elided.
            QFontMetricsF metrics(layout.font());
            if (metrics.width(elideChar) + line.naturalTextWidth() >= lineWidth)
                elideText = metrics.elidedText(elideText, Qt::TextElideMode(elideMode), lineWidth);
        }
        return elideText;
    }
}

/*!
    Lays out the QQuickTextPrivate::layout QTextLayout in the constraints of the QQuickText.

    Returns the size of the final text.  This can be used to position the text vertically (the text is
    already absolutely positioned horizontally).
*/

QRectF QQuickTextPrivate::setupTextLayout(qreal *const baseline)
{
    Q_Q(QQuickText);

    bool singlelineElide = elideMode != QQuickText::ElideNone && q->widthValid();
    bool multilineElide = elideMode == QQuickText::ElideRight
            && q->widthValid()
            && (q->heightValid() || maximumLineCountValid);

    if ((!requireImplicitSize || (implicitWidthValid && implicitHeightValid))
            && ((singlelineElide && q->width() <= 0.) || (multilineElide && q->heightValid() && q->height() <= 0.))) {
        // we are elided and we have a zero width or height
        widthExceeded = q->widthValid() && q->width() <= 0.;
        heightExceeded = q->heightValid() && q->height() <= 0.;

        if (!truncated) {
            truncated = true;
            emit q->truncatedChanged();
        }
        if (lineCount) {
            lineCount = 0;
            emit q->lineCountChanged();
        }

        QFontMetricsF fm(font);
        qreal height = (lineHeightMode() == QQuickText::FixedHeight) ? lineHeight() : qCeil(fm.height()) * lineHeight();
        *baseline = fm.ascent();
        return QRectF(0, 0, 0, height);
    }

    bool shouldUseDesignMetrics = renderType != QQuickText::NativeRendering;
    if (!visibleImgTags.isEmpty())
        visibleImgTags.clear();
    layout.setCacheEnabled(true);
    QTextOption textOption = layout.textOption();
    if (textOption.alignment() != q->effectiveHAlign()
            || textOption.wrapMode() != QTextOption::WrapMode(wrapMode)
            || textOption.useDesignMetrics() != shouldUseDesignMetrics) {
        textOption.setAlignment(Qt::Alignment(q->effectiveHAlign()));
        textOption.setWrapMode(QTextOption::WrapMode(wrapMode));
        textOption.setUseDesignMetrics(shouldUseDesignMetrics);
        layout.setTextOption(textOption);
    }
    if (layout.font() != font)
        layout.setFont(font);

    lineWidth = (q->widthValid() || implicitWidthValid) && q->width() > 0
            ? q->width()
            : FLT_MAX;
    qreal maxHeight = q->heightValid() ? q->height() : FLT_MAX;

    const bool customLayout = isLineLaidOutConnected();
    const bool wasTruncated = truncated;

    bool canWrap = wrapMode != QQuickText::NoWrap && q->widthValid();

    bool horizontalFit = fontSizeMode() & QQuickText::HorizontalFit && q->widthValid();
    bool verticalFit = fontSizeMode() & QQuickText::VerticalFit
            && (q->heightValid() || (maximumLineCountValid && canWrap));

    const bool pixelSize = font.pixelSize() != -1;
    QString layoutText = layout.text();

    int largeFont = pixelSize ? font.pixelSize() : font.pointSize();
    int smallFont = fontSizeMode() != QQuickText::FixedSize
            ? qMin(pixelSize ? minimumPixelSize() : minimumPointSize(), largeFont)
            : largeFont;
    int scaledFontSize = largeFont;

    widthExceeded = q->width() <= 0 && (singlelineElide || canWrap || horizontalFit);
    heightExceeded = q->height() <= 0 && (multilineElide || verticalFit);

    QRectF br;

    QFont scaledFont = font;

    int visibleCount = 0;
    bool elide;
    qreal height = 0;
    QString elideText;
    bool once = true;
    int elideStart = 0;
    int elideEnd = 0;

    int eos = multilengthEos;

    // Repeated layouts with reduced font sizes or abbreviated strings may be required if the text
    // doesn't fit within the item dimensions,  or a binding to implicitWidth/Height changes
    // the item dimensions.
    for (;;) {
        if (!once) {
            if (pixelSize)
                scaledFont.setPixelSize(scaledFontSize);
            else
                scaledFont.setPointSize(scaledFontSize);
            if (layout.font() != scaledFont)
                layout.setFont(scaledFont);
        }

        layout.beginLayout();

        bool wrapped = false;
        bool truncateHeight = false;
        truncated = false;
        elide = false;
        int unwrappedLineCount = 1;
        int maxLineCount = maximumLineCount();
        height = 0;
        qreal naturalHeight = 0;
        qreal previousHeight = 0;
        br = QRectF();

        QRectF unelidedRect;
        QTextLine line = layout.createLine();
        for (visibleCount = 1; ; ++visibleCount) {
            if (customLayout) {
                setupCustomLineGeometry(line, naturalHeight);
            } else {
                setLineGeometry(line, lineWidth, naturalHeight);
            }

            unelidedRect = br.united(line.naturalTextRect());

            // Elide the previous line if the accumulated height of the text exceeds the height
            // of the element.
            if (multilineElide && naturalHeight > maxHeight && visibleCount > 1) {
                elide = true;
                heightExceeded = true;
                if (eos != -1)  // There's an abbreviated string available, skip the rest as it's
                    break;      // all going to be discarded.

                truncated = true;
                truncateHeight = true;

                visibleCount -= 1;

                QTextLine previousLine = layout.lineAt(visibleCount - 1);
                elideText = layoutText.at(line.textStart() - 1) != QChar::LineSeparator
                        ? elidedText(lineWidth, previousLine, &line)
                        : elidedText(lineWidth, previousLine);
                elideStart = previousLine.textStart();
                // elideEnd isn't required for right eliding.

                height = previousHeight;
                break;
            }

            const QTextLine previousLine = line;
            line = layout.createLine();
            if (!line.isValid()) {
                if (singlelineElide && visibleCount == 1 && previousLine.naturalTextWidth() > lineWidth) {
                    // Elide a single previousLine of  text if its width exceeds the element width.
                    elide = true;
                    widthExceeded = true;
                    if (eos != -1) // There's an abbreviated string available.
                        break;

                    truncated = true;
                    elideText = layout.engine()->elidedText(
                            Qt::TextElideMode(elideMode),
                            QFixed::fromReal(lineWidth),
                            0,
                            previousLine.textStart(),
                            previousLine.textLength());
                    elideStart = previousLine.textStart();
                    elideEnd = elideStart + previousLine.textLength();
                } else {
                    br = unelidedRect;
                    height = naturalHeight;
                }
                break;
            } else {
                const bool wrappedLine = layoutText.at(line.textStart() - 1) != QChar::LineSeparator;
                wrapped |= wrappedLine;

                if (!wrappedLine)
                    ++unwrappedLineCount;

                // Stop if the maximum number of lines has been reached and elide the last line
                // if enabled.
                if (visibleCount == maxLineCount) {
                    truncated = true;
                    heightExceeded |= wrapped;

                    if (multilineElide) {
                        elide = true;
                        if (eos != -1)  // There's an abbreviated string available
                            break;
                        elideText = wrappedLine
                                ? elidedText(lineWidth, previousLine, &line)
                                : elidedText(lineWidth, previousLine);
                        elideStart = previousLine.textStart();
                        // elideEnd isn't required for right eliding.
                    } else {
                        br = unelidedRect;
                        height = naturalHeight;
                    }
                    break;
                }
            }
            br = unelidedRect;
            previousHeight = height;
            height = naturalHeight;
        }
        widthExceeded |= wrapped;

        // Save the implicit size of the text on the first layout only.
        if (once) {
            once = false;

            // If implicit sizes are required layout any additional lines up to the maximum line
            // count.
            if ((requireImplicitSize) && line.isValid() && unwrappedLineCount < maxLineCount) {
                // Layout the remainder of the wrapped lines up to maxLineCount to get the implicit
                // height.
                for (int lineCount = layout.lineCount(); lineCount < maxLineCount; ++lineCount) {
                    line = layout.createLine();
                    if (!line.isValid())
                        break;
                    if (layoutText.at(line.textStart() - 1) == QChar::LineSeparator)
                        ++unwrappedLineCount;
                    setLineGeometry(line, lineWidth, naturalHeight);
                }

                // Create the remainder of the unwrapped lines up to maxLineCount to get the
                // implicit width.
                const int eol = line.isValid()
                        ? line.textStart() + line.textLength()
                        : layoutText.length();
                if (eol < layoutText.length() && layoutText.at(eol) != QChar::LineSeparator)
                    line = layout.createLine();
                for (; line.isValid() && unwrappedLineCount <= maxLineCount; ++unwrappedLineCount)
                    line = layout.createLine();
            }
            layout.endLayout();

            const qreal naturalWidth = layout.maximumWidth();

            bool wasInLayout = internalWidthUpdate;
            internalWidthUpdate = true;
            q->setImplicitSize(naturalWidth, naturalHeight);
            internalWidthUpdate = wasInLayout;

            // Update any variables that are dependent on the validity of the width or height.
            singlelineElide = elideMode != QQuickText::ElideNone && q->widthValid();
            multilineElide = elideMode == QQuickText::ElideRight
                    && q->widthValid()
                    && (q->heightValid() || maximumLineCountValid);
            canWrap = wrapMode != QQuickText::NoWrap && q->widthValid();

            horizontalFit = fontSizeMode() & QQuickText::HorizontalFit && q->widthValid();
            verticalFit = fontSizeMode() & QQuickText::VerticalFit
                    && (q->heightValid() || (maximumLineCountValid && canWrap));

            const qreal oldWidth = lineWidth;
            const qreal oldHeight = maxHeight;

            lineWidth = q->widthValid() && q->width() > 0 ? q->width() : naturalWidth;
            maxHeight = q->heightValid() ? q->height() : FLT_MAX;

            // If the width of the item has changed and it's possible the result of wrapping,
            // eliding, scaling has changed, or the text is not left aligned do another layout.
            if ((lineWidth < qMin(oldWidth, naturalWidth) || (widthExceeded && lineWidth > oldWidth))
                    && (singlelineElide || multilineElide || canWrap || horizontalFit
                        || q->effectiveHAlign() != QQuickText::AlignLeft)) {
                widthExceeded = false;
                heightExceeded = false;
                continue;
            }

            // If the height of the item has changed and it's possible the result of eliding,
            // line count truncation or scaling has changed, do another layout.
            if ((maxHeight < qMin(oldHeight, naturalHeight) || (heightExceeded && maxHeight > oldHeight))
                    && (multilineElide || (canWrap && maximumLineCountValid))) {
                widthExceeded = false;
                heightExceeded = false;
                continue;
            }

            // If the horizontal alignment is not left and the width was not valid we need to relayout
            // now that we know the maximum line width.
            if (!implicitWidthValid && unwrappedLineCount > 1 && q->effectiveHAlign() != QQuickText::AlignLeft) {
                widthExceeded = false;
                heightExceeded = false;
                continue;
            }
        } else {
            layout.endLayout();
        }

        // If the next needs to be elided and there's an abbreviated string available
        // go back and do another layout with the abbreviated string.
        if (eos != -1 && elide) {
            int start = eos + 1;
            eos = text.indexOf(QLatin1Char('\x9c'),  start);
            layoutText = text.mid(start, eos != -1 ? eos - start : -1);
            layoutText.replace(QLatin1Char('\n'), QChar::LineSeparator);
            layout.setText(layoutText);
            textHasChanged = true;
            continue;
        }

        br.moveTop(0);

        if (!horizontalFit && !verticalFit)
            break;

        // Try and find a font size that better fits the dimensions of the element.
        if (horizontalFit) {
            if (unelidedRect.width() > lineWidth || (!verticalFit && wrapped)) {
                widthExceeded = true;
                largeFont = scaledFontSize - 1;
                if (smallFont > largeFont)
                    break;
                scaledFontSize = (smallFont + largeFont) / 2;
                if (pixelSize)
                    scaledFont.setPixelSize(scaledFontSize);
                else
                    scaledFont.setPointSize(scaledFontSize);
                continue;
            } else if (!verticalFit) {
                smallFont = scaledFontSize;
                if (smallFont == largeFont)
                    break;
                scaledFontSize = (smallFont + largeFont + 1) / 2;
            }
        }

        if (verticalFit) {
            if (truncateHeight || unelidedRect.height() > maxHeight) {
                heightExceeded = true;
                largeFont = scaledFontSize - 1;
                if (smallFont > largeFont)
                    break;
                scaledFontSize = (smallFont + largeFont) / 2;

            } else {
                smallFont = scaledFontSize;
                if (smallFont == largeFont)
                    break;
                scaledFontSize = (smallFont + largeFont + 1) / 2;
            }
        }
    }

    implicitWidthValid = true;
    implicitHeightValid = true;

    if (eos != multilengthEos)
        truncated = true;

    if (elide) {
        if (!elideLayout) {
            elideLayout = new QTextLayout;
            elideLayout->setCacheEnabled(true);
        }
        if (styledText) {
            QList<QTextLayout::FormatRange> formats;
            switch (elideMode) {
            case QQuickText::ElideRight:
                elideFormats(elideStart, elideText.length() - 1, 0, &formats);
                break;
            case QQuickText::ElideLeft:
                elideFormats(elideEnd - elideText.length() + 1, elideText.length() - 1, 1, &formats);
                break;
            case QQuickText::ElideMiddle: {
                const int index = elideText.indexOf(elideChar);
                if (index != -1) {
                    elideFormats(elideStart, index, 0, &formats);
                    elideFormats(
                            elideEnd - elideText.length() + index + 1,
                            elideText.length() - index - 1,
                            index + 1,
                            &formats);
                }
                break;
            }
            default:
                break;
            }
            elideLayout->setAdditionalFormats(formats);
        }

        elideLayout->setFont(layout.font());
        elideLayout->setTextOption(layout.textOption());
        elideLayout->setText(elideText);
        elideLayout->beginLayout();

        QTextLine elidedLine = elideLayout->createLine();
        elidedLine.setPosition(QPointF(0, height));
        if (customLayout) {
            setupCustomLineGeometry(elidedLine, height, visibleCount - 1);
        } else {
            setLineGeometry(elidedLine, lineWidth, height);
        }
        elideLayout->endLayout();

        br = br.united(elidedLine.naturalTextRect());

        if (visibleCount == 1)
            layout.clearLayout();
    } else {
        delete elideLayout;
        elideLayout = 0;
    }

    QTextLine firstLine = visibleCount == 1 && elideLayout
            ? elideLayout->lineAt(0)
            : layout.lineAt(0);
    Q_ASSERT(firstLine.isValid());
    *baseline = firstLine.y() + firstLine.ascent();

    if (!customLayout)
        br.setHeight(height);

    //Update the number of visible lines
    if (lineCount != visibleCount) {
        lineCount = visibleCount;
        emit q->lineCountChanged();
    }

    if (truncated != wasTruncated)
        emit q->truncatedChanged();

    return br;
}

void QQuickTextPrivate::setLineGeometry(QTextLine &line, qreal lineWidth, qreal &height)
{
    Q_Q(QQuickText);
    line.setLineWidth(lineWidth);

    if (imgTags.isEmpty()) {
        line.setPosition(QPointF(line.position().x(), height));
        height += (lineHeightMode() == QQuickText::FixedHeight) ? lineHeight() : line.height() * lineHeight();
        return;
    }

    qreal textTop = 0;
    qreal textHeight = line.height();
    qreal totalLineHeight = textHeight;

    QList<QQuickStyledTextImgTag *> imagesInLine;

    foreach (QQuickStyledTextImgTag *image, imgTags) {
        if (image->position >= line.textStart() &&
            image->position < line.textStart() + line.textLength()) {

            if (!image->pix) {
                QUrl url = q->baseUrl().resolved(image->url);
                image->pix = new QQuickPixmap(qmlEngine(q), url, image->size);
                if (image->pix->isLoading()) {
                    image->pix->connectFinished(q, SLOT(imageDownloadFinished()));
                    if (!extra.isAllocated() || !extra->nbActiveDownloads)
                        extra.value().nbActiveDownloads = 0;
                    extra->nbActiveDownloads++;
                } else if (image->pix->isReady()) {
                    if (!image->size.isValid()) {
                        image->size = image->pix->implicitSize();
                        // if the size of the image was not explicitly set, we need to
                        // call updateLayout() once again.
                        needToUpdateLayout = true;
                    }
                } else if (image->pix->isError()) {
                    qmlInfo(q) << image->pix->error();
                }
            }

            qreal ih = qreal(image->size.height());
            if (image->align == QQuickStyledTextImgTag::Top)
                image->pos.setY(0);
            else if (image->align == QQuickStyledTextImgTag::Middle)
                image->pos.setY((textHeight / 2.0) - (ih / 2.0));
            else
                image->pos.setY(textHeight - ih);
            imagesInLine << image;
            textTop = qMax(textTop, qAbs(image->pos.y()));
        }
    }

    foreach (QQuickStyledTextImgTag *image, imagesInLine) {
        totalLineHeight = qMax(totalLineHeight, textTop + image->pos.y() + image->size.height());
        image->pos.setX(line.cursorToX(image->position));
        image->pos.setY(image->pos.y() + height + textTop);
        visibleImgTags << image;
    }

    line.setPosition(QPointF(line.position().x(), height + textTop));
    height += (lineHeightMode() == QQuickText::FixedHeight) ? lineHeight() : totalLineHeight * lineHeight();
}

/*!
    Returns the y offset when aligning text with a non-1.0 lineHeight
*/
int QQuickTextPrivate::lineHeightOffset() const
{
    QFontMetricsF fm(font);
    qreal fontHeight = qCeil(fm.height());  // QScriptLine and therefore QTextLine rounds up
    return lineHeightMode() == QQuickText::FixedHeight ? fontHeight - lineHeight()
                                                       : (1.0 - lineHeight()) * fontHeight;
}

/*!
    Ensures the QQuickTextPrivate::doc variable is set to a valid text document
*/
void QQuickTextPrivate::ensureDoc()
{
    if (!extra.isAllocated() || !extra->doc) {
        Q_Q(QQuickText);
        extra.value().doc = new QQuickTextDocumentWithImageResources(q);
        extra->doc->setPageSize(QSizeF(0, 0));
        extra->doc->setDocumentMargin(0);
        extra->doc->setBaseUrl(q->baseUrl());
        qmlobject_connect(extra->doc, QQuickTextDocumentWithImageResources, SIGNAL(imagesLoaded()),
                          q, QQuickText, SLOT(q_imagesLoaded()));
    }
}

/*!
    \qmltype Text
    \instantiates QQuickText
    \inqmlmodule QtQuick
    \ingroup qtquick-visual
    \inherits Item
    \brief Specifies how to add formatted text to a scene

    Text items can display both plain and rich text. For example, red text with
    a specific font and size can be defined like this:

    \qml
    Text {
        text: "Hello World!"
        font.family: "Helvetica"
        font.pointSize: 24
        color: "red"
    }
    \endqml

    Rich text is defined using HTML-style markup:

    \qml
    Text {
        text: "<b>Hello</b> <i>World!</i>"
    }
    \endqml

    \image declarative-text.png

    If height and width are not explicitly set, Text will attempt to determine how
    much room is needed and set it accordingly. Unless \l wrapMode is set, it will always
    prefer width to height (all text will be placed on a single line).

    The \l elide property can alternatively be used to fit a single line of
    plain text to a set width.

    Note that the \l{Supported HTML Subset} is limited. Also, if the text contains
    HTML img tags that load remote images, the text is reloaded.

    Text provides read-only text. For editable text, see \l TextEdit.

    \sa {Qt Quick Examples - Text#Fonts}{Fonts example}
*/
QQuickText::QQuickText(QQuickItem *parent)
: QQuickImplicitSizeItem(*(new QQuickTextPrivate), parent)
{
    Q_D(QQuickText);
    d->init();
}

QQuickText::~QQuickText()
{
}

/*!
  \qmlproperty bool QtQuick::Text::clip
  This property holds whether the text is clipped.

  Note that if the text does not fit in the bounding rectangle it will be abruptly chopped.

  If you want to display potentially long text in a limited space, you probably want to use \c elide instead.
*/

/*!
    \qmlsignal QtQuick::Text::lineLaidOut(object line)

    This signal is emitted for each line of text that is laid out during the layout
    process. The specified \a line object provides more details about the line that
    is currently being laid out.

    This gives the opportunity to position and resize a line as it is being laid out.
    It can for example be used to create columns or lay out text around objects.

    The properties of the specified \a line object are:
    \list
    \li number (read-only)
    \li x
    \li y
    \li width
    \li height
    \endlist

    For example, this will move the first 5 lines of a Text item by 100 pixels to the right:
    \code
    onLineLaidOut: {
        if (line.number < 5) {
            line.x = line.x + 100
            line.width = line.width - 100
        }
    }
    \endcode

    The corresponding handler is \c onLineLaidOut.
*/

/*!
    \qmlsignal QtQuick::Text::linkActivated(string link)

    This signal is emitted when the user clicks on a link embedded in the text.
    The link must be in rich text or HTML format and the
    \a link string provides access to the particular link.

    \snippet qml/text/onLinkActivated.qml 0

    The example code will display the text
    "See the \l{http://qt-project.org}{Qt Project website}."

    Clicking on the highlighted link will output
    \tt{http://qt-project.org link activated} to the console.

    The corresponding handler is \c onLinkActivated.
*/

/*!
    \qmlproperty string QtQuick::Text::font.family

    Sets the family name of the font.

    The family name is case insensitive and may optionally include a foundry name, e.g. "Helvetica [Cronyx]".
    If the family is available from more than one foundry and the foundry isn't specified, an arbitrary foundry is chosen.
    If the family isn't available a family will be set using the font matching algorithm.
*/

/*!
    \qmlproperty bool QtQuick::Text::font.bold

    Sets whether the font weight is bold.
*/

/*!
    \qmlproperty enumeration QtQuick::Text::font.weight

    Sets the font's weight.

    The weight can be one of:
    \list
    \li Font.Light
    \li Font.Normal - the default
    \li Font.DemiBold
    \li Font.Bold
    \li Font.Black
    \endlist

    \qml
    Text { text: "Hello"; font.weight: Font.DemiBold }
    \endqml
*/

/*!
    \qmlproperty bool QtQuick::Text::font.italic

    Sets whether the font has an italic style.
*/

/*!
    \qmlproperty bool QtQuick::Text::font.underline

    Sets whether the text is underlined.
*/

/*!
    \qmlproperty bool QtQuick::Text::font.strikeout

    Sets whether the font has a strikeout style.
*/

/*!
    \qmlproperty real QtQuick::Text::font.pointSize

    Sets the font size in points. The point size must be greater than zero.
*/

/*!
    \qmlproperty int QtQuick::Text::font.pixelSize

    Sets the font size in pixels.

    Using this function makes the font device dependent.
    Use \c pointSize to set the size of the font in a device independent manner.
*/

/*!
    \qmlproperty real QtQuick::Text::font.letterSpacing

    Sets the letter spacing for the font.

    Letter spacing changes the default spacing between individual letters in the font.
    A positive value increases the letter spacing by the corresponding pixels; a negative value decreases the spacing.
*/

/*!
    \qmlproperty real QtQuick::Text::font.wordSpacing

    Sets the word spacing for the font.

    Word spacing changes the default spacing between individual words.
    A positive value increases the word spacing by a corresponding amount of pixels,
    while a negative value decreases the inter-word spacing accordingly.
*/

/*!
    \qmlproperty enumeration QtQuick::Text::font.capitalization

    Sets the capitalization for the text.

    \list
    \li Font.MixedCase - This is the normal text rendering option where no capitalization change is applied.
    \li Font.AllUppercase - This alters the text to be rendered in all uppercase type.
    \li Font.AllLowercase - This alters the text to be rendered in all lowercase type.
    \li Font.SmallCaps - This alters the text to be rendered in small-caps type.
    \li Font.Capitalize - This alters the text to be rendered with the first character of each word as an uppercase character.
    \endlist

    \qml
    Text { text: "Hello"; font.capitalization: Font.AllLowercase }
    \endqml
*/
QFont QQuickText::font() const
{
    Q_D(const QQuickText);
    return d->sourceFont;
}

void QQuickText::setFont(const QFont &font)
{
    Q_D(QQuickText);
    if (d->sourceFont == font)
        return;

    d->sourceFont = font;
    QFont oldFont = d->font;
    d->font = font;

    if (!antialiasing())
        d->font.setStyleStrategy(QFont::NoAntialias);

    if (d->font.pointSizeF() != -1) {
        // 0.5pt resolution
        qreal size = qRound(d->font.pointSizeF()*2.0);
        d->font.setPointSizeF(size/2.0);
    }

    if (oldFont != d->font) {
        // if the format changes the size of the text
        // with headings or <font> tag, we need to re-parse
        if (d->formatModifiesFontSize)
            d->textHasChanged = true;
        d->implicitWidthValid = false;
        d->implicitHeightValid = false;
        d->updateLayout();
    }

    emit fontChanged(d->sourceFont);
}

void QQuickText::itemChange(ItemChange change, const ItemChangeData &value)
{
    Q_D(QQuickText);
    Q_UNUSED(value);
    if (change == ItemAntialiasingHasChanged) {
        if (!antialiasing())
            d->font.setStyleStrategy(QFont::NoAntialias);
        else
            d->font.setStyleStrategy(QFont::PreferAntialias);
        d->implicitWidthValid = false;
        d->implicitHeightValid = false;
        d->updateLayout();
    }
    QQuickItem::itemChange(change, value);
}

/*!
    \qmlproperty string QtQuick::Text::text

    The text to display. Text supports both plain and rich text strings.

    The item will try to automatically determine whether the text should
    be treated as styled text. This determination is made using Qt::mightBeRichText().
*/
QString QQuickText::text() const
{
    Q_D(const QQuickText);
    return d->text;
}

void QQuickText::setText(const QString &n)
{
    Q_D(QQuickText);
    if (d->text == n)
        return;

    d->richText = d->format == RichText;
    d->styledText = d->format == StyledText || (d->format == AutoText && Qt::mightBeRichText(n));
    d->text = n;
    if (isComponentComplete()) {
        if (d->richText) {
            d->ensureDoc();
            d->extra->doc->setText(n);
            d->rightToLeftText = d->extra->doc->toPlainText().isRightToLeft();
        } else {
            d->rightToLeftText = d->text.isRightToLeft();
        }
        d->determineHorizontalAlignment();
    }
    d->textHasChanged = true;
    d->implicitWidthValid = false;
    d->implicitHeightValid = false;
    qDeleteAll(d->imgTags);
    d->imgTags.clear();
    d->updateLayout();
    setAcceptHoverEvents(d->richText || d->styledText);
    emit textChanged(d->text);
}

/*!
    \qmlproperty color QtQuick::Text::color

    The text color.

    An example of green text defined using hexadecimal notation:
    \qml
    Text {
        color: "#00FF00"
        text: "green text"
    }
    \endqml

    An example of steel blue text defined using an SVG color name:
    \qml
    Text {
        color: "steelblue"
        text: "blue text"
    }
    \endqml
*/
QColor QQuickText::color() const
{
    Q_D(const QQuickText);
    return QColor::fromRgba(d->color);
}

void QQuickText::setColor(const QColor &color)
{
    Q_D(QQuickText);
    QRgb rgb = color.rgba();
    if (d->color == rgb)
        return;

    d->color = rgb;
    if (isComponentComplete())  {
        d->updateType = QQuickTextPrivate::UpdatePaintNode;
        update();
    }
    emit colorChanged();
}

/*!
    \qmlproperty color QtQuick::Text::linkColor

    The color of links in the text.

    This property works with the StyledText \l textFormat, but not with RichText.
    Link color in RichText can be specified by including CSS style tags in the
    text.
*/

QColor QQuickText::linkColor() const
{
    Q_D(const QQuickText);
    return QColor::fromRgba(d->linkColor);
}

void QQuickText::setLinkColor(const QColor &color)
{
    Q_D(QQuickText);
    QRgb rgb = color.rgba();
    if (d->linkColor == rgb)
        return;

    d->linkColor = rgb;
    update();
    emit linkColorChanged();
}

/*!
    \qmlproperty enumeration QtQuick::Text::style

    Set an additional text style.

    Supported text styles are:
    \list
    \li Text.Normal - the default
    \li Text.Outline
    \li Text.Raised
    \li Text.Sunken
    \endlist

    \qml
    Row {
        Text { font.pointSize: 24; text: "Normal" }
        Text { font.pointSize: 24; text: "Raised"; style: Text.Raised; styleColor: "#AAAAAA" }
        Text { font.pointSize: 24; text: "Outline";style: Text.Outline; styleColor: "red" }
        Text { font.pointSize: 24; text: "Sunken"; style: Text.Sunken; styleColor: "#AAAAAA" }
    }
    \endqml

    \image declarative-textstyle.png
*/
QQuickText::TextStyle QQuickText::style() const
{
    Q_D(const QQuickText);
    return d->style;
}

void QQuickText::setStyle(QQuickText::TextStyle style)
{
    Q_D(QQuickText);
    if (d->style == style)
        return;

    d->style = style;
    if (isComponentComplete()) {
        d->updateType = QQuickTextPrivate::UpdatePaintNode;
        update();
    }
    emit styleChanged(d->style);
}

/*!
    \qmlproperty color QtQuick::Text::styleColor

    Defines the secondary color used by text styles.

    \c styleColor is used as the outline color for outlined text, and as the
    shadow color for raised or sunken text. If no style has been set, it is not
    used at all.

    \qml
    Text { font.pointSize: 18; text: "hello"; style: Text.Raised; styleColor: "gray" }
    \endqml

    \sa style
 */
QColor QQuickText::styleColor() const
{
    Q_D(const QQuickText);
    return QColor::fromRgba(d->styleColor);
}

void QQuickText::setStyleColor(const QColor &color)
{
    Q_D(QQuickText);
    QRgb rgb = color.rgba();
    if (d->styleColor == rgb)
        return;

    d->styleColor = rgb;
    if (isComponentComplete()) {
        d->updateType = QQuickTextPrivate::UpdatePaintNode;
        update();
    }
    emit styleColorChanged();
}

/*!
    \qmlproperty enumeration QtQuick::Text::horizontalAlignment
    \qmlproperty enumeration QtQuick::Text::verticalAlignment
    \qmlproperty enumeration QtQuick::Text::effectiveHorizontalAlignment

    Sets the horizontal and vertical alignment of the text within the Text items
    width and height. By default, the text is vertically aligned to the top. Horizontal
    alignment follows the natural alignment of the text, for example text that is read
    from left to right will be aligned to the left.

    The valid values for \c horizontalAlignment are \c Text.AlignLeft, \c Text.AlignRight, \c Text.AlignHCenter and
    \c Text.AlignJustify.  The valid values for \c verticalAlignment are \c Text.AlignTop, \c Text.AlignBottom
    and \c Text.AlignVCenter.

    Note that for a single line of text, the size of the text is the area of the text. In this common case,
    all alignments are equivalent. If you want the text to be, say, centered in its parent, then you will
    need to either modify the Item::anchors, or set horizontalAlignment to Text.AlignHCenter and bind the width to
    that of the parent.

    When using the attached property LayoutMirroring::enabled to mirror application
    layouts, the horizontal alignment of text will also be mirrored. However, the property
    \c horizontalAlignment will remain unchanged. To query the effective horizontal alignment
    of Text, use the read-only property \c effectiveHorizontalAlignment.
*/
QQuickText::HAlignment QQuickText::hAlign() const
{
    Q_D(const QQuickText);
    return d->hAlign;
}

void QQuickText::setHAlign(HAlignment align)
{
    Q_D(QQuickText);
    bool forceAlign = d->hAlignImplicit && d->effectiveLayoutMirror;
    d->hAlignImplicit = false;
    if (d->setHAlign(align, forceAlign) && isComponentComplete())
        d->updateLayout();
}

void QQuickText::resetHAlign()
{
    Q_D(QQuickText);
    d->hAlignImplicit = true;
    if (isComponentComplete() && d->determineHorizontalAlignment())
        d->updateLayout();
}

QQuickText::HAlignment QQuickText::effectiveHAlign() const
{
    Q_D(const QQuickText);
    QQuickText::HAlignment effectiveAlignment = d->hAlign;
    if (!d->hAlignImplicit && d->effectiveLayoutMirror) {
        switch (d->hAlign) {
        case QQuickText::AlignLeft:
            effectiveAlignment = QQuickText::AlignRight;
            break;
        case QQuickText::AlignRight:
            effectiveAlignment = QQuickText::AlignLeft;
            break;
        default:
            break;
        }
    }
    return effectiveAlignment;
}

bool QQuickTextPrivate::setHAlign(QQuickText::HAlignment alignment, bool forceAlign)
{
    Q_Q(QQuickText);
    if (hAlign != alignment || forceAlign) {
        QQuickText::HAlignment oldEffectiveHAlign = q->effectiveHAlign();
        hAlign = alignment;

        emit q->horizontalAlignmentChanged(hAlign);
        if (oldEffectiveHAlign != q->effectiveHAlign())
            emit q->effectiveHorizontalAlignmentChanged();
        return true;
    }
    return false;
}

bool QQuickTextPrivate::determineHorizontalAlignment()
{
    if (hAlignImplicit) {
#ifndef QT_NO_IM
        bool alignToRight = text.isEmpty() ? qApp->inputMethod()->inputDirection() == Qt::RightToLeft : rightToLeftText;
#else
        bool alignToRight = rightToLeftText;
#endif
        return setHAlign(alignToRight ? QQuickText::AlignRight : QQuickText::AlignLeft);
    }
    return false;
}

void QQuickTextPrivate::mirrorChange()
{
    Q_Q(QQuickText);
    if (q->isComponentComplete()) {
        if (!hAlignImplicit && (hAlign == QQuickText::AlignRight || hAlign == QQuickText::AlignLeft)) {
            updateLayout();
            emit q->effectiveHorizontalAlignmentChanged();
        }
    }
}

QQuickText::VAlignment QQuickText::vAlign() const
{
    Q_D(const QQuickText);
    return d->vAlign;
}

void QQuickText::setVAlign(VAlignment align)
{
    Q_D(QQuickText);
    if (d->vAlign == align)
        return;

    d->vAlign = align;

    if (isComponentComplete())
        d->updateLayout();

    emit verticalAlignmentChanged(align);
}

/*!
    \qmlproperty enumeration QtQuick::Text::wrapMode

    Set this property to wrap the text to the Text item's width.  The text will only
    wrap if an explicit width has been set.  wrapMode can be one of:

    \list
    \li Text.NoWrap (default) - no wrapping will be performed. If the text contains insufficient newlines, then \l contentWidth will exceed a set width.
    \li Text.WordWrap - wrapping is done on word boundaries only. If a word is too long, \l contentWidth will exceed a set width.
    \li Text.WrapAnywhere - wrapping is done at any point on a line, even if it occurs in the middle of a word.
    \li Text.Wrap - if possible, wrapping occurs at a word boundary; otherwise it will occur at the appropriate point on the line, even in the middle of a word.
    \endlist
*/
QQuickText::WrapMode QQuickText::wrapMode() const
{
    Q_D(const QQuickText);
    return d->wrapMode;
}

void QQuickText::setWrapMode(WrapMode mode)
{
    Q_D(QQuickText);
    if (mode == d->wrapMode)
        return;

    d->wrapMode = mode;
    d->updateLayout();

    emit wrapModeChanged();
}

/*!
    \qmlproperty int QtQuick::Text::lineCount

    Returns the number of lines visible in the text item.

    This property is not supported for rich text.

    \sa maximumLineCount
*/
int QQuickText::lineCount() const
{
    Q_D(const QQuickText);
    return d->lineCount;
}

/*!
    \qmlproperty bool QtQuick::Text::truncated

    Returns true if the text has been truncated due to \l maximumLineCount
    or \l elide.

    This property is not supported for rich text.

    \sa maximumLineCount, elide
*/
bool QQuickText::truncated() const
{
    Q_D(const QQuickText);
    return d->truncated;
}

/*!
    \qmlproperty int QtQuick::Text::maximumLineCount

    Set this property to limit the number of lines that the text item will show.
    If elide is set to Text.ElideRight, the text will be elided appropriately.
    By default, this is the value of the largest possible integer.

    This property is not supported for rich text.

    \sa lineCount, elide
*/
int QQuickText::maximumLineCount() const
{
    Q_D(const QQuickText);
    return d->maximumLineCount();
}

void QQuickText::setMaximumLineCount(int lines)
{
    Q_D(QQuickText);

    d->maximumLineCountValid = lines==INT_MAX ? false : true;
    if (d->maximumLineCount() != lines) {
        d->extra.value().maximumLineCount = lines;
        d->implicitHeightValid = false;
        d->updateLayout();
        emit maximumLineCountChanged();
    }
}

void QQuickText::resetMaximumLineCount()
{
    Q_D(QQuickText);
    setMaximumLineCount(INT_MAX);
    if (d->truncated != false) {
        d->truncated = false;
        emit truncatedChanged();
    }
}

/*!
    \qmlproperty enumeration QtQuick::Text::textFormat

    The way the text property should be displayed.

    Supported text formats are:

    \list
    \li Text.AutoText (default)
    \li Text.PlainText
    \li Text.StyledText
    \li Text.RichText
    \endlist

    If the text format is \c Text.AutoText the Text item
    will automatically determine whether the text should be treated as
    styled text.  This determination is made using Qt::mightBeRichText()
    which uses a fast and therefore simple heuristic. It mainly checks
    whether there is something that looks like a tag before the first
    line break. Although the result may be correct for common cases,
    there is no guarantee.

    Text.StyledText is an optimized format supporting some basic text
    styling markup, in the style of HTML 3.2:

    \code
    <b></b> - bold
    <strong></strong> - bold
    <i></i> - italic
    <br> - new line
    <p> - paragraph
    <u> - underlined text
    <font color="color_name" size="1-7"></font>
    <h1> to <h6> - headers
    <a href=""> - anchor
    <img src="" align="top,middle,bottom" width="" height=""> - inline images
    <ol type="">, <ul type=""> and <li> - ordered and unordered lists
    <pre></pre> - preformatted
    &gt; &lt; &amp;
    \endcode

    \c Text.StyledText parser is strict, requiring tags to be correctly nested.

    \table
    \row
    \li
    \qml
Column {
    Text {
        font.pointSize: 24
        text: "<b>Hello</b> <i>World!</i>"
    }
    Text {
        font.pointSize: 24
        textFormat: Text.RichText
        text: "<b>Hello</b> <i>World!</i>"
    }
    Text {
        font.pointSize: 24
        textFormat: Text.PlainText
        text: "<b>Hello</b> <i>World!</i>"
    }
}
    \endqml
    \li \image declarative-textformat.png
    \endtable

    Text.RichText supports a larger subset of HTML 4, as described on the
    \l {Supported HTML Subset} page. You should prefer using Text.PlainText
    or Text.StyledText instead, as they offer better performance.
*/
QQuickText::TextFormat QQuickText::textFormat() const
{
    Q_D(const QQuickText);
    return d->format;
}

void QQuickText::setTextFormat(TextFormat format)
{
    Q_D(QQuickText);
    if (format == d->format)
        return;
    d->format = format;
    bool wasRich = d->richText;
    d->richText = format == RichText;
    d->styledText = format == StyledText || (format == AutoText && Qt::mightBeRichText(d->text));

    if (isComponentComplete()) {
        if (!wasRich && d->richText) {
            d->ensureDoc();
            d->extra->doc->setText(d->text);
            d->rightToLeftText = d->extra->doc->toPlainText().isRightToLeft();
        } else {
            d->rightToLeftText = d->text.isRightToLeft();
            d->textHasChanged = true;
        }
        d->determineHorizontalAlignment();
    }
    d->updateLayout();
    setAcceptHoverEvents(d->richText || d->styledText);

    emit textFormatChanged(d->format);
}

/*!
    \qmlproperty enumeration QtQuick::Text::elide

    Set this property to elide parts of the text fit to the Text item's width.
    The text will only elide if an explicit width has been set.

    This property cannot be used with rich text.

    Eliding can be:
    \list
    \li Text.ElideNone  - the default
    \li Text.ElideLeft
    \li Text.ElideMiddle
    \li Text.ElideRight
    \endlist

    If this property is set to Text.ElideRight, it can be used with \l {wrapMode}{wrapped}
    text. The text will only elide if \c maximumLineCount, or \c height has been set.
    If both \c maximumLineCount and \c height are set, \c maximumLineCount will
    apply unless the lines do not fit in the height allowed.

    If the text is a multi-length string, and the mode is not \c Text.ElideNone,
    the first string that fits will be used, otherwise the last will be elided.

    Multi-length strings are ordered from longest to shortest, separated by the
    Unicode "String Terminator" character \c U009C (write this in QML with \c{"\u009C"} or \c{"\x9C"}).
*/
QQuickText::TextElideMode QQuickText::elideMode() const
{
    Q_D(const QQuickText);
    return d->elideMode;
}

void QQuickText::setElideMode(QQuickText::TextElideMode mode)
{
    Q_D(QQuickText);
    if (mode == d->elideMode)
        return;

    d->elideMode = mode;
    d->updateLayout();

    emit elideModeChanged(mode);
}

/*!
    \qmlproperty url QtQuick::Text::baseUrl

    This property specifies a base URL which is used to resolve relative URLs
    within the text.

    Urls are resolved to be within the same directory as the target of the base
    URL meaning any portion of the path after the last '/' will be ignored.

    \table
    \header \li Base URL \li Relative URL \li Resolved URL
    \row \li http://qt-project.org/ \li images/logo.png \li http://qt-project.org/images/logo.png
    \row \li http://qt-project.org/index.html \li images/logo.png \li http://qt-project.org/images/logo.png
    \row \li http://qt-project.org/content \li images/logo.png \li http://qt-project.org/content/images/logo.png
    \row \li http://qt-project.org/content/ \li images/logo.png \li http://qt-project.org/content/images/logo.png
    \row \li http://qt-project.org/content/index.html \li images/logo.png \li http://qt-project.org/content/images/logo.png
    \row \li http://qt-project.org/content/index.html \li ../images/logo.png \li http://qt-project.org/images/logo.png
    \row \li http://qt-project.org/content/index.html \li /images/logo.png \li http://qt-project.org/images/logo.png
    \endtable

    The default value is the url of the QML file instantiating the Text item.
*/

QUrl QQuickText::baseUrl() const
{
    Q_D(const QQuickText);
    if (d->baseUrl.isEmpty()) {
        if (QQmlContext *context = qmlContext(this))
            const_cast<QQuickTextPrivate *>(d)->baseUrl = context->baseUrl();
    }
    return d->baseUrl;
}

void QQuickText::setBaseUrl(const QUrl &url)
{
    Q_D(QQuickText);
    if (baseUrl() != url) {
        d->baseUrl = url;

        if (d->richText) {
            d->ensureDoc();
            d->extra->doc->setBaseUrl(url);
        }
        if (d->styledText) {
            d->textHasChanged = true;
            qDeleteAll(d->imgTags);
            d->imgTags.clear();
            d->updateLayout();
        }
        emit baseUrlChanged();
    }
}

void QQuickText::resetBaseUrl()
{
    if (QQmlContext *context = qmlContext(this))
        setBaseUrl(context->baseUrl());
    else
        setBaseUrl(QUrl());
}

/*! \internal */
QRectF QQuickText::boundingRect() const
{
    Q_D(const QQuickText);

    QRectF rect = d->layedOutTextRect;
    rect.moveLeft(QQuickTextUtil::alignedX(rect.width(), width(), effectiveHAlign()));
    rect.moveTop(QQuickTextUtil::alignedY(rect.height() + d->lineHeightOffset(), height(), d->vAlign));

    if (d->style != Normal)
        rect.adjust(-1, 0, 1, 2);
    // Could include font max left/right bearings to either side of rectangle.

    return rect;
}

QRectF QQuickText::clipRect() const
{
    Q_D(const QQuickText);

    QRectF rect = QQuickImplicitSizeItem::clipRect();
    if (d->style != Normal)
        rect.adjust(-1, 0, 1, 2);
    return rect;
}

/*! \internal */
void QQuickText::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickText);
    if (d->text.isEmpty()) {
        QQuickItem::geometryChanged(newGeometry, oldGeometry);
        return;
    }

    bool widthChanged = newGeometry.width() != oldGeometry.width();
    bool heightChanged = newGeometry.height() != oldGeometry.height();
    bool wrapped = d->wrapMode != QQuickText::NoWrap;
    bool elide = d->elideMode != QQuickText::ElideNone;
    bool scaleFont = d->fontSizeMode() != QQuickText::FixedSize && (widthValid() || heightValid());
    bool verticalScale = (d->fontSizeMode() & QQuickText::VerticalFit) && heightValid();

    bool widthMaximum = newGeometry.width() >= oldGeometry.width() && !d->widthExceeded;
    bool heightMaximum = newGeometry.height() >= oldGeometry.height() && !d->heightExceeded;

    if ((!widthChanged && !heightChanged) || d->internalWidthUpdate)
        goto geomChangeDone;

    if ((effectiveHAlign() != QQuickText::AlignLeft && widthChanged)
            || (vAlign() != QQuickText::AlignTop && heightChanged)) {
        // If the width has changed and we're not left aligned do an update so the text is
        // repositioned even if a full layout isn't required. And the same for vertical.
        d->updateType = QQuickTextPrivate::UpdatePaintNode;
        update();
    }

    if (!wrapped && !elide && !scaleFont)
        goto geomChangeDone; // left aligned unwrapped text without eliding never needs relayout

    if (elide // eliding and dimensions were and remain invalid;
            && ((widthValid() && oldGeometry.width() <= 0 && newGeometry.width() <= 0)
            || (heightValid() && oldGeometry.height() <= 0 && newGeometry.height() <= 0))) {
        goto geomChangeDone;
    }

    if (widthMaximum && heightMaximum && !d->isLineLaidOutConnected())  // Size is sufficient and growing.
        goto geomChangeDone;

    if (!(widthChanged || widthMaximum) && !d->isLineLaidOutConnected()) { // only height has changed
        if (newGeometry.height() > oldGeometry.height()) {
            if (!d->heightExceeded) // Height is adequate and growing.
                goto geomChangeDone;
            if (d->lineCount == d->maximumLineCount())  // Reached maximum line and height is growing.
                goto geomChangeDone;
        } else if (newGeometry.height() < oldGeometry.height()) {
            if (d->lineCount < 2 && !verticalScale && newGeometry.height() > 0)  // A single line won't be truncated until the text is 0 height.
                goto geomChangeDone;

            if (!verticalScale // no scaling, no eliding, and either unwrapped, or no maximum line count.
                    && d->elideMode != QQuickText::ElideRight
                    && !(d->maximumLineCountValid && d->widthExceeded)) {
                goto geomChangeDone;
            }
        }
    } else if (!heightChanged && widthMaximum) {
        goto geomChangeDone;
    }

    if (d->updateOnComponentComplete || d->textHasChanged) {
        // We need to re-elide
        d->updateLayout();
    } else {
        // We just need to re-layout
        d->updateSize();
    }

geomChangeDone:
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
}

void QQuickText::triggerPreprocess()
{
    Q_D(QQuickText);
    if (d->updateType == QQuickTextPrivate::UpdateNone)
        d->updateType = QQuickTextPrivate::UpdatePreprocess;
    update();
}

QSGNode *QQuickText::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    Q_UNUSED(data);
    Q_D(QQuickText);

    if (d->text.isEmpty()) {
        delete oldNode;
        return 0;
    }

    if (d->updateType != QQuickTextPrivate::UpdatePaintNode && oldNode != 0) {
        // Update done in preprocess() in the nodes
        d->updateType = QQuickTextPrivate::UpdateNone;
        return oldNode;
    }

    d->updateType = QQuickTextPrivate::UpdateNone;

    const qreal dy = QQuickTextUtil::alignedY(d->layedOutTextRect.height() + d->lineHeightOffset(), height(), d->vAlign);

    QQuickTextNode *node = 0;
    if (!oldNode)
        node = new QQuickTextNode(this);
    else
        node = static_cast<QQuickTextNode *>(oldNode);

    node->setUseNativeRenderer(d->renderType == NativeRendering);
    node->deleteContent();
    node->setMatrix(QMatrix4x4());

    const QColor color = QColor::fromRgba(d->color);
    const QColor styleColor = QColor::fromRgba(d->styleColor);
    const QColor linkColor = QColor::fromRgba(d->linkColor);

    if (d->richText) {
        const qreal dx = QQuickTextUtil::alignedX(d->layedOutTextRect.width(), width(), effectiveHAlign());
        d->ensureDoc();
        node->addTextDocument(QPointF(dx, dy), d->extra->doc, color, d->style, styleColor, linkColor);
    } else if (d->layedOutTextRect.width() > 0) {
        const qreal dx = QQuickTextUtil::alignedX(d->lineWidth, width(), effectiveHAlign());
        int unelidedLineCount = d->lineCount;
        if (d->elideLayout)
            unelidedLineCount -= 1;
        if (unelidedLineCount > 0) {
            node->addTextLayout(
                        QPointF(dx, dy),
                        &d->layout,
                        color, d->style, styleColor, linkColor,
                        QColor(), QColor(), -1, -1,
                        0, unelidedLineCount);
        }
        if (d->elideLayout)
            node->addTextLayout(QPointF(dx, dy), d->elideLayout, color, d->style, styleColor, linkColor);

        foreach (QQuickStyledTextImgTag *img, d->visibleImgTags) {
            QQuickPixmap *pix = img->pix;
            if (pix && pix->isReady())
                node->addImage(QRectF(img->pos.x() + dx, img->pos.y() + dy, pix->width(), pix->height()), pix->image());
        }
    }

    // The font caches have now been initialized on the render thread, so they have to be
    // invalidated before we can use them from the main thread again.
    invalidateFontCaches();

    return node;
}

void QQuickText::updatePolish()
{
    Q_D(QQuickText);
    if (d->polishSize) {
        d->updateSize();
        d->polishSize = false;
    }
    invalidateFontCaches();
}

/*!
    \qmlproperty real QtQuick::Text::contentWidth

    Returns the width of the text, including width past the width
    which is covered due to insufficient wrapping if WrapMode is set.
*/
qreal QQuickText::contentWidth() const
{
    Q_D(const QQuickText);
    return d->layedOutTextRect.width();
}

/*!
    \qmlproperty real QtQuick::Text::contentHeight

    Returns the height of the text, including height past the height
    which is covered due to there being more text than fits in the set height.
*/
qreal QQuickText::contentHeight() const
{
    Q_D(const QQuickText);
    return d->layedOutTextRect.height();
}

/*!
    \qmlproperty real QtQuick::Text::lineHeight

    Sets the line height for the text.
    The value can be in pixels or a multiplier depending on lineHeightMode.

    The default value is a multiplier of 1.0.
    The line height must be a positive value.
*/
qreal QQuickText::lineHeight() const
{
    Q_D(const QQuickText);
    return d->lineHeight();
}

void QQuickText::setLineHeight(qreal lineHeight)
{
    Q_D(QQuickText);

    if ((d->lineHeight() == lineHeight) || (lineHeight < 0.0))
        return;

    d->extra.value().lineHeight = lineHeight;
    d->implicitHeightValid = false;
    d->updateLayout();
    emit lineHeightChanged(lineHeight);
}

/*!
    \qmlproperty enumeration QtQuick::Text::lineHeightMode

    This property determines how the line height is specified.
    The possible values are:

    \list
    \li Text.ProportionalHeight (default) - this sets the spacing proportional to the
       line (as a multiplier). For example, set to 2 for double spacing.
    \li Text.FixedHeight - this sets the line height to a fixed line height (in pixels).
    \endlist
*/
QQuickText::LineHeightMode QQuickText::lineHeightMode() const
{
    Q_D(const QQuickText);
    return d->lineHeightMode();
}

void QQuickText::setLineHeightMode(LineHeightMode mode)
{
    Q_D(QQuickText);
    if (mode == d->lineHeightMode())
        return;

    d->implicitHeightValid = false;
    d->extra.value().lineHeightMode = mode;
    d->updateLayout();

    emit lineHeightModeChanged(mode);
}

/*!
    \qmlproperty enumeration QtQuick::Text::fontSizeMode

    This property specifies how the font size of the displayed text is determined.
    The possible values are:

    \list
    \li Text.FixedSize (default) - The size specified by \l font.pixelSize
    or \l font.pointSize is used.
    \li Text.HorizontalFit - The largest size up to the size specified that fits
    within the width of the item without wrapping is used.
    \li Text.VerticalFit - The largest size up to the size specified that fits
    the height of the item is used.
    \li Text.Fit - The largest size up to the size specified that fits within the
    width and height of the item is used.
    \endlist

    The font size of fitted text has a minimum bound specified by the
    minimumPointSize or minimumPixelSize property and maximum bound specified
    by either the \l font.pointSize or \l font.pixelSize properties.

    \qml
    Text { text: "Hello"; fontSizeMode: Text.Fit; minimumPixelSize: 10; font.pixelSize: 72 }
    \endqml

    If the text does not fit within the item bounds with the minimum font size
    the text will be elided as per the \l elide property.
*/

QQuickText::FontSizeMode QQuickText::fontSizeMode() const
{
    Q_D(const QQuickText);
    return d->fontSizeMode();
}

void QQuickText::setFontSizeMode(FontSizeMode mode)
{
    Q_D(QQuickText);
    if (d->fontSizeMode() == mode)
        return;

    d->polishSize = true;
    polish();

    d->extra.value().fontSizeMode = mode;
    emit fontSizeModeChanged();
}

/*!
    \qmlproperty int QtQuick::Text::minimumPixelSize

    This property specifies the minimum font pixel size of text scaled by the
    fontSizeMode property.

    If the fontSizeMode is Text.FixedSize or the \l font.pixelSize is -1 this
    property is ignored.
*/

int QQuickText::minimumPixelSize() const
{
    Q_D(const QQuickText);
    return d->minimumPixelSize();
}

void QQuickText::setMinimumPixelSize(int size)
{
    Q_D(QQuickText);
    if (d->minimumPixelSize() == size)
        return;

    if (d->fontSizeMode() != FixedSize && (widthValid() || heightValid())) {
        d->polishSize = true;
        polish();
    }
    d->extra.value().minimumPixelSize = size;
    emit minimumPixelSizeChanged();
}

/*!
    \qmlproperty int QtQuick::Text::minimumPointSize

    This property specifies the minimum font point \l size of text scaled by
    the fontSizeMode property.

    If the fontSizeMode is Text.FixedSize or the \l font.pointSize is -1 this
    property is ignored.
*/

int QQuickText::minimumPointSize() const
{
    Q_D(const QQuickText);
    return d->minimumPointSize();
}

void QQuickText::setMinimumPointSize(int size)
{
    Q_D(QQuickText);
    if (d->minimumPointSize() == size)
        return;

    if (d->fontSizeMode() != FixedSize && (widthValid() || heightValid())) {
        d->polishSize = true;
        polish();
    }
    d->extra.value().minimumPointSize = size;
    emit minimumPointSizeChanged();
}

/*!
    Returns the number of resources (images) that are being loaded asynchronously.
*/
int QQuickText::resourcesLoading() const
{
    Q_D(const QQuickText);
    if (d->richText && d->extra.isAllocated() && d->extra->doc)
        return d->extra->doc->resourcesLoading();
    return 0;
}

/*! \internal */
void QQuickText::componentComplete()
{
    Q_D(QQuickText);
    if (d->updateOnComponentComplete) {
        if (d->richText) {
            d->ensureDoc();
            d->extra->doc->setText(d->text);
            d->rightToLeftText = d->extra->doc->toPlainText().isRightToLeft();
        } else {
            d->rightToLeftText = d->text.isRightToLeft();
        }
        d->determineHorizontalAlignment();
    }
    QQuickItem::componentComplete();
    if (d->updateOnComponentComplete)
        d->updateLayout();
}

QString QQuickTextPrivate::anchorAt(const QTextLayout *layout, const QPointF &mousePos)
{
    for (int i = 0; i < layout->lineCount(); ++i) {
        QTextLine line = layout->lineAt(i);
        if (line.naturalTextRect().contains(mousePos)) {
            int charPos = line.xToCursor(mousePos.x(), QTextLine::CursorOnCharacter);
            foreach (const QTextLayout::FormatRange &formatRange, layout->additionalFormats()) {
                if (formatRange.format.isAnchor()
                        && charPos >= formatRange.start
                        && charPos < formatRange.start + formatRange.length) {
                    return formatRange.format.anchorHref();
                }
            }
            break;
        }
    }
    return QString();
}

QString QQuickTextPrivate::anchorAt(const QPointF &mousePos) const
{
    Q_Q(const QQuickText);
    QPointF translatedMousePos = mousePos;
    translatedMousePos.ry() -= QQuickTextUtil::alignedY(layedOutTextRect.height() + lineHeightOffset(), q->height(), vAlign);
    if (styledText) {
        QString link = anchorAt(&layout, translatedMousePos);
        if (link.isEmpty() && elideLayout)
            link = anchorAt(elideLayout, translatedMousePos);
        return link;
    } else if (richText && extra.isAllocated() && extra->doc) {
        translatedMousePos.rx() -= QQuickTextUtil::alignedX(layedOutTextRect.width(), q->width(), q->effectiveHAlign());
        return extra->doc->documentLayout()->anchorAt(translatedMousePos);
    }
    return QString();
}

bool QQuickTextPrivate::isLinkActivatedConnected()
{
    Q_Q(QQuickText);
    IS_SIGNAL_CONNECTED(q, QQuickText, linkActivated, (const QString &));
}

/*!  \internal */
void QQuickText::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickText);

    QString link;
    if (d->isLinkActivatedConnected())
        link = d->anchorAt(event->localPos());

    if (link.isEmpty()) {
        event->setAccepted(false);
    } else {
        d->extra.value().activeLink = link;
    }

    // ### may malfunction if two of the same links are clicked & dragged onto each other)

    if (!event->isAccepted())
        QQuickItem::mousePressEvent(event);
}


/*! \internal */
void QQuickText::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickText);

    // ### confirm the link, and send a signal out

    QString link;
    if (d->isLinkActivatedConnected())
        link = d->anchorAt(event->localPos());

    if (!link.isEmpty() && d->extra.isAllocated() && d->extra->activeLink == link)
        emit linkActivated(d->extra->activeLink);
    else
        event->setAccepted(false);

    if (!event->isAccepted())
        QQuickItem::mouseReleaseEvent(event);
}

bool QQuickTextPrivate::isLinkHoveredConnected()
{
    Q_Q(QQuickText);
    IS_SIGNAL_CONNECTED(q, QQuickText, linkHovered, (const QString &));
}

/*!
    \qmlsignal QtQuick::Text::linkHovered(string link)
    \since 5.2

    This signal is emitted when the user hovers a link embedded in the
    text. The link must be in rich text or HTML format and the \a link
    string provides access to the particular link.

    The corresponding handler is \c onLinkHovered.

    \sa hoveredLink, linkAt()
*/

/*!
    \qmlproperty string QtQuick::Text::hoveredLink
    \since 5.2

    This property contains the link string when the user hovers a link
    embedded in the text. The link must be in rich text or HTML format
    and the \a hoveredLink string provides access to the particular link.

    \sa linkHovered, linkAt()
*/

QString QQuickText::hoveredLink() const
{
    Q_D(const QQuickText);
    if (const_cast<QQuickTextPrivate *>(d)->isLinkHoveredConnected()) {
        if (d->extra.isAllocated())
            return d->extra->hoveredLink;
    } else {
#ifndef QT_NO_CURSOR
        if (QQuickWindow *wnd = window()) {
            QPointF pos = QCursor::pos(wnd->screen()) - wnd->position() - mapToScene(QPointF(0, 0));
            return d->anchorAt(pos);
        }
#endif // QT_NO_CURSOR
    }
    return QString();
}

void QQuickTextPrivate::processHoverEvent(QHoverEvent *event)
{
    Q_Q(QQuickText);
    QString link;
    if (isLinkHoveredConnected()) {
        if (event->type() != QEvent::HoverLeave)
            link = anchorAt(event->posF());

        if ((!extra.isAllocated() && !link.isEmpty()) || (extra.isAllocated() && extra->hoveredLink != link)) {
            extra.value().hoveredLink = link;
            emit q->linkHovered(extra->hoveredLink);
        }
    }
    event->setAccepted(!link.isEmpty());
}

void QQuickText::hoverEnterEvent(QHoverEvent *event)
{
    Q_D(QQuickText);
    d->processHoverEvent(event);
}

void QQuickText::hoverMoveEvent(QHoverEvent *event)
{
    Q_D(QQuickText);
    d->processHoverEvent(event);
}

void QQuickText::hoverLeaveEvent(QHoverEvent *event)
{
    Q_D(QQuickText);
    d->processHoverEvent(event);
}

/*!
    \qmlproperty enumeration QtQuick::Text::renderType

    Override the default rendering type for this component.

    Supported render types are:
    \list
    \li Text.QtRendering - the default
    \li Text.NativeRendering
    \endlist

    Select Text.NativeRendering if you prefer text to look native on the target platform and do
    not require advanced features such as transformation of the text. Using such features in
    combination with the NativeRendering render type will lend poor and sometimes pixelated
    results.
*/
QQuickText::RenderType QQuickText::renderType() const
{
    Q_D(const QQuickText);
    return d->renderType;
}

void QQuickText::setRenderType(QQuickText::RenderType renderType)
{
    Q_D(QQuickText);
    if (d->renderType == renderType)
        return;

    d->renderType = renderType;
    emit renderTypeChanged();

    if (isComponentComplete())
        d->updateLayout();
}

/*!
    \qmlmethod QtQuick::Text::doLayout()

    Triggers a re-layout of the displayed text.
*/
void QQuickText::doLayout()
{
    Q_D(QQuickText);
    d->updateSize();
}

/*!
    \qmlmethod QtQuick::Text::linkAt(real x, real y)
    \since 5.3

    Returns the link string at point \a x, \a y in content coordinates,
    or an empty string if no link exists at that point.

    \sa hoveredLink
*/
QString QQuickText::linkAt(qreal x, qreal y) const
{
    Q_D(const QQuickText);
    return d->anchorAt(QPointF(x, y));
}

/*!
 * \internal
 *
 * Invalidates font caches owned by the text objects owned by the element
 * to work around the fact that text objects cannot be used from multiple threads.
 */
void QQuickText::invalidateFontCaches()
{
    Q_D(QQuickText);

    if (d->richText && d->extra.isAllocated() && d->extra->doc != 0) {
        QTextBlock block;
        for (block = d->extra->doc->firstBlock(); block.isValid(); block = block.next()) {
            if (block.layout() != 0 && block.layout()->engine() != 0)
                block.layout()->engine()->resetFontEngineCache();
        }
    } else {
        if (d->layout.engine() != 0)
            d->layout.engine()->resetFontEngineCache();
    }
}

QT_END_NAMESPACE
