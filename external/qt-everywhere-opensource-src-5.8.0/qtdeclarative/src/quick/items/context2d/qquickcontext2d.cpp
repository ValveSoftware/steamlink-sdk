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

#include "qquickcontext2d_p.h"
#include "qquickcontext2dcommandbuffer_p.h"
#include "qquickcanvasitem_p.h"
#include <private/qquickcontext2dtexture_p.h>
#include <private/qquickitem_p.h>
#include <QtQuick/private/qquickshadereffectsource_p.h>
#include <qsgrendererinterface.h>

#include <QtQuick/private/qsgcontext_p.h>
#include <private/qquicksvgparser_p.h>
#include <private/qquickpath_p.h>

#include <private/qquickimage_p_p.h>

#include <qqmlinfo.h>
#include <private/qv8engine_p.h>

#include <qqmlengine.h>
#include <private/qv4domerrors_p.h>
#include <private/qv4engine_p.h>
#include <private/qv4object_p.h>
#include <private/qv4qobjectwrapper_p.h>
#include <private/qquickwindow_p.h>

#include <private/qv4value_p.h>
#include <private/qv4functionobject_p.h>
#include <private/qv4objectproto_p.h>
#include <private/qv4scopedvalue_p.h>

#include <QtCore/qmath.h>
#include <QtCore/qvector.h>
#include <QtCore/private/qnumeric_p.h>
#include <QtCore/QRunnable>
#include <QtGui/qguiapplication.h>
#include <QtGui/qopenglframebufferobject.h>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>

#ifndef QT_NO_OPENGL
# include <private/qsgdefaultrendercontext_p.h>
#endif

#include <cmath>
#if defined(Q_OS_QNX) || defined(Q_OS_ANDROID)
#include <ctype.h>
#endif

QT_BEGIN_NAMESPACE
/*!
    \qmltype Context2D
    \instantiates QQuickContext2D
    \inqmlmodule QtQuick
    \ingroup qtquick-canvas
    \since 5.0
    \brief Provides 2D context for shapes on a Canvas item

    The Context2D object can be created by \c Canvas item's \c getContext()
    method:
    \code
    Canvas {
      id:canvas
      onPaint:{
         var ctx = canvas.getContext('2d');
         //...
      }
    }
    \endcode
    The Context2D API implements the same \l
    {http://www.w3.org/TR/2dcontext}{W3C Canvas 2D Context API standard} with
    some enhanced features.

    The Context2D API provides the rendering \b{context} which defines the
    methods and attributes needed to draw on the \c Canvas item. The following
    assigns the canvas rendering context to a \c{context} variable:
    \code
    var context = mycanvas.getContext("2d")
    \endcode

    The Context2D API renders the canvas as a coordinate system whose origin
    (0,0) is at the top left corner, as shown in the figure below. Coordinates
    increase along the \c{x} axis from left to right and along the \c{y} axis
    from top to bottom of the canvas.
    \image qml-item-canvas-context.gif
*/



Q_CORE_EXPORT double qstrtod(const char *s00, char const **se, bool *ok);

#define DEGREES(t) ((t) * 180.0 / M_PI)

#define CHECK_CONTEXT(r)     if (!r || !r->d()->context || !r->d()->context->bufferValid()) \
                                V4THROW_ERROR("Not a Context2D object");

#define CHECK_CONTEXT_SETTER(r)     if (!r || !r->d()->context || !r->d()->context->bufferValid()) \
                                       V4THROW_ERROR("Not a Context2D object");
#define qClamp(val, min, max) qMin(qMax(val, min), max)
#define CHECK_RGBA(c) (c == '-' || c == '.' || (c >=0 && c <= 9))
QColor qt_color_from_string(const QV4::Value &name)
{
    QByteArray str = name.toQString().toUtf8();

    char *p = str.data();
    int len = str.length();
    //rgb/hsl color string has at least 7 characters
    if (!p || len > 255 || len <= 7)
        return QColor(p);
    else {
        bool isRgb(false), isHsl(false), hasAlpha(false);
        Q_UNUSED(isHsl)

        while (isspace(*p)) p++;
        if (strncmp(p, "rgb", 3) == 0)
            isRgb = true;
        else if (strncmp(p, "hsl", 3) == 0)
            isHsl = true;
        else
            return QColor(p);

        p+=3; //skip "rgb" or "hsl"
        hasAlpha = (*p == 'a') ? true : false;

        ++p; //skip "("

        if (hasAlpha) ++p; //skip "a"

        int rh, gs, bl, alpha = 255;

        //red
        while (isspace(*p)) p++;
        rh = strtol(p, &p, 10);
        if (*p == '%') {
            rh = qRound(rh/100.0 * 255);
            ++p;
        }
        if (*p++ != ',') return QColor();

        //green
        while (isspace(*p)) p++;
        gs = strtol(p, &p, 10);
        if (*p == '%') {
            gs = qRound(gs/100.0 * 255);
            ++p;
        }
        if (*p++ != ',') return QColor();

        //blue
        while (isspace(*p)) p++;
        bl = strtol(p, &p, 10);
        if (*p == '%') {
            bl = qRound(bl/100.0 * 255);
            ++p;
        }

        if (hasAlpha) {
            if (*p++!= ',') return QColor();
            while (isspace(*p)) p++;
            bool ok = false;
            alpha = qRound(qstrtod(p, const_cast<const char **>(&p), &ok) * 255);
        }

        if (*p != ')') return QColor();
        if (isRgb)
            return QColor::fromRgba(qRgba(qClamp(rh, 0, 255), qClamp(gs, 0, 255), qClamp(bl, 0, 255), qClamp(alpha, 0, 255)));
        else if (isHsl)
            return QColor::fromHsl(qClamp(rh, 0, 359), qClamp(gs, 0, 255), qClamp(bl, 0, 255), qClamp(alpha, 0, 255));
    }
    return QColor();
}

static int qParseFontSizeFromToken(const QStringRef &fontSizeToken, bool &ok)
{
    ok = false;
    float size = fontSizeToken.trimmed().toFloat(&ok);
    if (ok) {
        return int(size);
    }
    qWarning().nospace() << "Context2D: A font size of " << fontSizeToken << " is invalid.";
    return 0;
}

/*
    Attempts to set the font size of \a font to \a fontSizeToken, returning
    \c true if successful. If the font size is invalid, \c false is returned
    and a warning is printed.
*/
static bool qSetFontSizeFromToken(QFont &font, const QStringRef &fontSizeToken)
{
    const QStringRef trimmedToken = fontSizeToken.trimmed();
    const QStringRef unitStr = trimmedToken.right(2);
    const QStringRef value = trimmedToken.left(trimmedToken.size() - 2);
    bool ok = false;
    int size = 0;
    if (unitStr == QLatin1String("px")) {
        size = qParseFontSizeFromToken(value, ok);
        if (ok) {
            font.setPixelSize(size);
            return true;
        }
    } else if (unitStr == QLatin1String("pt")) {
        size = qParseFontSizeFromToken(value, ok);
        if (ok) {
            font.setPointSize(size);
            return true;
        }
    } else {
        qWarning().nospace() << "Context2D: Invalid font size unit in font string.";
    }
    return false;
}

/*
    Returns a list of all of the families in \a fontFamiliesString, where
    each family is separated by spaces. Families with spaces in their name
    must be quoted.
*/
static QStringList qExtractFontFamiliesFromString(const QStringRef &fontFamiliesString)
{
    QStringList extractedFamilies;
    int quoteIndex = -1;
    QString currentFamily;
    for (int index = 0; index < fontFamiliesString.size(); ++index) {
        const QChar ch = fontFamiliesString.at(index);
        if (ch == '"' || ch == '\'') {
            if (quoteIndex == -1) {
                quoteIndex = index;
            } else {
                if (ch == fontFamiliesString.at(quoteIndex)) {
                    // Found the matching quote. +1/-1 because we don't want the quote as part of the name.
                    const QString family = fontFamiliesString.mid(quoteIndex + 1, index - quoteIndex - 1).toString();
                    extractedFamilies.push_back(family);
                    currentFamily.clear();
                    quoteIndex = -1;
                } else {
                    qWarning().nospace() << "Context2D: Mismatched quote in font string.";
                    return QStringList();
                }
            }
        } else if (ch == ' ' && quoteIndex == -1) {
            // This is a space that's not within quotes...
            if (!currentFamily.isEmpty()) {
                // and there is a current family; consider it the end of the current family.
                extractedFamilies.push_back(currentFamily);
                currentFamily.clear();
            } // else: ignore the space
        } else {
            currentFamily.push_back(ch);
        }
    }
    if (!currentFamily.isEmpty()) {
        if (quoteIndex == -1) {
            // This is the end of the string, so add this family to our list.
            extractedFamilies.push_back(currentFamily);
        } else {
            qWarning().nospace() << "Context2D: Unclosed quote in font string.";
            return QStringList();
        }
    }
    if (extractedFamilies.isEmpty()) {
        qWarning().nospace() << "Context2D: Missing or misplaced font family in font string"
            << " (it must come after the font size).";
    }
    return extractedFamilies;
}

/*!
    Tries to set a family on \a font using the families provided in \a fontFamilyTokens.

    The list is ordered by preference, with the first family having the highest preference.
    If the first family is invalid, the next family in the list is evaluated.
    This process is repeated until a valid font is found (at which point the function
    will return \c true and the family set on \a font) or there are no more
    families left, at which point a warning is printed and \c false is returned.
*/
static bool qSetFontFamilyFromTokens(QFont &font, const QStringList &fontFamilyTokens)
{
    for (const QString &fontFamilyToken : fontFamilyTokens) {
        QFontDatabase fontDatabase;
        if (fontDatabase.hasFamily(fontFamilyToken)) {
            font.setFamily(fontFamilyToken);
            return true;
        } else {
            // Can't find a family matching this name; if it's a generic family,
            // try searching for the default family for it by using style hints.
            int styleHint = -1;
            if (fontFamilyToken.compare(QLatin1String("serif")) == 0) {
                styleHint = QFont::Serif;
            } else if (fontFamilyToken.compare(QLatin1String("sans-serif")) == 0) {
                styleHint = QFont::SansSerif;
            } else if (fontFamilyToken.compare(QLatin1String("cursive")) == 0) {
                styleHint = QFont::Cursive;
            } else if (fontFamilyToken.compare(QLatin1String("monospace")) == 0) {
                styleHint = QFont::Monospace;
            } else if (fontFamilyToken.compare(QLatin1String("fantasy")) == 0) {
                styleHint = QFont::Fantasy;
            }
            if (styleHint != -1) {
                QFont tmp;
                tmp.setStyleHint(static_cast<QFont::StyleHint>(styleHint));
                font.setFamily(tmp.defaultFamily());
                return true;
            }
        }
    }
    qWarning("Context2D: The font families specified are invalid: %s", qPrintable(fontFamilyTokens.join(QString()).trimmed()));
    return false;
}

enum FontToken
{
    NoTokens = 0x00,
    FontStyle = 0x01,
    FontVariant = 0x02,
    FontWeight = 0x04
};

#define Q_TRY_SET_TOKEN(token, value, setStatement) \
if (!(usedTokens & token)) { \
    usedTokens |= token; \
    setStatement; \
} else { \
    qWarning().nospace() << "Context2D: Duplicate token " << QLatin1String(value) << " found in font string."; \
    return currentFont; \
}

/*!
    Parses a font string based on the CSS shorthand font property.

    See: http://www.w3.org/TR/css3-fonts/#font-prop
*/
static QFont qt_font_from_string(const QString& fontString, const QFont &currentFont) {
    if (fontString.isEmpty()) {
        qWarning().nospace() << "Context2D: Font string is empty.";
        return currentFont;
    }

    // We know that font-size must be specified and it must be before font-family
    // (which could potentially have "px" or "pt" in its name), so extract it now.
    int fontSizeEnd = fontString.indexOf(QLatin1String("px"));
    if (fontSizeEnd == -1)
        fontSizeEnd = fontString.indexOf(QLatin1String("pt"));
    if (fontSizeEnd == -1) {
        qWarning().nospace() << "Context2D: Invalid font size unit in font string.";
        return currentFont;
    }

    int fontSizeStart = fontString.lastIndexOf(' ', fontSizeEnd);
    if (fontSizeStart == -1) {
        // The font size might be the first token in the font string, which is OK.
        // Regardless, we'll find out if the font is invalid with qSetFontSizeFromToken().
        fontSizeStart = 0;
    } else {
        // Don't want to take the leading space.
        ++fontSizeStart;
    }

    // + 2 for the unit, +1 for the space that we require.
    fontSizeEnd += 3;

    QFont newFont;
    if (!qSetFontSizeFromToken(newFont, fontString.midRef(fontSizeStart, fontSizeEnd - fontSizeStart)))
        return currentFont;

    // We don't want to parse the size twice, so remove it now.
    QString remainingFontString = fontString;
    remainingFontString.remove(fontSizeStart, fontSizeEnd - fontSizeStart);
    QStringRef remainingFontStringRef(&remainingFontString);

    // Next, we have to take any font families out, as QString::split() will ruin quoted family names.
    const QStringRef fontFamiliesString = remainingFontStringRef.mid(fontSizeStart);
    remainingFontStringRef.truncate(fontSizeStart);
    QStringList fontFamilies = qExtractFontFamiliesFromString(fontFamiliesString);
    if (fontFamilies.isEmpty()) {
        return currentFont;
    }
    if (!qSetFontFamilyFromTokens(newFont, fontFamilies))
        return currentFont;

    // Now that we've removed the messy parts, we can split the font string on spaces.
    const QStringRef trimmedTokensStr = remainingFontStringRef.trimmed();
    if (trimmedTokensStr.isEmpty()) {
        // No optional properties.
        return newFont;
    }
    const auto tokens = trimmedTokensStr.split(QLatin1Char(' '));

    int usedTokens = NoTokens;
    // Optional properties can be in any order, but font-size and font-family must be last.
    for (const QStringRef &token : tokens) {
        if (token.compare(QLatin1String("normal")) == 0) {
            if (!(usedTokens & FontStyle) || !(usedTokens & FontVariant) || !(usedTokens & FontWeight)) {
                // Could be font-style, font-variant or font-weight.
                if (!(usedTokens & FontStyle)) {
                    // QFont::StyleNormal is the default for QFont::style.
                    usedTokens = usedTokens | FontStyle;
                } else if (!(usedTokens & FontVariant)) {
                    // QFont::MixedCase is the default for QFont::capitalization.
                    usedTokens |= FontVariant;
                } else if (!(usedTokens & FontWeight)) {
                    // QFont::Normal is the default for QFont::weight.
                    usedTokens |= FontWeight;
                }
            } else {
                qWarning().nospace() << "Context2D: Duplicate token \"normal\" found in font string.";
                return currentFont;
            }
        } else if (token.compare(QLatin1String("bold")) == 0) {
            Q_TRY_SET_TOKEN(FontWeight, "bold", newFont.setBold(true))
        } else if (token.compare(QLatin1String("italic")) == 0) {
            Q_TRY_SET_TOKEN(FontStyle, "italic", newFont.setStyle(QFont::StyleItalic))
        } else if (token.compare(QLatin1String("oblique")) == 0) {
            Q_TRY_SET_TOKEN(FontStyle, "oblique", newFont.setStyle(QFont::StyleOblique))
        } else if (token.compare(QLatin1String("small-caps")) == 0) {
            Q_TRY_SET_TOKEN(FontVariant, "small-caps", newFont.setCapitalization(QFont::SmallCaps))
        } else {
            bool conversionOk = false;
            int weight = token.toInt(&conversionOk);
            if (conversionOk) {
                if (weight >= 0 && weight <= 99) {
                    Q_TRY_SET_TOKEN(FontWeight, "<font-weight>", newFont.setWeight(weight))
                    continue;
                } else {
                    qWarning().nospace() << "Context2D: Invalid font weight " << weight << " found in font string; "
                        << "must be between 0 and 99, inclusive.";
                    return currentFont;
                }
            }
            // The token is invalid or in the wrong place/order in the font string.
            qWarning().nospace() << "Context2D: Invalid or misplaced token " << token << " found in font string.";
            return currentFont;
        }
    }
    return newFont;
}

class QQuickContext2DEngineData : public QV8Engine::Deletable
{
public:
    QQuickContext2DEngineData(QV4::ExecutionEngine *engine);
    ~QQuickContext2DEngineData();

    QV4::PersistentValue contextPrototype;
    QV4::PersistentValue gradientProto;
    QV4::PersistentValue pixelArrayProto;
};

V4_DEFINE_EXTENSION(QQuickContext2DEngineData, engineData)

namespace QV4 {
namespace Heap {

struct QQuickJSContext2D : Object {
    void init() { Object::init(); }
    QQuickContext2D* context;
};

struct QQuickJSContext2DPrototype : Object {
    void init() { Object::init(); }
};

struct QQuickContext2DStyle : Object {
    void init()
    {
        brush = new QBrush;
        patternRepeatX = false;
        patternRepeatY = false;
    }
    void destroy() {
        delete brush;
        Object::destroy();
    }

    QBrush *brush;
    bool patternRepeatX:1;
    bool patternRepeatY:1;
};

struct QQuickJSContext2DPixelData : Object {
    void init();
    void destroy() {
        delete image;
        Object::destroy();
    }

    QImage *image;
};

struct QQuickJSContext2DImageData : Object {
    void init();

    QV4::Value pixelData;
};

}
}

struct QQuickJSContext2D : public QV4::Object
{
    V4_OBJECT2(QQuickJSContext2D, QV4::Object)

    static QV4::ReturnedValue method_get_globalAlpha(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_set_globalAlpha(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_globalCompositeOperation(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_set_globalCompositeOperation(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_fillStyle(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_set_fillStyle(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_fillRule(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_set_fillRule(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_strokeStyle(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_set_strokeStyle(QV4::CallContext *ctx);

    static QV4::ReturnedValue method_get_lineCap(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_set_lineCap(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_lineJoin(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_set_lineJoin(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_lineWidth(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_set_lineWidth(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_miterLimit(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_set_miterLimit(QV4::CallContext *ctx);

    static QV4::ReturnedValue method_get_shadowBlur(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_set_shadowBlur(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_shadowColor(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_set_shadowColor(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_shadowOffsetX(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_set_shadowOffsetX(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_shadowOffsetY(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_set_shadowOffsetY(QV4::CallContext *ctx);

    // should these two be on the proto?
    static QV4::ReturnedValue method_get_path(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_set_path(QV4::CallContext *ctx);

    static QV4::ReturnedValue method_get_font(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_set_font(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_textAlign(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_set_textAlign(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_textBaseline(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_set_textBaseline(QV4::CallContext *ctx);
};

DEFINE_OBJECT_VTABLE(QQuickJSContext2D);


struct QQuickJSContext2DPrototype : public QV4::Object
{
    V4_OBJECT2(QQuickJSContext2DPrototype, QV4::Object)
public:
    static QV4::Heap::QQuickJSContext2DPrototype *create(QV4::ExecutionEngine *engine)
    {
        QV4::Scope scope(engine);
        QV4::Scoped<QQuickJSContext2DPrototype> o(scope, engine->memoryManager->allocObject<QQuickJSContext2DPrototype>());

        o->defineDefaultProperty(QStringLiteral("quadraticCurveTo"), method_quadraticCurveTo, 0);
        o->defineDefaultProperty(QStringLiteral("restore"), method_restore, 0);
        o->defineDefaultProperty(QStringLiteral("moveTo"), method_moveTo, 0);
        o->defineDefaultProperty(QStringLiteral("lineTo"), method_lineTo, 0);
        o->defineDefaultProperty(QStringLiteral("caretBlinkRate"), method_caretBlinkRate, 0);
        o->defineDefaultProperty(QStringLiteral("clip"), method_clip, 0);
        o->defineDefaultProperty(QStringLiteral("setTransform"), method_setTransform, 0);
        o->defineDefaultProperty(QStringLiteral("text"), method_text, 0);
        o->defineDefaultProperty(QStringLiteral("roundedRect"), method_roundedRect, 0);
        o->defineDefaultProperty(QStringLiteral("createPattern"), method_createPattern, 0);
        o->defineDefaultProperty(QStringLiteral("stroke"), method_stroke, 0);
        o->defineDefaultProperty(QStringLiteral("arc"), method_arc, 0);
        o->defineDefaultProperty(QStringLiteral("createImageData"), method_createImageData, 0);
        o->defineDefaultProperty(QStringLiteral("measureText"), method_measureText, 0);
        o->defineDefaultProperty(QStringLiteral("ellipse"), method_ellipse, 0);
        o->defineDefaultProperty(QStringLiteral("fill"), method_fill, 0);
        o->defineDefaultProperty(QStringLiteral("save"), method_save, 0);
        o->defineDefaultProperty(QStringLiteral("scale"), method_scale, 0);
        o->defineDefaultProperty(QStringLiteral("drawImage"), method_drawImage, 0);
        o->defineDefaultProperty(QStringLiteral("transform"), method_transform, 0);
        o->defineDefaultProperty(QStringLiteral("fillText"), method_fillText, 0);
        o->defineDefaultProperty(QStringLiteral("strokeText"), method_strokeText, 0);
        o->defineDefaultProperty(QStringLiteral("translate"), method_translate, 0);
        o->defineDefaultProperty(QStringLiteral("createRadialGradient"), method_createRadialGradient, 0);
        o->defineDefaultProperty(QStringLiteral("shear"), method_shear, 0);
        o->defineDefaultProperty(QStringLiteral("isPointInPath"), method_isPointInPath, 0);
        o->defineDefaultProperty(QStringLiteral("bezierCurveTo"), method_bezierCurveTo, 0);
        o->defineDefaultProperty(QStringLiteral("resetTransform"), method_resetTransform, 0);
        o->defineDefaultProperty(QStringLiteral("arcTo"), method_arcTo, 0);
        o->defineDefaultProperty(QStringLiteral("fillRect"), method_fillRect, 0);
        o->defineDefaultProperty(QStringLiteral("createConicalGradient"), method_createConicalGradient, 0);
        o->defineDefaultProperty(QStringLiteral("drawFocusRing"), method_drawFocusRing, 0);
        o->defineDefaultProperty(QStringLiteral("beginPath"), method_beginPath, 0);
        o->defineDefaultProperty(QStringLiteral("clearRect"), method_clearRect, 0);
        o->defineDefaultProperty(QStringLiteral("rect"), method_rect, 0);
        o->defineDefaultProperty(QStringLiteral("reset"), method_reset, 0);
        o->defineDefaultProperty(QStringLiteral("rotate"), method_rotate, 0);
        o->defineDefaultProperty(QStringLiteral("setCaretSelectionRect"), method_setCaretSelectionRect, 0);
        o->defineDefaultProperty(QStringLiteral("putImageData"), method_putImageData, 0);
        o->defineDefaultProperty(QStringLiteral("getImageData"), method_getImageData, 0);
        o->defineDefaultProperty(QStringLiteral("createLinearGradient"), method_createLinearGradient, 0);
        o->defineDefaultProperty(QStringLiteral("strokeRect"), method_strokeRect, 0);
        o->defineDefaultProperty(QStringLiteral("closePath"), method_closePath, 0);
        o->defineAccessorProperty(QStringLiteral("canvas"), QQuickJSContext2DPrototype::method_get_canvas, 0);

        return o->d();
    }

    static QV4::ReturnedValue method_get_canvas(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_restore(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_reset(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_save(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_rotate(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_scale(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_translate(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_setTransform(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_transform(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_resetTransform(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_shear(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_createLinearGradient(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_createRadialGradient(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_createConicalGradient(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_createPattern(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_clearRect(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_fillRect(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_strokeRect(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_arc(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_arcTo(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_beginPath(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_bezierCurveTo(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_clip(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_closePath(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_fill(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_lineTo(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_moveTo(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_quadraticCurveTo(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_rect(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_roundedRect(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_ellipse(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_text(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_stroke(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_isPointInPath(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_drawFocusRing(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_setCaretSelectionRect(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_caretBlinkRate(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_fillText(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_strokeText(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_measureText(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_drawImage(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_createImageData(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_getImageData(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_putImageData(QV4::CallContext *ctx);

};

DEFINE_OBJECT_VTABLE(QQuickJSContext2DPrototype);


struct QQuickContext2DStyle : public QV4::Object
{
    V4_OBJECT2(QQuickContext2DStyle, QV4::Object)
    V4_NEEDS_DESTROY

    static QV4::ReturnedValue gradient_proto_addColorStop(QV4::CallContext *ctx);
};



DEFINE_OBJECT_VTABLE(QQuickContext2DStyle);

QImage qt_image_convolute_filter(const QImage& src, const QVector<qreal>& weights, int radius = 0)
{
    // weights 3x3 => delta 1
    int delta = radius ? radius : qFloor(qSqrt(weights.size()) / qreal(2));
    int filterDim = 2 * delta + 1;

    QImage dst = QImage(src.size(), src.format());

    int w = src.width();
    int h = src.height();

    const QRgb *sr = (const QRgb *)(src.constBits());
    int srcStride = src.bytesPerLine() / 4;

    QRgb *dr = (QRgb*)dst.bits();
    int dstStride = dst.bytesPerLine() / 4;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int red = 0;
            int green = 0;
            int blue = 0;
            int alpha = 0;

            qreal redF = 0;
            qreal greenF = 0;
            qreal blueF = 0;
            qreal alphaF = 0;

            int sy = y;
            int sx = x;

            for (int cy = 0; cy < filterDim; ++cy) {
                int scy = sy + cy - delta;

                if (scy < 0 || scy >= h)
                    continue;

                const QRgb *sry = sr + scy * srcStride;

                for (int cx = 0; cx < filterDim; ++cx) {
                    int scx = sx + cx - delta;

                    if (scx < 0 || scx >= w)
                        continue;

                    const QRgb col = sry[scx];

                    if (radius) {
                        red += qRed(col);
                        green += qGreen(col);
                        blue += qBlue(col);
                        alpha += qAlpha(col);
                    } else {
                        qreal wt = weights[cy * filterDim + cx];

                        redF += qRed(col) * wt;
                        greenF += qGreen(col) * wt;
                        blueF += qBlue(col) * wt;
                        alphaF += qAlpha(col) * wt;
                    }
                }
            }

            if (radius)
                dr[x] = qRgba(qRound(red * weights[0]), qRound(green * weights[0]), qRound(blue * weights[0]), qRound(alpha * weights[0]));
            else
                dr[x] = qRgba(qRound(redF), qRound(greenF), qRound(blueF), qRound(alphaF));
        }

        dr += dstStride;
    }

    return dst;
}

void qt_image_boxblur(QImage& image, int radius, bool quality)
{
    int passes = quality? 3: 1;
    int filterSize = 2 * radius + 1;
    for (int i = 0; i < passes; ++i)
        image = qt_image_convolute_filter(image, QVector<qreal>() << 1.0 / (filterSize * filterSize), radius);
}

static QPainter::CompositionMode qt_composite_mode_from_string(const QString &compositeOperator)
{
    if (compositeOperator == QLatin1String("source-over")) {
        return QPainter::CompositionMode_SourceOver;
    } else if (compositeOperator == QLatin1String("source-out")) {
        return QPainter::CompositionMode_SourceOut;
    } else if (compositeOperator == QLatin1String("source-in")) {
        return QPainter::CompositionMode_SourceIn;
    } else if (compositeOperator == QLatin1String("source-atop")) {
        return QPainter::CompositionMode_SourceAtop;
    } else if (compositeOperator == QLatin1String("destination-atop")) {
        return QPainter::CompositionMode_DestinationAtop;
    } else if (compositeOperator == QLatin1String("destination-in")) {
        return QPainter::CompositionMode_DestinationIn;
    } else if (compositeOperator == QLatin1String("destination-out")) {
        return QPainter::CompositionMode_DestinationOut;
    } else if (compositeOperator == QLatin1String("destination-over")) {
        return QPainter::CompositionMode_DestinationOver;
    } else if (compositeOperator == QLatin1String("lighter")) {
        return QPainter::CompositionMode_Plus;
    } else if (compositeOperator == QLatin1String("copy")) {
        return QPainter::CompositionMode_Source;
    } else if (compositeOperator == QLatin1String("xor")) {
        return QPainter::CompositionMode_Xor;
    } else if (compositeOperator == QLatin1String("qt-clear")) {
        return QPainter::CompositionMode_Clear;
    } else if (compositeOperator == QLatin1String("qt-destination")) {
        return QPainter::CompositionMode_Destination;
    } else if (compositeOperator == QLatin1String("qt-multiply")) {
        return QPainter::CompositionMode_Multiply;
    } else if (compositeOperator == QLatin1String("qt-screen")) {
        return QPainter::CompositionMode_Screen;
    } else if (compositeOperator == QLatin1String("qt-overlay")) {
        return QPainter::CompositionMode_Overlay;
    } else if (compositeOperator == QLatin1String("qt-darken")) {
        return QPainter::CompositionMode_Darken;
    } else if (compositeOperator == QLatin1String("qt-lighten")) {
        return QPainter::CompositionMode_Lighten;
    } else if (compositeOperator == QLatin1String("qt-color-dodge")) {
        return QPainter::CompositionMode_ColorDodge;
    } else if (compositeOperator == QLatin1String("qt-color-burn")) {
        return QPainter::CompositionMode_ColorBurn;
    } else if (compositeOperator == QLatin1String("qt-hard-light")) {
        return QPainter::CompositionMode_HardLight;
    } else if (compositeOperator == QLatin1String("qt-soft-light")) {
        return QPainter::CompositionMode_SoftLight;
    } else if (compositeOperator == QLatin1String("qt-difference")) {
        return QPainter::CompositionMode_Difference;
    } else if (compositeOperator == QLatin1String("qt-exclusion")) {
        return QPainter::CompositionMode_Exclusion;
    }
    return QPainter::CompositionMode_SourceOver;
}

static QString qt_composite_mode_to_string(QPainter::CompositionMode op)
{
    switch (op) {
    case QPainter::CompositionMode_SourceOver:
        return QStringLiteral("source-over");
    case QPainter::CompositionMode_DestinationOver:
        return QStringLiteral("destination-over");
    case QPainter::CompositionMode_Clear:
        return QStringLiteral("qt-clear");
    case QPainter::CompositionMode_Source:
        return QStringLiteral("copy");
    case QPainter::CompositionMode_Destination:
        return QStringLiteral("qt-destination");
    case QPainter::CompositionMode_SourceIn:
        return QStringLiteral("source-in");
    case QPainter::CompositionMode_DestinationIn:
        return QStringLiteral("destination-in");
    case QPainter::CompositionMode_SourceOut:
        return QStringLiteral("source-out");
    case QPainter::CompositionMode_DestinationOut:
        return QStringLiteral("destination-out");
    case QPainter::CompositionMode_SourceAtop:
        return QStringLiteral("source-atop");
    case QPainter::CompositionMode_DestinationAtop:
        return QStringLiteral("destination-atop");
    case QPainter::CompositionMode_Xor:
        return QStringLiteral("xor");
    case QPainter::CompositionMode_Plus:
        return QStringLiteral("lighter");
    case QPainter::CompositionMode_Multiply:
        return QStringLiteral("qt-multiply");
    case QPainter::CompositionMode_Screen:
        return QStringLiteral("qt-screen");
    case QPainter::CompositionMode_Overlay:
        return QStringLiteral("qt-overlay");
    case QPainter::CompositionMode_Darken:
        return QStringLiteral("qt-darken");
    case QPainter::CompositionMode_Lighten:
        return QStringLiteral("lighter");
    case QPainter::CompositionMode_ColorDodge:
        return QStringLiteral("qt-color-dodge");
    case QPainter::CompositionMode_ColorBurn:
        return QStringLiteral("qt-color-burn");
    case QPainter::CompositionMode_HardLight:
        return QStringLiteral("qt-hard-light");
    case QPainter::CompositionMode_SoftLight:
        return QStringLiteral("qt-soft-light");
    case QPainter::CompositionMode_Difference:
        return QStringLiteral("qt-difference");
    case QPainter::CompositionMode_Exclusion:
        return QStringLiteral("qt-exclusion");
    default:
        break;
    }
    return QString();
}

struct QQuickJSContext2DPixelData : public QV4::Object
{
    V4_OBJECT2(QQuickJSContext2DPixelData, QV4::Object)
    V4_NEEDS_DESTROY

    static QV4::ReturnedValue getIndexed(const QV4::Managed *m, uint index, bool *hasProperty);
    static void putIndexed(QV4::Managed *m, uint index, const QV4::Value &value);

    static QV4::ReturnedValue proto_get_length(QV4::CallContext *ctx);
};

void QV4::Heap::QQuickJSContext2DPixelData::init()
{
    Object::init();
    image = new QImage;
    QV4::Scope scope(internalClass->engine);
    QV4::ScopedObject o(scope, this);
    o->setArrayType(QV4::Heap::ArrayData::Custom);
}

DEFINE_OBJECT_VTABLE(QQuickJSContext2DPixelData);

struct QQuickJSContext2DImageData : public QV4::Object
{
    V4_OBJECT2(QQuickJSContext2DImageData, QV4::Object)

    static QV4::ReturnedValue method_get_width(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_height(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_data(QV4::CallContext *ctx);

    static void markObjects(QV4::Heap::Base *that, QV4::ExecutionEngine *engine) {
        static_cast<QQuickJSContext2DImageData::Data *>(that)->pixelData.mark(engine);
        QV4::Object::markObjects(that, engine);
    }
};

void QV4::Heap::QQuickJSContext2DImageData::init()
{
    Object::init();
    pixelData = QV4::Primitive::undefinedValue();

    QV4::Scope scope(internalClass->engine);
    QV4::ScopedObject o(scope, this);

    o->defineAccessorProperty(QStringLiteral("width"), ::QQuickJSContext2DImageData::method_get_width, 0);
    o->defineAccessorProperty(QStringLiteral("height"), ::QQuickJSContext2DImageData::method_get_height, 0);
    o->defineAccessorProperty(QStringLiteral("data"), ::QQuickJSContext2DImageData::method_get_data, 0);
}

DEFINE_OBJECT_VTABLE(QQuickJSContext2DImageData);

static QV4::ReturnedValue qt_create_image_data(qreal w, qreal h, QV4::ExecutionEngine *v4, const QImage& image)
{
    QV4::Scope scope(v4);
    QQuickContext2DEngineData *ed = engineData(scope.engine);
    QV4::Scoped<QQuickJSContext2DPixelData> pixelData(scope, scope.engine->memoryManager->allocObject<QQuickJSContext2DPixelData>());
    QV4::ScopedObject p(scope, ed->pixelArrayProto.value());
    pixelData->setPrototype(p);

    if (image.isNull()) {
        *pixelData->d()->image = QImage(w, h, QImage::Format_ARGB32);
        pixelData->d()->image->fill(0x00000000);
    } else {
        Q_ASSERT(image.width() == qRound(w) && image.height() == qRound(h));
        *pixelData->d()->image = image.format() == QImage::Format_ARGB32 ? image : image.convertToFormat(QImage::Format_ARGB32);
    }

    QV4::Scoped<QQuickJSContext2DImageData> imageData(scope, scope.engine->memoryManager->allocObject<QQuickJSContext2DImageData>());
    imageData->d()->pixelData = pixelData.asReturnedValue();
    return imageData.asReturnedValue();
}

//static script functions

/*!
    \qmlproperty QtQuick::Canvas QtQuick::Context2D::canvas
     Holds the canvas item that the context paints on.

     This property is read only.
*/
QV4::ReturnedValue QQuickJSContext2DPrototype::method_get_canvas(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    return QV4::QObjectWrapper::wrap(scope.engine, r->d()->context->canvas());
}

/*!
    \qmlmethod object QtQuick::Context2D::restore()
    Pops the top state on the stack, restoring the context to that state.

    \sa save()
*/
QV4::ReturnedValue QQuickJSContext2DPrototype::method_restore(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    r->d()->context->popState();
    return ctx->thisObject().asReturnedValue();
}

/*!
    \qmlmethod object QtQuick::Context2D::reset()
    Resets the context state and properties to the default values.
*/
QV4::ReturnedValue QQuickJSContext2DPrototype::method_reset(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    r->d()->context->reset();

    return ctx->thisObject().asReturnedValue();
}

/*!
    \qmlmethod object QtQuick::Context2D::save()
    Pushes the current state onto the state stack.

    Before changing any state attributes, you should save the current state
    for future reference. The context maintains a stack of drawing states.
    Each state consists of the current transformation matrix, clipping region,
    and values of the following attributes:
    \list
    \li strokeStyle
    \li fillStyle
    \li fillRule
    \li globalAlpha
    \li lineWidth
    \li lineCap
    \li lineJoin
    \li miterLimit
    \li shadowOffsetX
    \li shadowOffsetY
    \li shadowBlur
    \li shadowColor
    \li globalCompositeOperation
    \li \l font
    \li textAlign
    \li textBaseline
    \endlist

    The current path is NOT part of the drawing state. The path can be reset by
    invoking the beginPath() method.
*/
QV4::ReturnedValue QQuickJSContext2DPrototype::method_save(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    r->d()->context->pushState();

    return ctx->thisObject().asReturnedValue();
}

// transformations
/*!
    \qmlmethod object QtQuick::Context2D::rotate(real angle)
    Rotate the canvas around the current origin by \a angle in radians and clockwise direction.

    \code
    ctx.rotate(Math.PI/2);
    \endcode

    \image qml-item-canvas-rotate.png

    The rotation transformation matrix is as follows:

    \image qml-item-canvas-math-rotate.png

    where the \a angle of rotation is in radians.

*/
QV4::ReturnedValue QQuickJSContext2DPrototype::method_rotate(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 1)
        r->d()->context->rotate(ctx->args()[0].toNumber());
    return ctx->thisObject().asReturnedValue();
}

/*!
    \qmlmethod object QtQuick::Context2D::scale(real x, real y)

    Increases or decreases the size of each unit in the canvas grid by multiplying the scale factors
    to the current tranform matrix.
    \a x is the scale factor in the horizontal direction and \a y is the scale factor in the
    vertical direction.

    The following code doubles the horizontal size of an object drawn on the canvas and halves its
    vertical size:

    \code
    ctx.scale(2.0, 0.5);
    \endcode

    \image qml-item-canvas-scale.png
*/
QV4::ReturnedValue QQuickJSContext2DPrototype::method_scale(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)


    if (ctx->argc() >= 2)
        r->d()->context->scale(ctx->args()[0].toNumber(), ctx->args()[1].toNumber());
    return ctx->thisObject().asReturnedValue();
}

/*!
    \qmlmethod object QtQuick::Context2D::setTransform(real a, real b, real c, real d, real e, real f)
    Changes the transformation matrix to the matrix given by the arguments as described below.

    Modifying the transformation matrix directly enables you to perform scaling,
    rotating, and translating transformations in a single step.

    Each point on the canvas is multiplied by the matrix before anything is
    drawn. The \l{http://www.w3.org/TR/2dcontext/#transformations}{HTML Canvas 2D Context specification}
    defines the transformation matrix as:

    \image qml-item-canvas-math.png
    where:
    \list
    \li \c{a} is the scale factor in the horizontal (x) direction
    \image qml-item-canvas-scalex.png
    \li \c{c} is the skew factor in the x direction
    \image qml-item-canvas-skewx.png
    \li \c{e} is the translation in the x direction
    \image qml-item-canvas-translate.png
    \li \c{b} is the skew factor in the y (vertical) direction
    \image qml-item-canvas-skewy.png
    \li \c{d} is the scale factor in the y direction
    \image qml-item-canvas-scaley.png
    \li \c{f} is the translation in the y direction
    \image qml-item-canvas-translatey.png
    \li the last row remains constant
    \endlist
    The scale factors and skew factors are multiples; \c{e} and \c{f} are
    coordinate space units, just like the units in the translate(x,y)
    method.

    \sa transform()
*/
QV4::ReturnedValue QQuickJSContext2DPrototype::method_setTransform(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)


    if (ctx->argc() >= 6)
        r->d()->context->setTransform( ctx->args()[0].toNumber()
                                                        , ctx->args()[1].toNumber()
                                                        , ctx->args()[2].toNumber()
                                                        , ctx->args()[3].toNumber()
                                                        , ctx->args()[4].toNumber()
                                                        , ctx->args()[5].toNumber());

    return ctx->thisObject().asReturnedValue();
}

/*!
    \qmlmethod object QtQuick::Context2D::transform(real a, real b, real c, real d, real e, real f)

    This method is very similar to setTransform(), but instead of replacing the old
    transform matrix, this method applies the given tranform matrix to the current matrix by multiplying to it.

    The setTransform(a, b, c, d, e, f) method actually resets the current transform to the identity matrix,
    and then invokes the transform(a, b, c, d, e, f) method with the same arguments.

    \sa setTransform()
*/
QV4::ReturnedValue QQuickJSContext2DPrototype::method_transform(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 6)
        r->d()->context->transform( ctx->args()[0].toNumber()
                                                  , ctx->args()[1].toNumber()
                                                  , ctx->args()[2].toNumber()
                                                  , ctx->args()[3].toNumber()
                                                  , ctx->args()[4].toNumber()
                                                  , ctx->args()[5].toNumber());

    return ctx->thisObject().asReturnedValue();
}

/*!
    \qmlmethod object QtQuick::Context2D::translate(real x, real y)

    Translates the origin of the canvas by a horizontal distance of \a x,
    and a vertical distance of \a y, in coordinate space units.

    Translating the origin enables you to draw patterns of different objects on the canvas
    without having to measure the coordinates manually for each shape.
*/
QV4::ReturnedValue QQuickJSContext2DPrototype::method_translate(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 2)
            r->d()->context->translate(ctx->args()[0].toNumber(), ctx->args()[1].toNumber());
    return ctx->thisObject().asReturnedValue();
}


/*!
    \qmlmethod object QtQuick::Context2D::resetTransform()

    Reset the transformation matrix to the default value (equivalent to calling
    setTransform(\c 1, \c 0, \c 0, \c 1, \c 0, \c 0)).

    \sa transform(), setTransform(), reset()
*/
QV4::ReturnedValue QQuickJSContext2DPrototype::method_resetTransform(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    r->d()->context->setTransform(1, 0, 0, 1, 0, 0);

    return ctx->thisObject().asReturnedValue();
}


/*!
    \qmlmethod object QtQuick::Context2D::shear(real sh, real sv)

    Shears the transformation matrix by \a sh in the horizontal direction and
    \a sv in the vertical direction.
*/
QV4::ReturnedValue QQuickJSContext2DPrototype::method_shear(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 2)
            r->d()->context->shear(ctx->args()[0].toNumber(), ctx->args()[1].toNumber());

    return ctx->thisObject().asReturnedValue();
}
// compositing

/*!
    \qmlproperty real QtQuick::Context2D::globalAlpha

     Holds the current alpha value applied to rendering operations.
     The value must be in the range from \c 0.0 (fully transparent) to \c 1.0 (fully opaque).
     The default value is \c 1.0.
*/
QV4::ReturnedValue QQuickJSContext2D::method_get_globalAlpha(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    return QV4::Encode(r->d()->context->state.globalAlpha);
}

QV4::ReturnedValue QQuickJSContext2D::method_set_globalAlpha(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT_SETTER(r)

    double globalAlpha = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();

    if (!qt_is_finite(globalAlpha))
        return QV4::Encode::undefined();

    if (globalAlpha >= 0.0 && globalAlpha <= 1.0 && r->d()->context->state.globalAlpha != globalAlpha) {
        r->d()->context->state.globalAlpha = globalAlpha;
        r->d()->context->buffer()->setGlobalAlpha(r->d()->context->state.globalAlpha);
    }
    return QV4::Encode::undefined();
}

/*!
    \qmlproperty string QtQuick::Context2D::globalCompositeOperation
     Holds the current the current composition operation, from the list below:
     \list
     \li source-atop      - A atop B. Display the source image wherever both images are opaque.
                           Display the destination image wherever the destination image is opaque but the source image is transparent.
                           Display transparency elsewhere.
     \li source-in        - A in B. Display the source image wherever both the source image and destination image are opaque.
                           Display transparency elsewhere.
     \li source-out       - A out B. Display the source image wherever the source image is opaque and the destination image is transparent.
                           Display transparency elsewhere.
     \li source-over      - (default) A over B. Display the source image wherever the source image is opaque.
                           Display the destination image elsewhere.
     \li destination-atop - B atop A. Same as source-atop but using the destination image instead of the source image and vice versa.
     \li destination-in   - B in A. Same as source-in but using the destination image instead of the source image and vice versa.
     \li destination-out  - B out A. Same as source-out but using the destination image instead of the source image and vice versa.
     \li destination-over - B over A. Same as source-over but using the destination image instead of the source image and vice versa.
     \li lighter          - A plus B. Display the sum of the source image and destination image, with color values approaching 255 (100%) as a limit.
     \li copy             - A (B is ignored). Display the source image instead of the destination image.
     \li xor              - A xor B. Exclusive OR of the source image and destination image.
     \endlist

     Additionally, this property also accepts the compositon modes listed in QPainter::CompositionMode. According to the W3C standard, these
     extension composition modes are provided as "vendorName-operationName" syntax, for example: QPainter::CompositionMode_Exclusion is provided as
     "qt-exclusion".
*/
QV4::ReturnedValue QQuickJSContext2D::method_get_globalCompositeOperation(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    return QV4::Encode(scope.engine->newString(qt_composite_mode_to_string(r->d()->context->state.globalCompositeOperation)));
}

QV4::ReturnedValue QQuickJSContext2D::method_set_globalCompositeOperation(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT_SETTER(r)

    if (!ctx->argc())
        return ctx->engine()->throwTypeError();

    QString mode = ctx->args()[0].toQString();
    QPainter::CompositionMode cm = qt_composite_mode_from_string(mode);
    if (cm == QPainter::CompositionMode_SourceOver && mode != QLatin1String("source-over"))
        return QV4::Encode::undefined();

    if (cm != r->d()->context->state.globalCompositeOperation) {
        r->d()->context->state.globalCompositeOperation = cm;
        r->d()->context->buffer()->setGlobalCompositeOperation(cm);
    }
    return QV4::Encode::undefined();
}

// colors and styles
/*!
    \qmlproperty variant QtQuick::Context2D::fillStyle
     Holds the current style used for filling shapes.
     The style can be either a string containing a CSS color, a CanvasGradient or CanvasPattern object. Invalid values are ignored.
     This property accepts several color syntaxes:
     \list
     \li 'rgb(red, green, blue)' - for example: 'rgb(255, 100, 55)' or 'rgb(100%, 70%, 30%)'
     \li 'rgba(red, green, blue, alpha)' - for example: 'rgb(255, 100, 55, 1.0)' or 'rgb(100%, 70%, 30%, 0.5)'
     \li 'hsl(hue, saturation, lightness)'
     \li 'hsla(hue, saturation, lightness, alpha)'
     \li '#RRGGBB' - for example: '#00FFCC'
     \li Qt.rgba(red, green, blue, alpha) - for example: Qt.rgba(0.3, 0.7, 1, 1.0)
     \endlist
     If the \c fillStyle or \l strokeStyle is assigned many times in a loop, the last Qt.rgba() syntax should be chosen, as it has the
     best performance, because it's already a valid QColor value, does not need to be parsed everytime.

     The default value is  '#000000'.
     \sa createLinearGradient()
     \sa createRadialGradient()
     \sa createPattern()
     \sa strokeStyle
 */
QV4::ReturnedValue QQuickJSContext2D::method_get_fillStyle(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    QColor color = r->d()->context->state.fillStyle.color();
    if (color.isValid()) {
        if (color.alpha() == 255)
            return QV4::Encode(scope.engine->newString(color.name()));
        QString alphaString = QString::number(color.alphaF(), 'f');
        while (alphaString.endsWith(QLatin1Char('0')))
            alphaString.chop(1);
        if (alphaString.endsWith(QLatin1Char('.')))
            alphaString += QLatin1Char('0');
        QString str = QString::fromLatin1("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(alphaString);
        return QV4::Encode(scope.engine->newString(str));
    }
    return r->d()->context->m_fillStyle.value();
}

QV4::ReturnedValue QQuickJSContext2D::method_set_fillStyle(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT_SETTER(r)

    QV4::ScopedValue value(scope, ctx->argument(0));

   if (value->as<Object>()) {
       QColor color = scope.engine->toVariant(value, qMetaTypeId<QColor>()).value<QColor>();
       if (color.isValid()) {
           r->d()->context->state.fillStyle = color;
           r->d()->context->buffer()->setFillStyle(color);
           r->d()->context->m_fillStyle.set(scope.engine, value);
       } else {
           QV4::Scoped<QQuickContext2DStyle> style(scope, value->as<QQuickContext2DStyle>());
           if (style && *style->d()->brush != r->d()->context->state.fillStyle) {
               r->d()->context->state.fillStyle = *style->d()->brush;
               r->d()->context->buffer()->setFillStyle(*style->d()->brush, style->d()->patternRepeatX, style->d()->patternRepeatY);
               r->d()->context->m_fillStyle.set(scope.engine, value);
               r->d()->context->state.fillPatternRepeatX = style->d()->patternRepeatX;
               r->d()->context->state.fillPatternRepeatY = style->d()->patternRepeatY;
           }
       }
   } else if (value->isString()) {
       QColor color = qt_color_from_string(value);
       if (color.isValid() && r->d()->context->state.fillStyle != QBrush(color)) {
            r->d()->context->state.fillStyle = QBrush(color);
            r->d()->context->buffer()->setFillStyle(r->d()->context->state.fillStyle);
            r->d()->context->m_fillStyle.set(scope.engine, value);
       }
   }
   return QV4::Encode::undefined();
}
/*!
    \qmlproperty enumeration QtQuick::Context2D::fillRule
     Holds the current fill rule used for filling shapes. The following fill rules supported:
     \list
     \li Qt.OddEvenFill
     \li Qt.WindingFill
     \endlist
     Note: Unlike the QPainterPath, the Canvas API uses the winding fill as the default fill rule.
     The fillRule property is part of the context rendering state.

     \sa fillStyle
 */
QV4::ReturnedValue QQuickJSContext2D::method_get_fillRule(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    return scope.engine->fromVariant(r->d()->context->state.fillRule);
}

QV4::ReturnedValue QQuickJSContext2D::method_set_fillRule(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT_SETTER(r)

    QV4::ScopedValue value(scope, ctx->argument(0));

    if ((value->isString() && value->toQString() == QLatin1String("WindingFill"))
        || (value->isInt32() && value->integerValue() == Qt::WindingFill)) {
        r->d()->context->state.fillRule = Qt::WindingFill;
    } else if ((value->isString() && value->toQStringNoThrow() == QLatin1String("OddEvenFill"))
               || (value->isInt32() && value->integerValue() == Qt::OddEvenFill)) {
        r->d()->context->state.fillRule = Qt::OddEvenFill;
    } else {
        //error
    }
    r->d()->context->m_path.setFillRule(r->d()->context->state.fillRule);
    return QV4::Encode::undefined();
}
/*!
    \qmlproperty variant QtQuick::Context2D::strokeStyle
     Holds the current color or style to use for the lines around shapes,
     The style can be either a string containing a CSS color, a CanvasGradient or CanvasPattern object.
     Invalid values are ignored.

     The default value is  '#000000'.

     \sa createLinearGradient()
     \sa createRadialGradient()
     \sa createPattern()
     \sa fillStyle
 */
QV4::ReturnedValue QQuickJSContext2D::method_get_strokeStyle(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    QColor color = r->d()->context->state.strokeStyle.color();
    if (color.isValid()) {
        if (color.alpha() == 255)
            return QV4::Encode(scope.engine->newString(color.name()));
        QString alphaString = QString::number(color.alphaF(), 'f');
        while (alphaString.endsWith(QLatin1Char('0')))
            alphaString.chop(1);
        if (alphaString.endsWith(QLatin1Char('.')))
            alphaString += QLatin1Char('0');
        QString str = QString::fromLatin1("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(alphaString);
        return QV4::Encode(scope.engine->newString(str));
    }
    return r->d()->context->m_strokeStyle.value();
}

QV4::ReturnedValue QQuickJSContext2D::method_set_strokeStyle(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT_SETTER(r)

    QV4::ScopedValue value(scope, ctx->argument(0));

    if (value->as<Object>()) {
        QColor color = scope.engine->toVariant(value, qMetaTypeId<QColor>()).value<QColor>();
        if (color.isValid()) {
            r->d()->context->state.fillStyle = color;
            r->d()->context->buffer()->setStrokeStyle(color);
            r->d()->context->m_strokeStyle.set(scope.engine, value);
        } else {
            QV4::Scoped<QQuickContext2DStyle> style(scope, value->as<QQuickContext2DStyle>());
            if (style && *style->d()->brush != r->d()->context->state.strokeStyle) {
                r->d()->context->state.strokeStyle = *style->d()->brush;
                r->d()->context->buffer()->setStrokeStyle(*style->d()->brush, style->d()->patternRepeatX, style->d()->patternRepeatY);
                r->d()->context->m_strokeStyle.set(scope.engine, value);
                r->d()->context->state.strokePatternRepeatX = style->d()->patternRepeatX;
                r->d()->context->state.strokePatternRepeatY = style->d()->patternRepeatY;

            }
        }
    } else if (value->isString()) {
        QColor color = qt_color_from_string(value);
        if (color.isValid() && r->d()->context->state.strokeStyle != QBrush(color)) {
             r->d()->context->state.strokeStyle = QBrush(color);
             r->d()->context->buffer()->setStrokeStyle(r->d()->context->state.strokeStyle);
             r->d()->context->m_strokeStyle.set(scope.engine, value);
        }
    }
    return QV4::Encode::undefined();
}

/*!
  \qmlmethod object QtQuick::Context2D::createLinearGradient(real x0, real y0, real x1, real y1)
   Returns a CanvasGradient object that represents a linear gradient that transitions the color along a line between
   the start point (\a x0, \a y0) and the end point (\a x1, \a y1).

   A gradient is a smooth transition between colors. There are two types of gradients: linear and radial.
   Gradients must have two or more color stops, representing color shifts positioned from 0 to 1 between
   to the gradient's starting and end points or circles.

    \sa CanvasGradient::addColorStop()
    \sa createRadialGradient()
    \sa createConicalGradient()
    \sa createPattern()
    \sa fillStyle
    \sa strokeStyle
  */

QV4::ReturnedValue QQuickJSContext2DPrototype::method_createLinearGradient(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 4) {
        qreal x0 = ctx->args()[0].toNumber();
        qreal y0 = ctx->args()[1].toNumber();
        qreal x1 = ctx->args()[2].toNumber();
        qreal y1 = ctx->args()[3].toNumber();

        if (!qt_is_finite(x0)
         || !qt_is_finite(y0)
         || !qt_is_finite(x1)
         || !qt_is_finite(y1)) {
            V4THROW_DOM(DOMEXCEPTION_NOT_SUPPORTED_ERR, "createLinearGradient(): Incorrect arguments")
        }
        QQuickContext2DEngineData *ed = engineData(scope.engine);

        QV4::Scoped<QQuickContext2DStyle> gradient(scope, scope.engine->memoryManager->allocObject<QQuickContext2DStyle>());
        QV4::ScopedObject p(scope, ed->gradientProto.value());
        gradient->setPrototype(p);
        *gradient->d()->brush = QLinearGradient(x0, y0, x1, y1);
        return gradient.asReturnedValue();
    }

    return ctx->thisObject().asReturnedValue();
}

/*!
  \qmlmethod object QtQuick::Context2D::createRadialGradient(real x0, real y0, real r0, real x1, real y1, real r1)
   Returns a CanvasGradient object that represents a radial gradient that paints along the cone given by the start circle with
   origin (x0, y0) and radius r0, and the end circle with origin (x1, y1) and radius r1.

    \sa CanvasGradient::addColorStop()
    \sa createLinearGradient()
    \sa createConicalGradient()
    \sa createPattern()
    \sa fillStyle
    \sa strokeStyle
  */

QV4::ReturnedValue QQuickJSContext2DPrototype::method_createRadialGradient(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 6) {
        qreal x0 = ctx->args()[0].toNumber();
        qreal y0 = ctx->args()[1].toNumber();
        qreal r0 = ctx->args()[2].toNumber();
        qreal x1 = ctx->args()[3].toNumber();
        qreal y1 = ctx->args()[4].toNumber();
        qreal r1 = ctx->args()[5].toNumber();

        if (!qt_is_finite(x0)
         || !qt_is_finite(y0)
         || !qt_is_finite(x1)
         || !qt_is_finite(r0)
         || !qt_is_finite(r1)
         || !qt_is_finite(y1)) {
            V4THROW_DOM(DOMEXCEPTION_NOT_SUPPORTED_ERR, "createRadialGradient(): Incorrect arguments")
        }

        if (r0 < 0 || r1 < 0)
            V4THROW_DOM(DOMEXCEPTION_INDEX_SIZE_ERR, "createRadialGradient(): Incorrect arguments")

        QQuickContext2DEngineData *ed = engineData(scope.engine);

        QV4::Scoped<QQuickContext2DStyle> gradient(scope, scope.engine->memoryManager->allocObject<QQuickContext2DStyle>());
        QV4::ScopedObject p(scope, ed->gradientProto.value());
        gradient->setPrototype(p);
        *gradient->d()->brush = QRadialGradient(QPointF(x1, y1), r0+r1, QPointF(x0, y0));
        return gradient.asReturnedValue();
    }

    return ctx->thisObject().asReturnedValue();
}

/*!
  \qmlmethod object QtQuick::Context2D::createConicalGradient(real x, real y, real angle)
   Returns a CanvasGradient object that represents a conical gradient that interpolate colors counter-clockwise around a center point (\c x, \c y)
   with start angle \c angle in units of radians.

    \sa CanvasGradient::addColorStop()
    \sa createLinearGradient()
    \sa createRadialGradient()
    \sa createPattern()
    \sa fillStyle
    \sa strokeStyle
  */

QV4::ReturnedValue QQuickJSContext2DPrototype::method_createConicalGradient(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 3) {
        qreal x = ctx->args()[0].toNumber();
        qreal y = ctx->args()[1].toNumber();
        qreal angle = DEGREES(ctx->args()[2].toNumber());
        if (!qt_is_finite(x) || !qt_is_finite(y)) {
            V4THROW_DOM(DOMEXCEPTION_NOT_SUPPORTED_ERR, "createConicalGradient(): Incorrect arguments");
        }

        if (!qt_is_finite(angle)) {
            V4THROW_DOM(DOMEXCEPTION_INDEX_SIZE_ERR, "createConicalGradient(): Incorrect arguments");
        }

        QQuickContext2DEngineData *ed = engineData(scope.engine);

        QV4::Scoped<QQuickContext2DStyle> gradient(scope, scope.engine->memoryManager->allocObject<QQuickContext2DStyle>());
        QV4::ScopedObject p(scope, ed->gradientProto.value());
        gradient->setPrototype(p);
        *gradient->d()->brush = QConicalGradient(x, y, angle);
        return gradient.asReturnedValue();
    }

    return ctx->thisObject().asReturnedValue();
}
/*!
  \qmlmethod variant QtQuick::Context2D::createPattern(color color, enumeration patternMode)
  This is a overload function.
  Returns a CanvasPattern object that uses the given \a color and \a patternMode.
  The valid pattern modes are:
    \list
    \li Qt.SolidPattern
    \li Qt.Dense1Pattern
    \li Qt.Dense2Pattern
    \li Qt.Dense3Pattern
    \li Qt.Dense4Pattern
    \li Qt.Dense5Pattern
    \li Qt.Dense6Pattern
    \li Qt.Dense7Pattern
    \li Qt.HorPattern
    \li Qt.VerPattern
    \li Qt.CrossPattern
    \li Qt.BDiagPattern
    \li Qt.FDiagPattern
    \li Qt.DiagCrossPattern
\endlist
    \sa Qt::BrushStyle
 */
/*!
  \qmlmethod variant QtQuick::Context2D::createPattern(Image image, string repetition)
  Returns a CanvasPattern object that uses the given image and repeats in the direction(s) given by the repetition argument.

  The \a image parameter must be a valid Image item, a valid CanvasImageData object or loaded image url, if there is no image data, throws an INVALID_STATE_ERR exception.

  The allowed values for \a repetition are:

  \list
  \li "repeat"    - both directions
  \li "repeat-x   - horizontal only
  \li "repeat-y"  - vertical only
  \li "no-repeat" - neither
  \endlist

  If the repetition argument is empty or null, the value "repeat" is used.

  \sa strokeStyle
  \sa fillStyle
  */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_createPattern(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 2) {
        QV4::Scoped<QQuickContext2DStyle> pattern(scope, scope.engine->memoryManager->allocObject<QQuickContext2DStyle>());

        QColor color = scope.engine->toVariant(ctx->args()[0], qMetaTypeId<QColor>()).value<QColor>();
        if (color.isValid()) {
            int patternMode = ctx->args()[1].toInt32();
            Qt::BrushStyle style = Qt::SolidPattern;
            if (patternMode >= 0 && patternMode < Qt::LinearGradientPattern) {
                style = static_cast<Qt::BrushStyle>(patternMode);
            }
            *pattern->d()->brush = QBrush(color, style);
        } else {
            QImage patternTexture;

            if (const QV4::Object *o = ctx->args()[0].as<Object>()) {
                QV4::ScopedString s(scope, scope.engine->newString(QStringLiteral("data")));
                QV4::Scoped<QQuickJSContext2DPixelData> pixelData(scope, o->get(s));
                if (!!pixelData) {
                    patternTexture = *pixelData->d()->image;
                }
            } else {
                patternTexture = r->d()->context->createPixmap(QUrl(ctx->args()[0].toQStringNoThrow()))->image();
            }

            if (!patternTexture.isNull()) {
                pattern->d()->brush->setTextureImage(patternTexture);

                QString repetition = ctx->args()[1].toQStringNoThrow();
                if (repetition == QLatin1String("repeat") || repetition.isEmpty()) {
                    pattern->d()->patternRepeatX = true;
                    pattern->d()->patternRepeatY = true;
                } else if (repetition == QLatin1String("repeat-x")) {
                    pattern->d()->patternRepeatX = true;
                    pattern->d()->patternRepeatY = false;
                } else if (repetition == QLatin1String("repeat-y")) {
                    pattern->d()->patternRepeatX = false;
                    pattern->d()->patternRepeatY = true;
                } else if (repetition == QLatin1String("no-repeat")) {
                    pattern->d()->patternRepeatX = false;
                    pattern->d()->patternRepeatY = false;
                } else {
                    //TODO: exception: SYNTAX_ERR
                }

            }
        }

        return pattern.asReturnedValue();

    }
    return QV4::Encode::undefined();
}

// line styles
/*!
    \qmlproperty string QtQuick::Context2D::lineCap
     Holds the current line cap style.
     The possible line cap styles are:
    \list
    \li butt - the end of each line has a flat edge perpendicular to the direction of the line, this is the default line cap value.
    \li round - a semi-circle with the diameter equal to the width of the line must then be added on to the end of the line.
    \li square - a rectangle with the length of the line width and the width of half the line width, placed flat against the edge perpendicular to the direction of the line.
    \endlist
    Other values are ignored.
*/
QV4::ReturnedValue QQuickJSContext2D::method_get_lineCap(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    switch (r->d()->context->state.lineCap) {
    case Qt::RoundCap:
        return QV4::Encode(scope.engine->newString(QStringLiteral("round")));
    case Qt::SquareCap:
        return QV4::Encode(scope.engine->newString(QStringLiteral("square")));
    case Qt::FlatCap:
    default:
        break;
    }
    return QV4::Encode(scope.engine->newString(QStringLiteral("butt")));
}

QV4::ReturnedValue QQuickJSContext2D::method_set_lineCap(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT_SETTER(r)

    QString lineCap = ctx->args()[0].toQString();
    Qt::PenCapStyle cap;
    if (lineCap == QLatin1String("round"))
        cap = Qt::RoundCap;
    else if (lineCap == QLatin1String("butt"))
        cap = Qt::FlatCap;
    else if (lineCap == QLatin1String("square"))
        cap = Qt::SquareCap;
    else
        return QV4::Encode::undefined();

    if (cap != r->d()->context->state.lineCap) {
        r->d()->context->state.lineCap = cap;
        r->d()->context->buffer()->setLineCap(cap);
    }
    return QV4::Encode::undefined();
}

/*!
    \qmlproperty string QtQuick::Context2D::lineJoin
     Holds the current line join style. A join exists at any point in a subpath
     shared by two consecutive lines. When a subpath is closed, then a join also exists
     at its first point (equivalent to its last point) connecting the first and last lines in the subpath.

    The possible line join styles are:
    \list
    \li bevel - this is all that is rendered at joins.
    \li round - a filled arc connecting the two aforementioned corners of the join, abutting (and not overlapping) the aforementioned triangle, with the diameter equal to the line width and the origin at the point of the join, must be rendered at joins.
    \li miter - a second filled triangle must (if it can given the miter length) be rendered at the join, this is the default line join style.
    \endlist
    Other values are ignored.
*/
QV4::ReturnedValue QQuickJSContext2D::method_get_lineJoin(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    switch (r->d()->context->state.lineJoin) {
    case Qt::RoundJoin:
        return QV4::Encode(scope.engine->newString(QStringLiteral("round")));
    case Qt::BevelJoin:
        return QV4::Encode(scope.engine->newString(QStringLiteral("bevel")));
    case Qt::MiterJoin:
    default:
        break;
    }
    return QV4::Encode(scope.engine->newString(QStringLiteral("miter")));
}

QV4::ReturnedValue QQuickJSContext2D::method_set_lineJoin(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT_SETTER(r)

    if (!ctx->argc())
        return ctx->engine()->throwTypeError();

    QString lineJoin = ctx->args()[0].toQString();
    Qt::PenJoinStyle join;
    if (lineJoin == QLatin1String("round"))
        join = Qt::RoundJoin;
    else if (lineJoin == QLatin1String("bevel"))
        join = Qt::BevelJoin;
    else if (lineJoin == QLatin1String("miter"))
        join = Qt::SvgMiterJoin;
    else
        return QV4::Encode::undefined();

    if (join != r->d()->context->state.lineJoin) {
        r->d()->context->state.lineJoin = join;
        r->d()->context->buffer()->setLineJoin(join);
    }
    return QV4::Encode::undefined();
}

/*!
    \qmlproperty real QtQuick::Context2D::lineWidth
     Holds the current line width. Values that are not finite values greater than zero are ignored.
 */
QV4::ReturnedValue QQuickJSContext2D::method_get_lineWidth(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    return QV4::Encode(r->d()->context->state.lineWidth);
}

QV4::ReturnedValue QQuickJSContext2D::method_set_lineWidth(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT_SETTER(r)

    qreal w = ctx->argc() ? ctx->args()[0].toNumber() : -1;

    if (w > 0 && qt_is_finite(w) && w != r->d()->context->state.lineWidth) {
        r->d()->context->state.lineWidth = w;
        r->d()->context->buffer()->setLineWidth(w);
    }
    return QV4::Encode::undefined();
}

/*!
    \qmlproperty real QtQuick::Context2D::miterLimit
     Holds the current miter limit ratio.
     The default miter limit value is 10.0.
 */
QV4::ReturnedValue QQuickJSContext2D::method_get_miterLimit(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    return QV4::Encode(r->d()->context->state.miterLimit);
}

QV4::ReturnedValue QQuickJSContext2D::method_set_miterLimit(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT_SETTER(r)

    qreal ml = ctx->argc() ? ctx->args()[0].toNumber() : -1;

    if (ml > 0 && qt_is_finite(ml) && ml != r->d()->context->state.miterLimit) {
        r->d()->context->state.miterLimit = ml;
        r->d()->context->buffer()->setMiterLimit(ml);
    }
    return QV4::Encode::undefined();
}

// shadows
/*!
    \qmlproperty real QtQuick::Context2D::shadowBlur
     Holds the current level of blur applied to shadows
 */
QV4::ReturnedValue QQuickJSContext2D::method_get_shadowBlur(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    return QV4::Encode(r->d()->context->state.shadowBlur);
}

QV4::ReturnedValue QQuickJSContext2D::method_set_shadowBlur(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT_SETTER(r)

    qreal blur = ctx->argc() ? ctx->args()[0].toNumber() : -1;

    if (blur > 0 && qt_is_finite(blur) && blur != r->d()->context->state.shadowBlur) {
        r->d()->context->state.shadowBlur = blur;
        r->d()->context->buffer()->setShadowBlur(blur);
    }
    return QV4::Encode::undefined();
}

/*!
    \qmlproperty string QtQuick::Context2D::shadowColor
     Holds the current shadow color.
 */
QV4::ReturnedValue QQuickJSContext2D::method_get_shadowColor(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    return QV4::Encode(scope.engine->newString(r->d()->context->state.shadowColor.name()));
}

QV4::ReturnedValue QQuickJSContext2D::method_set_shadowColor(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT_SETTER(r)

    QColor color;
    if (ctx->argc())
        color = qt_color_from_string(ctx->args()[0]);

    if (color.isValid() && color != r->d()->context->state.shadowColor) {
        r->d()->context->state.shadowColor = color;
        r->d()->context->buffer()->setShadowColor(color);
    }
    return QV4::Encode::undefined();
}


/*!
    \qmlproperty qreal QtQuick::Context2D::shadowOffsetX
     Holds the current shadow offset in the positive horizontal distance.

     \sa shadowOffsetY
 */
QV4::ReturnedValue QQuickJSContext2D::method_get_shadowOffsetX(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    return QV4::Encode(r->d()->context->state.shadowOffsetX);
}

QV4::ReturnedValue QQuickJSContext2D::method_set_shadowOffsetX(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT_SETTER(r)

    qreal offsetX = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();
    if (qt_is_finite(offsetX) && offsetX != r->d()->context->state.shadowOffsetX) {
        r->d()->context->state.shadowOffsetX = offsetX;
        r->d()->context->buffer()->setShadowOffsetX(offsetX);
    }
    return QV4::Encode::undefined();
}
/*!
    \qmlproperty qreal QtQuick::Context2D::shadowOffsetY
     Holds the current shadow offset in the positive vertical distance.

     \sa shadowOffsetX
 */
QV4::ReturnedValue QQuickJSContext2D::method_get_shadowOffsetY(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    return QV4::Encode(r->d()->context->state.shadowOffsetY);
}

QV4::ReturnedValue QQuickJSContext2D::method_set_shadowOffsetY(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT_SETTER(r)

    qreal offsetY = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();
    if (qt_is_finite(offsetY) && offsetY != r->d()->context->state.shadowOffsetY) {
        r->d()->context->state.shadowOffsetY = offsetY;
        r->d()->context->buffer()->setShadowOffsetY(offsetY);
    }
    return QV4::Encode::undefined();
}

QV4::ReturnedValue QQuickJSContext2D::method_get_path(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    return r->d()->context->m_v4path.value();
}

QV4::ReturnedValue QQuickJSContext2D::method_set_path(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT_SETTER(r)

    QV4::ScopedValue value(scope, ctx->argument(0));
    r->d()->context->beginPath();
    QV4::Scoped<QV4::QObjectWrapper> qobjectWrapper(scope, value);
    if (!!qobjectWrapper) {
        if (QQuickPath *path = qobject_cast<QQuickPath*>(qobjectWrapper->object()))
            r->d()->context->m_path = path->path();
    } else {
        QString path =value->toQStringNoThrow();
        QQuickSvgParser::parsePathDataFast(path, r->d()->context->m_path);
    }
    r->d()->context->m_v4path.set(scope.engine, value);
    return QV4::Encode::undefined();
}

//rects
/*!
  \qmlmethod object QtQuick::Context2D::clearRect(real x, real y, real w, real h)
  Clears all pixels on the canvas in the given rectangle to transparent black.
  */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_clearRect(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)


    if (ctx->argc() >= 4)
        r->d()->context->clearRect(ctx->args()[0].toNumber(),
                              ctx->args()[1].toNumber(),
                              ctx->args()[2].toNumber(),
                              ctx->args()[3].toNumber());

    return ctx->thisObject().asReturnedValue();
}
/*!
  \qmlmethod object QtQuick::Context2D::fillRect(real x, real y, real w, real h)
   Paint the specified rectangular area using the fillStyle.

   \sa fillStyle
  */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_fillRect(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 4)
        r->d()->context->fillRect(ctx->args()[0].toNumber(), ctx->args()[1].toNumber(), ctx->args()[2].toNumber(), ctx->args()[3].toNumber());
    return ctx->thisObject().asReturnedValue();
}

/*!
  \qmlmethod object QtQuick::Context2D::strokeRect(real x, real y, real w, real h)
   Stroke the specified rectangle's path using the strokeStyle, lineWidth, lineJoin,
   and (if appropriate) miterLimit attributes.

   \sa strokeStyle
   \sa lineWidth
   \sa lineJoin
   \sa miterLimit
  */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_strokeRect(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 4)
        r->d()->context->strokeRect(ctx->args()[0].toNumber(), ctx->args()[1].toNumber(), ctx->args()[2].toNumber(), ctx->args()[3].toNumber());

    return ctx->thisObject().asReturnedValue();
}

// Complex shapes (paths) API
/*!
    \qmlmethod object QtQuick::Context2D::arc(real x, real y, real radius,
        real startAngle, real endAngle, bool anticlockwise)

    Adds an arc to the current subpath that lies on the circumference of the
    circle whose center is at the point (\a x, \a y) and whose radius is
    \a radius.

    Both \c startAngle and \c endAngle are measured from the x-axis in radians.

    \image qml-item-canvas-arc.png

    \image qml-item-canvas-startAngle.png

    The \a anticlockwise parameter is \c true for each arc in the figure above
    because they are all drawn in the anticlockwise direction.

    \sa arcTo, {http://www.w3.org/TR/2dcontext/#dom-context-2d-arc}{W3C's 2D
    Context Standard for arc()}
*/
QV4::ReturnedValue QQuickJSContext2DPrototype::method_arc(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 5) {
        bool antiClockwise = false;

        if (ctx->argc() == 6)
            antiClockwise = ctx->args()[5].toBoolean();

        qreal radius = ctx->args()[2].toNumber();

        if (qt_is_finite(radius) && radius < 0)
           V4THROW_DOM(DOMEXCEPTION_INDEX_SIZE_ERR, "Incorrect argument radius");

        r->d()->context->arc(ctx->args()[0].toNumber(),
                        ctx->args()[1].toNumber(),
                        radius,
                        ctx->args()[3].toNumber(),
                        ctx->args()[4].toNumber(),
                        antiClockwise);
    }

    return ctx->thisObject().asReturnedValue();
}

/*!
    \qmlmethod object QtQuick::Context2D::arcTo(real x1, real y1, real x2,
        real y2, real radius)

    Adds an arc with the given control points and radius to the current subpath,
    connected to the previous point by a straight line. To draw an arc, you
    begin with the same steps you followed to create a line:

    \list
    \li Call the beginPath() method to set a new path.
    \li Call the moveTo(\c x, \c y) method to set your starting position on the
        canvas at the point (\c x, \c y).
    \li To draw an arc or circle, call the arcTo(\a x1, \a y1, \a x2, \a y2,
        \a radius) method. This adds an arc with starting point (\a x1, \a y1),
        ending point (\a x2, \a y2), and \a radius to the current subpath and
        connects it to the previous subpath by a straight line.
    \endlist

    \image qml-item-canvas-arcTo.png

    \sa arc, {http://www.w3.org/TR/2dcontext/#dom-context-2d-arcto}{W3C's 2D
    Context Standard for arcTo()}
*/
QV4::ReturnedValue QQuickJSContext2DPrototype::method_arcTo(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 5) {
        qreal radius = ctx->args()[4].toNumber();

        if (qt_is_finite(radius) && radius < 0)
           V4THROW_DOM(DOMEXCEPTION_INDEX_SIZE_ERR, "Incorrect argument radius");

        r->d()->context->arcTo(ctx->args()[0].toNumber(),
                          ctx->args()[1].toNumber(),
                          ctx->args()[2].toNumber(),
                          ctx->args()[3].toNumber(),
                          radius);
    }

    return ctx->thisObject().asReturnedValue();
}

/*!
  \qmlmethod object QtQuick::Context2D::beginPath()

   Resets the current path to a new path.
  */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_beginPath(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    r->d()->context->beginPath();

    return ctx->thisObject().asReturnedValue();
}

/*!
  \qmlmethod object QtQuick::Context2D::bezierCurveTo(real cp1x, real cp1y, real cp2x, real cp2y, real x, real y)

  Adds a cubic bezier curve between the current position and the given endPoint using the control points specified by (\c cp1x, cp1y),
  and (\c cp2x, \c cp2y).
  After the curve is added, the current position is updated to be at the end point (\c x, \c y) of the curve.
  The following code produces the path shown below:
  \code
  ctx.strokeStyle = Qt.rgba(0, 0, 0, 1);
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(20, 0);//start point
  ctx.bezierCurveTo(-10, 90, 210, 90, 180, 0);
  ctx.stroke();
  \endcode
   \image qml-item-canvas-bezierCurveTo.png
  \sa {http://www.w3.org/TR/2dcontext/#dom-context-2d-beziercurveto}{W3C 2d context standard for bezierCurveTo}
  \sa {http://www.openrise.com/lab/FlowerPower/}{The beautiful flower demo by using bezierCurveTo}
  */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_bezierCurveTo(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)


    if (ctx->argc() >= 6) {
        qreal cp1x = ctx->args()[0].toNumber();
        qreal cp1y = ctx->args()[1].toNumber();
        qreal cp2x = ctx->args()[2].toNumber();
        qreal cp2y = ctx->args()[3].toNumber();
        qreal x = ctx->args()[4].toNumber();
        qreal y = ctx->args()[5].toNumber();

        if (!qt_is_finite(cp1x) || !qt_is_finite(cp1y) || !qt_is_finite(cp2x) || !qt_is_finite(cp2y) || !qt_is_finite(x) || !qt_is_finite(y))
            return ctx->thisObject().asReturnedValue();

        r->d()->context->bezierCurveTo(cp1x, cp1y, cp2x, cp2y, x, y);
    }

    return ctx->thisObject().asReturnedValue();
}

/*!
  \qmlmethod object QtQuick::Context2D::clip()

   Creates the clipping region from the current path.
   Any parts of the shape outside the clipping path are not displayed.
   To create a complex shape using the \c clip() method:

    \list 1
    \li Call the \c{context.beginPath()} method to set the clipping path.
    \li Define the clipping path by calling any combination of the \c{lineTo},
    \c{arcTo}, \c{arc}, \c{moveTo}, etc and \c{closePath} methods.
    \li Call the \c{context.clip()} method.
    \endlist

    The new shape displays.  The following shows how a clipping path can
    modify how an image displays:

    \image qml-item-canvas-clip-complex.png
    \sa beginPath()
    \sa closePath()
    \sa stroke()
    \sa fill()
   \sa {http://www.w3.org/TR/2dcontext/#dom-context-2d-clip}{W3C 2d context standard for clip}
  */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_clip(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    r->d()->context->clip();
    return ctx->thisObject().asReturnedValue();
}

/*!
  \qmlmethod object QtQuick::Context2D::closePath()
   Closes the current subpath by drawing a line to the beginning of the subpath, automatically starting a new path.
   The current point of the new path is the previous subpath's first point.

   \sa {http://www.w3.org/TR/2dcontext/#dom-context-2d-closepath}{W3C 2d context standard for closePath}
  */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_closePath(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)


    r->d()->context->closePath();

    return ctx->thisObject().asReturnedValue();
}

/*!
  \qmlmethod object QtQuick::Context2D::fill()

   Fills the subpaths with the current fill style.

   \sa {http://www.w3.org/TR/2dcontext/#dom-context-2d-fill}{W3C 2d context standard for fill}

   \sa fillStyle
  */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_fill(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r);
    r->d()->context->fill();
    return ctx->thisObject().asReturnedValue();
}

/*!
  \qmlmethod object QtQuick::Context2D::lineTo(real x, real y)

   Draws a line from the current position to the point (x, y).
 */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_lineTo(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)


    if (ctx->argc() >= 2) {
        qreal x = ctx->args()[0].toNumber();
        qreal y = ctx->args()[1].toNumber();

        if (!qt_is_finite(x) || !qt_is_finite(y))
            return ctx->thisObject().asReturnedValue();

        r->d()->context->lineTo(x, y);
    }

    return ctx->thisObject().asReturnedValue();
}

/*!
  \qmlmethod object QtQuick::Context2D::moveTo(real x, real y)

   Creates a new subpath with the given point.
 */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_moveTo(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 2) {
        qreal x = ctx->args()[0].toNumber();
        qreal y = ctx->args()[1].toNumber();

        if (!qt_is_finite(x) || !qt_is_finite(y))
            return ctx->thisObject().asReturnedValue();
        r->d()->context->moveTo(x, y);
    }
    return ctx->thisObject().asReturnedValue();
}

/*!
  \qmlmethod object QtQuick::Context2D::quadraticCurveTo(real cpx, real cpy, real x, real y)

   Adds a quadratic bezier curve between the current point and the endpoint (\c x, \c y) with the control point specified by (\c cpx, \c cpy).

   See \l{http://www.w3.org/TR/2dcontext/#dom-context-2d-quadraticcurveto}{W3C 2d context standard for quadraticCurveTo}
 */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_quadraticCurveTo(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 4) {
        qreal cpx = ctx->args()[0].toNumber();
        qreal cpy = ctx->args()[1].toNumber();
        qreal x = ctx->args()[2].toNumber();
        qreal y = ctx->args()[3].toNumber();

        if (!qt_is_finite(cpx) || !qt_is_finite(cpy) || !qt_is_finite(x) || !qt_is_finite(y))
            return ctx->thisObject().asReturnedValue();

        r->d()->context->quadraticCurveTo(cpx, cpy, x, y);
    }

    return ctx->thisObject().asReturnedValue();
}

/*!
  \qmlmethod object QtQuick::Context2D::rect(real x, real y, real w, real h)

   Adds a rectangle at position (\c x, \c y), with the given width \c w and height \c h, as a closed subpath.
 */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_rect(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 4)
        r->d()->context->rect(ctx->args()[0].toNumber(), ctx->args()[1].toNumber(), ctx->args()[2].toNumber(), ctx->args()[3].toNumber());
    return ctx->thisObject().asReturnedValue();
}

/*!
  \qmlmethod object QtQuick::Context2D::roundedRect(real x, real y, real w, real h,  real xRadius, real yRadius)

   Adds the given rectangle rect with rounded corners to the path. The \c xRadius and \c yRadius arguments specify the radius of the
   ellipses defining the corners of the rounded rectangle.
 */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_roundedRect(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 6)
        r->d()->context->roundedRect(ctx->args()[0].toNumber()
                              , ctx->args()[1].toNumber()
                              , ctx->args()[2].toNumber()
                              , ctx->args()[3].toNumber()
                              , ctx->args()[4].toNumber()
                              , ctx->args()[5].toNumber());
    return ctx->thisObject().asReturnedValue();
}

/*!
  \qmlmethod object QtQuick::Context2D::ellipse(real x, real y, real w, real h)

  Creates an ellipse within the bounding rectangle defined by its top-left corner at (\a x, \ y), width \a w and height \a h,
  and adds it to the path as a closed subpath.

  The ellipse is composed of a clockwise curve, starting and finishing at zero degrees (the 3 o'clock position).
 */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_ellipse(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)


    if (ctx->argc() >= 4)
        r->d()->context->ellipse(ctx->args()[0].toNumber(), ctx->args()[1].toNumber(), ctx->args()[2].toNumber(), ctx->args()[3].toNumber());

    return ctx->thisObject().asReturnedValue();
}

/*!
  \qmlmethod object QtQuick::Context2D::text(string text, real x, real y)

  Adds the given \c text to the path as a set of closed subpaths created from the current context font supplied.
  The subpaths are positioned so that the left end of the text's baseline lies at the point specified by (\c x, \c y).
 */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_text(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 3) {
        qreal x = ctx->args()[1].toNumber();
        qreal y = ctx->args()[2].toNumber();

        if (!qt_is_finite(x) || !qt_is_finite(y))
            return ctx->thisObject().asReturnedValue();
        r->d()->context->text(ctx->args()[0].toQStringNoThrow(), x, y);
    }
    return ctx->thisObject().asReturnedValue();
}

/*!
  \qmlmethod object QtQuick::Context2D::stroke()

   Strokes the subpaths with the current stroke style.

   See \l{http://www.w3.org/TR/2dcontext/#dom-context-2d-stroke}{W3C 2d context standard for stroke}

   \sa strokeStyle
  */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_stroke(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    r->d()->context->stroke();
    return ctx->thisObject().asReturnedValue();
}

/*!
  \qmlmethod object QtQuick::Context2D::isPointInPath(real x, real y)

   Returns true if the given point is in the current path.

   \sa {http://www.w3.org/TR/2dcontext/#dom-context-2d-ispointinpath}{W3C 2d context standard for isPointInPath}
  */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_isPointInPath(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    bool pointInPath = false;
    if (ctx->argc() >= 2)
        pointInPath = r->d()->context->isPointInPath(ctx->args()[0].toNumber(), ctx->args()[1].toNumber());
    return QV4::Primitive::fromBoolean(pointInPath).asReturnedValue();
}

QV4::ReturnedValue QQuickJSContext2DPrototype::method_drawFocusRing(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);

    V4THROW_DOM(DOMEXCEPTION_NOT_SUPPORTED_ERR, "Context2D::drawFocusRing is not supported");
}

QV4::ReturnedValue QQuickJSContext2DPrototype::method_setCaretSelectionRect(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);

    V4THROW_DOM(DOMEXCEPTION_NOT_SUPPORTED_ERR, "Context2D::setCaretSelectionRect is not supported");
}

QV4::ReturnedValue QQuickJSContext2DPrototype::method_caretBlinkRate(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);

    V4THROW_DOM(DOMEXCEPTION_NOT_SUPPORTED_ERR, "Context2D::caretBlinkRate is not supported");
}

/*!
    \qmlproperty string QtQuick::Context2D::font
    Holds the current font settings.

    A subset of the
    \l {http://www.w3.org/TR/2dcontext/#dom-context-2d-font}{w3C 2d context standard for font}
    is supported:

    \list
        \li font-style (optional):
        normal | italic | oblique
        \li font-variant (optional): normal | small-caps
        \li font-weight (optional): normal | bold | 0 ... 99
        \li font-size: Npx | Npt (where N is a positive number)
        \li font-family: See \l {http://www.w3.org/TR/CSS2/fonts.html#propdef-font-family}
    \endlist

    \note The font-size and font-family properties are mandatory and must be in
    the order they are shown in above. In addition, a font family with spaces in
    its name must be quoted.

    The default font value is "10px sans-serif".
  */
QV4::ReturnedValue QQuickJSContext2D::method_get_font(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    return QV4::Encode(scope.engine->newString(r->d()->context->state.font.toString()));
}

QV4::ReturnedValue QQuickJSContext2D::method_set_font(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT_SETTER(r)

    QV4::ScopedString s(scope, ctx->argument(0), QV4::ScopedString::Convert);
    if (scope.engine->hasException)
        return QV4::Encode::undefined();
    QFont font = qt_font_from_string(s->toQString(), r->d()->context->state.font);
    if (font != r->d()->context->state.font) {
        r->d()->context->state.font = font;
    }
    return QV4::Encode::undefined();
}

/*!
  \qmlproperty string QtQuick::Context2D::textAlign

  Holds the current text alignment settings.
  The possible values are:
  \list
    \li start
    \li end
    \li left
    \li right
    \li center
  \endlist
  Other values are ignored. The default value is "start".
  */
QV4::ReturnedValue QQuickJSContext2D::method_get_textAlign(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    switch (r->d()->context->state.textAlign) {
    case QQuickContext2D::End:
        return QV4::Encode(scope.engine->newString(QStringLiteral("end")));
    case QQuickContext2D::Left:
        return QV4::Encode(scope.engine->newString(QStringLiteral("left")));
    case QQuickContext2D::Right:
        return QV4::Encode(scope.engine->newString(QStringLiteral("right")));
    case QQuickContext2D::Center:
        return QV4::Encode(scope.engine->newString(QStringLiteral("center")));
    case QQuickContext2D::Start:
    default:
        break;
    }
    return QV4::Encode(scope.engine->newString(QStringLiteral("start")));
}

QV4::ReturnedValue QQuickJSContext2D::method_set_textAlign(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT_SETTER(r)

    QV4::ScopedString s(scope, ctx->argument(0), QV4::ScopedString::Convert);
    if (scope.engine->hasException)
        return QV4::Encode::undefined();
    QString textAlign = s->toQString();

    QQuickContext2D::TextAlignType ta;
    if (textAlign == QLatin1String("start"))
        ta = QQuickContext2D::Start;
    else if (textAlign == QLatin1String("end"))
        ta = QQuickContext2D::End;
    else if (textAlign == QLatin1String("left"))
        ta = QQuickContext2D::Left;
    else if (textAlign == QLatin1String("right"))
        ta = QQuickContext2D::Right;
    else if (textAlign == QLatin1String("center"))
        ta = QQuickContext2D::Center;
    else
        return QV4::Encode::undefined();

    if (ta != r->d()->context->state.textAlign)
        r->d()->context->state.textAlign = ta;

    return QV4::Encode::undefined();
}

/*!
  \qmlproperty string QtQuick::Context2D::textBaseline

  Holds the current baseline alignment settings.
  The possible values are:
  \list
    \li top
    \li hanging
    \li middle
    \li alphabetic
    \li ideographic
    \li bottom
  \endlist
  Other values are ignored. The default value is "alphabetic".
  */
QV4::ReturnedValue QQuickJSContext2D::method_get_textBaseline(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    switch (r->d()->context->state.textBaseline) {
    case QQuickContext2D::Hanging:
        return QV4::Encode(scope.engine->newString(QStringLiteral("hanging")));
    case QQuickContext2D::Top:
        return QV4::Encode(scope.engine->newString(QStringLiteral("top")));
    case QQuickContext2D::Bottom:
        return QV4::Encode(scope.engine->newString(QStringLiteral("bottom")));
    case QQuickContext2D::Middle:
        return QV4::Encode(scope.engine->newString(QStringLiteral("middle")));
    case QQuickContext2D::Alphabetic:
    default:
        break;
    }
    return QV4::Encode(scope.engine->newString(QStringLiteral("alphabetic")));
}

QV4::ReturnedValue QQuickJSContext2D::method_set_textBaseline(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT_SETTER(r)
    QV4::ScopedString s(scope, ctx->argument(0), QV4::ScopedString::Convert);
    if (scope.engine->hasException)
        return QV4::Encode::undefined();
    QString textBaseline = s->toQString();

    QQuickContext2D::TextBaseLineType tb;
    if (textBaseline == QLatin1String("alphabetic"))
        tb = QQuickContext2D::Alphabetic;
    else if (textBaseline == QLatin1String("hanging"))
        tb = QQuickContext2D::Hanging;
    else if (textBaseline == QLatin1String("top"))
        tb = QQuickContext2D::Top;
    else if (textBaseline == QLatin1String("bottom"))
        tb = QQuickContext2D::Bottom;
    else if (textBaseline == QLatin1String("middle"))
        tb = QQuickContext2D::Middle;
    else
        return QV4::Encode::undefined();

    if (tb != r->d()->context->state.textBaseline)
        r->d()->context->state.textBaseline = tb;

    return QV4::Encode::undefined();
}

/*!
  \qmlmethod object QtQuick::Context2D::fillText(text, x, y)
  Fills the given text at the given position.
  \sa font
  \sa textAlign
  \sa textBaseline
  \sa strokeText
  */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_fillText(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 3) {
        qreal x = ctx->args()[1].toNumber();
        qreal y = ctx->args()[2].toNumber();
        if (!qt_is_finite(x) || !qt_is_finite(y))
            return ctx->thisObject().asReturnedValue();
        QPainterPath textPath = r->d()->context->createTextGlyphs(x, y, ctx->args()[0].toQStringNoThrow());
        r->d()->context->buffer()->fill(textPath);
    }
    return ctx->thisObject().asReturnedValue();
}
/*!
  \qmlmethod object QtQuick::Context2D::strokeText(text, x, y)
  Strokes the given text at the given position.
  \sa font
  \sa textAlign
  \sa textBaseline
  \sa fillText
  */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_strokeText(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 3)
        r->d()->context->drawText(ctx->args()[0].toQStringNoThrow(), ctx->args()[1].toNumber(), ctx->args()[2].toNumber(), false);
    return ctx->thisObject().asReturnedValue();
}

/*!
  \qmlmethod object QtQuick::Context2D::measureText(text)

  Returns an object with a \c width property, whose value is equivalent to
  calling \l {QFontMetrics::width()} with the given \a text in the current font.
  */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_measureText(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 1) {
        QFontMetrics fm(r->d()->context->state.font);
        uint width = fm.width(ctx->args()[0].toQStringNoThrow());
        QV4::ScopedObject tm(scope, scope.engine->newObject());
        tm->put(QV4::ScopedString(scope, scope.engine->newIdentifier(QStringLiteral("width"))).getPointer(),
                QV4::ScopedValue(scope, QV4::Primitive::fromDouble(width)));
        return tm.asReturnedValue();
    }
    return QV4::Encode::undefined();
}

// drawing images
/*!
  \qmlmethod QtQuick::Context2D::drawImage(variant image, real dx, real dy)
  Draws the given \a image on the canvas at position (\a dx, \a dy).
  Note:
  The \a image type can be an Image item, an image url or a CanvasImageData object.
  When given as Image item, if the image isn't fully loaded, this method draws nothing.
  When given as url string, the image should be loaded by calling Canvas item's Canvas::loadImage() method first.
  This image been drawing is subject to the current context clip path, even the given \c image is a CanvasImageData object.

  \sa CanvasImageData
  \sa Image
  \sa Canvas::loadImage
  \sa Canvas::isImageLoaded
  \sa Canvas::imageLoaded

  \sa {http://www.w3.org/TR/2dcontext/#dom-context-2d-drawimage}{W3C 2d context standard for drawImage}
  */
/*!
  \qmlmethod QtQuick::Context2D::drawImage(variant image, real dx, real dy, real dw, real dh)
  This is an overloaded function.
  Draws the given item as \a image onto the canvas at point (\a dx, \a dy) and with width \a dw,
  height \a dh.

  Note:
  The \a image type can be an Image item, an image url or a CanvasImageData object.
  When given as Image item, if the image isn't fully loaded, this method draws nothing.
  When given as url string, the image should be loaded by calling Canvas item's Canvas::loadImage() method first.
  This image been drawing is subject to the current context clip path, even the given \c image is a CanvasImageData object.

  \sa CanvasImageData
  \sa Image
  \sa Canvas::loadImage()
  \sa Canvas::isImageLoaded
  \sa Canvas::imageLoaded

  \sa {http://www.w3.org/TR/2dcontext/#dom-context-2d-drawimage}{W3C 2d context standard for drawImage}
  */
/*!
  \qmlmethod QtQuick::Context2D::drawImage(variant image, real sx, real sy, real sw, real sh, real dx, real dy, real dw, real dh)
  This is an overloaded function.
  Draws the given item as \a image from source point (\a sx, \a sy) and source width \a sw, source height \a sh
  onto the canvas at point (\a dx, \a dy) and with width \a dw, height \a dh.


  Note:
  The \a image type can be an Image or Canvas item, an image url or a CanvasImageData object.
  When given as Image item, if the image isn't fully loaded, this method draws nothing.
  When given as url string, the image should be loaded by calling Canvas item's Canvas::loadImage() method first.
  This image been drawing is subject to the current context clip path, even the given \c image is a CanvasImageData object.

  \sa CanvasImageData
  \sa Image
  \sa Canvas::loadImage()
  \sa Canvas::isImageLoaded
  \sa Canvas::imageLoaded

  \sa {http://www.w3.org/TR/2dcontext/#dom-context-2d-drawimage}{W3C 2d context standard for drawImage}
*/
QV4::ReturnedValue QQuickJSContext2DPrototype::method_drawImage(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject());
    CHECK_CONTEXT(r)

    qreal sx, sy, sw, sh, dx, dy, dw, dh;

    if (!ctx->argc())
        return ctx->thisObject().asReturnedValue();

    //FIXME:This function should be moved to QQuickContext2D::drawImage(...)
    if (!r->d()->context->state.invertibleCTM)
        return ctx->thisObject().asReturnedValue();

    QQmlRefPointer<QQuickCanvasPixmap> pixmap;

    QV4::ScopedValue arg(scope, ctx->args()[0]);
    if (arg->isString()) {
        QUrl url(arg->toQString());
        if (!url.isValid())
            V4THROW_DOM(DOMEXCEPTION_TYPE_MISMATCH_ERR, "drawImage(), type mismatch");

        pixmap = r->d()->context->createPixmap(url);
    } else if (arg->isObject()) {
        QV4::Scoped<QV4::QObjectWrapper> qobjectWrapper(scope, arg);
        if (!!qobjectWrapper) {
            if (QQuickImage *imageItem = qobject_cast<QQuickImage*>(qobjectWrapper->object())) {
                pixmap = r->d()->context->createPixmap(imageItem->source());
            } else if (QQuickCanvasItem *canvas = qobject_cast<QQuickCanvasItem*>(qobjectWrapper->object())) {
                QImage img = canvas->toImage();
                if (!img.isNull())
                    pixmap.adopt(new QQuickCanvasPixmap(img));
            } else {
                V4THROW_DOM(DOMEXCEPTION_TYPE_MISMATCH_ERR, "drawImage(), type mismatch");
            }
        } else {
            QV4::Scoped<QQuickJSContext2DImageData> imageData(scope, arg);
            if (!!imageData) {
                QV4::Scoped<QQuickJSContext2DPixelData> pix(scope, imageData->d()->pixelData.as<QQuickJSContext2DPixelData>());
                if (pix && !pix->d()->image->isNull()) {
                    pixmap.adopt(new QQuickCanvasPixmap(*pix->d()->image));
                } else {
                    V4THROW_DOM(DOMEXCEPTION_TYPE_MISMATCH_ERR, "drawImage(), type mismatch");
                }
            } else {
                QUrl url(arg->toQStringNoThrow());
                if (url.isValid())
                    pixmap = r->d()->context->createPixmap(url);
                else
                    V4THROW_DOM(DOMEXCEPTION_TYPE_MISMATCH_ERR, "drawImage(), type mismatch");
            }
        }
    } else {
        V4THROW_DOM(DOMEXCEPTION_TYPE_MISMATCH_ERR, "drawImage(), type mismatch");
    }

    if (pixmap.isNull() || !pixmap->isValid())
        return ctx->thisObject().asReturnedValue();

    if (ctx->argc() >= 9) {
        sx = ctx->args()[1].toNumber();
        sy = ctx->args()[2].toNumber();
        sw = ctx->args()[3].toNumber();
        sh = ctx->args()[4].toNumber();
        dx = ctx->args()[5].toNumber();
        dy = ctx->args()[6].toNumber();
        dw = ctx->args()[7].toNumber();
        dh = ctx->args()[8].toNumber();
    } else if (ctx->argc() >= 5) {
        sx = 0;
        sy = 0;
        sw = pixmap->width();
        sh = pixmap->height();
        dx = ctx->args()[1].toNumber();
        dy = ctx->args()[2].toNumber();
        dw = ctx->args()[3].toNumber();
        dh = ctx->args()[4].toNumber();
    } else if (ctx->argc() >= 3) {
        dx = ctx->args()[1].toNumber();
        dy = ctx->args()[2].toNumber();
        sx = 0;
        sy = 0;
        sw = pixmap->width();
        sh = pixmap->height();
        dw = sw;
        dh = sh;
    } else {
        return ctx->thisObject().asReturnedValue();
    }

    if (!qt_is_finite(sx)
     || !qt_is_finite(sy)
     || !qt_is_finite(sw)
     || !qt_is_finite(sh)
     || !qt_is_finite(dx)
     || !qt_is_finite(dy)
     || !qt_is_finite(dw)
     || !qt_is_finite(dh))
        return ctx->thisObject().asReturnedValue();

    if (sx < 0
    || sy < 0
    || sw == 0
    || sh == 0
    || sx + sw > pixmap->width()
    || sy + sh > pixmap->height()
    || sx + sw < 0 || sy + sh < 0) {
            V4THROW_DOM(DOMEXCEPTION_INDEX_SIZE_ERR, "drawImage(), index size error");
    }

    r->d()->context->buffer()->drawPixmap(pixmap, QRectF(sx, sy, sw, sh), QRectF(dx, dy, dw, dh));

    return ctx->thisObject().asReturnedValue();
}

// pixel manipulation
/*!
    \qmltype CanvasImageData
    \inqmlmodule QtQuick
    \ingroup qtquick-canvas
    \brief Contains image pixel data in RGBA order

     The CanvasImageData object holds the image pixel data.

     The CanvasImageData object has the actual dimensions of the data stored in
     this object and holds the one-dimensional array containing the data in RGBA order,
     as integers in the range 0 to 255.

     \sa width
     \sa height
     \sa data
     \sa Context2D::createImageData()
     \sa Context2D::getImageData()
     \sa Context2D::putImageData()
  */
/*!
  \qmlproperty int QtQuick::CanvasImageData::width
  Holds the actual width dimension of the data in the ImageData object, in device pixels.
 */
QV4::ReturnedValue QQuickJSContext2DImageData::method_get_width(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2DImageData> imageData(scope, ctx->thisObject());
    if (!imageData)
        return ctx->engine()->throwTypeError();
    QV4::Scoped<QQuickJSContext2DPixelData> r(scope, imageData->d()->pixelData.as<QQuickJSContext2DPixelData>());
    if (!r)
        return QV4::Encode(0);
    return QV4::Encode(r->d()->image->width());
}

/*!
  \qmlproperty int QtQuick::CanvasImageData::height
  Holds the actual height dimension of the data in the ImageData object, in device pixels.
  */
QV4::ReturnedValue QQuickJSContext2DImageData::method_get_height(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2DImageData> imageData(scope, ctx->thisObject());
    if (!imageData)
        return ctx->engine()->throwTypeError();
    QV4::Scoped<QQuickJSContext2DPixelData> r(scope, imageData->d()->pixelData.as<QQuickJSContext2DPixelData>());
    if (!r)
        return QV4::Encode(0);
    return QV4::Encode(r->d()->image->height());
}

/*!
  \qmlproperty object QtQuick::CanvasImageData::data
  Holds the one-dimensional array containing the data in RGBA order, as integers in the range 0 to 255.
 */
QV4::ReturnedValue QQuickJSContext2DImageData::method_get_data(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2DImageData> imageData(scope, ctx->thisObject());
    if (!imageData)
        return ctx->engine()->throwTypeError();
    return imageData->d()->pixelData.asReturnedValue();
}

/*!
    \qmltype CanvasPixelArray
    \inqmlmodule QtQuick
    \ingroup qtquick-canvas
    \brief Provides ordered and indexed access to the components of each pixel in image data

  The CanvasPixelArray object provides ordered, indexed access to the color components of each pixel of the image data.
  The CanvasPixelArray can be accessed as normal Javascript array.
    \sa CanvasImageData
    \sa {http://www.w3.org/TR/2dcontext/#canvaspixelarray}{W3C 2d context standard for PixelArray}
  */

/*!
  \qmlproperty int QtQuick::CanvasPixelArray::length
  The CanvasPixelArray object represents h×w×4 integers which w and h comes from CanvasImageData.
  The length attribute of a CanvasPixelArray object must return this h×w×4 number value.
  This property is read only.
*/
QV4::ReturnedValue QQuickJSContext2DPixelData::proto_get_length(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2DPixelData> r(scope, ctx->thisObject().as<QQuickJSContext2DPixelData>());
    if (!r || r->d()->image->isNull())
        return QV4::Encode::undefined();

    return QV4::Encode(r->d()->image->width() * r->d()->image->height() * 4);
}

QV4::ReturnedValue QQuickJSContext2DPixelData::getIndexed(const QV4::Managed *m, uint index, bool *hasProperty)
{
    Q_ASSERT(m->as<QQuickJSContext2DPixelData>());
    QV4::ExecutionEngine *v4 = static_cast<const QQuickJSContext2DPixelData *>(m)->engine();
    QV4::Scope scope(v4);
    QV4::Scoped<QQuickJSContext2DPixelData> r(scope, static_cast<const QQuickJSContext2DPixelData *>(m));

    if (index < static_cast<quint32>(r->d()->image->width() * r->d()->image->height() * 4)) {
        if (hasProperty)
            *hasProperty = true;
        const quint32 w = r->d()->image->width();
        const quint32 row = (index / 4) / w;
        const quint32 col = (index / 4) % w;
        const QRgb* pixel = reinterpret_cast<const QRgb*>(r->d()->image->constScanLine(row));
        pixel += col;
        switch (index % 4) {
        case 0:
            return QV4::Encode(qRed(*pixel));
        case 1:
            return QV4::Encode(qGreen(*pixel));
        case 2:
            return QV4::Encode(qBlue(*pixel));
        case 3:
            return QV4::Encode(qAlpha(*pixel));
        }
    }
    if (hasProperty)
        *hasProperty = false;
    return QV4::Encode::undefined();
}

void QQuickJSContext2DPixelData::putIndexed(QV4::Managed *m, uint index, const QV4::Value &value)
{
    Q_ASSERT(m->as<QQuickJSContext2DPixelData>());
    QV4::ExecutionEngine *v4 = static_cast<QQuickJSContext2DPixelData *>(m)->engine();
    QV4::Scope scope(v4);
    if (scope.hasException())
        return;

    QV4::Scoped<QQuickJSContext2DPixelData> r(scope, static_cast<QQuickJSContext2DPixelData *>(m));

    const int v = value.toInt32();
    if (r && index < static_cast<quint32>(r->d()->image->width() * r->d()->image->height() * 4) && v >= 0 && v <= 255) {
        const quint32 w = r->d()->image->width();
        const quint32 row = (index / 4) / w;
        const quint32 col = (index / 4) % w;

        QRgb* pixel = reinterpret_cast<QRgb*>(r->d()->image->scanLine(row));
        pixel += col;
        switch (index % 4) {
        case 0:
            *pixel = qRgba(v, qGreen(*pixel), qBlue(*pixel), qAlpha(*pixel));
            break;
        case 1:
            *pixel = qRgba(qRed(*pixel), v, qBlue(*pixel), qAlpha(*pixel));
            break;
        case 2:
            *pixel = qRgba(qRed(*pixel), qGreen(*pixel), v, qAlpha(*pixel));
            break;
        case 3:
            *pixel = qRgba(qRed(*pixel), qGreen(*pixel), qBlue(*pixel), v);
            break;
        }
    }
}
/*!
    \qmlmethod CanvasImageData QtQuick::Context2D::createImageData(real sw, real sh)

    Creates a CanvasImageData object with the given dimensions(\a sw, \a sh).
*/
/*!
    \qmlmethod CanvasImageData QtQuick::Context2D::createImageData(CanvasImageData imageData)

    Creates a CanvasImageData object with the same dimensions as the argument.
*/
/*!
    \qmlmethod CanvasImageData QtQuick::Context2D::createImageData(Url imageUrl)

    Creates a CanvasImageData object with the given image loaded from \a imageUrl.

    \note The \a imageUrl must be already loaded before this function call,
    otherwise an empty CanvasImageData obect will be returned.

    \sa Canvas::loadImage(), QtQuick::Canvas::unloadImage(),
        QtQuick::Canvas::isImageLoaded
  */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_createImageData(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    if (ctx->argc() == 1) {
        QV4::ScopedValue arg0(scope, ctx->args()[0]);
        QV4::Scoped<QQuickJSContext2DImageData> imgData(scope, arg0);
        if (!!imgData) {
            QV4::Scoped<QQuickJSContext2DPixelData> pa(scope, imgData->d()->pixelData.as<QQuickJSContext2DPixelData>());
            if (pa) {
                qreal w = pa->d()->image->width();
                qreal h = pa->d()->image->height();
                return qt_create_image_data(w, h, scope.engine, QImage());
            }
        } else if (arg0->isString()) {
            QImage image = r->d()->context->createPixmap(QUrl(arg0->toQStringNoThrow()))->image();
            return qt_create_image_data(image.width(), image.height(), scope.engine, image);
        }
    } else if (ctx->argc() == 2) {
        qreal w = ctx->args()[0].toNumber();
        qreal h = ctx->args()[1].toNumber();

        if (!qt_is_finite(w) || !qt_is_finite(h))
            V4THROW_DOM(DOMEXCEPTION_NOT_SUPPORTED_ERR, "createImageData(): invalid arguments");

        if (w > 0 && h > 0)
            return qt_create_image_data(w, h, scope.engine, QImage());
        else
            V4THROW_DOM(DOMEXCEPTION_INDEX_SIZE_ERR, "createImageData(): invalid arguments");
    }
    return QV4::Encode::undefined();
}

/*!
  \qmlmethod CanvasImageData QtQuick::Context2D::getImageData(real sx, real sy, real sw, real sh)
  Returns an CanvasImageData object containing the image data for the given rectangle of the canvas.
  */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_getImageData(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)

    if (ctx->argc() >= 4) {
        qreal x = ctx->args()[0].toNumber();
        qreal y = ctx->args()[1].toNumber();
        qreal w = ctx->args()[2].toNumber();
        qreal h = ctx->args()[3].toNumber();
        if (!qt_is_finite(x) || !qt_is_finite(y) || !qt_is_finite(w) || !qt_is_finite(h))
            V4THROW_DOM(DOMEXCEPTION_NOT_SUPPORTED_ERR, "getImageData(): Invalid arguments");

        if (w <= 0 || h <= 0)
            V4THROW_DOM(DOMEXCEPTION_INDEX_SIZE_ERR, "getImageData(): Invalid arguments");

        QImage image = r->d()->context->canvas()->toImage(QRectF(x, y, w, h));
        return qt_create_image_data(w, h, scope.engine, image);
    }
    return QV4::Encode::null();
}

/*!
  \qmlmethod object QtQuick::Context2D::putImageData(CanvasImageData imageData, real dx, real dy, real dirtyX, real dirtyY, real dirtyWidth, real dirtyHeight)
  Paints the data from the given ImageData object onto the canvas. If a dirty rectangle (\a dirtyX, \a dirtyY, \a dirtyWidth, \a dirtyHeight) is provided, only the pixels from that rectangle are painted.
  */
QV4::ReturnedValue QQuickJSContext2DPrototype::method_putImageData(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickJSContext2D> r(scope, ctx->thisObject().as<QQuickJSContext2D>());
    CHECK_CONTEXT(r)
    if (ctx->argc() < 7)
        return QV4::Encode::undefined();

    QV4::ScopedValue arg0(scope, ctx->args()[0]);
    if (!arg0->isObject())
        V4THROW_DOM(DOMEXCEPTION_TYPE_MISMATCH_ERR, "Context2D::putImageData, the image data type mismatch");

    qreal dx = ctx->args()[1].toNumber();
    qreal dy = ctx->args()[2].toNumber();
    qreal w, h, dirtyX, dirtyY, dirtyWidth, dirtyHeight;

    if (!qt_is_finite(dx) || !qt_is_finite(dy))
        V4THROW_DOM(DOMEXCEPTION_NOT_SUPPORTED_ERR, "putImageData() : Invalid arguments");

    QV4::Scoped<QQuickJSContext2DImageData> imageData(scope, arg0);
    if (!imageData)
        return ctx->thisObject().asReturnedValue();

    QV4::Scoped<QQuickJSContext2DPixelData> pixelArray(scope, imageData->d()->pixelData.as<QQuickJSContext2DPixelData>());
    if (pixelArray) {
        w = pixelArray->d()->image->width();
        h = pixelArray->d()->image->height();

        if (ctx->argc() == 7) {
            dirtyX = ctx->args()[3].toNumber();
            dirtyY = ctx->args()[4].toNumber();
            dirtyWidth = ctx->args()[5].toNumber();
            dirtyHeight = ctx->args()[6].toNumber();

            if (!qt_is_finite(dirtyX) || !qt_is_finite(dirtyY) || !qt_is_finite(dirtyWidth) || !qt_is_finite(dirtyHeight))
                V4THROW_DOM(DOMEXCEPTION_NOT_SUPPORTED_ERR, "putImageData() : Invalid arguments");


            if (dirtyWidth < 0) {
                dirtyX = dirtyX+dirtyWidth;
                dirtyWidth = -dirtyWidth;
            }

            if (dirtyHeight < 0) {
                dirtyY = dirtyY+dirtyHeight;
                dirtyHeight = -dirtyHeight;
            }

            if (dirtyX < 0) {
                dirtyWidth = dirtyWidth+dirtyX;
                dirtyX = 0;
            }

            if (dirtyY < 0) {
                dirtyHeight = dirtyHeight+dirtyY;
                dirtyY = 0;
            }

            if (dirtyX+dirtyWidth > w) {
                dirtyWidth = w - dirtyX;
            }

            if (dirtyY+dirtyHeight > h) {
                dirtyHeight = h - dirtyY;
            }

            if (dirtyWidth <=0 || dirtyHeight <= 0)
                return ctx->thisObject().asReturnedValue();
        } else {
            dirtyX = 0;
            dirtyY = 0;
            dirtyWidth = w;
            dirtyHeight = h;
        }

        QImage image = pixelArray->d()->image->copy(dirtyX, dirtyY, dirtyWidth, dirtyHeight);
        r->d()->context->buffer()->drawImage(image, QRectF(dirtyX, dirtyY, dirtyWidth, dirtyHeight), QRectF(dx, dy, dirtyWidth, dirtyHeight));
    }
    return ctx->thisObject().asReturnedValue();
}

/*!
    \qmltype CanvasGradient
    \inqmlmodule QtQuick
    \since 5.0
    \ingroup qtquick-canvas
    \brief Provides an opaque CanvasGradient interface
  */

/*!
  \qmlmethod CanvasGradient QtQuick::CanvasGradient::addColorStop(real offsetof, string color)
  Adds a color stop with the given color to the gradient at the given offset.
  0.0 is the offset at one end of the gradient, 1.0 is the offset at the other end.

  For example:
  \code
  var gradient = ctx.createLinearGradient(0, 0, 100, 100);
  gradient.addColorStop(0.3, Qt.rgba(1, 0, 0, 1));
  gradient.addColorStop(0.7, 'rgba(0, 255, 255, 1');
  \endcode
  */
QV4::ReturnedValue QQuickContext2DStyle::gradient_proto_addColorStop(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQuickContext2DStyle> style(scope, ctx->thisObject().as<QQuickContext2DStyle>());
    if (!style)
        V4THROW_ERROR("Not a CanvasGradient object");

    if (ctx->argc() == 2) {

        if (!style->d()->brush->gradient())
            V4THROW_ERROR("Not a valid CanvasGradient object, can't get the gradient information");
        QGradient gradient = *(style->d()->brush->gradient());
        qreal pos = ctx->args()[0].toNumber();
        QColor color;

        if (ctx->args()[1].as<Object>()) {
            color = scope.engine->toVariant(ctx->args()[1], qMetaTypeId<QColor>()).value<QColor>();
        } else {
            color = qt_color_from_string(ctx->args()[1]);
        }
        if (pos < 0.0 || pos > 1.0 || !qt_is_finite(pos)) {
            V4THROW_DOM(DOMEXCEPTION_INDEX_SIZE_ERR, "CanvasGradient: parameter offset out of range");
        }

        if (color.isValid()) {
            gradient.setColorAt(pos, color);
        } else {
            V4THROW_DOM(DOMEXCEPTION_SYNTAX_ERR, "CanvasGradient: parameter color is not a valid color string");
        }
        *style->d()->brush = gradient;
    }

    return ctx->thisObject().asReturnedValue();
}

void QQuickContext2D::scale(qreal x,  qreal y)
{
    if (!state.invertibleCTM)
        return;

    if (!qt_is_finite(x) || !qt_is_finite(y))
        return;

    QTransform newTransform = state.matrix;
    newTransform.scale(x, y);

    if (!newTransform.isInvertible()) {
        state.invertibleCTM = false;
        return;
    }

    state.matrix = newTransform;
    buffer()->updateMatrix(state.matrix);
    m_path = QTransform().scale(1.0 / x, 1.0 / y).map(m_path);
}

void QQuickContext2D::rotate(qreal angle)
{
    if (!state.invertibleCTM)
        return;

    if (!qt_is_finite(angle))
        return;

    QTransform newTransform =state.matrix;
    newTransform.rotate(DEGREES(angle));

    if (!newTransform.isInvertible()) {
        state.invertibleCTM = false;
        return;
    }

    state.matrix = newTransform;
    buffer()->updateMatrix(state.matrix);
    m_path = QTransform().rotate(-DEGREES(angle)).map(m_path);
}

void QQuickContext2D::shear(qreal h, qreal v)
{
    if (!state.invertibleCTM)
        return;

    if (!qt_is_finite(h) || !qt_is_finite(v))
        return ;

    QTransform newTransform = state.matrix;
    newTransform.shear(h, v);

    if (!newTransform.isInvertible()) {
        state.invertibleCTM = false;
        return;
    }

    state.matrix = newTransform;
    buffer()->updateMatrix(state.matrix);
    m_path = QTransform().shear(-h, -v).map(m_path);
}

void QQuickContext2D::translate(qreal x, qreal y)
{
    if (!state.invertibleCTM)
        return;

    if (!qt_is_finite(x) || !qt_is_finite(y))
        return ;

    QTransform newTransform = state.matrix;
    newTransform.translate(x, y);

    if (!newTransform.isInvertible()) {
        state.invertibleCTM = false;
        return;
    }

    state.matrix = newTransform;
    buffer()->updateMatrix(state.matrix);
    m_path = QTransform().translate(-x, -y).map(m_path);
}

void QQuickContext2D::transform(qreal a, qreal b, qreal c, qreal d, qreal e, qreal f)
{
    if (!state.invertibleCTM)
        return;

    if (!qt_is_finite(a) || !qt_is_finite(b) || !qt_is_finite(c) || !qt_is_finite(d) || !qt_is_finite(e) || !qt_is_finite(f))
        return;

    QTransform transform(a, b, c, d, e, f);
    QTransform newTransform = state.matrix * transform;

    if (!newTransform.isInvertible()) {
        state.invertibleCTM = false;
        return;
    }
    state.matrix = newTransform;
    buffer()->updateMatrix(state.matrix);
    m_path = transform.inverted().map(m_path);
}

void QQuickContext2D::setTransform(qreal a, qreal b, qreal c, qreal d, qreal e, qreal f)
{
    if (!qt_is_finite(a) || !qt_is_finite(b) || !qt_is_finite(c) || !qt_is_finite(d) || !qt_is_finite(e) || !qt_is_finite(f))
        return;

    QTransform ctm = state.matrix;
    if (!ctm.isInvertible())
        return;

    state.matrix = ctm.inverted() * state.matrix;
    m_path = ctm.map(m_path);
    state.invertibleCTM = true;
    transform(a, b, c, d, e, f);
}

void QQuickContext2D::fill()
{
    if (!state.invertibleCTM)
        return;

    if (!m_path.elementCount())
        return;

    m_path.setFillRule(state.fillRule);
    buffer()->fill(m_path);
}

void QQuickContext2D::clip()
{
    if (!state.invertibleCTM)
        return;

    QPainterPath clipPath = m_path;
    clipPath.closeSubpath();
    if (state.clip) {
        state.clipPath = clipPath.intersected(state.clipPath);
    } else {
        state.clip = true;
        state.clipPath = clipPath;
    }
    buffer()->clip(state.clip, state.clipPath);
}

void QQuickContext2D::stroke()
{
    if (!state.invertibleCTM)
        return;

    if (!m_path.elementCount())
        return;

    buffer()->stroke(m_path);
}

void QQuickContext2D::fillRect(qreal x, qreal y, qreal w, qreal h)
{
    if (!state.invertibleCTM)
        return;

    if (!qt_is_finite(x) || !qt_is_finite(y) || !qt_is_finite(w) || !qt_is_finite(h))
        return;

    buffer()->fillRect(QRectF(x, y, w, h));
}

void QQuickContext2D::strokeRect(qreal x, qreal y, qreal w, qreal h)
{
    if (!state.invertibleCTM)
        return;

    if (!qt_is_finite(x) || !qt_is_finite(y) || !qt_is_finite(w) || !qt_is_finite(h))
        return;

    buffer()->strokeRect(QRectF(x, y, w, h));
}

void QQuickContext2D::clearRect(qreal x, qreal y, qreal w, qreal h)
{
    if (!state.invertibleCTM)
        return;

    if (!qt_is_finite(x) || !qt_is_finite(y) || !qt_is_finite(w) || !qt_is_finite(h))
        return;

    buffer()->clearRect(QRectF(x, y, w, h));
}

void QQuickContext2D::drawText(const QString& text, qreal x, qreal y, bool fill)
{
    if (!state.invertibleCTM)
        return;

    if (!qt_is_finite(x) || !qt_is_finite(y))
        return;

    QPainterPath textPath = createTextGlyphs(x, y, text);
    if (fill)
        buffer()->fill(textPath);
    else
        buffer()->stroke(textPath);
}


void QQuickContext2D::beginPath()
{
    if (!m_path.elementCount())
        return;
    m_path = QPainterPath();
}

void QQuickContext2D::closePath()
{
    if (!m_path.elementCount())
        return;

    QRectF boundRect = m_path.boundingRect();
    if (boundRect.width() || boundRect.height())
        m_path.closeSubpath();
    //FIXME:QPainterPath set the current point to (0,0) after close subpath
    //should be the first point of the previous subpath
}

void QQuickContext2D::moveTo( qreal x, qreal y)
{
    if (!state.invertibleCTM)
        return;

    //FIXME: moveTo should not close the previous subpath
    m_path.moveTo(QPointF(x, y));
}

void QQuickContext2D::lineTo( qreal x, qreal y)
{
    if (!state.invertibleCTM)
        return;

    QPointF pt(x, y);

    if (!m_path.elementCount())
        m_path.moveTo(pt);
    else if (m_path.currentPosition() != pt)
        m_path.lineTo(pt);
}

void QQuickContext2D::quadraticCurveTo(qreal cpx, qreal cpy,
                                           qreal x, qreal y)
{
    if (!state.invertibleCTM)
        return;

    if (!m_path.elementCount())
        m_path.moveTo(QPointF(cpx, cpy));

    QPointF pt(x, y);
    if (m_path.currentPosition() != pt)
        m_path.quadTo(QPointF(cpx, cpy), pt);
}

void QQuickContext2D::bezierCurveTo(qreal cp1x, qreal cp1y,
                                        qreal cp2x, qreal cp2y,
                                        qreal x, qreal y)
{
    if (!state.invertibleCTM)
        return;

    if (!m_path.elementCount())
        m_path.moveTo(QPointF(cp1x, cp1y));

    QPointF pt(x, y);
    if (m_path.currentPosition() != pt)
        m_path.cubicTo(QPointF(cp1x, cp1y), QPointF(cp2x, cp2y),  pt);
}

void QQuickContext2D::addArcTo(const QPointF& p1, const QPointF& p2, float radius)
{
    QPointF p0(m_path.currentPosition());

    QPointF p1p0((p0.x() - p1.x()), (p0.y() - p1.y()));
    QPointF p1p2((p2.x() - p1.x()), (p2.y() - p1.y()));
    float p1p0_length = std::sqrt(p1p0.x() * p1p0.x() + p1p0.y() * p1p0.y());
    float p1p2_length = std::sqrt(p1p2.x() * p1p2.x() + p1p2.y() * p1p2.y());

    double cos_phi = (p1p0.x() * p1p2.x() + p1p0.y() * p1p2.y()) / (p1p0_length * p1p2_length);

    // The points p0, p1, and p2 are on the same straight line (HTML5, 4.8.11.1.8)
    // We could have used areCollinear() here, but since we're reusing
    // the variables computed above later on we keep this logic.
    if (qFuzzyCompare(std::abs(cos_phi), 1.0)) {
        m_path.lineTo(p1);
        return;
    }

    float tangent = radius / std::tan(std::acos(cos_phi) / 2);
    float factor_p1p0 = tangent / p1p0_length;
    QPointF t_p1p0((p1.x() + factor_p1p0 * p1p0.x()), (p1.y() + factor_p1p0 * p1p0.y()));

    QPointF orth_p1p0(p1p0.y(), -p1p0.x());
    float orth_p1p0_length = std::sqrt(orth_p1p0.x() * orth_p1p0.x() + orth_p1p0.y() * orth_p1p0.y());
    float factor_ra = radius / orth_p1p0_length;

    // angle between orth_p1p0 and p1p2 to get the right vector orthographic to p1p0
    double cos_alpha = (orth_p1p0.x() * p1p2.x() + orth_p1p0.y() * p1p2.y()) / (orth_p1p0_length * p1p2_length);
    if (cos_alpha < 0.f)
        orth_p1p0 = QPointF(-orth_p1p0.x(), -orth_p1p0.y());

    QPointF p((t_p1p0.x() + factor_ra * orth_p1p0.x()), (t_p1p0.y() + factor_ra * orth_p1p0.y()));

    // calculate angles for addArc
    orth_p1p0 = QPointF(-orth_p1p0.x(), -orth_p1p0.y());
    float sa = std::acos(orth_p1p0.x() / orth_p1p0_length);
    if (orth_p1p0.y() < 0.f)
        sa = 2 * M_PI - sa;

    // anticlockwise logic
    bool anticlockwise = false;

    float factor_p1p2 = tangent / p1p2_length;
    QPointF t_p1p2((p1.x() + factor_p1p2 * p1p2.x()), (p1.y() + factor_p1p2 * p1p2.y()));
    QPointF orth_p1p2((t_p1p2.x() - p.x()), (t_p1p2.y() - p.y()));
    float orth_p1p2_length = std::sqrt(orth_p1p2.x() * orth_p1p2.x() + orth_p1p2.y() * orth_p1p2.y());
    float ea = std::acos(orth_p1p2.x() / orth_p1p2_length);
    if (orth_p1p2.y() < 0)
        ea = 2 * M_PI - ea;
    if ((sa > ea) && ((sa - ea) < M_PI))
        anticlockwise = true;
    if ((sa < ea) && ((ea - sa) > M_PI))
        anticlockwise = true;

    arc(p.x(), p.y(), radius, sa, ea, anticlockwise);
}

void QQuickContext2D::arcTo(qreal x1, qreal y1,
                                qreal x2, qreal y2,
                                qreal radius)
{
    if (!state.invertibleCTM)
        return;

    if (!qt_is_finite(x1) || !qt_is_finite(y1) || !qt_is_finite(x2) || !qt_is_finite(y2) || !qt_is_finite(radius))
        return;

    QPointF st(x1, y1);
    QPointF end(x2, y2);

    if (!m_path.elementCount())
        m_path.moveTo(st);
    else if (st == m_path.currentPosition() || st == end || !radius)
        lineTo(x1, y1);
    else
        addArcTo(st, end, radius);
 }

void QQuickContext2D::rect(qreal x, qreal y, qreal w, qreal h)
{
    if (!state.invertibleCTM)
        return;
    if (!qt_is_finite(x) || !qt_is_finite(y) || !qt_is_finite(w) || !qt_is_finite(h))
        return;

    if (!w && !h) {
        m_path.moveTo(x, y);
        return;
    }
    m_path.addRect(x, y, w, h);
}

void QQuickContext2D::roundedRect(qreal x, qreal y,
                               qreal w, qreal h,
                               qreal xr, qreal yr)
{
    if (!state.invertibleCTM)
        return;

    if (!qt_is_finite(x) || !qt_is_finite(y) || !qt_is_finite(w) || !qt_is_finite(h) || !qt_is_finite(xr) || !qt_is_finite(yr))
        return;

    if (!w && !h) {
        m_path.moveTo(x, y);
        return;
    }
    m_path.addRoundedRect(QRectF(x, y, w, h), xr, yr, Qt::AbsoluteSize);
}

void QQuickContext2D::ellipse(qreal x, qreal y,
                           qreal w, qreal h)
{
    if (!state.invertibleCTM)
        return;

    if (!qt_is_finite(x) || !qt_is_finite(y) || !qt_is_finite(w) || !qt_is_finite(h))
        return;

    if (!w && !h) {
        m_path.moveTo(x, y);
        return;
    }

    m_path.addEllipse(x, y, w, h);
}

void QQuickContext2D::text(const QString& str, qreal x, qreal y)
{
    if (!state.invertibleCTM)
        return;

    QPainterPath path;
    path.addText(x, y, state.font, str);
    m_path.addPath(path);
}

void QQuickContext2D::arc(qreal xc, qreal yc, qreal radius, qreal sar, qreal ear, bool antiClockWise)
{
    if (!state.invertibleCTM)
        return;

    if (!qt_is_finite(xc) || !qt_is_finite(yc) || !qt_is_finite(sar) || !qt_is_finite(ear) || !qt_is_finite(radius))
        return;

    if (sar == ear)
        return;


    //### HACK

    // In Qt we don't switch the coordinate system for degrees
    // and still use the 0,0 as bottom left for degrees so we need
    // to switch
    sar = -sar;
    ear = -ear;
    antiClockWise = !antiClockWise;
    //end hack

    float sa = DEGREES(sar);
    float ea = DEGREES(ear);

    double span = 0;

    double xs     = xc - radius;
    double ys     = yc - radius;
    double width  = radius*2;
    double height = radius*2;
    if ((!antiClockWise && (ea - sa >= 360)) || (antiClockWise && (sa - ea >= 360)))
        // If the anticlockwise argument is false and endAngle-startAngle is equal to or greater than 2*PI, or, if the
        // anticlockwise argument is true and startAngle-endAngle is equal to or greater than 2*PI, then the arc is the whole
        // circumference of this circle.
        span = 360;
    else {
        if (!antiClockWise && (ea < sa)) {
            span += 360;
        } else if (antiClockWise && (sa < ea)) {
            span -= 360;
        }
        //### this is also due to switched coordinate system
        // we would end up with a 0 span instead of 360
        if (!(qFuzzyCompare(span + (ea - sa) + 1, 1) &&
              qFuzzyCompare(qAbs(span), 360))) {
            span   += ea - sa;
        }
    }

    // If the path is empty, move to where the arc will start to avoid painting a line from (0,0)
    if (!m_path.elementCount())
        m_path.arcMoveTo(xs, ys, width, height, sa);
    else if (!radius) {
        m_path.lineTo(xc, yc);
        return;
    }

    m_path.arcTo(xs, ys, width, height, sa, span);
}

int baseLineOffset(QQuickContext2D::TextBaseLineType value, const QFontMetrics &metrics)
{
    int offset = 0;
    switch (value) {
    case QQuickContext2D::Top:
    case QQuickContext2D::Hanging:
        break;
    case QQuickContext2D::Middle:
        offset = (metrics.ascent() >> 1) + metrics.height() - metrics.ascent();
        break;
    case QQuickContext2D::Alphabetic:
        offset = metrics.ascent();
        break;
    case QQuickContext2D::Bottom:
        offset = metrics.height();
       break;
    }
    return offset;
}

static int textAlignOffset(QQuickContext2D::TextAlignType value, const QFontMetrics &metrics, const QString &text)
{
    int offset = 0;
    if (value == QQuickContext2D::Start)
        value = QGuiApplication::layoutDirection() == Qt::LeftToRight ? QQuickContext2D::Left : QQuickContext2D::Right;
    else if (value == QQuickContext2D::End)
        value = QGuiApplication::layoutDirection() == Qt::LeftToRight ? QQuickContext2D::Right: QQuickContext2D::Left;
    switch (value) {
    case QQuickContext2D::Center:
        offset = metrics.width(text)/2;
        break;
    case QQuickContext2D::Right:
        offset = metrics.width(text);
    case QQuickContext2D::Left:
    default:
        break;
    }
    return offset;
}

void QQuickContext2D::setGrabbedImage(const QImage& grab)
{
    m_grabbedImage = grab;
    m_grabbed = true;
}

QQmlRefPointer<QQuickCanvasPixmap> QQuickContext2D::createPixmap(const QUrl& url)
{
    return m_canvas->loadedPixmap(url);
}

QPainterPath QQuickContext2D::createTextGlyphs(qreal x, qreal y, const QString& text)
{
    const QFontMetrics metrics(state.font);
    int yoffset = baseLineOffset(static_cast<QQuickContext2D::TextBaseLineType>(state.textBaseline), metrics);
    int xoffset = textAlignOffset(static_cast<QQuickContext2D::TextAlignType>(state.textAlign), metrics, text);

    QPainterPath textPath;

    textPath.addText(x - xoffset, y - yoffset+metrics.ascent(), state.font, text);
    return textPath;
}


static inline bool areCollinear(const QPointF& a, const QPointF& b, const QPointF& c)
{
    // Solved from comparing the slopes of a to b and b to c: (ay-by)/(ax-bx) == (cy-by)/(cx-bx)
    return qFuzzyCompare((c.y() - b.y()) * (a.x() - b.x()), (a.y() - b.y()) * (c.x() - b.x()));
}

static inline bool withinRange(qreal p, qreal a, qreal b)
{
    return (p >= a && p <= b) || (p >= b && p <= a);
}

bool QQuickContext2D::isPointInPath(qreal x, qreal y) const
{
    if (!state.invertibleCTM)
        return false;

    if (!m_path.elementCount())
        return false;

    if (!qt_is_finite(x) || !qt_is_finite(y))
        return false;

    QPointF point(x, y);
    QTransform ctm = state.matrix;
    QPointF p = ctm.inverted().map(point);
    if (!qt_is_finite(p.x()) || !qt_is_finite(p.y()))
        return false;

    const_cast<QQuickContext2D *>(this)->m_path.setFillRule(state.fillRule);

    bool contains = m_path.contains(p);

    if (!contains) {
        // check whether the point is on the border
        QPolygonF border = m_path.toFillPolygon();

        QPointF p1 = border.at(0);
        QPointF p2;

        for (int i = 1; i < border.size(); ++i) {
            p2 = border.at(i);
            if (areCollinear(p, p1, p2)
                    // Once we know that the points are collinear we
                    // only need to check one of the coordinates
                    && (qAbs(p2.x() - p1.x()) > qAbs(p2.y() - p1.y()) ?
                        withinRange(p.x(), p1.x(), p2.x()) :
                        withinRange(p.y(), p1.y(), p2.y()))) {
                return true;
            }
            p1 = p2;
        }
    }
    return contains;
}

class QQuickContext2DThreadCleanup : public QObject
{
public:
    QQuickContext2DThreadCleanup(QOpenGLContext *gl, QQuickContext2DTexture *t, QOffscreenSurface *s)
        : context(gl), texture(t), surface(s)
    { }

    ~QQuickContext2DThreadCleanup()
    {
#ifndef QT_NO_OPENGL
        context->makeCurrent(surface);
        delete texture;
        context->doneCurrent();
        delete context;
#endif
        surface->deleteLater();
    }

    QOpenGLContext *context;
    QQuickContext2DTexture *texture;
    QOffscreenSurface *surface;
};

class QQuickContext2DTextureCleanup : public QRunnable
{
public:
    QQuickContext2DTexture *texture;
    void run() Q_DECL_OVERRIDE { delete texture; }
};

QMutex QQuickContext2D::mutex;

QQuickContext2D::QQuickContext2D(QObject *parent)
    : QQuickCanvasContext(parent)
    , m_buffer(new QQuickContext2DCommandBuffer)
    , m_v4engine(0)
    , m_surface(0)
    , m_glContext(0)
    , m_thread(0)
    , m_grabbed(false)
{
}

QQuickContext2D::~QQuickContext2D()
{
    mutex.lock();
    m_texture->setItem(0);
    delete m_buffer;

    if (m_renderTarget == QQuickCanvasItem::FramebufferObject) {
#ifndef QT_NO_OPENGL
        if (m_renderStrategy == QQuickCanvasItem::Immediate && m_glContext) {
            Q_ASSERT(QThread::currentThread() == m_glContext->thread());
            m_glContext->makeCurrent(m_surface.data());
            delete m_texture;
            m_glContext->doneCurrent();
            delete m_glContext;
        } else if (m_texture->isOnCustomThread()) {
            Q_ASSERT(m_glContext);
            QQuickContext2DThreadCleanup *cleaner = new QQuickContext2DThreadCleanup(m_glContext, m_texture, m_surface.take());
            cleaner->moveToThread(m_texture->thread());
            cleaner->deleteLater();
        } else {
            if (m_canvas->window()) {
                QQuickContext2DTextureCleanup *c = new QQuickContext2DTextureCleanup;
                c->texture = m_texture;
                m_canvas->window()->scheduleRenderJob(c, QQuickWindow::AfterSynchronizingStage);
            } else {
                m_texture->deleteLater();
            }
        }
#endif
    } else {
        // Image based does not have GL resources, but must still be deleted
        // on its designated thread after it has completed whatever it might
        // currently be doing.
        m_texture->deleteLater();
    }
    mutex.unlock();
}

QV4::ReturnedValue QQuickContext2D::v4value() const
{
    return m_v4value.value();
}

QStringList QQuickContext2D::contextNames() const
{
    return QStringList() << QStringLiteral("2d");
}

void QQuickContext2D::init(QQuickCanvasItem *canvasItem, const QVariantMap &args)
{
    Q_UNUSED(args);

    m_canvas = canvasItem;
    m_renderTarget = canvasItem->renderTarget();
    m_renderStrategy = canvasItem->renderStrategy();

#ifdef Q_OS_WIN
    if (m_renderTarget == QQuickCanvasItem::FramebufferObject
            && (m_renderStrategy != QQuickCanvasItem::Cooperative)) {
        // On windows a context needs to be unbound set up sharing, so
        // for simplicity we disallow FBO + !coop here.
        m_renderTarget = QQuickCanvasItem::Image;
    }
#endif

    // Disable threaded background rendering if the platform has issues with it
    if (m_renderTarget == QQuickCanvasItem::FramebufferObject
            && m_renderStrategy == QQuickCanvasItem::Threaded
            && !QGuiApplicationPrivate::platformIntegration()->hasCapability(QPlatformIntegration::ThreadedOpenGL)) {
        m_renderTarget = QQuickCanvasItem::Image;
    }

    // Disable Framebuffer Object based rendering when not running with OpenGL
    if (m_renderTarget == QQuickCanvasItem::FramebufferObject) {
        QSGRendererInterface *rif = canvasItem->window()->rendererInterface();
        if (rif && rif->graphicsApi() != QSGRendererInterface::OpenGL)
            m_renderTarget = QQuickCanvasItem::Image;
    }

    switch (m_renderTarget) {
    case QQuickCanvasItem::Image:
        m_texture = new QQuickContext2DImageTexture;
        break;
    case QQuickCanvasItem::FramebufferObject:
#ifndef QT_NO_OPENGL
        m_texture = new QQuickContext2DFBOTexture;
#else
        // It shouldn't be possible to use a FramebufferObject without OpenGL
        m_texture = nullptr;
#endif
        break;
    }

    m_texture->setItem(canvasItem);
    m_texture->setCanvasWindow(canvasItem->canvasWindow().toRect());
    m_texture->setTileSize(canvasItem->tileSize());
    m_texture->setCanvasSize(canvasItem->canvasSize().toSize());
    m_texture->setSmooth(canvasItem->smooth());
    m_texture->setAntialiasing(canvasItem->antialiasing());
    m_texture->setOnCustomThread(m_renderStrategy == QQuickCanvasItem::Threaded);
    m_thread = QThread::currentThread();

    QThread *renderThread = m_thread;
#ifndef QT_NO_OPENGL
    QQuickWindow *window = canvasItem->window();
    QQuickWindowPrivate *wd = QQuickWindowPrivate::get(window);
    QThread *sceneGraphThread = wd->context->thread();

    if (m_renderStrategy == QQuickCanvasItem::Threaded)
        renderThread = QQuickContext2DRenderThread::instance(qmlEngine(canvasItem));
    else if (m_renderStrategy == QQuickCanvasItem::Cooperative)
        renderThread = sceneGraphThread;
#else
    if (m_renderStrategy == QQuickCanvasItem::Threaded)
        renderThread = QQuickContext2DRenderThread::instance(qmlEngine(canvasItem));
#endif


    if (renderThread && renderThread != QThread::currentThread())
        m_texture->moveToThread(renderThread);
#ifndef QT_NO_OPENGL
    if (m_renderTarget == QQuickCanvasItem::FramebufferObject && renderThread != sceneGraphThread) {
         auto openglRenderContext = static_cast<const QSGDefaultRenderContext *>(QQuickWindowPrivate::get(window)->context);
         QOpenGLContext *cc = openglRenderContext->openglContext();
         m_surface.reset(new QOffscreenSurface);
         m_surface->setFormat(window->format());
         m_surface->create();
         m_glContext = new QOpenGLContext;
         m_glContext->setFormat(cc->format());
         m_glContext->setShareContext(cc);
         if (renderThread != QThread::currentThread())
             m_glContext->moveToThread(renderThread);
         m_texture->initializeOpenGL(m_glContext, m_surface.data());
    }
#endif
    connect(m_texture, SIGNAL(textureChanged()), SIGNAL(textureChanged()));

    reset();
}

void QQuickContext2D::prepare(const QSize& canvasSize, const QSize& tileSize, const QRect& canvasWindow, const QRect& dirtyRect, bool smooth, bool antialiasing)
{
    if (m_texture->thread() == QThread::currentThread()) {
        m_texture->canvasChanged(canvasSize, tileSize, canvasWindow, dirtyRect, smooth, antialiasing);
    } else {
        QEvent *e =  new QQuickContext2DTexture::CanvasChangeEvent(canvasSize,
                                                                   tileSize,
                                                                   canvasWindow,
                                                                   dirtyRect,
                                                                   smooth,
                                                                   antialiasing);
        QCoreApplication::postEvent(m_texture, e);
    }
}

void QQuickContext2D::flush()
{
    if (m_buffer) {
        if (m_texture->thread() == QThread::currentThread())
            m_texture->paint(m_buffer);
        else
            QCoreApplication::postEvent(m_texture, new QQuickContext2DTexture::PaintEvent(m_buffer));
    }
    m_buffer = new QQuickContext2DCommandBuffer();
}

QQuickContext2DTexture *QQuickContext2D::texture() const
{
    return m_texture;
}

QImage QQuickContext2D::toImage(const QRectF& bounds)
{
    if (m_texture->thread() == QThread::currentThread()) {
        // if we're either not rendering to an fbo or we have a separate opengl context we can just
        // flush. Otherwise we have to make sure the shared opengl context is current before we do
        // so. It may or may not be current already, depending on how this method is called.
        if (m_renderTarget != QQuickCanvasItem::FramebufferObject || m_glContext) {
            flush();
            m_texture->grabImage(bounds);
        } else {
#ifndef QT_NO_OPENGL
            QQuickWindow *window = m_canvas->window();
            QOpenGLContext *ctx =  window ? window->openglContext() : 0;
            if (ctx && ctx->isValid()) {
                if (ctx == QOpenGLContext::currentContext()) {
                    flush();
                } else {
                    ctx->makeCurrent(window);
                    flush();
                    ctx->doneCurrent();
                }
                m_texture->grabImage(bounds);
            } else {
                qWarning() << "Cannot read pixels from canvas before opengl context is valid";
                return QImage();
            }
#else
            flush();
            m_texture->grabImage(bounds);
#endif
        }
    } else if (m_renderStrategy == QQuickCanvasItem::Cooperative) {
        qWarning() << "Pixel readback is not supported in Cooperative mode, please try Threaded or Immediate mode";
        return QImage();
    } else {
        flush();
        QCoreApplication::postEvent(m_texture, new QEvent(QEvent::Type(QEvent::User + 10)));
        QMetaObject::invokeMethod(m_texture,
                                  "grabImage",
                                  Qt::BlockingQueuedConnection,
                                  Q_ARG(QRectF, bounds));
    }
    QImage img = m_grabbedImage;
    m_grabbedImage = QImage();
    m_grabbed = false;
    return img;
}


QQuickContext2DEngineData::QQuickContext2DEngineData(QV4::ExecutionEngine *v4)
{
    QV4::Scope scope(v4);

    QV4::ScopedObject proto(scope, QQuickJSContext2DPrototype::create(v4));
    proto->defineAccessorProperty(QStringLiteral("strokeStyle"), QQuickJSContext2D::method_get_strokeStyle, QQuickJSContext2D::method_set_strokeStyle);
    proto->defineAccessorProperty(QStringLiteral("font"), QQuickJSContext2D::method_get_font, QQuickJSContext2D::method_set_font);
    proto->defineAccessorProperty(QStringLiteral("fillRule"), QQuickJSContext2D::method_get_fillRule, QQuickJSContext2D::method_set_fillRule);
    proto->defineAccessorProperty(QStringLiteral("globalAlpha"), QQuickJSContext2D::method_get_globalAlpha, QQuickJSContext2D::method_set_globalAlpha);
    proto->defineAccessorProperty(QStringLiteral("lineCap"), QQuickJSContext2D::method_get_lineCap, QQuickJSContext2D::method_set_lineCap);
    proto->defineAccessorProperty(QStringLiteral("shadowOffsetX"), QQuickJSContext2D::method_get_shadowOffsetX, QQuickJSContext2D::method_set_shadowOffsetX);
    proto->defineAccessorProperty(QStringLiteral("shadowOffsetY"), QQuickJSContext2D::method_get_shadowOffsetY, QQuickJSContext2D::method_set_shadowOffsetY);
    proto->defineAccessorProperty(QStringLiteral("globalCompositeOperation"), QQuickJSContext2D::method_get_globalCompositeOperation, QQuickJSContext2D::method_set_globalCompositeOperation);
    proto->defineAccessorProperty(QStringLiteral("miterLimit"), QQuickJSContext2D::method_get_miterLimit, QQuickJSContext2D::method_set_miterLimit);
    proto->defineAccessorProperty(QStringLiteral("fillStyle"), QQuickJSContext2D::method_get_fillStyle, QQuickJSContext2D::method_set_fillStyle);
    proto->defineAccessorProperty(QStringLiteral("shadowColor"), QQuickJSContext2D::method_get_shadowColor, QQuickJSContext2D::method_set_shadowColor);
    proto->defineAccessorProperty(QStringLiteral("textBaseline"), QQuickJSContext2D::method_get_textBaseline, QQuickJSContext2D::method_set_textBaseline);
    proto->defineAccessorProperty(QStringLiteral("path"), QQuickJSContext2D::method_get_path, QQuickJSContext2D::method_set_path);
    proto->defineAccessorProperty(QStringLiteral("lineJoin"), QQuickJSContext2D::method_get_lineJoin, QQuickJSContext2D::method_set_lineJoin);
    proto->defineAccessorProperty(QStringLiteral("lineWidth"), QQuickJSContext2D::method_get_lineWidth, QQuickJSContext2D::method_set_lineWidth);
    proto->defineAccessorProperty(QStringLiteral("textAlign"), QQuickJSContext2D::method_get_textAlign, QQuickJSContext2D::method_set_textAlign);
    proto->defineAccessorProperty(QStringLiteral("shadowBlur"), QQuickJSContext2D::method_get_shadowBlur, QQuickJSContext2D::method_set_shadowBlur);
    contextPrototype = proto;

    proto = scope.engine->newObject();
    proto->defineDefaultProperty(QStringLiteral("addColorStop"), QQuickContext2DStyle::gradient_proto_addColorStop, 0);
    gradientProto = proto;

    proto = scope.engine->newObject();
    proto->defineAccessorProperty(scope.engine->id_length(), QQuickJSContext2DPixelData::proto_get_length, 0);
    pixelArrayProto = proto;
}

QQuickContext2DEngineData::~QQuickContext2DEngineData()
{
}

void QQuickContext2D::popState()
{
    if (m_stateStack.isEmpty())
        return;

    QQuickContext2D::State newState = m_stateStack.pop();

    if (state.matrix != newState.matrix)
        buffer()->updateMatrix(newState.matrix);

    if (newState.globalAlpha != state.globalAlpha)
        buffer()->setGlobalAlpha(newState.globalAlpha);

    if (newState.globalCompositeOperation != state.globalCompositeOperation)
        buffer()->setGlobalCompositeOperation(newState.globalCompositeOperation);

    if (newState.fillStyle != state.fillStyle)
        buffer()->setFillStyle(newState.fillStyle);

    if (newState.strokeStyle != state.strokeStyle)
        buffer()->setStrokeStyle(newState.strokeStyle);

    if (newState.lineWidth != state.lineWidth)
        buffer()->setLineWidth(newState.lineWidth);

    if (newState.lineCap != state.lineCap)
        buffer()->setLineCap(newState.lineCap);

    if (newState.lineJoin != state.lineJoin)
        buffer()->setLineJoin(newState.lineJoin);

    if (newState.miterLimit != state.miterLimit)
        buffer()->setMiterLimit(newState.miterLimit);

    if (newState.clip != state.clip || newState.clipPath != state.clipPath)
        buffer()->clip(newState.clip, newState.clipPath);

    if (newState.shadowBlur != state.shadowBlur)
        buffer()->setShadowBlur(newState.shadowBlur);

    if (newState.shadowColor != state.shadowColor)
        buffer()->setShadowColor(newState.shadowColor);

    if (newState.shadowOffsetX != state.shadowOffsetX)
        buffer()->setShadowOffsetX(newState.shadowOffsetX);

    if (newState.shadowOffsetY != state.shadowOffsetY)
        buffer()->setShadowOffsetY(newState.shadowOffsetY);
    m_path = state.matrix.map(m_path);
    state = newState;
    m_path = state.matrix.inverted().map(m_path);
}
void QQuickContext2D::pushState()
{
    m_stateStack.push(state);
}

void QQuickContext2D::reset()
{
    QQuickContext2D::State newState;

    m_path = QPainterPath();

    newState.clipPath.setFillRule(Qt::WindingFill);

    m_stateStack.clear();
    m_stateStack.push(newState);
    popState();
    m_buffer->clearRect(QRectF(0, 0, m_canvas->width(), m_canvas->height()));
}

void QQuickContext2D::setV4Engine(QV4::ExecutionEngine *engine)
{
    if (m_v4engine != engine) {
        m_v4engine = engine;

        if (m_v4engine == 0)
            return;

        QQuickContext2DEngineData *ed = engineData(engine);
        QV4::Scope scope(engine);
        QV4::Scoped<QQuickJSContext2D> wrapper(scope, engine->memoryManager->allocObject<QQuickJSContext2D>());
        QV4::ScopedObject p(scope, ed->contextPrototype.value());
        wrapper->setPrototype(p);
        wrapper->d()->context = this;
        m_v4value = wrapper;
    }
}

QT_END_NAMESPACE
