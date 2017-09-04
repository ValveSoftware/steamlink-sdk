/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
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
#include <private/chartpresenter_p.h>
#include <QtCharts/QChart>
#include <private/chartitem_p.h>
#include <private/qchart_p.h>
#include <QtCharts/QAbstractAxis>
#include <private/qabstractaxis_p.h>
#include <private/chartdataset_p.h>
#include <private/chartanimation_p.h>
#include <private/qabstractseries_p.h>
#include <QtCharts/QAreaSeries>
#include <private/chartaxiselement_p.h>
#include <private/chartbackground_p.h>
#include <private/cartesianchartlayout_p.h>
#include <private/polarchartlayout_p.h>
#include <private/charttitle_p.h>
#include <QtCore/QTimer>
#include <QtGui/QTextDocument>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>

QT_CHARTS_BEGIN_NAMESPACE

ChartPresenter::ChartPresenter(QChart *chart, QChart::ChartType type)
    : QObject(chart),
      m_chart(chart),
      m_options(QChart::NoAnimation),
      m_animationDuration(ChartAnimationDuration),
      m_animationCurve(QEasingCurve::OutQuart),
      m_state(ShowState),
      m_background(0),
      m_plotAreaBackground(0),
      m_title(0),
      m_localizeNumbers(false)
#ifndef QT_NO_OPENGL
      , m_glWidget(0)
      , m_glUseWidget(true)
#endif
{
    if (type == QChart::ChartTypeCartesian)
        m_layout = new CartesianChartLayout(this);
    else if (type == QChart::ChartTypePolar)
        m_layout = new PolarChartLayout(this);
    Q_ASSERT(m_layout);
}

ChartPresenter::~ChartPresenter()
{
#ifndef QT_NO_OPENGL
    delete m_glWidget.data();
#endif
}

void ChartPresenter::setGeometry(const QRectF rect)
{
    if (m_rect != rect) {
        m_rect = rect;
        foreach (ChartItem *chart, m_chartItems) {
            chart->domain()->setSize(rect.size());
            chart->setPos(rect.topLeft());
        }
#ifndef QT_NO_OPENGL
        if (!m_glWidget.isNull())
            m_glWidget->setGeometry(m_rect.toRect());
#endif
        emit plotAreaChanged(m_rect);
    }
}

QRectF ChartPresenter::geometry() const
{
    return m_rect;
}

void ChartPresenter::handleAxisAdded(QAbstractAxis *axis)
{
    axis->d_ptr->initializeGraphics(rootItem());
    axis->d_ptr->initializeAnimations(m_options, m_animationDuration, m_animationCurve);
    ChartAxisElement *item = axis->d_ptr->axisItem();
    item->setPresenter(this);
    item->setThemeManager(m_chart->d_ptr->m_themeManager);
    m_axisItems<<item;
    m_axes<<axis;
    m_layout->invalidate();
}

void ChartPresenter::handleAxisRemoved(QAbstractAxis *axis)
{
    ChartAxisElement *item = axis->d_ptr->m_item.take();
    if (item->animation())
        item->animation()->stopAndDestroyLater();
    item->hide();
    item->disconnect();
    item->deleteLater();
    m_axisItems.removeAll(item);
    m_axes.removeAll(axis);
    m_layout->invalidate();
}


void ChartPresenter::handleSeriesAdded(QAbstractSeries *series)
{
    series->d_ptr->initializeGraphics(rootItem());
    series->d_ptr->initializeAnimations(m_options, m_animationDuration, m_animationCurve);
    series->d_ptr->setPresenter(this);
    ChartItem *chart = series->d_ptr->chartItem();
    chart->setPresenter(this);
    chart->setThemeManager(m_chart->d_ptr->m_themeManager);
    chart->setDataSet(m_chart->d_ptr->m_dataset);
    chart->domain()->setSize(m_rect.size());
    chart->setPos(m_rect.topLeft());
    chart->handleDomainUpdated(); //this could be moved to intializeGraphics when animator is refactored
    m_chartItems<<chart;
    m_series<<series;
    m_layout->invalidate();
}

void ChartPresenter::handleSeriesRemoved(QAbstractSeries *series)
{
    ChartItem *chart  = series->d_ptr->m_item.take();
    chart->hide();
    chart->cleanup();
    series->disconnect(chart);
    chart->deleteLater();
    if (chart->animation())
        chart->animation()->stopAndDestroyLater();
    m_chartItems.removeAll(chart);
    m_series.removeAll(series);
    m_layout->invalidate();
}

void ChartPresenter::setAnimationOptions(QChart::AnimationOptions options)
{
    if (m_options != options) {
        QChart::AnimationOptions oldOptions = m_options;
        m_options = options;
        if (options.testFlag(QChart::SeriesAnimations) != oldOptions.testFlag(QChart::SeriesAnimations)) {
            foreach (QAbstractSeries *series, m_series)
                series->d_ptr->initializeAnimations(m_options, m_animationDuration,
                                                    m_animationCurve);
        }
        if (options.testFlag(QChart::GridAxisAnimations) != oldOptions.testFlag(QChart::GridAxisAnimations)) {
            foreach (QAbstractAxis *axis, m_axes)
                axis->d_ptr->initializeAnimations(m_options, m_animationDuration, m_animationCurve);
        }
        m_layout->invalidate(); // So that existing animations don't just stop halfway
    }
}

void ChartPresenter::setAnimationDuration(int msecs)
{
    if (m_animationDuration != msecs) {
        m_animationDuration = msecs;
        foreach (QAbstractSeries *series, m_series)
            series->d_ptr->initializeAnimations(m_options, m_animationDuration, m_animationCurve);
        foreach (QAbstractAxis *axis, m_axes)
            axis->d_ptr->initializeAnimations(m_options, m_animationDuration, m_animationCurve);
        m_layout->invalidate(); // So that existing animations don't just stop halfway
    }
}

void ChartPresenter::setAnimationEasingCurve(const QEasingCurve &curve)
{
    if (m_animationCurve != curve) {
        m_animationCurve = curve;
        foreach (QAbstractSeries *series, m_series)
            series->d_ptr->initializeAnimations(m_options, m_animationDuration, m_animationCurve);
        foreach (QAbstractAxis *axis, m_axes)
            axis->d_ptr->initializeAnimations(m_options, m_animationDuration, m_animationCurve);
        m_layout->invalidate(); // So that existing animations don't just stop halfway
    }
}

void ChartPresenter::setState(State state,QPointF point)
{
	m_state=state;
	m_statePoint=point;
}

QChart::AnimationOptions ChartPresenter::animationOptions() const
{
    return m_options;
}

void ChartPresenter::createBackgroundItem()
{
    if (!m_background) {
        m_background = new ChartBackground(rootItem());
        m_background->setPen(Qt::NoPen); // Theme doesn't touch pen so don't use default
        m_background->setBrush(QChartPrivate::defaultBrush());
        m_background->setZValue(ChartPresenter::BackgroundZValue);
    }
}

void ChartPresenter::createPlotAreaBackgroundItem()
{
    if (!m_plotAreaBackground) {
        if (m_chart->chartType() == QChart::ChartTypeCartesian)
            m_plotAreaBackground = new QGraphicsRectItem(rootItem());
        else
            m_plotAreaBackground = new QGraphicsEllipseItem(rootItem());
        // Use transparent pen instead of Qt::NoPen, as Qt::NoPen causes
        // antialising artifacts with axis lines for some reason.
        m_plotAreaBackground->setPen(QPen(Qt::transparent));
        m_plotAreaBackground->setBrush(Qt::NoBrush);
        m_plotAreaBackground->setZValue(ChartPresenter::PlotAreaZValue);
        m_plotAreaBackground->setVisible(false);
    }
}

void ChartPresenter::createTitleItem()
{
    if (!m_title) {
        m_title = new ChartTitle(rootItem());
        m_title->setZValue(ChartPresenter::BackgroundZValue);
    }
}

void ChartPresenter::startAnimation(ChartAnimation *animation)
{
    animation->stop();
    QTimer::singleShot(0, animation, SLOT(startChartAnimation()));
}

void ChartPresenter::setBackgroundBrush(const QBrush &brush)
{
    createBackgroundItem();
    m_background->setBrush(brush);
    m_layout->invalidate();
}

QBrush ChartPresenter::backgroundBrush() const
{
    if (!m_background)
        return QBrush();
    return m_background->brush();
}

void ChartPresenter::setBackgroundPen(const QPen &pen)
{
    createBackgroundItem();
    m_background->setPen(pen);
    m_layout->invalidate();
}

QPen ChartPresenter::backgroundPen() const
{
    if (!m_background)
        return QPen();
    return m_background->pen();
}

void ChartPresenter::setBackgroundRoundness(qreal diameter)
{
    createBackgroundItem();
    m_background->setDiameter(diameter);
    m_layout->invalidate();
}

qreal ChartPresenter::backgroundRoundness() const
{
    if (!m_background)
        return 0;
    return m_background->diameter();
}

void ChartPresenter::setPlotAreaBackgroundBrush(const QBrush &brush)
{
    createPlotAreaBackgroundItem();
    m_plotAreaBackground->setBrush(brush);
    m_layout->invalidate();
}

QBrush ChartPresenter::plotAreaBackgroundBrush() const
{
    if (!m_plotAreaBackground)
        return QBrush();
    return m_plotAreaBackground->brush();
}

void ChartPresenter::setPlotAreaBackgroundPen(const QPen &pen)
{
    createPlotAreaBackgroundItem();
    m_plotAreaBackground->setPen(pen);
    m_layout->invalidate();
}

QPen ChartPresenter::plotAreaBackgroundPen() const
{
    if (!m_plotAreaBackground)
        return QPen();
    return m_plotAreaBackground->pen();
}

void ChartPresenter::setTitle(const QString &title)
{
    createTitleItem();
    m_title->setText(title);
    m_layout->invalidate();
}

QString ChartPresenter::title() const
{
    if (!m_title)
        return QString();
    return m_title->text();
}

void ChartPresenter::setTitleFont(const QFont &font)
{
    createTitleItem();
    m_title->setFont(font);
    m_layout->invalidate();
}

QFont ChartPresenter::titleFont() const
{
    if (!m_title)
        return QFont();
    return m_title->font();
}

void ChartPresenter::setTitleBrush(const QBrush &brush)
{
    createTitleItem();
    m_title->setDefaultTextColor(brush.color());
    m_layout->invalidate();
}

QBrush ChartPresenter::titleBrush() const
{
    if (!m_title)
        return QBrush();
    return QBrush(m_title->defaultTextColor());
}

void ChartPresenter::setBackgroundVisible(bool visible)
{
    createBackgroundItem();
    m_background->setVisible(visible);
}


bool ChartPresenter::isBackgroundVisible() const
{
    if (!m_background)
        return false;
    return m_background->isVisible();
}

void ChartPresenter::setPlotAreaBackgroundVisible(bool visible)
{
    createPlotAreaBackgroundItem();
    m_plotAreaBackground->setVisible(visible);
}

bool ChartPresenter::isPlotAreaBackgroundVisible() const
{
    if (!m_plotAreaBackground)
        return false;
    return m_plotAreaBackground->isVisible();
}

void ChartPresenter::setBackgroundDropShadowEnabled(bool enabled)
{
    createBackgroundItem();
    m_background->setDropShadowEnabled(enabled);
}

bool ChartPresenter::isBackgroundDropShadowEnabled() const
{
    if (!m_background)
        return false;
    return m_background->isDropShadowEnabled();
}

void ChartPresenter::setLocalizeNumbers(bool localize)
{
    m_localizeNumbers = localize;
    m_layout->invalidate();
}

void ChartPresenter::setLocale(const QLocale &locale)
{
    m_locale = locale;
    m_layout->invalidate();
}

AbstractChartLayout *ChartPresenter::layout()
{
    return m_layout;
}

QLegend *ChartPresenter::legend()
{
    return m_chart->legend();
}

void ChartPresenter::setVisible(bool visible)
{
    m_chart->setVisible(visible);
}

ChartBackground *ChartPresenter::backgroundElement()
{
    return m_background;
}

QAbstractGraphicsShapeItem *ChartPresenter::plotAreaElement()
{
    return m_plotAreaBackground;
}

QList<ChartAxisElement *>  ChartPresenter::axisItems() const
{
    return m_axisItems;
}

QList<ChartItem *> ChartPresenter::chartItems() const
{
    return m_chartItems;
}

ChartTitle *ChartPresenter::titleElement()
{
    return m_title;
}

QRectF ChartPresenter::textBoundingRect(const QFont &font, const QString &text, qreal angle)
{
    static QGraphicsTextItem dummyTextItem;
    static bool initMargin = true;
    if (initMargin) {
        dummyTextItem.document()->setDocumentMargin(textMargin());
        initMargin = false;
    }

    dummyTextItem.setFont(font);
    dummyTextItem.setHtml(text);
    QRectF boundingRect = dummyTextItem.boundingRect();

    // Take rotation into account
    if (angle) {
        QTransform transform;
        transform.rotate(angle);
        boundingRect = transform.mapRect(boundingRect);
    }

    return boundingRect;
}

// boundingRect parameter returns the rotated bounding rect of the text
QString ChartPresenter::truncatedText(const QFont &font, const QString &text, qreal angle,
                                      qreal maxWidth, qreal maxHeight, QRectF &boundingRect)
{
    QString truncatedString(text);
    boundingRect = textBoundingRect(font, truncatedString, angle);
    if (boundingRect.width() > maxWidth || boundingRect.height() > maxHeight) {
        // It can be assumed that almost any amount of string manipulation is faster
        // than calculating one bounding rectangle, so first prepare a list of truncated strings
        // to try.
        static QRegExp truncateMatcher(QStringLiteral("&#?[0-9a-zA-Z]*;$"));

        QVector<QString> testStrings(text.length());
        int count(0);
        static QLatin1Char closeTag('>');
        static QLatin1Char openTag('<');
        static QLatin1Char semiColon(';');
        static QLatin1String ellipsis("...");
        while (truncatedString.length() > 1) {
            int chopIndex(-1);
            int chopCount(1);
            QChar lastChar(truncatedString.at(truncatedString.length() - 1));

            if (lastChar == closeTag)
                chopIndex = truncatedString.lastIndexOf(openTag);
            else if (lastChar == semiColon)
                chopIndex = truncateMatcher.indexIn(truncatedString, 0);

            if (chopIndex != -1)
                chopCount = truncatedString.length() - chopIndex;
            truncatedString.chop(chopCount);
            testStrings[count] = truncatedString + ellipsis;
            count++;
        }

        // Binary search for best fit
        int minIndex(0);
        int maxIndex(count - 1);
        int bestIndex(count);
        QRectF checkRect;

        while (maxIndex >= minIndex) {
            int mid = (maxIndex + minIndex) / 2;
            checkRect = textBoundingRect(font, testStrings.at(mid), angle);
            if (checkRect.width() > maxWidth || checkRect.height() > maxHeight) {
                // Checked index too large, all under this are also too large
                minIndex = mid + 1;
            } else {
                // Checked index fits, all over this also fit
                maxIndex = mid - 1;
                bestIndex = mid;
                boundingRect = checkRect;
            }
        }
        // Default to "..." if nothing fits
        if (bestIndex == count) {
            boundingRect = textBoundingRect(font, ellipsis, angle);
            truncatedString = ellipsis;
        } else {
            truncatedString = testStrings.at(bestIndex);
        }
    }

    return truncatedString;
}

QString ChartPresenter::numberToString(double value, char f, int prec)
{
    if (m_localizeNumbers)
        return m_locale.toString(value, f, prec);
    else
        return QString::number(value, f, prec);
}

QString ChartPresenter::numberToString(int value)
{
    if (m_localizeNumbers)
        return m_locale.toString(value);
    else
        return QString::number(value);
}

void ChartPresenter::updateGLWidget()
{
#ifndef QT_NO_OPENGL
    // GLWidget pointer is wrapped in QPointer as its parent is not in our control, and therefore
    // can potentially get deleted unexpectedly.
    if (!m_glWidget.isNull() && m_glWidget->needsReset()) {
        m_glWidget->hide();
        delete m_glWidget.data();
        m_glWidget.clear();
    }
    if (m_glWidget.isNull() && m_glUseWidget && m_chart->scene()) {
        // Find the view of the scene. If the scene has multiple views, only the first view is
        // chosen.
        QList<QGraphicsView *> views = m_chart->scene()->views();
        if (views.size()) {
            QGraphicsView *firstView = views.at(0);
            m_glWidget = new GLWidget(m_chart->d_ptr->m_dataset->glXYSeriesDataManager(),
                                      m_chart, firstView);
            m_glWidget->setGeometry(m_rect.toRect());
            m_glWidget->show();
        }
    }
    // Make sure we update the widget in a timely manner
    if (!m_glWidget.isNull())
        m_glWidget->update();
#endif
}

#include "moc_chartpresenter_p.cpp"

QT_CHARTS_END_NAMESPACE
