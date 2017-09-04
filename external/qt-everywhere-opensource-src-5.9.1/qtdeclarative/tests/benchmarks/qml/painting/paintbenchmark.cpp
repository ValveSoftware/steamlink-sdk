/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <QApplication>
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QGLWidget>
#include <QTextLayout>
#include <QVBoxLayout>
#include <QTime>
#include <QDebug>
#include <QStaticText>

int iterations = 20;
const int count = 600;
const int lines = 12;
const int spacing = 36;
QSizeF size(1000, 800);
const qreal lineWidth = 1000;
QString strings[lines];
QGLWidget *testWidget = 0;

void paint_QTextLayout(QPainter &p, bool useCache)
{
    static bool first = true;
    static QTextLayout *textLayout[lines];
    if (first) {
        for (int i = 0; i < lines; ++i) {
            textLayout[i] = new QTextLayout(strings[i]);
            int leading = p.fontMetrics().leading();
            qreal height = 0;
            qreal widthUsed = 0;
            textLayout[i]->setCacheEnabled(useCache);
            textLayout[i]->beginLayout();
            while (1) {
                QTextLine line = textLayout[i]->createLine();
                if (!line.isValid())
                        break;

                line.setLineWidth(lineWidth);
                height += leading;
                line.setPosition(QPointF(0, height));
                height += line.height();
                widthUsed = qMax(widthUsed, line.naturalTextWidth());
            }
            textLayout[i]->endLayout();
        }
        first = false;
    }
    for (int i = 0; i < count; ++i) {
        for (int j = 0; j < lines; ++j) {
            textLayout[j]->draw(&p, QPoint(0, j*spacing));
        }
    }
}

void paint_QTextLayout_noCache(QPainter &p)
{
    paint_QTextLayout(p, false);
}

void paint_QTextLayout_cache(QPainter &p)
{
    paint_QTextLayout(p, true);
}

void paint_QStaticText(QPainter &p, bool useOptimizations)
{
    static QStaticText *staticText[lines];
    static bool first = true;
    if (first) {
        for (int i = 0; i < lines; ++i) {
            staticText[i] = new QStaticText(strings[i]);
            if (useOptimizations)
                staticText[i]->setPerformanceHint(QStaticText::AggressiveCaching);
            else
                staticText[i]->setPerformanceHint(QStaticText::ModerateCaching);
        }
        first = false;
    }
    for (int i = 0; i < count; ++i) {
        for (int j = 0; j < lines; ++j) {
            p.drawStaticText(QPointF(0, 30 + j*spacing), *staticText[j]);
        }
    }
}

void paint_QStaticText_noOptimizations(QPainter &p)
{
    paint_QStaticText(p, false);
}

void paint_QStaticText_optimizations(QPainter &p)
{
    paint_QStaticText(p, true);
}

void paint_QPixmapCachedText(QPainter &p)
{
    static bool first = true;
    static QPixmap cacheText[lines];
    if (first) {
        for (int i = 0; i < lines; ++i) {
            QRectF trueSize;
            trueSize = p.boundingRect(QRectF(QPointF(0,0), size), 0, strings[i]);
            cacheText[i] = QPixmap(trueSize.size().toSize());
            cacheText[i].fill(Qt::transparent);
            QPainter paint(&cacheText[i]);
            paint.setPen(Qt::black);
            paint.drawText(QRectF(QPointF(0,0), trueSize.size().toSize()), strings[i]);
        }
        first = false;
    }
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < lines; ++j) {
            p.drawPixmap(0,j*spacing,cacheText[j]);
        }
    }
}

void paint_RoundedRect(QPainter &p)
{
    static bool first = true;
    if (first) {
        if (testWidget) {
            QGLFormat format = testWidget->format();
            if (!format.sampleBuffers())
                qWarning() << "Cannot paint antialiased rounded rect without sampleBuffers";
        }
        first = false;
    }
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::black);
    p.setBrush(Qt::red);
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < lines; ++j) {
            QSize size((j+1)*50, spacing-1);
            p.drawRoundedRect(QRectF(QPointF(0,j*spacing), size), 8, 8);
        }
    }
}

// Disable this test case since this cannot be compiled with Qt5.
#if 0
void paint_QPixmapCachedRoundedRect(QPainter &p)
{
    static bool first = true;
    static QPixmap cacheRect;
    if (first) {
        const int pw = 0;
        const int radius = 8;
        cacheRect = QPixmap(radius*2 + 3 + pw*2, radius*2 + 3 + pw*2);
        cacheRect.fill(Qt::transparent);
        QPainter paint(&cacheRect);
        paint.setRenderHint(QPainter::Antialiasing);
        paint.setPen(Qt::black);
        paint.setBrush(Qt::red);
        if (pw%2)
            paint.drawRoundedRect(QRectF(qreal(pw)/2+1, qreal(pw)/2+1, cacheRect.width()-(pw+1), cacheRect.height()-(pw+1)), radius, radius);
        else
            paint.drawRoundedRect(QRectF(qreal(pw)/2, qreal(pw)/2, cacheRect.width()-pw, cacheRect.height()-pw), radius, radius);

        first = false;
    }
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < lines; ++j) {
            QSize size((j+1)*50, spacing-1);

            p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);

            const int pw = 0;

            int xOffset = (cacheRect.width()-1)/2;
            int yOffset = (cacheRect.height()-1)/2;

            QMargins margins(xOffset, yOffset, xOffset, yOffset);
            QTileRules rules(Qt::StretchTile, Qt::StretchTile);
            //NOTE: even though our item may have qreal-based width and height, qDrawBorderPixmap only supports QRects
            qDrawBorderPixmap(&p, QRect(-pw/2, j*spacing-pw/2, size.width()+pw, size.height()+pw), margins, cacheRect, cacheRect.rect(), margins, rules);
        }
    }
}
#endif

void paint_pathCacheRoundedRect(QPainter &p)
{
    static bool first = true;
    static QPainterPath path[lines];
    if (first) {
        for (int j = 0; j < lines; ++j) {
            path[j].addRoundedRect(QRectF(0,0,(j+1)*50, spacing-1), 8, 8);
        }
        first = false;
    }
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::black);
    p.setBrush(Qt::red);
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < lines; ++j) {
            p.translate(0,j*spacing);
            p.drawPath(path[j]);
            p.translate(0,-j*spacing);
        }
    }
}

void paint_QPixmap63x63_opaque(QPainter &p)
{
    static bool first = true;
    static QPixmap pm;
    if (first) {
        pm.load("data/63x63_opaque.png");
        first = false;
    }
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < lines; ++j) {
            p.drawPixmap((i%10) * 64,j*spacing, pm);
        }
    }
}

void paint_QPixmap64x64_opaque(QPainter &p)
{
    static bool first = true;
    static QPixmap pm;
    if (first) {
        pm.load("data/64x64_opaque.png");
        first = false;
    }
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < lines; ++j) {
            p.drawPixmap((i%10) * 64,j*spacing, pm);
        }
    }
}

void paint_QPixmap63x63(QPainter &p)
{
    static bool first = true;
    static QPixmap pm;
    if (first) {
        pm.load("data/63x63.png");
        first = false;
    }
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < lines; ++j) {
            p.drawPixmap((i%10) * 64,j*spacing, pm);
        }
    }
}

void paint_QPixmap64x64(QPainter &p)
{
    static bool first = true;
    static QPixmap pm;
    if (first) {
        pm.load("data/64x64.png");
        first = false;
    }
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < lines; ++j) {
            p.drawPixmap((i%10) * 64,j*spacing, pm);
        }
    }
}
typedef void(*PaintFunc)(QPainter &);

struct {
    const char *name;
    PaintFunc func;
} funcs[] = {
    { "QTextLayoutNoCache", &paint_QTextLayout_noCache },
    { "QTextLayoutWithCache", &paint_QTextLayout_cache },
    { "QStaticTextNoBackendOptimizations", &paint_QStaticText_noOptimizations },
    { "QStaticTextWithBackendOptimizations", &paint_QStaticText_optimizations },
    { "CachedText", &paint_QPixmapCachedText },
    { "RoundedRect", &paint_RoundedRect },
    // { "CachedRoundedRect", &paint_QPixmapCachedRoundedRect },
    { "PathCacheRoundedRect", &paint_pathCacheRoundedRect },
    { "QPixmap63x63_opaque", &paint_QPixmap63x63_opaque },
    { "QPixmap64x64_opaque", &paint_QPixmap64x64_opaque },
    { "QPixmap63x63", &paint_QPixmap63x63 },
    { "QPixmap64x64", &paint_QPixmap64x64 },
    { 0, 0 }
};

PaintFunc testFunc = 0;

class MyGLWidget : public QGLWidget
{
public:
    MyGLWidget(const QGLFormat &format) : QGLWidget(format), frames(0) {
        const char chars[] = "abcd efgh ijkl mnop qrst uvwx yz!$. ABCD 1234";
        int len = strlen(chars);
        for (int i = 0; i < lines; ++i) {
            for (int j = 0; j < 60; j++) {
                strings[i] += QChar(chars[rand() % len]);
            }
        }
    }

    void paintEvent(QPaintEvent *) {
        static int last = 0;
        static bool firstRun = true;
        if (firstRun) {
            timer.start();
            firstRun = false;
        } else {
            int elapsed = timer.elapsed();
            qDebug() << "frame elapsed:" << elapsed - last;
            last = elapsed;
        }
        QPainter p(this);
        p.fillRect(rect(), Qt::white);
        p.setPen(Qt::black);
        QTime drawTimer;
        drawTimer.start();
        testFunc(p);
        qDebug() << "draw time" << drawTimer.elapsed();
        if (iterations--)
            update();
        else
            qApp->quit();
    }

    QTime timer;
    int frames;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    bool sampleBuffers = false;

    for (int i = 1; i < argc; ++i) {
        QString arg = argv[i];
        if (arg == "-test") {
            arg = argv[++i];
            int j = 0;
            while (funcs[j].name) {
                if (arg == funcs[j].name) {
                    testFunc = funcs[j].func;
                    qDebug() << "Running test" << arg;
                    break;
                }
                ++j;
            }
        } else if (arg == "-iterations") {
            arg = argv[++i];
            iterations = arg.toInt();
        } else if (arg == "-sampleBuffers") {
            sampleBuffers = true;
        }
    }

    if (testFunc == 0) {
        qDebug() << "Usage: textspeed -test <test> [-sampleBuffers] [-iterations n]";
        qDebug() << "where <test> can be:";
        int j = 0;
        while (funcs[j].name) {
            qDebug() << "  " << funcs[j].name;
            ++j;
        }
        exit(1);
    }

    QWidget w;
    QGLFormat format = QGLFormat::defaultFormat();
    format.setSampleBuffers(sampleBuffers);
    testWidget = new MyGLWidget(format);
    testWidget->setAutoFillBackground(false);
    QVBoxLayout *layout = new QVBoxLayout(&w);
    w.setLayout(layout);
    layout->addWidget(testWidget);
    w.showFullScreen();
    app.exec();

    return 0;
}
