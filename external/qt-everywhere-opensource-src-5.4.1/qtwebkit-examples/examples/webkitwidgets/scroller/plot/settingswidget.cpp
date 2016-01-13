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

#include <QVariant>
#include <QSlider>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QToolButton>
#include <QCheckBox>
#include <QScrollBar>
#include <QPainter>
#include <QScrollArea>
#include <QScrollPrepareEvent>
#include <QApplication>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <qnumeric.h>

#include <QEasingCurve>

#include <QDebug>

#include "math.h"

#include "settingswidget.h"
#include "qscroller.h"
#include "qscrollerproperties.h"

class SnapOverlay : public QWidget
{
    Q_OBJECT
public:
    SnapOverlay(QWidget *w)
        : QWidget(w)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);

        if (QAbstractScrollArea *area = qobject_cast<QAbstractScrollArea *>(w)) {
            connect(area->horizontalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(update()));
            connect(area->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(update()));
            area->viewport()->installEventFilter(this);
        }
    }
    void clear(Qt::Orientation o)
    {
        m_snap[o].clear();
        update();
    }

    void set(Qt::Orientation o, qreal first, qreal step)
    {
        m_snap[o] = QList<qreal>() << -Q_INFINITY << first << step;
        update();
    }

    void set(Qt::Orientation o, const QList<qreal> &list)
    {
        m_snap[o] = list;
        update();
    }

protected:
    bool eventFilter(QObject *o, QEvent *e)
    {
        if (QAbstractScrollArea *area = qobject_cast<QAbstractScrollArea *>(parentWidget())) {
            if (area->viewport() == o) {
                if (e->type() == QEvent::Move || e->type() == QEvent::Resize) {
                    setGeometry(area->viewport()->rect());
                }
            }
        }
        return false;
    }

    void paintEvent(QPaintEvent *e)
    {
        if (QAbstractScrollArea *area = qobject_cast<QAbstractScrollArea *>(parentWidget())) {
            int dx = area->horizontalScrollBar()->value();
            int dy = area->verticalScrollBar()->value();

            QPainter paint(this);
            paint.fillRect(e->rect(), Qt::transparent);
            paint.setPen(QPen(Qt::red, 9));

            if (m_snap[Qt::Horizontal].isEmpty()) {
            } else if (m_snap[Qt::Horizontal][0] == -Q_INFINITY) {
                int start = int(m_snap[Qt::Horizontal][1]);
                int step = int(m_snap[Qt::Horizontal][2]);
                if (step > 0) {
                    for (int i = start; i < area->horizontalScrollBar()->maximum(); i += step)
                        paint.drawPoint(i - dx, 5);
                }
            } else {
                foreach (qreal r, m_snap[Qt::Horizontal])
                    paint.drawPoint(int(r) - dx, 5);
            }
            paint.setPen(QPen(Qt::green, 9));
            if (m_snap[Qt::Vertical].isEmpty()) {
            } else if (m_snap[Qt::Vertical][0] == -Q_INFINITY) {
                int start = int(m_snap[Qt::Vertical][1]);
                int step = int(m_snap[Qt::Vertical][2]);
                if (step > 0) {
                    for (int i = start; i < area->verticalScrollBar()->maximum(); i += step)
                        paint.drawPoint(5, i - dy);
                }
            } else {
                foreach (qreal r, m_snap[Qt::Vertical])
                    paint.drawPoint(5, int(r) - dy);
            }
        }
    }

private:
    QMap<Qt::Orientation, QList<qreal> > m_snap;
};

struct MetricItem
{
    QScrollerProperties::ScrollMetric metric;
    const char *name;
    int scaling;
    const char *unit;
    QVariant min, max;
    QVariant step;
};

class MetricItemUpdater : public QObject
{
    Q_OBJECT
public:
    MetricItemUpdater(MetricItem *item)
        : m_item(item)
        , m_widget(0)
        , m_slider(0)
        , m_combo(0)
        , m_valueLabel(0)
    {
        m_frameRateType = QVariant::fromValue(QScrollerProperties::Standard).userType();
        m_overshootPolicyType = QVariant::fromValue(QScrollerProperties::OvershootWhenScrollable).userType();

        if (m_item->min.type() == QVariant::EasingCurve) {
            m_combo = new QComboBox();
            m_combo->addItem("OutQuad", QEasingCurve::OutQuad);
            m_combo->addItem("OutCubic", QEasingCurve::OutCubic);
            m_combo->addItem("OutQuart", QEasingCurve::OutQuart);
            m_combo->addItem("OutQuint", QEasingCurve::OutQuint);
            m_combo->addItem("OutExpo", QEasingCurve::OutExpo);
            m_combo->addItem("OutSine", QEasingCurve::OutSine);
            m_combo->addItem("OutCirc", QEasingCurve::OutCirc);
        } else if (m_item->min.userType() == m_frameRateType) {
            m_combo = new QComboBox();
            m_combo->addItem("Standard", QScrollerProperties::Standard);
            m_combo->addItem("60 FPS",   QScrollerProperties::Fps60);
            m_combo->addItem("30 FPS",   QScrollerProperties::Fps30);
            m_combo->addItem("20 FPS",   QScrollerProperties::Fps20);
        } else if (m_item->min.userType() == m_overshootPolicyType) {
            m_combo = new QComboBox();
            m_combo->addItem("When Scrollable", QScrollerProperties::OvershootWhenScrollable);
            m_combo->addItem("Always On",       QScrollerProperties::OvershootAlwaysOn);
            m_combo->addItem("Always Off",      QScrollerProperties::OvershootAlwaysOff);
        } else {
            m_slider = new QSlider(Qt::Horizontal);
            m_slider->setSingleStep(1);
            m_slider->setMinimum(-1);
            m_slider->setMaximum(qRound((m_item->max.toReal() - m_item->min.toReal()) / m_item->step.toReal()));
            m_slider->setValue(-1);
            m_valueLabel = new QLabel();
        }
        m_nameLabel = new QLabel(QLatin1String(m_item->name));
        if (m_item->unit && m_item->unit[0])
            m_nameLabel->setText(m_nameLabel->text() + QLatin1String("   [") + QLatin1String(m_item->unit) + QLatin1String("]"));
        m_resetButton = new QToolButton();
        m_resetButton->setText(QLatin1String("Reset"));
        m_resetButton->setEnabled(false);

        connect(m_resetButton, SIGNAL(clicked()), this, SLOT(reset()));
        if (m_slider) {
            connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(controlChanged(int)));
            m_slider->setMinimum(0);
        } else if (m_combo) {
            connect(m_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(controlChanged(int)));
        }
    }

    void setScroller(QWidget *widget)
    {
        m_widget = widget;
        QScroller *scroller = QScroller::scroller(widget);
        QScrollerProperties properties = QScroller::scroller(widget)->scrollerProperties();

        if (m_slider)
            m_slider->setEnabled(scroller);
        if (m_combo)
            m_combo->setEnabled(scroller);
        m_nameLabel->setEnabled(scroller);
        if (m_valueLabel)
            m_valueLabel->setEnabled(scroller);
        m_resetButton->setEnabled(scroller);

        if (!scroller)
            return;

        m_default_value = properties.scrollMetric(m_item->metric);
        valueChanged(m_default_value);
    }

    QWidget *nameLabel()    { return m_nameLabel; }
    QWidget *valueLabel()   { return m_valueLabel; }
    QWidget *valueControl() { if (m_combo) return m_combo; else return m_slider; }
    QWidget *resetButton()  { return m_resetButton; }

private slots:
    void valueChanged(const QVariant &v)
    {
        m_value = v;
        if (m_slider) {
            switch (m_item->min.type()) {
            case QMetaType::Float:
            case QVariant::Double: {
                m_slider->setValue(qRound((m_value.toReal() * m_item->scaling - m_item->min.toReal()) / m_item->step.toReal()));
                break;
            }
            case QVariant::Int: {
                m_slider->setValue((m_value.toInt() * m_item->scaling - m_item->min.toInt()) / m_item->step.toInt());
                break;
            }
            default: break;
            }
        } else if (m_combo) {
            if (m_item->min.type() == QVariant::EasingCurve) {
                m_combo->setCurrentIndex(m_combo->findData(v.toEasingCurve().type()));
            } else if (m_item->min.userType() == m_overshootPolicyType) {
                m_combo->setCurrentIndex(m_combo->findData(v.value<QScrollerProperties::OvershootPolicy>()));
            } else if (m_item->min.userType() == m_frameRateType) {
                m_combo->setCurrentIndex(m_combo->findData(v.value<QScrollerProperties::FrameRates>()));
            }
        }
    }

    void controlChanged(int value)
    {
        bool combo = (m_combo && (sender() == m_combo));
        QString text;

        if (m_slider && !combo) {
            switch (m_item->min.type()) {
            case QMetaType::Float:
            case QVariant::Double: {
                qreal d = m_item->min.toReal() + qreal(value) * m_item->step.toReal();
                text = QString::number(d);
                m_value = d / qreal(m_item->scaling);
                break;
            }
            case QVariant::Int: {
                int i = m_item->min.toInt() + qRound(qreal(value) * m_item->step.toReal());
                text = QString::number(i);
                m_value = i / m_item->scaling;
                break;
            }
            default: break;
            }
        } else if (m_combo && combo) {
            if (m_item->min.type() == QVariant::EasingCurve) {
                m_value = QVariant(QEasingCurve(static_cast<QEasingCurve::Type>(m_combo->itemData(value).toInt())));
            } else if (m_item->min.userType() == m_overshootPolicyType) {
                m_value = QVariant::fromValue(static_cast<QScrollerProperties::OvershootPolicy>(m_combo->itemData(value).toInt()));
            } else if (m_item->min.userType() == m_frameRateType) {
                m_value = QVariant::fromValue(static_cast<QScrollerProperties::FrameRates>(m_combo->itemData(value).toInt()));
            }
        }
        if (m_valueLabel)
            m_valueLabel->setText(text);
        if (m_widget && QScroller::scroller(m_widget)) {
            QScrollerProperties properties = QScroller::scroller(m_widget)->scrollerProperties();
            properties.setScrollMetric(m_item->metric, m_value);
            QScroller::scroller(m_widget)->setScrollerProperties(properties);
        }

        m_resetButton->setEnabled(m_value != m_default_value);
    }

    void reset()
    {
        QScrollerProperties properties = QScroller::scroller(m_widget)->scrollerProperties();
        properties.setScrollMetric(m_item->metric, m_value);
        QScroller::scroller(m_widget)->setScrollerProperties(properties);
        valueChanged(m_default_value);
    }

private:
    MetricItem *m_item;
    int m_frameRateType;
    int m_overshootPolicyType;

    QWidget *m_widget;
    QSlider *m_slider;
    QComboBox *m_combo;
    QLabel *m_nameLabel, *m_valueLabel;
    QToolButton *m_resetButton;

    QVariant m_value, m_default_value;
};

#define METRIC(x) QScrollerProperties::x, #x

MetricItem items[] = {
    { METRIC(MousePressEventDelay),           1000, "ms",       qreal(0), qreal(2000), qreal(10) },
    { METRIC(DragStartDistance),              1000, "mm",       qreal(1), qreal(20), qreal(0.1) },
    { METRIC(DragVelocitySmoothingFactor),    1,    "",         qreal(0), qreal(1), qreal(0.1) },
    { METRIC(AxisLockThreshold),              1,    "",         qreal(0), qreal(1), qreal(0.01) },

    { METRIC(ScrollingCurve),                 1,    "",         QEasingCurve(), 0, 0 },
    { METRIC(DecelerationFactor),             1,    "",         qreal(0), qreal(3), qreal(0.01) },

    { METRIC(MinimumVelocity),                1,    "m/s",      qreal(0), qreal(7), qreal(0.01) },
    { METRIC(MaximumVelocity),                1,    "m/s",      qreal(0), qreal(7), qreal(0.01) },
    { METRIC(MaximumClickThroughVelocity),    1,    "m/s",      qreal(0), qreal(7), qreal(0.01) },

    { METRIC(AcceleratingFlickMaximumTime),   1000, "ms",       qreal(100), qreal(5000), qreal(100) },
    { METRIC(AcceleratingFlickSpeedupFactor), 1,    "",         qreal(1), qreal(7), qreal(0.1) },

    { METRIC(SnapPositionRatio),              1,    "",         qreal(0.1), qreal(0.9), qreal(0.1) },
    { METRIC(SnapTime),                       1000, "ms",       qreal(0), qreal(2000), qreal(10) },

    { METRIC(OvershootDragResistanceFactor),  1,    "",         qreal(0), qreal(1), qreal(0.01) },
    { METRIC(OvershootDragDistanceFactor),    1,    "",         qreal(0), qreal(1), qreal(0.01) },
    { METRIC(OvershootScrollDistanceFactor),  1,    "",         qreal(0), qreal(1), qreal(0.01) },
    { METRIC(OvershootScrollTime),            1000, "ms",       qreal(0), qreal(2000), qreal(10) },

    { METRIC(HorizontalOvershootPolicy),      1,    "",         QVariant::fromValue(QScrollerProperties::OvershootWhenScrollable), 0, 0 },
    { METRIC(VerticalOvershootPolicy),        1,    "",         QVariant::fromValue(QScrollerProperties::OvershootWhenScrollable), 0, 0 },
    { METRIC(FrameRate),                      1,    "",         QVariant::fromValue(QScrollerProperties::Standard), 0, 0 },
};

#undef METRIC

void SettingsWidget::addToGrid(QGridLayout *grid, QWidget *label, int widgetCount, ...)
{
    va_list args;
    va_start(args, widgetCount);

    int rows = grid->rowCount();
    int cols = grid->columnCount();

    if (label) {
        if (m_smallscreen)
            grid->addWidget(label, rows++, 0, 1, qMax(cols, widgetCount));
        else
            grid->addWidget(label, rows, 0);
    }
    for (int i = 0; i < widgetCount; i++) {
        if (QWidget *w = va_arg(args, QWidget *))
            grid->addWidget(w, rows, m_smallscreen ? i : i + 1);
    }
    va_end(args);
}

SettingsWidget::SettingsWidget(bool smallscreen)
    : QScrollArea()
    , m_widget(0)
    , m_snapoverlay(0)
    , m_smallscreen(smallscreen)
{
    setWindowTitle(QLatin1String("Settings"));
    QWidget *view = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(view);
    QGroupBox *grp;
    QGridLayout *grid;

    // GROUP: SCROLL METRICS

    grp = new QGroupBox(QLatin1String("Scroll Metrics"));
    grid = new QGridLayout();
    grid->setVerticalSpacing(m_smallscreen ? 4 : 2);

    for (int i = 0; i < int(sizeof(items) / sizeof(items[0])); i++) {
        MetricItemUpdater *u = new MetricItemUpdater(items + i);
        u->setParent(this);
        addToGrid(grid, u->nameLabel(), 3, u->valueControl(), u->valueLabel(), u->resetButton());
        m_metrics.append(u);
    }
    grp->setLayout(grid);
    layout->addWidget(grp);

    // GROUP: SCROLL TO

    grp = new QGroupBox(QLatin1String("Scroll To"));
    grid = new QGridLayout();
    grid->setVerticalSpacing(m_smallscreen ? 4 : 2);

    m_scrollx = new QSpinBox();
    m_scrolly = new QSpinBox();
    m_scrolltime = new QSpinBox();
    m_scrolltime->setRange(0, 10000);
    m_scrolltime->setValue(1000);
    m_scrolltime->setSuffix(QLatin1String(" ms"));
    QPushButton *go = new QPushButton(QLatin1String("Go"));
    connect(go, SIGNAL(clicked()), this, SLOT(scrollTo()));
    connect(m_scrollx, SIGNAL(editingFinished()), this, SLOT(scrollTo()));
    connect(m_scrolly, SIGNAL(editingFinished()), this, SLOT(scrollTo()));
    connect(m_scrolltime, SIGNAL(editingFinished()), this, SLOT(scrollTo()));
    grid->addWidget(new QLabel(QLatin1String("X:")), 0, 0);
    grid->addWidget(m_scrollx, 0, 1);
    grid->addWidget(new QLabel(QLatin1String("Y:")), 0, 2);
    grid->addWidget(m_scrolly, 0, 3);
    int row = smallscreen ? 1 : 0;
    int col = smallscreen ? 0 : 4;
    grid->addWidget(new QLabel(QLatin1String("in")), row, col++);
    grid->addWidget(m_scrolltime, row, col++);
    if (smallscreen) {
        grid->addWidget(go, row, col + 1);
    } else {
        grid->addWidget(go, row, col);
        grid->setColumnStretch(5, 1);
        grid->setColumnStretch(6, 1);
    }
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);
    grp->setLayout(grid);
    layout->addWidget(grp);

    QLayout *snapbox = new QHBoxLayout();

    // GROUP: SNAP POINTS X

    grp = new QGroupBox(QLatin1String("Snap Positions X"));
    QBoxLayout *vbox = new QVBoxLayout();
    vbox->setSpacing(m_smallscreen ? 4 : 2);
    m_snapx = new QComboBox();
    m_snapx->addItem(QLatin1String("No Snapping"),      NoSnap);
    m_snapx->addItem(QLatin1String("Snap to Interval"), SnapToInterval);
    m_snapx->addItem(QLatin1String("Snap to List"),     SnapToList);
    connect(m_snapx, SIGNAL(currentIndexChanged(int)), this, SLOT(snapModeChanged(int)));
    vbox->addWidget(m_snapx);

    m_snapxinterval = new QWidget();
    grid = new QGridLayout();
    grid->setVerticalSpacing(m_smallscreen ? 4 : 2);
    m_snapxfirst = new QSpinBox();
    connect(m_snapxfirst, SIGNAL(valueChanged(int)), this, SLOT(snapPositionsChanged()));
    grid->addWidget(new QLabel("First:"), 0, 0);
    grid->addWidget(m_snapxfirst, 0, 1);
    m_snapxstep = new QSpinBox();
    connect(m_snapxstep, SIGNAL(valueChanged(int)), this, SLOT(snapPositionsChanged()));
    grid->addWidget(new QLabel("Interval:"), 0, 2);
    grid->addWidget(m_snapxstep, 0, 3);
    m_snapxinterval->setLayout(grid);
    vbox->addWidget(m_snapxinterval);
    m_snapxinterval->hide();

    m_snapxlist = new QPlainTextEdit();
    m_snapxlist->setToolTip(QLatin1String("One snap position per line. Empty lines are ignored."));
    m_snapxlist->installEventFilter(this);
    connect(m_snapxlist, SIGNAL(textChanged()), this, SLOT(snapPositionsChanged()));
    vbox->addWidget(m_snapxlist);
    m_snapxlist->hide();

    vbox->addStretch(100);
    grp->setLayout(vbox);
    snapbox->addWidget(grp);

    // GROUP: SNAP POINTS Y

    grp = new QGroupBox(QLatin1String("Snap Positions Y"));
    vbox = new QVBoxLayout();
    vbox->setSpacing(m_smallscreen ? 4 : 2);
    m_snapy = new QComboBox();
    m_snapy->addItem(QLatin1String("No Snapping"),      NoSnap);
    m_snapy->addItem(QLatin1String("Snap to Interval"), SnapToInterval);
    m_snapy->addItem(QLatin1String("Snap to List"),     SnapToList);
    connect(m_snapy, SIGNAL(currentIndexChanged(int)), this, SLOT(snapModeChanged(int)));
    vbox->addWidget(m_snapy);

    m_snapyinterval = new QWidget();
    grid = new QGridLayout();
    grid->setVerticalSpacing(m_smallscreen ? 4 : 2);
    m_snapyfirst = new QSpinBox();
    connect(m_snapyfirst, SIGNAL(valueChanged(int)), this, SLOT(snapPositionsChanged()));
    grid->addWidget(new QLabel("First:"), 0, 0);
    grid->addWidget(m_snapyfirst, 0, 1);
    m_snapystep = new QSpinBox();
    connect(m_snapystep, SIGNAL(valueChanged(int)), this, SLOT(snapPositionsChanged()));
    grid->addWidget(new QLabel("Interval:"), 0, 2);
    grid->addWidget(m_snapystep, 0, 3);
    m_snapyinterval->setLayout(grid);
    vbox->addWidget(m_snapyinterval);
    m_snapyinterval->hide();

    m_snapylist = new QPlainTextEdit();
    m_snapylist->setToolTip(QLatin1String("One snap position per line. Empty lines are ignored."));
    m_snapylist->installEventFilter(this);
    connect(m_snapylist, SIGNAL(textChanged()), this, SLOT(snapPositionsChanged()));
    vbox->addWidget(m_snapylist);
    m_snapylist->hide();

    vbox->addStretch(100);
    grp->setLayout(vbox);
    snapbox->addWidget(grp);

    layout->addLayout(snapbox);

    layout->addStretch(100);
    setWidget(view);
    setWidgetResizable(true);
}

void SettingsWidget::setScroller(QWidget *widget)
{
    delete m_snapoverlay;
    if (m_widget)
        m_widget->removeEventFilter(this);
    QAbstractScrollArea *area = qobject_cast<QAbstractScrollArea *>(widget);
    if (area)
        widget = area->viewport();
    m_widget = widget;
    m_widget->installEventFilter(this);
    m_snapoverlay = new SnapOverlay(area);
    QScrollerProperties properties = QScroller::scroller(widget)->scrollerProperties();

    QMutableListIterator<MetricItemUpdater *> it(m_metrics);
    while (it.hasNext())
        it.next()->setScroller(widget);

    if (!widget)
        return;

    updateScrollRanges();
}

bool SettingsWidget::eventFilter(QObject *o, QEvent *e)
{
    if (o == m_widget && e->type() == QEvent::Resize)
        updateScrollRanges();
    return false;
}

void SettingsWidget::updateScrollRanges()
{
    QScrollPrepareEvent spe(QPoint(0, 0));
    QApplication::sendEvent(m_widget, &spe);

    QSizeF vp = spe.viewportSize();
    QRectF maxc = spe.contentPosRange();

    m_scrollx->setRange(qRound(-vp.width()), qRound(maxc.width() + vp.width()));
    m_scrolly->setRange(qRound(-vp.height()), qRound(maxc.height() + vp.height()));

    m_snapxfirst->setRange(maxc.left(), maxc.right());
    m_snapxstep->setRange(0, maxc.width());
    m_snapyfirst->setRange(maxc.top(), maxc.bottom());
    m_snapystep->setRange(0, maxc.height());
}

void SettingsWidget::scrollTo()
{
    if (QApplication::activePopupWidget())
        return;
    if ((sender() == m_scrollx) && !m_scrollx->hasFocus())
        return;
    if ((sender() == m_scrolly) && !m_scrolly->hasFocus())
        return;
    if ((sender() == m_scrolltime) && !m_scrolltime->hasFocus())
        return;

    if (QScroller *scroller = QScroller::scroller(m_widget))
        scroller->scrollTo(QPointF(m_scrollx->value(), m_scrolly->value()), m_scrolltime->value());
}

void SettingsWidget::snapModeChanged(int mode)
{
    if (sender() == m_snapx) {
        m_snapxmode = static_cast<SnapMode>(mode);
        m_snapxinterval->setVisible(mode == SnapToInterval);
        m_snapxlist->setVisible(mode == SnapToList);
        snapPositionsChanged();
    } else if (sender() == m_snapy) {
        m_snapymode = static_cast<SnapMode>(mode);
        m_snapyinterval->setVisible(mode == SnapToInterval);
        m_snapylist->setVisible(mode == SnapToList);
        snapPositionsChanged();
    }
}

void SettingsWidget::snapPositionsChanged()
{
    QScroller *s = QScroller::scroller(m_widget);
    if (!s)
        return;

    switch (m_snapxmode) {
    case NoSnap:
        s->setSnapPositionsX(QList<qreal>());
        m_snapoverlay->clear(Qt::Horizontal);
        break;
    case SnapToInterval:
        s->setSnapPositionsX(m_snapxfirst->value(), m_snapxstep->value());
        m_snapoverlay->set(Qt::Horizontal, m_snapxfirst->value(), m_snapxstep->value());
        break;
    case SnapToList:
        s->setSnapPositionsX(toPositionList(m_snapxlist, m_snapxfirst->minimum(), m_snapxfirst->maximum()));
        m_snapoverlay->set(Qt::Horizontal, toPositionList(m_snapxlist, m_snapxfirst->minimum(), m_snapxfirst->maximum()));
        break;
    }
    switch (m_snapymode) {
    case NoSnap:
        s->setSnapPositionsY(QList<qreal>());
        m_snapoverlay->clear(Qt::Vertical);
        break;
    case SnapToInterval:
        s->setSnapPositionsY(m_snapyfirst->value(), m_snapystep->value());
        m_snapoverlay->set(Qt::Vertical, m_snapyfirst->value(), m_snapystep->value());
        break;
    case SnapToList:
        s->setSnapPositionsY(toPositionList(m_snapylist, m_snapyfirst->minimum(), m_snapyfirst->maximum()));
        m_snapoverlay->set(Qt::Vertical, toPositionList(m_snapylist, m_snapyfirst->minimum(), m_snapyfirst->maximum()));
        break;
    }
}

QList<qreal> SettingsWidget::toPositionList(QPlainTextEdit *list, int vmin, int vmax)
{
    QList<qreal> snaps;
    QList<QTextEdit::ExtraSelection> extrasel;
    QTextEdit::ExtraSelection uline;
    uline.format.setUnderlineColor(Qt::red);
    uline.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    int line = 0;

    foreach (const QString &str, list->toPlainText().split(QLatin1Char('\n'))) {
        ++line;
        if (str.isEmpty())
            continue;
        bool ok = false;
        double d = str.toDouble(&ok);
        if (ok && d >= vmin && d <= vmax) {
            snaps << d;
        } else {
            QTextEdit::ExtraSelection esel = uline;
            esel.cursor = QTextCursor(list->document()->findBlockByLineNumber(line - 1));
            esel.cursor.select(QTextCursor::LineUnderCursor);
            extrasel << esel;
        }
    }
    list->setExtraSelections(extrasel);
    return snaps;
}

#include "settingswidget.moc"
