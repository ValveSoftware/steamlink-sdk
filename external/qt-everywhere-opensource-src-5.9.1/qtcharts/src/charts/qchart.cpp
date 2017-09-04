/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

 A theme is a built-in collection of UI style related settings applied to all
 the visual elements of a chart, such as colors, pens, brushes, and fonts of
 series, as well as axes, title, and legend. The \l {Chart themes example}
 illustrates how to use themes.

 \note Changing the theme will overwrite all customizations previously applied
 to the series.

 \value ChartThemeLight
        The light theme, which is the default theme.
 \value ChartThemeBlueCerulean
        The cerulean blue theme.
 \value ChartThemeDark
        The dark theme.
 \value ChartThemeBrownSand
        The sand brown theme.
 \value ChartThemeBlueNcs
        The natural color system (NCS) blue theme.
 \value ChartThemeHighContrast
        The high contrast theme.
 \value ChartThemeBlueIcy
        The icy blue theme.
 \value ChartThemeQt
        The Qt theme.
 */

/*!
 \enum QChart::AnimationOption

 This enum describes the animations enabled in the chart.

 \value NoAnimation
        Animation is disabled in the chart. This is the default value.
 \value GridAxisAnimations
        Grid axis animation is enabled in the chart.
 \value SeriesAnimations
        Series animation is enabled in the chart.
 \value AllAnimations
        All animation types are enabled in the chart.
 */

/*!
 \enum QChart::ChartType

 This enum describes the chart type.

 \value ChartTypeUndefined
        The chart type is not defined.
 \value ChartTypeCartesian
        A cartesian chart.
 \value ChartTypePolar
        A polar chart.
 */

/*!
 \class QChart
 \inmodule Qt Charts
 \brief The QChart class manages the graphical representation of the chart's
 series, legends, and axes.

 QChart is a QGraphicsWidget that you can show in a QGraphicsScene. It manages the graphical
 representation of different types of series and other chart related objects like legend and
 axes. To simply show a chart in a layout, the convenience class QChartView can be used
 instead of QChart. In addition, line, spline, area, and scatter series can be presented as
 polar charts by using the QPolarChart class.

 \sa QChartView, QPolarChart
 */

/*!
 \property QChart::animationOptions
 \brief The animation options for the chart.

 Animations are enabled or disabled based on this setting.
 */

/*!
 \property QChart::animationDuration
 \brief The duration of the animation for the chart.
 */

/*!
 \property QChart::animationEasingCurve
 \brief The easing curve of the animation for the chart.
 */

/*!
 \property QChart::backgroundVisible
 \brief Whether the chart background is visible.
 \sa setBackgroundBrush(), setBackgroundPen(), plotAreaBackgroundVisible
 */

/*!
 \property QChart::dropShadowEnabled
 \brief Whether the background drop shadow effect is enabled.

 If set to \c true, the background drop shadow effect is enabled. If set to \c false, it
 is disabled.

 \note The drop shadow effect depends on the theme, and therefore the setting may
 change if the theme is changed.
 */

/*!
 \property QChart::backgroundRoundness
 \brief The diameter of the rounding circle at the corners of the chart background.
 */

/*!
 \property QChart::margins
 \brief The minimum margins allowed between the edge of the chart rectangle and
 the plot area.

 The margins are used for drawing the title, axes, and legend.
 */

/*!
 \property QChart::theme
 \brief The theme used for the chart.
 */

/*!
 \property QChart::title
 \brief The title of the chart.

 The title is shown as a headline on top of the chart. Chart titles support HTML formatting.
 */

/*!
 \property QChart::chartType
 \brief Whether the chart is a cartesian chart or a polar chart.

 This property is set internally and it is read only.
 \sa QPolarChart
 */

/*!
 \property QChart::plotAreaBackgroundVisible
 \brief Whether the chart plot area background is visible.

 \note By default, the plot area background is invisible and the plot area uses
 the general chart background.
 \sa setPlotAreaBackgroundBrush(), setPlotAreaBackgroundPen(), backgroundVisible
 */

/*!
  \property QChart::localizeNumbers
  \brief Whether numbers are localized.

  When \c{true}, all generated numbers appearing in various series and axis labels will be
  localized using the QLocale set with the \l locale property.
  When \c{false}, the \e C locale is always used.
  Defaults to \c{false}.

  \note This property does not affect QDateTimeAxis labels, which always use the QLocale set with
  the locale property.

  \sa locale
*/

/*!
  \property QChart::locale
  \brief The locale used to format various chart labels.

  Labels are localized only when \l localizeNumbers is \c true, except for QDateTimeAxis
  labels, which always use the QLocale set with this property.

  Defaults to the application default locale at the time when the chart is constructed.

  \sa localizeNumbers
*/

/*!
  \property QChart::plotArea
  \brief The rectangle within which the chart is drawn.

  The plot area does not include the area defined by margins.
*/

/*!
 \internal
 Constructs a chart object of \a type that is a child of \a parent.
 The properties specified by \a wFlags are passed to the QGraphicsWidget constructor.
 This constructor is called only by subclasses.
*/
QChart::QChart(QChart::ChartType type, QGraphicsItem *parent, Qt::WindowFlags wFlags)
    : QGraphicsWidget(parent, wFlags),
      d_ptr(new QChartPrivate(this, type))
{
    d_ptr->init();
}

/*!
 Constructs a chart object that is a child of \a parent.
 The properties specified by \a wFlags are passed to the QGraphicsWidget constructor.
 */
QChart::QChart(QGraphicsItem *parent, Qt::WindowFlags wFlags)
    : QGraphicsWidget(parent, wFlags),
      d_ptr(new QChartPrivate(this, ChartTypeCartesian))
{
    d_ptr->init();
}

/*!
 Deletes the chart object and its children, such as the series and axis objects added to it.
 */
QChart::~QChart()
{
    //start by deleting dataset, it will remove all series and axes
    delete d_ptr->m_dataset;
    d_ptr->m_dataset = 0;
}

/*!
 Adds the series \a series to the chart and takes ownership of it.

 \note A newly added series is not attached to any axes by default, not even those that
 might have been created for the chart
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
 Removes the series \a series from the chart.
 The chart releases the ownership of the specified \a series object.

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
 Sets the brush that is used for painting the background of the chart area to \a brush.
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
 Sets the pen that is used for painting the background of the chart area to \a pen.
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
 Sets the font that is used for drawing the chart title to \a font.
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
 Sets the brush used for drawing the title text to \a brush.
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
 Zooms into the view by a factor of two.
 */
void QChart::zoomIn()
{
    d_ptr->zoomIn(2.0);
}

/*!
 Zooms into the view to a maximum level at which the rectangle \a rect is still
 fully visible.
 \note This is not supported for polar charts.
 */
void QChart::zoomIn(const QRectF &rect)
{
    if (d_ptr->m_type == QChart::ChartTypePolar)
        return;
    d_ptr->zoomIn(rect);
}

/*!
 Zooms out of the view by a factor of two.
 */
void QChart::zoomOut()
{
    d_ptr->zoomOut(2.0);
}

/*!
 Zooms into the view by the custom factor \a factor.

 A factor over 1.0 zooms into the view and a factor between 0.0 and 1.0 zooms
 out of it.
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

 \note This will also reset scrolling and explicit axis range settings specified between
 the first zoom operation and calling this method. If no zoom operation has been
 performed, this method does nothing.
 */
void QChart::zoomReset()
{
    d_ptr->zoomReset();
}

/*!
 Returns \c true if any series has a zoomed domain.
 */
bool QChart::isZoomed()
{
   return d_ptr->isZoomed();
}

/*!
 Returns a pointer to the horizontal axis attached to the specified \a series.
 If no series is specified, the first horizontal axis added to the chart is returned.

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
 If no series is specified, the first vertical axis added to the chart is returned.

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
 Returns the axes attached to the series \a series with the orientation specified
 by \a orientation. If no series is specified, all axes added to the chart with
 the specified orientation are returned.
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

 The axes specific to the series can be later obtained from the chart by providing the series
 as the parameter for the axes() function call.
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
    Sets the brush used to fill the background of the plot area of the chart to \a brush.

    \sa plotArea(), plotAreaBackgroundVisible, setPlotAreaBackgroundPen(), plotAreaBackgroundBrush()
 */
void QChart::setPlotAreaBackgroundBrush(const QBrush &brush)
{
    d_ptr->m_presenter->setPlotAreaBackgroundBrush(brush);
}

/*!
    Returns the brush used to fill the background of the plot area of the chart.

    \sa plotArea(), plotAreaBackgroundVisible, plotAreaBackgroundPen(), setPlotAreaBackgroundBrush()
 */
QBrush QChart::plotAreaBackgroundBrush() const
{
    return d_ptr->m_presenter->plotAreaBackgroundBrush();
}

/*!
    Sets the pen used to draw the background of the plot area of the chart to \a pen.

    \sa plotArea(), plotAreaBackgroundVisible, setPlotAreaBackgroundBrush(), plotAreaBackgroundPen()
 */
void QChart::setPlotAreaBackgroundPen(const QPen &pen)
{
    d_ptr->m_presenter->setPlotAreaBackgroundPen(pen);
}

/*!
    Returns the pen used to draw the background of the plot area of the chart.

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
    Scrolls the visible area of the chart by the distance specified by \a dx and \a dy.

    For polar charts, \a dx indicates the angle along the angular axis instead of distance.
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
  Adds the axis \a axis to the chart and attaches it to the series \a series as a
  bottom-aligned horizontal axis.
  The chart takes ownership of both the axis and the  series.
  Any horizontal axes previously attached to the series are deleted.

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
  Adds the axis \a axis to the chart and attaches it to the series \a series as a
  left-aligned vertical axis.
  The chart takes ownership of both the axis and the series.
  Any vertical axes previously attached to the series are deleted.

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
  Adds the axis \a axis to the chart aligned as specified by \a alignment.
  The chart takes the ownership of the axis.

  \sa removeAxis(), createDefaultAxes(), QAbstractSeries::attachAxis()
*/
void QChart::addAxis(QAbstractAxis *axis, Qt::Alignment alignment)
{
    d_ptr->m_dataset->addAxis(axis, alignment);
}

/*!
  Removes the axis \a axis from the chart.
  The chart releases the ownership of the specified \a axis object.

  \sa addAxis(), createDefaultAxes(), QAbstractSeries::detachAxis()
*/
void QChart::removeAxis(QAbstractAxis *axis)
{
    d_ptr->m_dataset->removeAxis(axis);
}

/*!
  Returns the value in the series specified by \a series at the position
  specified by \a position in a chart.
*/
QPointF QChart::mapToValue(const QPointF &position, QAbstractSeries *series)
{
    return d_ptr->m_dataset->mapToValue(position, series);
}

/*!
  Returns the position on the chart that corresponds to the value \a value in the
  series specified by \a series.
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
