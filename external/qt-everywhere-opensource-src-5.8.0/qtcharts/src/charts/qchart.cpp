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

#include <QtCharts/QChart>
#include <private/qchart_p.h>
#include <private/legendscroller_p.h>
#include <private/qlegend_p.h>
#include <private/chartbackground_p.h>
#include <QtCharts/QAbstractAxis>
#include <private/abstractchartlayout_p.h>
#include <private/charttheme_p.h>
#include <private/chartpresenter_p.h>
#include <private/chartdataset_p.h>
#include <QtWidgets/QGraphicsScene>
#include <QGraphicsSceneResizeEvent>

QT_CHARTS_BEGIN_NAMESPACE

/*!
 \enum QChart::ChartTheme

 This enum describes the theme used by the chart.

 \value ChartThemeLight The default theme
 \value ChartThemeBlueCerulean
 \value ChartThemeDark
 \value ChartThemeBrownSand
 \value ChartThemeBlueNcs
 \value ChartThemeHighContrast
 \value ChartThemeBlueIcy
 \value ChartThemeQt
 */

/*!
 \enum QChart::AnimationOption

 For enabling/disabling animations. Defaults to NoAnimation.

 \value NoAnimation
 \value GridAxisAnimations
 \value SeriesAnimations
 \value AllAnimations
 */

/*!
 \enum QChart::ChartType

 This enum describes the chart type.

 \value ChartTypeUndefined
 \value ChartTypeCartesian
 \value ChartTypePolar
 */

/*!
 \class QChart
 \inmodule Qt Charts
 \brief Main chart API for Qt Charts.

 QChart is a QGraphicsWidget that you can show in a QGraphicsScene. It manages the graphical
 representation of different types of series and other chart related objects like legend and
 axes. If you simply want to show a chart in a layout, you can use the
 convenience class QChartView instead of QChart.
 \sa QChartView, QPolarChart
 */

/*!
 \property QChart::animationOptions
 The animation \a options for the chart. Animations are enabled/disabled based on this setting.
 */

/*!
 \property QChart::animationDuration
 The duration of the animation for the chart.
 */

/*!
 \property QChart::animationEasingCurve
 The easing curve of the animation for the chart.
 */

/*!
 \property QChart::backgroundVisible
 Specifies whether the chart background is visible or not.
 \sa setBackgroundBrush(), setBackgroundPen(), plotAreaBackgroundVisible
 */

/*!
 \property QChart::dropShadowEnabled
 If set to true, the background drop shadow effect is enabled. If set to false, it is disabled. Note that the drop
 shadow effect depends on theme, which means the setting may be changed if you switch to another theme.
 */

/*!
 \property QChart::backgroundRoundness
 The diameter of the rounding circle at the corners of the chart background.
 */

/*!
 \property QChart::margins
 Margins between the plot area (axes) and the edge of the chart widget.
 */

/*!
 \property QChart::theme
 Theme is a built-in collection of UI style related settings applied for all visual elements of a chart, like colors,
 pens, brushes, and fonts of series, axes, title, and legend. \l {Chart themes example} shows an example with a few
 different themes.
 \note Changing the theme will overwrite all customizations previously applied to the series.
 */

/*!
 \property QChart::title
 Title is the name (label) of a chart. It is shown as a headline on top of the chart. Chart title supports html formatting.
 */

/*!
 \property QChart::chartType
 Chart type indicates if the chart is a cartesian chart or a polar chart.
 This property is set internally and it is read only.
 \sa QPolarChart
 */

/*!
 \property QChart::plotAreaBackgroundVisible
 Specifies whether the chart plot area background is visible or not.
 \note By default the plot area background is not visible and the plot area uses
 the general chart background.
 \sa setPlotAreaBackgroundBrush(), setPlotAreaBackgroundPen(), backgroundVisible
 */

/*!
  \property QChart::localizeNumbers
  \since QtCharts 2.0
  When \c{true}, all generated numbers appearing in various series and axis labels will be
  localized using the QLocale set with the locale property.
  When \c{false}, the "C" locale is always used.
  Defaults to \c{false}.
  \note This property doesn't affect QDateTimeAxis labels, which always use the QLocale set with
  the locale property.

  \sa locale
*/

/*!
  \property QChart::locale
  \since QtCharts 2.0
  Sets the locale used to format various chart labels when localizeNumbers is \c{true}.
  This also determines the locale used to format QDateTimeAxis labels regardless of
  localizeNumbers property.
  Defaults to application default locale at the time the chart is constructed.

  \sa localizeNumbers
*/

/*!
  \property QChart::plotArea
  Holds the rectangle within which the drawing of the chart is done.
  It does not include the area defined by margins.
*/

/*!
 \internal
 Constructs a chart object of \a type which is a child of a \a parent.
 Parameter \a wFlags is passed to the QGraphicsWidget constructor.
 This constructor is called only by subclasses.
*/
QChart::QChart(QChart::ChartType type, QGraphicsItem *parent, Qt::WindowFlags wFlags)
    : QGraphicsWidget(parent, wFlags),
      d_ptr(new QChartPrivate(this, type))
{
    d_ptr->init();
}

/*!
 Constructs a chart object which is a child of a \a parent.
 Parameter \a wFlags is passed to the QGraphicsWidget constructor.
 */
QChart::QChart(QGraphicsItem *parent, Qt::WindowFlags wFlags)
    : QGraphicsWidget(parent, wFlags),
      d_ptr(new QChartPrivate(this, ChartTypeCartesian))
{
    d_ptr->init();
}

/*!
 Destroys the chart object and its children, like series and axis objects added to it.
 */
QChart::~QChart()
{
    //start by deleting dataset, it will remove all series and axes
    delete d_ptr->m_dataset;
    d_ptr->m_dataset = 0;
}

/*!
 Adds the \a series onto the chart and takes the ownership of it.

 \note A newly added series is attached to no axes by default, including any axes that were created for the chart
 using createDefaultAxes() before the series was added to the chart. If no axes are attached to
 the newly added series before the chart is shown, the series will get drawn as if it had axes with ranges
 that exactly fit the series to the plot area of the chart. This can be confusing if the same chart also displays other
 series that have properly attached axes, so always make sure you either call createDefaultAxes() after
 a series has been added or explicitly attach axes for the series.

 \sa removeSeries(), removeAllSeries(), createDefaultAxes(), QAbstractSeries::attachAxis()
 */
void QChart::addSeries(QAbstractSeries *series)
{
    Q_ASSERT(series);
    d_ptr->m_dataset->addSeries(series);
}

/*!
 Removes the \a series from the chart.
 The chart releases its ownership of the specified \a series object.

 \sa addSeries(), removeAllSeries()
 */
void QChart::removeSeries(QAbstractSeries *series)
{
    Q_ASSERT(series);
    d_ptr->m_dataset->removeSeries(series);
}

/*!
 Removes and deletes all series objects that have been added to the chart.

 \sa addSeries(), removeSeries()
 */
void QChart::removeAllSeries()
{
    foreach (QAbstractSeries *s ,  d_ptr->m_dataset->series()){
        removeSeries(s);
        delete s;
    }
}

/*!
 Sets the \a brush that is used for painting the background of the chart area.
 */
void QChart::setBackgroundBrush(const QBrush &brush)
{
    d_ptr->m_presenter->setBackgroundBrush(brush);
}

/*!
 Gets the brush that is used for painting the background of the chart area.
 */
QBrush QChart::backgroundBrush() const
{
    return d_ptr->m_presenter->backgroundBrush();
}

/*!
 Sets the \a pen that is used for painting the background of the chart area.
 */
void QChart::setBackgroundPen(const QPen &pen)
{
    d_ptr->m_presenter->setBackgroundPen(pen);
}

/*!
 Gets the pen that is used for painting the background of the chart area.
 */
QPen QChart::backgroundPen() const
{
    return d_ptr->m_presenter->backgroundPen();
}

void QChart::setTitle(const QString &title)
{
    d_ptr->m_presenter->setTitle(title);
}

QString QChart::title() const
{
    return d_ptr->m_presenter->title();
}

/*!
 Sets the \a font that is used for drawing the chart title.
 */
void QChart::setTitleFont(const QFont &font)
{
    d_ptr->m_presenter->setTitleFont(font);
}

/*!
 Gets the font that is used for drawing the chart title.
 */
QFont QChart::titleFont() const
{
    return d_ptr->m_presenter->titleFont();
}

/*!
 Sets the \a brush used for drawing the title text.
 */
void QChart::setTitleBrush(const QBrush &brush)
{
    d_ptr->m_presenter->setTitleBrush(brush);
}

/*!
 Returns the brush used for drawing the title text.
 */
QBrush QChart::titleBrush() const
{
    return d_ptr->m_presenter->titleBrush();
}

void QChart::setTheme(QChart::ChartTheme theme)
{
    d_ptr->m_themeManager->setTheme(theme);
}

QChart::ChartTheme QChart::theme() const
{
    return d_ptr->m_themeManager->theme()->id();
}

/*!
 Zooms in the view by a factor of two.
 */
void QChart::zoomIn()
{
    d_ptr->zoomIn(2.0);
}

/*!
 Zooms in the view to a maximum level at which \a rect is still fully visible.
 \note This is not supported for polar charts.
 */
void QChart::zoomIn(const QRectF &rect)
{
    if (d_ptr->m_type == QChart::ChartTypePolar)
        return;
    d_ptr->zoomIn(rect);
}

/*!
 Zooms out the view by a factor of two.
 */
void QChart::zoomOut()
{
    d_ptr->zoomOut(2.0);
}

/*!
 Zooms in the view by a custom \a factor.

 A factor over 1.0 zooms the view in and factor between 0.0 and 1.0 zooms out.
 */
void QChart::zoom(qreal factor)
{
    if (qFuzzyCompare(factor, 0))
        return;

    if (qFuzzyCompare(factor, (qreal)1.0))
        return;

    if (factor < 0)
        return;

    if (factor > 1.0)
        d_ptr->zoomIn(factor);
    else
        d_ptr->zoomOut(1.0 / factor);
}


/*!
 Resets the series domains to what they were before any zoom method was called.
 Note that this will also reset any scrolls and explicit axis range settings done between
 the first zoom operation and calling this method. If no zoom operation has been
 done, this method does nothing.
 */
void QChart::zoomReset()
{
    d_ptr->zoomReset();
}

/*!
 Returns true if any series has a zoomed domain.
 */
bool QChart::isZoomed()
{
   return d_ptr->isZoomed();
}

/*!
 Returns a pointer to the horizontal axis attached to the specified \a series.
 If no \a series is specified, the first horizontal axis added to the chart is returned.

 \sa addAxis(), QAbstractSeries::attachAxis()
 */
QAbstractAxis *QChart::axisX(QAbstractSeries *series) const
{
    QList<QAbstractAxis *> axisList = axes(Qt::Horizontal, series);
    if (axisList.count())
        return axisList[0];
    return 0;
}

/*!
 Returns a pointer to the vertical axis attached to the specified \a series.
 If no \a series is specified, the first vertical axis added to the chart is returned.

 \sa addAxis(), QAbstractSeries::attachAxis()
 */
QAbstractAxis *QChart::axisY(QAbstractSeries *series) const
{
    QList<QAbstractAxis *> axisList = axes(Qt::Vertical, series);
    if (axisList.count())
        return axisList[0];
    return 0;
}

/*!
 Returns the axes attached to the \a series with \a orientation. If no \a series is provided,
 then all axes added to the chart with the specified orientation are returned.
 \sa addAxis(), createDefaultAxes()
 */
QList<QAbstractAxis *> QChart::axes(Qt::Orientations orientation, QAbstractSeries *series) const
{
    QList<QAbstractAxis *> result ;

    if (series) {
        foreach (QAbstractAxis *axis, series->attachedAxes()){
            if (orientation.testFlag(axis->orientation()))
                result << axis;
        }
    } else {
        foreach (QAbstractAxis *axis, d_ptr->m_dataset->axes()){
            if (orientation.testFlag(axis->orientation()) && !result.contains(axis))
                result << axis;
        }
    }

    return result;
}

/*!
 Creates axes for the chart based on the series that have already been added to the chart. Any axes previously added to
 the chart will be deleted.

 \note This function has to be called after all series have been added to the chart. The axes created by this function
 will NOT get automatically attached to any series added to the chart after this function has been called.
 A series with no axes attached will by default scale to utilize the entire plot area of the chart, which can be confusing
 if there are other series with properly attached axes also present.

 \table
     \header
         \li Series type
         \li X-axis
         \li Y-axis
     \row
         \li QXYSeries
         \li QValueAxis
         \li QValueAxis
     \row
         \li QBarSeries
         \li QBarCategoryAxis
         \li QValueAxis
     \row
         \li QPieSeries
         \li None
         \li None
 \endtable

 If there are several QXYSeries derived series added to the chart and no series of other types have been added, then only one pair of axes is created.
 If there are several series of different types added to the chart, then each series gets its own axes pair.

 The axes specific to the series can be later obtained from the chart by providing the series as the parameter for axes() function call.
 QPieSeries does not create any axes.

 \sa axisX(), axisY(), axes(), setAxisX(), setAxisY(), QAbstractSeries::attachAxis()
 */
void QChart::createDefaultAxes()
{
    d_ptr->m_dataset->createDefaultAxes();
}

/*!
 Returns the legend object of the chart. Ownership stays with the chart.
 */
QLegend *QChart::legend() const
{
    return d_ptr->m_legend;
}

void QChart::setMargins(const QMargins &margins)
{
    d_ptr->m_presenter->layout()->setMargins(margins);
}

QMargins QChart::margins() const
{
    return d_ptr->m_presenter->layout()->margins();
}

QChart::ChartType QChart::chartType() const
{
    return d_ptr->m_type;
}

QRectF QChart::plotArea() const
{
    return d_ptr->m_presenter->geometry();
}

/*!
    Sets the \a brush for the background of the plot area of the chart.

    \sa plotArea(), plotAreaBackgroundVisible, setPlotAreaBackgroundPen(), plotAreaBackgroundBrush()
 */
void QChart::setPlotAreaBackgroundBrush(const QBrush &brush)
{
    d_ptr->m_presenter->setPlotAreaBackgroundBrush(brush);
}

/*!
    Returns the brush for the background of the plot area of the chart.

    \sa plotArea(), plotAreaBackgroundVisible, plotAreaBackgroundPen(), setPlotAreaBackgroundBrush()
 */
QBrush QChart::plotAreaBackgroundBrush() const
{
    return d_ptr->m_presenter->plotAreaBackgroundBrush();
}

/*!
    Sets the \a pen for the background of the plot area of the chart.

    \sa plotArea(), plotAreaBackgroundVisible, setPlotAreaBackgroundBrush(), plotAreaBackgroundPen()
 */
void QChart::setPlotAreaBackgroundPen(const QPen &pen)
{
    d_ptr->m_presenter->setPlotAreaBackgroundPen(pen);
}

/*!
    Returns the pen for the background of the plot area of the chart.

    \sa plotArea(), plotAreaBackgroundVisible, plotAreaBackgroundBrush(), setPlotAreaBackgroundPen()
 */
QPen QChart::plotAreaBackgroundPen() const
{
    return d_ptr->m_presenter->plotAreaBackgroundPen();
}

void QChart::setPlotAreaBackgroundVisible(bool visible)
{
    d_ptr->m_presenter->setPlotAreaBackgroundVisible(visible);
}

bool QChart::isPlotAreaBackgroundVisible() const
{
    return d_ptr->m_presenter->isPlotAreaBackgroundVisible();
}

void QChart::setLocalizeNumbers(bool localize)
{
    d_ptr->m_presenter->setLocalizeNumbers(localize);
}

bool QChart::localizeNumbers() const
{
    return d_ptr->m_presenter->localizeNumbers();
}

void QChart::setLocale(const QLocale &locale)
{
    d_ptr->m_presenter->setLocale(locale);
}

QLocale QChart::locale() const
{
    return d_ptr->m_presenter->locale();
}

void QChart::setAnimationOptions(AnimationOptions options)
{
    d_ptr->m_presenter->setAnimationOptions(options);
}

QChart::AnimationOptions QChart::animationOptions() const
{
    return d_ptr->m_presenter->animationOptions();
}

void QChart::setAnimationDuration(int msecs)
{
    d_ptr->m_presenter->setAnimationDuration(msecs);
}

int QChart::animationDuration() const
{
    return d_ptr->m_presenter->animationDuration();
}

void QChart::setAnimationEasingCurve(const QEasingCurve &curve)
{
    d_ptr->m_presenter->setAnimationEasingCurve(curve);
}

QEasingCurve QChart::animationEasingCurve() const
{
    return d_ptr->m_presenter->animationEasingCurve();
}

/*!
    Scrolls the visible area of the chart by the distance defined in the \a dx and \a dy.

    For polar charts, \a dx indicates the angle along angular axis instead of distance.
 */
void QChart::scroll(qreal dx, qreal dy)
{
    d_ptr->scroll(dx,dy);
}

void QChart::setBackgroundVisible(bool visible)
{
    d_ptr->m_presenter->setBackgroundVisible(visible);
}

bool QChart::isBackgroundVisible() const
{
    return d_ptr->m_presenter->isBackgroundVisible();
}

void QChart::setDropShadowEnabled(bool enabled)
{
    d_ptr->m_presenter->setBackgroundDropShadowEnabled(enabled);
}

bool QChart::isDropShadowEnabled() const
{
    return d_ptr->m_presenter->isBackgroundDropShadowEnabled();
}

void QChart::setBackgroundRoundness(qreal diameter)
{
    d_ptr->m_presenter->setBackgroundRoundness(diameter);
}

qreal QChart::backgroundRoundness() const
{
    return d_ptr->m_presenter->backgroundRoundness();
}

/*!
  Returns all series that are added to the chart.

  \sa addSeries(), removeSeries(), removeAllSeries()
*/
QList<QAbstractSeries *> QChart::series() const
{
    return d_ptr->m_dataset->series();
}

/*!
  Adds the \a axis to the chart and attaches it to the \a series as a bottom-aligned horizontal axis.
  The chart takes ownership of both the \a axis and the \a series.
  Any horizontal axes previously attached to the \a series are deleted.

  \sa axisX(), axisY(), setAxisY(), createDefaultAxes(), QAbstractSeries::attachAxis()
*/
void QChart::setAxisX(QAbstractAxis *axis ,QAbstractSeries *series)
{
    QList<QAbstractAxis*> list = axes(Qt::Horizontal, series);

    foreach (QAbstractAxis* a, list) {
        d_ptr->m_dataset->removeAxis(a);
        delete a;
    }

    if (!d_ptr->m_dataset->axes().contains(axis))
        d_ptr->m_dataset->addAxis(axis, Qt::AlignBottom);
    d_ptr->m_dataset->attachAxis(series, axis);
}

/*!
  Adds the \a axis to the chart and attaches it to the \a series as a left-aligned vertical axis.
  The chart takes ownership of both the \a axis and the \a series.
  Any vertical axes previously attached to the \a series are deleted.

  \sa axisX(), axisY(), setAxisX(), createDefaultAxes(), QAbstractSeries::attachAxis()
*/
void QChart::setAxisY(QAbstractAxis *axis ,QAbstractSeries *series)
{
    QList<QAbstractAxis*> list = axes(Qt::Vertical, series);

    foreach (QAbstractAxis* a, list) {
        d_ptr->m_dataset->removeAxis(a);
        delete a;
    }

    if (!d_ptr->m_dataset->axes().contains(axis))
        d_ptr->m_dataset->addAxis(axis, Qt::AlignLeft);
    d_ptr->m_dataset->attachAxis(series, axis);
}

/*!
  Adds the \a axis to the chart with \a alignment. The chart takes the ownership of the axis.

  \sa removeAxis(), createDefaultAxes(), QAbstractSeries::attachAxis()
*/
void QChart::addAxis(QAbstractAxis *axis, Qt::Alignment alignment)
{
    d_ptr->m_dataset->addAxis(axis, alignment);
}

/*!
  Removes the \a axis from the chart.
  The chart releases its ownership of the specified \a axis object.

  \sa addAxis(), createDefaultAxes(), QAbstractSeries::detachAxis()
*/
void QChart::removeAxis(QAbstractAxis *axis)
{
    d_ptr->m_dataset->removeAxis(axis);
}

/*!
  Returns the value in the \a series domain that corresponds to the \a position relative to chart widget.
*/
QPointF QChart::mapToValue(const QPointF &position, QAbstractSeries *series)
{
    return d_ptr->m_dataset->mapToValue(position, series);
}

/*!
  Returns the position on the chart widget that corresponds to the \a value in the \a series domain.
*/
QPointF QChart::mapToPosition(const QPointF &value, QAbstractSeries *series)
{
    return d_ptr->m_dataset->mapToPosition(value, series);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QChartPrivate::QChartPrivate(QChart *q, QChart::ChartType type):
    q_ptr(q),
    m_legend(0),
    m_dataset(new ChartDataSet(q)),
    m_presenter(new ChartPresenter(q, type)),
    m_themeManager(new ChartThemeManager(q)),
    m_type(type)
{
    QObject::connect(m_dataset, SIGNAL(seriesAdded(QAbstractSeries*)), m_presenter, SLOT(handleSeriesAdded(QAbstractSeries*)));
    QObject::connect(m_dataset, SIGNAL(seriesRemoved(QAbstractSeries*)), m_presenter, SLOT(handleSeriesRemoved(QAbstractSeries*)));
    QObject::connect(m_dataset, SIGNAL(axisAdded(QAbstractAxis*)), m_presenter, SLOT(handleAxisAdded(QAbstractAxis*)));
    QObject::connect(m_dataset, SIGNAL(axisRemoved(QAbstractAxis*)), m_presenter, SLOT(handleAxisRemoved(QAbstractAxis*)));
    QObject::connect(m_dataset, SIGNAL(seriesAdded(QAbstractSeries*)), m_themeManager, SLOT(handleSeriesAdded(QAbstractSeries*)));
    QObject::connect(m_dataset, SIGNAL(seriesRemoved(QAbstractSeries*)), m_themeManager, SLOT(handleSeriesRemoved(QAbstractSeries*)));
    QObject::connect(m_dataset, SIGNAL(axisAdded(QAbstractAxis*)), m_themeManager, SLOT(handleAxisAdded(QAbstractAxis*)));
    QObject::connect(m_dataset, SIGNAL(axisRemoved(QAbstractAxis*)), m_themeManager, SLOT(handleAxisRemoved(QAbstractAxis*)));
    QObject::connect(m_presenter, &ChartPresenter::plotAreaChanged, q, &QChart::plotAreaChanged);
}

QChartPrivate::~QChartPrivate()
{
    delete m_themeManager;
}

// Hackish solution to the problem of explicitly assigning the default pen/brush/font
// to a series or axis and having theme override it:
// Initialize pens, brushes, and fonts to something nobody is likely to ever use,
// so that default theme initialization will always set these properly.
QPen &QChartPrivate::defaultPen()
{
    static QPen defaultPen(QColor(1, 2, 0), 0.93247536);
    return defaultPen;
}

QBrush &QChartPrivate::defaultBrush()
{
    static QBrush defaultBrush(QColor(1, 2, 0), Qt::Dense7Pattern);
    return defaultBrush;
}

QFont &QChartPrivate::defaultFont()
{
    static bool defaultFontInitialized(false);
    static QFont defaultFont;
    if (!defaultFontInitialized) {
        defaultFont.setPointSizeF(8.34563465);
        defaultFontInitialized = true;
    }
    return defaultFont;
}

void QChartPrivate::init()
{
    m_legend = new LegendScroller(q_ptr);
    q_ptr->setTheme(QChart::ChartThemeLight);
    q_ptr->setLayout(m_presenter->layout());
}

void QChartPrivate::zoomIn(qreal factor)
{
    QRectF rect = m_presenter->geometry();
    rect.setWidth(rect.width() / factor);
    rect.setHeight(rect.height() / factor);
    rect.moveCenter(m_presenter->geometry().center());
    zoomIn(rect);
}

void QChartPrivate::zoomIn(const QRectF &rect)
{
    if (!rect.isValid())
        return;

    QRectF r = rect.normalized();
    const QRectF geometry = m_presenter->geometry();
    r.translate(-geometry.topLeft());

    if (!r.isValid())
        return;

    QPointF zoomPoint(r.center().x() / geometry.width(), r.center().y() / geometry.height());
    m_presenter->setState(ChartPresenter::ZoomInState,zoomPoint);
    m_dataset->zoomInDomain(r);
    m_presenter->setState(ChartPresenter::ShowState,QPointF());

}

void QChartPrivate::zoomReset()
{
    m_dataset->zoomResetDomain();
}

bool QChartPrivate::isZoomed()
{
    return m_dataset->isZoomedDomain();
}

void QChartPrivate::zoomOut(qreal factor)
{
    const QRectF geometry = m_presenter->geometry();

    QRectF r;
    r.setSize(geometry.size() / factor);
    r.moveCenter(QPointF(geometry.size().width()/2 ,geometry.size().height()/2));
    if (!r.isValid())
        return;

    QPointF zoomPoint(r.center().x() / geometry.width(), r.center().y() / geometry.height());
    m_presenter->setState(ChartPresenter::ZoomOutState,zoomPoint);
    m_dataset->zoomOutDomain(r);
    m_presenter->setState(ChartPresenter::ShowState,QPointF());
}

void QChartPrivate::scroll(qreal dx, qreal dy)
{
    if (dx < 0) m_presenter->setState(ChartPresenter::ScrollLeftState,QPointF());
    if (dx > 0) m_presenter->setState(ChartPresenter::ScrollRightState,QPointF());
    if (dy < 0) m_presenter->setState(ChartPresenter::ScrollUpState,QPointF());
    if (dy > 0) m_presenter->setState(ChartPresenter::ScrollDownState,QPointF());

    m_dataset->scrollDomain(dx, dy);
    m_presenter->setState(ChartPresenter::ShowState,QPointF());
}

#include "moc_qchart.cpp"

QT_CHARTS_END_NAMESPACE
