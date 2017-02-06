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

#include <private/chartaxiselement_p.h>
#include <private/qabstractaxis_p.h>
#include <private/chartpresenter_p.h>
#include <private/abstractchartlayout_p.h>
#include <QtCharts/QCategoryAxis>
#include <QtCore/QtMath>
#include <QtCore/QDateTime>
#include <QtGui/QTextDocument>
#include <cmath>

QT_CHARTS_BEGIN_NAMESPACE

static const char *labelFormatMatchString = "%[\\-\\+#\\s\\d\\.\\'lhjztL]*([dicuoxfegXFEG])";
static const char *labelFormatMatchLocalizedString = "^([^%]*)%\\.(\\d+)([defgiEG])(.*)$";
static QRegExp *labelFormatMatcher = 0;
static QRegExp *labelFormatMatcherLocalized = 0;
class StaticLabelFormatMatcherDeleter
{
public:
    StaticLabelFormatMatcherDeleter() {}
    ~StaticLabelFormatMatcherDeleter() {
        delete labelFormatMatcher;
        delete labelFormatMatcherLocalized;
    }
};
static StaticLabelFormatMatcherDeleter staticLabelFormatMatcherDeleter;

ChartAxisElement::ChartAxisElement(QAbstractAxis *axis, QGraphicsItem *item, bool intervalAxis)
    : ChartElement(item),
      m_axis(axis),
      m_animation(0),
      m_grid(new QGraphicsItemGroup(item)),
      m_arrow(new QGraphicsItemGroup(item)),
      m_minorGrid(new QGraphicsItemGroup(item)),
      m_minorArrow(new QGraphicsItemGroup(item)),
      m_shades(new QGraphicsItemGroup(item)),
      m_labels(new QGraphicsItemGroup(item)),
      m_title(new QGraphicsTextItem(item)),
      m_intervalAxis(intervalAxis)

{
    //initial initialization
    m_arrow->setHandlesChildEvents(false);
    m_arrow->setZValue(ChartPresenter::AxisZValue);
    m_minorArrow->setHandlesChildEvents(false);
    m_minorArrow->setZValue(ChartPresenter::AxisZValue);
    m_labels->setZValue(ChartPresenter::AxisZValue);
    m_shades->setZValue(ChartPresenter::ShadesZValue);
    m_grid->setZValue(ChartPresenter::GridZValue);
    m_minorGrid->setZValue(ChartPresenter::GridZValue);
    m_title->setZValue(ChartPresenter::GridZValue);
    m_title->document()->setDocumentMargin(ChartPresenter::textMargin());
    handleVisibleChanged(axis->isVisible());
    connectSlots();

    setFlag(QGraphicsItem::ItemHasNoContents, true);
}

ChartAxisElement::~ChartAxisElement()
{
}

void ChartAxisElement::connectSlots()
{
    QObject::connect(axis(), SIGNAL(visibleChanged(bool)), this, SLOT(handleVisibleChanged(bool)));
    QObject::connect(axis(), SIGNAL(lineVisibleChanged(bool)), this, SLOT(handleArrowVisibleChanged(bool)));
    QObject::connect(axis(), SIGNAL(gridVisibleChanged(bool)), this, SLOT(handleGridVisibleChanged(bool)));
    QObject::connect(axis(), SIGNAL(labelsVisibleChanged(bool)), this, SLOT(handleLabelsVisibleChanged(bool)));
    QObject::connect(axis(), SIGNAL(shadesVisibleChanged(bool)), this, SLOT(handleShadesVisibleChanged(bool)));
    QObject::connect(axis(), SIGNAL(labelsAngleChanged(int)), this, SLOT(handleLabelsAngleChanged(int)));
    QObject::connect(axis(), SIGNAL(linePenChanged(const QPen&)), this, SLOT(handleArrowPenChanged(const QPen&)));
    QObject::connect(axis(), SIGNAL(labelsBrushChanged(const QBrush&)), this, SLOT(handleLabelsBrushChanged(const QBrush&)));
    QObject::connect(axis(), SIGNAL(labelsFontChanged(const QFont&)), this, SLOT(handleLabelsFontChanged(const QFont&)));
    QObject::connect(axis(), SIGNAL(gridLinePenChanged(const QPen&)), this, SLOT(handleGridPenChanged(const QPen&)));
    QObject::connect(axis(), SIGNAL(shadesPenChanged(const QPen&)), this, SLOT(handleShadesPenChanged(const QPen&)));
    QObject::connect(axis(), SIGNAL(shadesBrushChanged(const QBrush&)), this, SLOT(handleShadesBrushChanged(const QBrush&)));
    QObject::connect(axis(), SIGNAL(titleTextChanged(const QString&)), this, SLOT(handleTitleTextChanged(const QString&)));
    QObject::connect(axis(), SIGNAL(titleFontChanged(const QFont&)), this, SLOT(handleTitleFontChanged(const QFont&)));
    QObject::connect(axis(), SIGNAL(titleBrushChanged(const QBrush&)), this, SLOT(handleTitleBrushChanged(const QBrush&)));
    QObject::connect(axis(), SIGNAL(titleVisibleChanged(bool)), this, SLOT(handleTitleVisibleChanged(bool)));
    QObject::connect(axis()->d_ptr.data(), SIGNAL(rangeChanged(qreal, qreal)), this, SLOT(handleRangeChanged(qreal, qreal)));
    QObject::connect(axis(), SIGNAL(reverseChanged(bool)), this, SLOT(handleReverseChanged(bool)));
    QObject::connect(axis(), SIGNAL(lineVisibleChanged(bool)),
                     this, SLOT(handleMinorArrowVisibleChanged(bool)));
    QObject::connect(axis(), SIGNAL(linePenChanged(const QPen&)), this,
                     SLOT(handleMinorArrowPenChanged(const QPen&)));
    QObject::connect(axis(), SIGNAL(minorGridVisibleChanged(bool)),
                     this, SLOT(handleMinorGridVisibleChanged(bool)));
    QObject::connect(axis(), SIGNAL(minorGridLinePenChanged(const QPen&)),
                     this, SLOT(handleMinorGridPenChanged(const QPen&)));
    QObject::connect(axis(), SIGNAL(gridLineColorChanged(const QColor&)),
                     this, SLOT(handleGridLineColorChanged(const QColor&)));
    QObject::connect(axis(), SIGNAL(minorGridLineColorChanged(const QColor&)),
                     this, SLOT(handleMinorGridLineColorChanged(const QColor&)));

    if (axis()->type() == QAbstractAxis::AxisTypeCategory) {
        QCategoryAxis *categoryAxis = static_cast<QCategoryAxis *>(axis());
        QObject::connect(categoryAxis,
                         SIGNAL(labelsPositionChanged(QCategoryAxis::AxisLabelsPosition)),
                         this, SLOT(handleLabelsPositionChanged()));
    }
}

void ChartAxisElement::handleArrowVisibleChanged(bool visible)
{
    m_arrow->setVisible(visible);
}

void ChartAxisElement::handleMinorArrowVisibleChanged(bool visible)
{
    m_minorArrow->setVisible(visible);
}

void ChartAxisElement::handleGridVisibleChanged(bool visible)
{
    m_grid->setVisible(visible);
}

void ChartAxisElement::handleMinorGridVisibleChanged(bool visible)
{
    m_minorGrid->setVisible(visible);
}

void ChartAxisElement::handleLabelsPositionChanged()
{
    QGraphicsLayoutItem::updateGeometry();
    presenter()->layout()->invalidate();
}

void ChartAxisElement::handleLabelsVisibleChanged(bool visible)
{
    QGraphicsLayoutItem::updateGeometry();
    presenter()->layout()->invalidate();
    m_labels->setVisible(visible);
}

void ChartAxisElement::handleShadesVisibleChanged(bool visible)
{
    m_shades->setVisible(visible);
}

void ChartAxisElement::handleTitleVisibleChanged(bool visible)
{
    QGraphicsLayoutItem::updateGeometry();
    presenter()->layout()->invalidate();
    m_title->setVisible(visible);
}

void ChartAxisElement::handleLabelsAngleChanged(int angle)
{
    foreach (QGraphicsItem *item, m_labels->childItems())
        item->setRotation(angle);

    QGraphicsLayoutItem::updateGeometry();
    presenter()->layout()->invalidate();
}

void ChartAxisElement::handleLabelsBrushChanged(const QBrush &brush)
{
    foreach (QGraphicsItem *item, m_labels->childItems())
        static_cast<QGraphicsTextItem *>(item)->setDefaultTextColor(brush.color());
}

void ChartAxisElement::handleLabelsFontChanged(const QFont &font)
{
    foreach (QGraphicsItem *item, m_labels->childItems())
        static_cast<QGraphicsTextItem *>(item)->setFont(font);
    QGraphicsLayoutItem::updateGeometry();
    presenter()->layout()->invalidate();
}

void ChartAxisElement::handleTitleTextChanged(const QString &title)
{
    QGraphicsLayoutItem::updateGeometry();
    presenter()->layout()->invalidate();
    if (title.isEmpty() || !m_title->isVisible())
        m_title->setHtml(title);
}

void ChartAxisElement::handleTitleBrushChanged(const QBrush &brush)
{
    m_title->setDefaultTextColor(brush.color());
}

void ChartAxisElement::handleTitleFontChanged(const QFont &font)
{
    if (m_title->font() != font) {
        m_title->setFont(font);
        QGraphicsLayoutItem::updateGeometry();
        presenter()->layout()->invalidate();
    }
}

void ChartAxisElement::handleVisibleChanged(bool visible)
{
    setVisible(visible);
    if (!visible) {
        m_grid->setVisible(visible);
        m_arrow->setVisible(visible);
        m_minorGrid->setVisible(visible);
        m_minorArrow->setVisible(visible);
        m_shades->setVisible(visible);
        m_labels->setVisible(visible);
        m_title->setVisible(visible);
    } else {
        m_grid->setVisible(axis()->isGridLineVisible());
        m_arrow->setVisible(axis()->isLineVisible());
        m_minorGrid->setVisible(axis()->isMinorGridLineVisible());
        m_minorArrow->setVisible(axis()->isLineVisible());
        m_shades->setVisible(axis()->shadesVisible());
        m_labels->setVisible(axis()->labelsVisible());
        m_title->setVisible(axis()->isTitleVisible());
    }
    if (presenter()) {
        if (visible) {
            QSizeF before = effectiveSizeHint(Qt::PreferredSize);
            QSizeF after = sizeHint(Qt::PreferredSize);
            if (before != after)
                QGraphicsLayoutItem::updateGeometry();
        }
        presenter()->layout()->invalidate();
    }
}

void ChartAxisElement::handleRangeChanged(qreal min, qreal max)
{
    Q_UNUSED(min);
    Q_UNUSED(max);

    if (!isEmpty()) {
        QVector<qreal> layout = calculateLayout();
        updateLayout(layout);
        QSizeF before = effectiveSizeHint(Qt::PreferredSize);
        QSizeF after = sizeHint(Qt::PreferredSize);

        if (before != after) {
            QGraphicsLayoutItem::updateGeometry();
            // We don't want to call invalidate on layout, since it will change minimum size of
            // component, which we would like to avoid since it causes nasty flips when scrolling
            // or zooming, instead recalculate layout and use plotArea for extra space.
            presenter()->layout()->setGeometry(presenter()->layout()->geometry());
        }
    }
}

void ChartAxisElement::handleReverseChanged(bool reverse)
{
    Q_UNUSED(reverse);

    QGraphicsLayoutItem::updateGeometry();
    presenter()->layout()->invalidate();
}

bool ChartAxisElement::isEmpty()
{
    return axisGeometry().isEmpty()
           || gridGeometry().isEmpty()
           || qFuzzyCompare(min(), max());
}

qreal ChartAxisElement::min() const
{
    return m_axis->d_ptr->min();
}

qreal ChartAxisElement::max() const
{
    return m_axis->d_ptr->max();
}

QString ChartAxisElement::formatLabel(const QString &formatSpec, const QByteArray &array,
                                      qreal value, int precision, const QString &preStr,
                                      const QString &postStr) const
{
    QString retVal;
    if (!formatSpec.isEmpty()) {
        if (formatSpec.at(0) == QLatin1Char('d')
            || formatSpec.at(0) == QLatin1Char('i')
            || formatSpec.at(0) == QLatin1Char('c')) {
            if (presenter()->localizeNumbers())
                retVal = preStr + presenter()->locale().toString(qint64(value)) + postStr;
            else
                retVal = QString().sprintf(array, qint64(value));
        } else if (formatSpec.at(0) == QLatin1Char('u')
                 || formatSpec.at(0) == QLatin1Char('o')
                 || formatSpec.at(0) == QLatin1Char('x')
                 || formatSpec.at(0) == QLatin1Char('X')) {
            // These formats are not supported by localized numbers
            retVal = QString().sprintf(array, quint64(value));
        } else if (formatSpec.at(0) == QLatin1Char('f')
                   || formatSpec.at(0) == QLatin1Char('F')
                   || formatSpec.at(0) == QLatin1Char('e')
                   || formatSpec.at(0) == QLatin1Char('E')
                   || formatSpec.at(0) == QLatin1Char('g')
                   || formatSpec.at(0) == QLatin1Char('G')) {
            if (presenter()->localizeNumbers()) {
                retVal = preStr
                        + presenter()->locale().toString(value, formatSpec.at(0).toLatin1(),
                                                         precision)
                        + postStr;
            } else {
                retVal = QString().sprintf(array, value);
            }
        }
    }
    return retVal;
}

QStringList ChartAxisElement::createValueLabels(qreal min, qreal max, int ticks,
                                                const QString &format) const
{
    QStringList labels;

    if (max <= min || ticks < 1)
        return labels;

    if (format.isNull()) {
        int n = qMax(int(-qFloor(std::log10((max - min) / (ticks - 1)))), 0) + 1;
        for (int i = 0; i < ticks; i++) {
            qreal value = min + (i * (max - min) / (ticks - 1));
            labels << presenter()->numberToString(value, 'f', n);
        }
    } else {
        QByteArray array = format.toLatin1();
        QString formatSpec;
        QString preStr;
        QString postStr;
        int precision = 6; // Six is the default precision in Qt API
        if (presenter()->localizeNumbers()) {
            if (!labelFormatMatcherLocalized)
                labelFormatMatcherLocalized
                        = new QRegExp(QString::fromLatin1(labelFormatMatchLocalizedString));
            if (labelFormatMatcherLocalized->indexIn(format, 0) != -1) {
                preStr = labelFormatMatcherLocalized->cap(1);
                if (!labelFormatMatcherLocalized->cap(2).isEmpty())
                    precision = labelFormatMatcherLocalized->cap(2).toInt();
                formatSpec = labelFormatMatcherLocalized->cap(3);
                postStr = labelFormatMatcherLocalized->cap(4);
            }
        } else {
            if (!labelFormatMatcher)
                labelFormatMatcher = new QRegExp(QString::fromLatin1(labelFormatMatchString));
            if (labelFormatMatcher->indexIn(format, 0) != -1)
                formatSpec = labelFormatMatcher->cap(1);
        }
        for (int i = 0; i < ticks; i++) {
            qreal value = min + (i * (max - min) / (ticks - 1));
            labels << formatLabel(formatSpec, array, value, precision, preStr, postStr);
        }
    }

    return labels;
}

QStringList ChartAxisElement::createLogValueLabels(qreal min, qreal max, qreal base, int ticks,
                                                   const QString &format) const
{
    QStringList labels;

    if (max <= min || ticks < 1)
        return labels;

    int firstTick;
    if (base > 1)
        firstTick = qCeil(std::log10(min) / std::log10(base));
    else
        firstTick = qCeil(std::log10(max) / std::log10(base));

    if (format.isNull()) {
        int n = 0;
        if (ticks > 1)
            n = qMax(int(-qFloor(std::log10((max - min) / (ticks - 1)))), 0);
        n++;
        for (int i = firstTick; i < ticks + firstTick; i++) {
            qreal value = qPow(base, i);
            labels << presenter()->numberToString(value, 'f', n);
        }
    } else {
        QByteArray array = format.toLatin1();
        QString formatSpec;
        QString preStr;
        QString postStr;
        int precision = 6; // Six is the default precision in Qt API
        if (presenter()->localizeNumbers()) {
            if (!labelFormatMatcherLocalized)
                labelFormatMatcherLocalized =
                        new QRegExp(QString::fromLatin1(labelFormatMatchLocalizedString));
            if (labelFormatMatcherLocalized->indexIn(format, 0) != -1) {
                preStr = labelFormatMatcherLocalized->cap(1);
                if (!labelFormatMatcherLocalized->cap(2).isEmpty())
                    precision = labelFormatMatcherLocalized->cap(2).toInt();
                formatSpec = labelFormatMatcherLocalized->cap(3);
                postStr = labelFormatMatcherLocalized->cap(4);
            }
        } else {
            if (!labelFormatMatcher)
                labelFormatMatcher = new QRegExp(QString::fromLatin1(labelFormatMatchString));
            if (labelFormatMatcher->indexIn(format, 0) != -1)
                formatSpec = labelFormatMatcher->cap(1);
        }
        for (int i = firstTick; i < ticks + firstTick; i++) {
            qreal value = qPow(base, i);
            labels << formatLabel(formatSpec, array, value, precision, preStr, postStr);
        }
    }

    return labels;
}

QStringList ChartAxisElement::createDateTimeLabels(qreal min, qreal max,int ticks,
                                                   const QString &format) const
{
    QStringList labels;

    if (max <= min || ticks < 1)
        return labels;

    int n = qMax(int(-qFloor(std::log10((max - min) / (ticks - 1)))), 0);
    n++;
    for (int i = 0; i < ticks; i++) {
        qreal value = min + (i * (max - min) / (ticks - 1));
        labels << presenter()->locale().toString(QDateTime::fromMSecsSinceEpoch(value), format);
    }
    return labels;
}

void ChartAxisElement::axisSelected()
{
    emit clicked();
}

#include "moc_chartaxiselement_p.cpp"

QT_CHARTS_END_NAMESPACE
