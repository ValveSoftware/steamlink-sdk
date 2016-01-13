/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

#include <QPushButton>
#include <QTextStream>
#include <QColor>
#include <QPainter>
#include <QLabel>
#include <QResizeEvent>
#include <QAbstractScrollArea>

#include "plotwidget.h"
#include "qscroller.h"

PlotWidget::PlotWidget(bool /*smallscreen*/)
    : QWidget(), m_widget(0)
{
    setWindowTitle(QLatin1String("Plot"));
    m_clear = new QPushButton(QLatin1String("Clear"), this);
    connect(m_clear, SIGNAL(clicked()), this, SLOT(reset()));
    m_legend = new QLabel(this);
    QString legend;
    QTextStream ts(&legend);
    // ok. this wouldn't pass the w3c html verification...
    ts << "<table style=\"color:#000;\" border=\"0\">";
    ts << "<tr><td width=\"30\" bgcolor=\"" << QColor(Qt::red).light().name() << "\" /><td>Velocity X</td></tr>";
    ts << "<tr><td width=\"30\" bgcolor=\"" << QColor(Qt::red).dark().name() << "\" /><td>Velocity Y</td></tr>";
    ts << "<tr><td width=\"30\" bgcolor=\"" << QColor(Qt::green).light().name() << "\" /><td>Content Position X</td></tr>";
    ts << "<tr><td width=\"30\" bgcolor=\"" << QColor(Qt::green).dark().name() << "\" /><td>Content Position Y</td></tr>";
    ts << "<tr><td width=\"30\" bgcolor=\"" << QColor(Qt::blue).light().name() << "\" /><td>Overshoot Position X</td></tr>";
    ts << "<tr><td width=\"30\" bgcolor=\"" << QColor(Qt::blue).dark().name() << "\" /><td>Overshoot Position Y</td></tr>";
    ts << "</table>";
    m_legend->setText(legend);
}

void PlotWidget::setScroller(QWidget *widget)
{
    if (QAbstractScrollArea *area = qobject_cast<QAbstractScrollArea *>(widget))
        widget = area->viewport();

    if (m_widget)
        m_widget->removeEventFilter(this);
    m_widget = widget;
    reset();
    if (m_widget)
        m_widget->installEventFilter(this);
}

bool PlotWidget::eventFilter(QObject *obj, QEvent *ev)
{
    if (ev->type() == QEvent::Scroll) {
        QScrollEvent *se = static_cast<QScrollEvent *>(ev);
        QScroller *scroller = QScroller::scroller(m_widget);

        QPointF v = scroller->velocity();
        //v.rx() *= scroller->pixelPerMeter().x();
        //v.ry() *= scroller->pixelPerMeter().y();

        PlotItem pi = { v, se->contentPos(), se->overshootDistance() };
        addPlotItem(pi);
    }

    return QWidget::eventFilter(obj, ev);
}

static inline void doMaxMin(const QPointF &v, qreal &minmaxv)
{
    minmaxv = qMax(minmaxv, qMax(qAbs(v.x()), qAbs(v.y())));
}

void PlotWidget::addPlotItem(const PlotItem &pi)
{
    m_plotitems.append(pi);
    minMaxVelocity = minMaxPosition = 0;

    while (m_plotitems.size() > 500)
        m_plotitems.removeFirst();

    foreach (const PlotItem &pi, m_plotitems) {
        doMaxMin(pi.velocity, minMaxVelocity);
        doMaxMin(pi.contentPosition, minMaxPosition);
        doMaxMin(pi.overshootPosition, minMaxPosition);
    }
    update();
}

void PlotWidget::reset()
{
    m_plotitems.clear();
    minMaxVelocity = minMaxPosition = 0;
    update();
}

void PlotWidget::resizeEvent(QResizeEvent *)
{
    QSize cs = m_clear->sizeHint();
    QSize ls = m_legend->sizeHint();
    m_clear->setGeometry(4, 4, cs.width(), cs.height());
    m_legend->setGeometry(4, height() - ls.height() - 4, ls.width(), ls.height());
}

void PlotWidget::paintEvent(QPaintEvent *)
{
#define SCALE(v, mm)  ((qreal(1) - (v / mm)) * qreal(0.5) * height())

    QColor rvColor = Qt::red;
    QColor cpColor = Qt::green;
    QColor opColor = Qt::blue;


    QPainter p(this);
    //p.setRenderHints(QPainter::Antialiasing); //too slow for 60fps
    p.fillRect(rect(), Qt::white);

    p.setPen(Qt::black);
    p.drawLine(0, SCALE(0, 1), width(), SCALE(0, 1));

    if (m_plotitems.isEmpty())
        return;

    int x = 2;
    int offset = m_plotitems.size() - width() / 2;
    QList<PlotItem>::const_iterator it = m_plotitems.constBegin();
    if (offset > 0)
        it += (offset - 1);

    const PlotItem *last = &(*it++);

    while (it != m_plotitems.constEnd()) {
        p.setPen(rvColor.light());
        p.drawLine(qreal(x - 2), SCALE(last->velocity.x(), minMaxVelocity),
                   qreal(x), SCALE(it->velocity.x(), minMaxVelocity));
        p.setPen(rvColor.dark());
        p.drawLine(qreal(x - 2), SCALE(last->velocity.y(), minMaxVelocity),
                   qreal(x), SCALE(it->velocity.y(), minMaxVelocity));

        p.setPen(cpColor.light());
        p.drawLine(qreal(x - 2), SCALE(last->contentPosition.x(), minMaxPosition),
                   qreal(x), SCALE(it->contentPosition.x(), minMaxPosition));
        p.setPen(cpColor.dark());
        p.drawLine(qreal(x - 2), SCALE(last->contentPosition.y(), minMaxPosition),
                   qreal(x), SCALE(it->contentPosition.y(), minMaxPosition));

        p.setPen(opColor.light());
        p.drawLine(qreal(x - 2), SCALE(last->overshootPosition.x(), minMaxPosition),
                   qreal(x), SCALE(it->overshootPosition.x(), minMaxPosition));
        p.setPen(opColor.dark());
        p.drawLine(qreal(x - 2), SCALE(last->overshootPosition.y(), minMaxPosition),
                   qreal(x), SCALE(it->overshootPosition.y(), minMaxPosition));

        last = &(*it++);
        x += 2;
    }

    QString toptext = QString("%1 [m/s] / %2 [pix]").arg(minMaxVelocity, 0, 'f', 2).arg(minMaxPosition, 0, 'f', 2);
    QString bottomtext = QString("-%1 [m/s] / -%2 [pix]").arg(minMaxVelocity, 0, 'f', 2).arg(minMaxPosition, 0, 'f', 2);

    p.setPen(Qt::black);
    p.drawText(rect(), Qt::AlignTop | Qt::AlignHCenter, toptext);
    p.drawText(rect(), Qt::AlignBottom | Qt::AlignHCenter, bottomtext);
#undef SCALE
}
