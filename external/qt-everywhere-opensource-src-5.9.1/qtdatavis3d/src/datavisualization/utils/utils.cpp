/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "utils_p.h"

#include <QtGui/QPainter>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOffscreenSurface>
#include <QtCore/QCoreApplication>

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

#define NUM_IN_POWER(y, x) for (;y<x;y<<=1)
#define MIN_POWER 2

// Some values that only need to be resolved once
static bool staticsResolved = false;
static GLint maxTextureSize = 0;
static bool isES = false;

GLuint Utils::getNearestPowerOfTwo(GLuint value)
{
    GLuint powOfTwoValue = MIN_POWER;
    NUM_IN_POWER(powOfTwoValue, value);
    return powOfTwoValue;
}

QVector4D Utils::vectorFromColor(const QColor &color)
{
    return QVector4D(color.redF(), color.greenF(), color.blueF(), color.alphaF());
}

QColor Utils::colorFromVector(const QVector3D &colorVector)
{
    return QColor(colorVector.x() * 255.0f, colorVector.y() * 255.0f,
                  colorVector.z() * 255.0f, 255.0f);
}

QColor Utils::colorFromVector(const QVector4D &colorVector)
{
    return QColor(colorVector.x() * 255.0f, colorVector.y() * 255.0f,
                  colorVector.z() * 255.0f, colorVector.w() * 255.0f);
}

QImage Utils::printTextToImage(const QFont &font, const QString &text, const QColor &bgrColor,
                               const QColor &txtColor, bool labelBackground,
                               bool borders, int maxLabelWidth)
{
    if (!staticsResolved)
        resolveStatics();

    GLuint paddingWidth = 20;
    GLuint paddingHeight = 20;
    GLuint prePadding = 20;
    GLint targetWidth = maxTextureSize;

    // Calculate text dimensions
    QFont valueFont = font;
    valueFont.setPointSize(textureFontSize);
    QFontMetrics valueFM(valueFont);
    int valueStrWidth = valueFM.width(text);

    // ES2 needs to use maxLabelWidth always (when given) because of the power of 2 -issue.
    if (maxLabelWidth && (labelBackground || Utils::isOpenGLES()))
        valueStrWidth = maxLabelWidth;
    int valueStrHeight = valueFM.height();
    valueStrWidth += paddingWidth / 2; // Fix clipping problem with skewed fonts (italic or italic-style)
    QSize labelSize;
    qreal fontRatio = 1.0;

    if (Utils::isOpenGLES()) {
        // Test if text with slighly smaller font would fit into one step smaller texture
        // ie. if the text is just exceeded the smaller texture boundary, it would
        // make a label with large empty space
        uint testWidth = getNearestPowerOfTwo(valueStrWidth + prePadding) >> 1;
        int diffToFit = (valueStrWidth + prePadding) - testWidth;
        int maxSqueeze = int((valueStrWidth + prePadding) * 0.25f);
        if (diffToFit < maxSqueeze && maxTextureSize > GLint(testWidth))
            targetWidth = testWidth;
    }

    bool sizeOk = false;
    int currentFontSize = textureFontSize;
    do {
        if (Utils::isOpenGLES()) {
            // ES2 can't handle textures with dimensions not in power of 2. Resize labels accordingly.
            // Add some padding before converting to power of two to avoid too tight fit
            labelSize = QSize(valueStrWidth + prePadding, valueStrHeight + prePadding);
            labelSize.setWidth(getNearestPowerOfTwo(labelSize.width()));
            labelSize.setHeight(getNearestPowerOfTwo(labelSize.height()));
        } else {
            if (!labelBackground)
                labelSize = QSize(valueStrWidth, valueStrHeight);
            else
                labelSize = QSize(valueStrWidth + paddingWidth * 2, valueStrHeight + paddingHeight * 2);
        }

        if (!maxTextureSize || (labelSize.width() <= maxTextureSize
                                && (labelSize.width() <= targetWidth || !Utils::isOpenGLES()))) {
            // Make sure the label is not too wide
            sizeOk = true;
        } else if (--currentFontSize == 4) {
            qCritical() << "Label" << text << "is too long to be generated.";
            return QImage();
        } else {
            fontRatio = (qreal)currentFontSize / (qreal)textureFontSize;
            // Reduce font size and try again
            valueFont.setPointSize(currentFontSize);
            QFontMetrics currentValueFM(valueFont);
            if (maxLabelWidth && (labelBackground || Utils::isOpenGLES()))
                valueStrWidth = maxLabelWidth * fontRatio;
            else
                valueStrWidth = currentValueFM.width(text);
            valueStrHeight = currentValueFM.height();
            valueStrWidth += paddingWidth / 2;
        }
    } while (!sizeOk);

    // Create image
    QImage image = QImage(labelSize, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    // Init painter
    QPainter painter(&image);
    // Paint text
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.setFont(valueFont);
    if (!labelBackground) {
        painter.setPen(txtColor);
        if (Utils::isOpenGLES()) {
            painter.drawText((labelSize.width() - valueStrWidth) / 2.0f,
                             (labelSize.height() - valueStrHeight) / 2.0f,
                             valueStrWidth, valueStrHeight,
                             Qt::AlignCenter | Qt::AlignVCenter,
                             text);
        } else {
            painter.drawText(0, 0,
                             valueStrWidth, valueStrHeight,
                             Qt::AlignCenter | Qt::AlignVCenter,
                             text);
        }
    } else {
        painter.setBrush(QBrush(bgrColor));
        qreal radius = 10.0 * fontRatio;
        if (borders) {
            painter.setPen(QPen(QBrush(txtColor), 5.0 * fontRatio,
                                Qt::SolidLine, Qt::SquareCap, Qt::RoundJoin));
            painter.drawRoundedRect(5, 5,
                                    labelSize.width() - 10, labelSize.height() - 10,
                                    radius, radius);
        } else {
            painter.setPen(bgrColor);
            painter.drawRoundedRect(0, 0, labelSize.width(), labelSize.height(), radius, radius);
        }
        painter.setPen(txtColor);
        painter.drawText((labelSize.width() - valueStrWidth) / 2.0f,
                         (labelSize.height() - valueStrHeight) / 2.0f,
                         valueStrWidth, valueStrHeight,
                         Qt::AlignCenter | Qt::AlignVCenter,
                         text);
    }
    return image;
}

QVector4D Utils::getSelection(QPoint mousepos, int height)
{
    // This is the only one that works with OpenGL ES 2.0, so we're forced to use it
    // Item count will be limited to 256*256*256
    GLubyte pixel[4] = {255, 255, 255, 255};
    QOpenGLContext::currentContext()->functions()->glReadPixels(mousepos.x(), height - mousepos.y(),
                                                                1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                                                                (void *)pixel);
    QVector4D selectedColor(pixel[0], pixel[1], pixel[2], pixel[3]);
    return selectedColor;
}

QImage Utils::getGradientImage(QLinearGradient &gradient)
{
    QImage image(QSize(gradientTextureWidth, gradientTextureHeight), QImage::Format_RGB32);
    gradient.setFinalStop(qreal(gradientTextureWidth), qreal(gradientTextureHeight));
    gradient.setStart(0.0, 0.0);

    QPainter pmp(&image);
    pmp.setBrush(QBrush(gradient));
    pmp.setPen(Qt::NoPen);
    pmp.drawRect(0, 0, int(gradientTextureWidth), int(gradientTextureHeight));
    return image;
}

Utils::ParamType Utils::preParseFormat(const QString &format, QString &preStr, QString &postStr,
                                       int &precision, char &formatSpec)
{
    static QRegExp formatMatcher(QStringLiteral("^([^%]*)%([\\-\\+#\\s\\d\\.lhjztL]*)([dicuoxfegXFEG])(.*)$"));
    static QRegExp precisionMatcher(QStringLiteral("\\.(\\d+)"));

    Utils::ParamType retVal;

    if (formatMatcher.indexIn(format, 0) != -1) {
        preStr = formatMatcher.cap(1);
        // Six and 'g' are defaults in Qt API
        precision = 6;
        if (!formatMatcher.cap(2).isEmpty()) {
            if (precisionMatcher.indexIn(formatMatcher.cap(2), 0) != -1)
                precision = precisionMatcher.cap(1).toInt();
        }
        if (formatMatcher.cap(3).isEmpty())
            formatSpec = 'g';
        else
            formatSpec = formatMatcher.cap(3).at(0).toLatin1();
        postStr = formatMatcher.cap(4);
        retVal = mapFormatCharToParamType(formatSpec);
    } else {
        retVal = ParamTypeUnknown;
        // The out parameters are irrelevant in unknown case
    }

    return retVal;
}

Utils::ParamType Utils::mapFormatCharToParamType(char formatSpec)
{
    ParamType retVal = ParamTypeUnknown;
    if (formatSpec == 'd' || formatSpec == 'i' || formatSpec == 'c') {
        retVal = ParamTypeInt;
    } else if (formatSpec == 'u' || formatSpec == 'o'
               || formatSpec == 'x'|| formatSpec == 'X') {
        retVal = ParamTypeUInt;
    } else if (formatSpec == 'f' || formatSpec == 'F'
               || formatSpec == 'e' || formatSpec == 'E'
               || formatSpec == 'g' || formatSpec == 'G') {
        retVal = ParamTypeReal;
    }

    return retVal;
}

QString Utils::formatLabelSprintf(const QByteArray &format, Utils::ParamType paramType, qreal value)
{
    switch (paramType) {
    case ParamTypeInt:
        return QString().sprintf(format, (qint64)value);
    case ParamTypeUInt:
        return QString().sprintf(format, (quint64)value);
    case ParamTypeReal:
        return QString().sprintf(format, value);
    default:
        // Return format string to detect errors. Bars selection label logic also depends on this.
        return QString::fromUtf8(format);
    }
}

QString Utils::formatLabelLocalized(Utils::ParamType paramType, qreal value,
                                    const QLocale &locale, const QString &preStr, const QString &postStr,
                                    int precision, char formatSpec, const QByteArray &format)
{
    switch (paramType) {
    case ParamTypeInt:
    case ParamTypeUInt:
        return preStr + locale.toString(qint64(value)) + postStr;
    case ParamTypeReal:
        return preStr + locale.toString(value, formatSpec, precision) + postStr;
    default:
        // Return format string to detect errors. Bars selection label logic also depends on this.
        return QString::fromUtf8(format);
    }
}

QString Utils::defaultLabelFormat()
{
    static const QString defaultFormat(QStringLiteral("%.2f"));
    return defaultFormat;
}

float Utils::wrapValue(float value, float min, float max)
{
    if (value > max) {
        value = min + (value - max);

        // In case single wrap fails, jump to opposite end.
        if (value > max)
            value = min;
    }

    if (value < min) {
        value = max + (value - min);

        // In case single wrap fails, jump to opposite end.
        if (value < min)
            value = max;
    }

    return value;
}

QQuaternion Utils::calculateRotation(const QVector3D &xyzRotations)
{
    QQuaternion rotQuatX = QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, xyzRotations.x());
    QQuaternion rotQuatY = QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, xyzRotations.y());
    QQuaternion rotQuatZ = QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, xyzRotations.z());
    QQuaternion totalRotation = rotQuatY * rotQuatZ * rotQuatX;
    return totalRotation;
}

bool Utils::isOpenGLES()
{
    if (!staticsResolved)
        resolveStatics();
    return isES;
}

void Utils::resolveStatics()
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    QOffscreenSurface *dummySurface = 0;
    if (!ctx) {
        QSurfaceFormat surfaceFormat;
        dummySurface = new QOffscreenSurface();
        dummySurface->setFormat(surfaceFormat);
        dummySurface->create();
        ctx = new QOpenGLContext;
        ctx->setFormat(surfaceFormat);
        ctx->create();
        ctx->makeCurrent(dummySurface);
    }

#if defined(QT_OPENGL_ES_2)
    isES = true;
#elif (QT_VERSION < QT_VERSION_CHECK(5, 3, 0))
    isES = false;
#else
    isES = ctx->isOpenGLES();
#endif

    ctx->functions()->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
    // We support only ES2 emulation with software renderer for now
    QString versionStr;
#ifdef Q_OS_WIN
    const GLubyte *openGLVersion = ctx->functions()->glGetString(GL_VERSION);
    versionStr = QString::fromLatin1(reinterpret_cast<const char *>(openGLVersion)).toLower();
#endif
    if (versionStr.contains(QStringLiteral("mesa"))
            || QCoreApplication::testAttribute(Qt::AA_UseSoftwareOpenGL)) {
        qWarning("Only OpenGL ES2 emulation is available for software rendering.");
        isES = true;
    }
#endif

    if (dummySurface) {
        ctx->doneCurrent();
        delete ctx;
        delete dummySurface;
    }

    staticsResolved = true;
}

QT_END_NAMESPACE_DATAVISUALIZATION
