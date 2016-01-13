/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
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

#include <QtGui>

void createCurveIcons();

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    createCurveIcons();
    return app.exit();
}

void createCurveIcons()
{
    QDir dir(QDir::current());
    if (dir.dirName() == QLatin1String("debug") || dir.dirName() == QLatin1String("release")) {
        dir.cdUp();
    }
    dir.cdUp();
    dir.cdUp();
    dir.cdUp();
    QSize iconSize(128, 128);
    QPixmap pix(iconSize);
    QPainter painter(&pix);
    QLinearGradient gradient(0,0, 0, iconSize.height());
    gradient.setColorAt(0.0, QColor(240, 240, 240));
    gradient.setColorAt(1.0, QColor(224, 224, 224));
    QBrush brush(gradient);
    const QMetaObject &mo = QEasingCurve::staticMetaObject;
    QMetaEnum metaEnum = mo.enumerator(mo.indexOfEnumerator("Type"));
    QFont oldFont = painter.font();
    // Skip QEasingCurve::Custom
    QString output(QString::fromAscii("%1/images").arg(dir.absolutePath()));
    printf("Generating images to %s\n", qPrintable(output));
    for (int i = 0; i < QEasingCurve::NCurveTypes - 1; ++i) {
        painter.setFont(oldFont);
        QString name(QLatin1String(metaEnum.key(i)));
        painter.fillRect(QRect(QPoint(0, 0), iconSize), brush);
        QEasingCurve curve((QEasingCurve::Type)i);
        painter.setPen(QColor(0, 0, 255, 64));
        qreal xAxis = iconSize.height()/1.5;
        qreal yAxis = iconSize.width()/3;
        painter.drawLine(0, xAxis, iconSize.width(),  xAxis); // hor
        painter.drawLine(yAxis, 0, yAxis, iconSize.height()); // ver

        qreal curveScale = iconSize.height()/2;

        painter.drawLine(yAxis - 2, xAxis - curveScale, yAxis + 2, xAxis - curveScale); // hor
        painter.drawLine(yAxis + curveScale, xAxis + 2, yAxis + curveScale, xAxis - 2); // ver
        painter.drawText(yAxis + curveScale - 8, xAxis - curveScale - 4, QLatin1String("(1,1)"));

        painter.drawText(yAxis + 42, xAxis + 10, QLatin1String("progress"));
        painter.drawText(15, xAxis - curveScale - 10, QLatin1String("value"));

        painter.setPen(QPen(Qt::red, 1, Qt::DotLine));
        painter.drawLine(yAxis, xAxis - curveScale, yAxis + curveScale, xAxis - curveScale); // hor
        painter.drawLine(yAxis + curveScale, xAxis, yAxis + curveScale, xAxis - curveScale); // ver

        QPoint start(yAxis, xAxis - curveScale * curve.valueForProgress(0));

        painter.setPen(Qt::black);
        QFont font = oldFont;
        font.setPixelSize(oldFont.pixelSize() + 15);
        painter.setFont(font);
        painter.drawText(0, iconSize.height() - 20, iconSize.width(), 20, Qt::AlignHCenter, name);

        QPainterPath curvePath;
        curvePath.moveTo(start);
        for (qreal t = 0; t <= 1.0; t+=1.0/curveScale) {
            QPoint to;
            to.setX(yAxis + curveScale * t);
            to.setY(xAxis - curveScale * curve.valueForProgress(t));
            curvePath.lineTo(to);
        }
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.strokePath(curvePath, QColor(32, 32, 32));
        painter.setRenderHint(QPainter::Antialiasing, false);

        QString fileName(QString::fromAscii("qeasingcurve-%1.png").arg(name.toLower()));
        printf("%s\n", qPrintable(fileName));
        pix.save(QString::fromAscii("%1/%2").arg(output).arg(fileName), "PNG");
    }
}


