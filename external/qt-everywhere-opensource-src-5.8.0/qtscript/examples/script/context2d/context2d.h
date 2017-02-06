/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef CONTEXT2D_H
#define CONTEXT2D_H

#include "domimage.h"

#include <QPainter>
#include <QPainterPath>
#include <QString>
#include <QStack>
#include <QMetaType>
#include <QTimerEvent>

// [3]
class CanvasGradient
{
public:
    CanvasGradient(const QGradient &v)
        : value(v) {}
    CanvasGradient() {}

    QGradient value;
};
// [3]

Q_DECLARE_METATYPE(CanvasGradient)
Q_DECLARE_METATYPE(CanvasGradient*)

class ImageData {
};

class QContext2DCanvas;

//! [0]
class Context2D : public QObject
{
    Q_OBJECT
    // compositing
    Q_PROPERTY(qreal globalAlpha READ globalAlpha WRITE setGlobalAlpha)
    Q_PROPERTY(QString globalCompositeOperation READ globalCompositeOperation WRITE setGlobalCompositeOperation)
    Q_PROPERTY(QVariant strokeStyle READ strokeStyle WRITE setStrokeStyle)
    Q_PROPERTY(QVariant fillStyle READ fillStyle WRITE setFillStyle)
    // line caps/joins
    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth)
    Q_PROPERTY(QString lineCap READ lineCap WRITE setLineCap)
    Q_PROPERTY(QString lineJoin READ lineJoin WRITE setLineJoin)
    Q_PROPERTY(qreal miterLimit READ miterLimit WRITE setMiterLimit)
    // shadows
    Q_PROPERTY(qreal shadowOffsetX READ shadowOffsetX WRITE setShadowOffsetX)
    Q_PROPERTY(qreal shadowOffsetY READ shadowOffsetY WRITE setShadowOffsetY)
    Q_PROPERTY(qreal shadowBlur READ shadowBlur WRITE setShadowBlur)
    Q_PROPERTY(QString shadowColor READ shadowColor WRITE setShadowColor)
//! [0]

public:
    Context2D(QObject *parent = 0);
    void setSize(int width, int height);
    void setSize(const QSize &size);
    QSize size() const;

    void clear();
    void reset();

    // compositing
    qreal globalAlpha() const; // (default 1.0)
    QString globalCompositeOperation() const; // (default over)
    QVariant strokeStyle() const; // (default black)
    QVariant fillStyle() const; // (default black)

    void setGlobalAlpha(qreal alpha);
    void setGlobalCompositeOperation(const QString &op);
    void setStrokeStyle(const QVariant &style);
    void setFillStyle(const QVariant &style);

    // line caps/joins
    qreal lineWidth() const; // (default 1)
    QString lineCap() const; // "butt", "round", "square" (default "butt")
    QString lineJoin() const; // "round", "bevel", "miter" (default "miter")
    qreal miterLimit() const; // (default 10)

    void setLineWidth(qreal w);
    void setLineCap(const QString &s);
    void setLineJoin(const QString &s);
    void setMiterLimit(qreal m);

    // shadows
    qreal shadowOffsetX() const; // (default 0)
    qreal shadowOffsetY() const; // (default 0)
    qreal shadowBlur() const; // (default 0)
    QString shadowColor() const; // (default black)

    void setShadowOffsetX(qreal x);
    void setShadowOffsetY(qreal y);
    void setShadowBlur(qreal b);
    void setShadowColor(const QString &str);

//! [1]
public slots:
    void save(); // push state on state stack
    void restore(); // pop state stack and restore state

    void scale(qreal x, qreal y);
    void rotate(qreal angle);
    void translate(qreal x, qreal y);
    void transform(qreal m11, qreal m12, qreal m21, qreal m22,
                   qreal dx, qreal dy);
    void setTransform(qreal m11, qreal m12, qreal m21, qreal m22,
                      qreal dx, qreal dy);

    CanvasGradient createLinearGradient(qreal x0, qreal y0,
                                        qreal x1, qreal y1);
    CanvasGradient createRadialGradient(qreal x0, qreal y0,
                                        qreal r0, qreal x1,
                                        qreal y1, qreal r1);

    // rects
    void clearRect(qreal x, qreal y, qreal w, qreal h);
    void fillRect(qreal x, qreal y, qreal w, qreal h);
    void strokeRect(qreal x, qreal y, qreal w, qreal h);

    // path API
    void beginPath();
    void closePath();
    void moveTo(qreal x, qreal y);
    void lineTo(qreal x, qreal y);
    void quadraticCurveTo(qreal cpx, qreal cpy, qreal x, qreal y);
    void bezierCurveTo(qreal cp1x, qreal cp1y,
                       qreal cp2x, qreal cp2y, qreal x, qreal y);
    void arcTo(qreal x1, qreal y1, qreal x2, qreal y2, qreal radius);
    void rect(qreal x, qreal y, qreal w, qreal h);
    void arc(qreal x, qreal y, qreal radius,
             qreal startAngle, qreal endAngle,
             bool anticlockwise);
    void fill();
    void stroke();
    void clip();
    bool isPointInPath(qreal x, qreal y) const;
//! [1]

    // drawing images
    void drawImage(DomImage *image, qreal dx, qreal dy);
    void drawImage(DomImage *image, qreal dx, qreal dy,
                   qreal dw, qreal dh);
    void drawImage(DomImage *image, qreal sx, qreal sy,
                   qreal sw, qreal sh, qreal dx, qreal dy,
                   qreal dw, qreal dh);

    // pixel manipulation
    ImageData getImageData(qreal sx, qreal sy, qreal sw, qreal sh);
    void putImageData(ImageData image, qreal dx, qreal dy);

//! [2]
signals:
    void changed(const QImage &image);
//! [2]

protected:
    void timerEvent(QTimerEvent *e);

private:
    void beginPainting();
    const QImage &endPainting();
    void scheduleChange();

    int m_changeTimerId;
    QImage  m_image;
    QPainter m_painter;
    QPainterPath m_path;

    enum DirtyFlag {
        DirtyTransformationMatrix = 0x00001,
        DirtyClippingRegion       = 0x00002,
        DirtyStrokeStyle          = 0x00004,
        DirtyFillStyle            = 0x00008,
        DirtyGlobalAlpha          = 0x00010,
        DirtyLineWidth            = 0x00020,
        DirtyLineCap              = 0x00040,
        DirtyLineJoin             = 0x00080,
        DirtyMiterLimit           = 0x00100,
        MDirtyPen                 = DirtyStrokeStyle
                                  | DirtyLineWidth
                                  | DirtyLineCap
                                  | DirtyLineJoin
                                  | DirtyMiterLimit,
        DirtyShadowOffsetX        = 0x00200,
        DirtyShadowOffsetY        = 0x00400,
        DirtyShadowBlur           = 0x00800,
        DirtyShadowColor          = 0x01000,
        DirtyGlobalCompositeOperation = 0x2000,
        DirtyFont                 = 0x04000,
        DirtyTextAlign            = 0x08000,
        DirtyTextBaseline         = 0x10000,
        AllIsFullOfDirt           = 0xfffff
    };

    struct State {
        State() : flags(0) {}
        QMatrix matrix;
        QPainterPath clipPath;
        QBrush strokeStyle;
        QBrush fillStyle;
        qreal globalAlpha;
        qreal lineWidth;
        Qt::PenCapStyle lineCap;
        Qt::PenJoinStyle lineJoin;
        qreal miterLimit;
        qreal shadowOffsetX;
        qreal shadowOffsetY;
        qreal shadowBlur;
        QColor shadowColor;
        QPainter::CompositionMode globalCompositeOperation;
        QFont font;
        int textAlign;
        int textBaseline;
        int flags;
    };
    State m_state;
    QStack<State> m_stateStack;
};

#endif
