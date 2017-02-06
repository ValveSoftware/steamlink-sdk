/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#include "splineeditor.h"
#include "segmentproperties.h"

#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QDebug>
#include <QApplication>
#include <QVector>

const int canvasWidth = 640;
const int canvasHeight = 320;

const int canvasMargin = 160;

SplineEditor::SplineEditor(QWidget *parent) :
    QWidget(parent), m_pointListWidget(0), m_block(false)
{
    setFixedSize(canvasWidth + canvasMargin * 2, canvasHeight + canvasMargin * 2);

    m_controlPoints.append(QPointF(0.4, 0.075));
    m_controlPoints.append(QPointF(0.45,0.24));
    m_controlPoints.append(QPointF(0.5,0.5));

    m_controlPoints.append(QPointF(0.55,0.76));
    m_controlPoints.append(QPointF(0.7,0.9));
    m_controlPoints.append(QPointF(1.0, 1.0));

    m_numberOfSegments = 2;

    m_activeControlPoint = -1;

    m_mouseDrag = false;

    m_pointContextMenu = new QMenu(this);
    m_deleteAction = new QAction(tr("Delete point"), m_pointContextMenu);
    m_smoothAction = new QAction(tr("Smooth point"), m_pointContextMenu);
    m_cornerAction = new QAction(tr("Corner point"), m_pointContextMenu);

    m_smoothAction->setCheckable(true);

    m_pointContextMenu->addAction(m_deleteAction);
    m_pointContextMenu->addAction(m_smoothAction);
    m_pointContextMenu->addAction(m_cornerAction);

    m_curveContextMenu = new QMenu(this);

    m_addPoint = new QAction(tr("Add point"), m_pointContextMenu);

    m_curveContextMenu->addAction(m_addPoint);

    initPresets();

    invalidateSmoothList();
}

static inline QPointF mapToCanvas(const QPointF &point)
{
    return QPointF(point.x() * canvasWidth + canvasMargin,
                   canvasHeight - point.y() * canvasHeight + canvasMargin);
}

static inline QPointF mapFromCanvas(const QPointF &point)
{
    return QPointF((point.x() - canvasMargin) / canvasWidth ,
                   1 - (point.y() - canvasMargin) / canvasHeight);
}

static inline void paintControlPoint(const QPointF &controlPoint, QPainter *painter, bool edit,
                                     bool realPoint, bool active, bool smooth)
{
    int pointSize = 4;

    if (active)
        painter->setBrush(QColor(140, 140, 240, 255));
    else
        painter->setBrush(QColor(120, 120, 220, 255));

    if (realPoint) {
        pointSize = 6;
        painter->setBrush(QColor(80, 80, 210, 150));
    }

    painter->setPen(QColor(50, 50, 50, 140));

    if (!edit)
        painter->setBrush(QColor(160, 80, 80, 250));

    if (smooth) {
        painter->drawEllipse(QRectF(mapToCanvas(controlPoint).x() - pointSize + 0.5,
                                    mapToCanvas(controlPoint).y() - pointSize + 0.5,
                                    pointSize * 2, pointSize * 2));
    } else {
        painter->drawRect(QRectF(mapToCanvas(controlPoint).x() - pointSize + 0.5,
                                 mapToCanvas(controlPoint).y() - pointSize + 0.5,
                                 pointSize * 2, pointSize * 2));
    }
}

static inline bool indexIsRealPoint(int i)
{
    return  !((i + 1) % 3);
}

static inline int pointForControlPoint(int i)
{
    if ((i % 3) == 0)
        return i - 1;

    if ((i % 3) == 1)
        return i + 1;

    return i;
}

void drawCleanLine(QPainter *painter, const QPoint p1, QPoint p2)
{
    painter->drawLine(p1 + QPointF(0.5 , 0.5), p2 + QPointF(0.5, 0.5));
}

void SplineEditor::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    QPen pen(Qt::black);
    pen.setWidth(1);
    painter.fillRect(0,0,width() - 1, height() - 1, QBrush(Qt::white));
    painter.drawRect(0,0,width() - 1, height() - 1);

    painter.setRenderHint(QPainter::Antialiasing);

    pen = QPen(Qt::gray);
    pen.setWidth(1);
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);
    drawCleanLine(&painter,mapToCanvas(QPoint(0, 0)).toPoint(), mapToCanvas(QPoint(1, 0)).toPoint());
    drawCleanLine(&painter,mapToCanvas(QPoint(0, 1)).toPoint(), mapToCanvas(QPoint(1, 1)).toPoint());

    for (int i = 0; i < m_numberOfSegments; i++) {
        QPainterPath path;
        QPointF p0;

        if (i == 0)
            p0 = mapToCanvas(QPointF(0.0, 0.0));
        else
            p0 = mapToCanvas(m_controlPoints.at(i * 3 - 1));

        path.moveTo(p0);

        QPointF p1 = mapToCanvas(m_controlPoints.at(i * 3));
        QPointF p2 = mapToCanvas(m_controlPoints.at(i * 3 + 1));
        QPointF p3 = mapToCanvas(m_controlPoints.at(i * 3 + 2));
        path.cubicTo(p1, p2, p3);
        painter.strokePath(path, QPen(QBrush(Qt::black), 2));

        QPen pen(Qt::black);
        pen.setWidth(1);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.drawLine(p0, p1);
        painter.drawLine(p3, p2);
    }

    paintControlPoint(QPointF(0.0, 0.0), &painter, false, true, false, false);
    paintControlPoint(QPointF(1.0, 1.0), &painter, false, true, false, false);

    for (int i = 0; i < m_controlPoints.count() - 1; ++i)
        paintControlPoint(m_controlPoints.at(i),
                          &painter,
                          true,
                          indexIsRealPoint(i),
                          i == m_activeControlPoint,
                          isControlPointSmooth(i));
}

void SplineEditor::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_activeControlPoint = findControlPoint(e->pos());

        if (m_activeControlPoint != -1) {
            mouseMoveEvent(e);
        }
        m_mousePress = e->pos();
        e->accept();
    }
}

void SplineEditor::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_activeControlPoint = -1;

        m_mouseDrag = false;
        e->accept();
    }
}

#ifndef QT_NO_CONTEXTMENU
void SplineEditor::contextMenuEvent(QContextMenuEvent *e)
{
    int index = findControlPoint(e->pos());

    if (index > 0 && indexIsRealPoint(index)) {
        m_smoothAction->setChecked(isControlPointSmooth(index));
        QAction* action = m_pointContextMenu->exec(e->globalPos());
        if (action == m_deleteAction)
            deletePoint(index);
        else if (action == m_smoothAction)
            smoothPoint(index);
        else if (action == m_cornerAction)
            cornerPoint(index);
    } else {
        QAction* action = m_curveContextMenu->exec(e->globalPos());
        if (action == m_addPoint)
            addPoint(e->pos());
    }
}
#endif // QT_NO_CONTEXTMENU

void SplineEditor::invalidate()
{
    QEasingCurve easingCurve(QEasingCurve::BezierSpline);

    for (int i = 0; i < m_numberOfSegments; ++i) {
        easingCurve.addCubicBezierSegment(m_controlPoints.at(i * 3),
                                          m_controlPoints.at(i * 3 + 1),
                                          m_controlPoints.at(i * 3 + 2));
    }
    setEasingCurve(easingCurve);
    invalidateSegmentProperties();
}

void SplineEditor::invalidateSmoothList()
{
    m_smoothList.clear();

    for (int i = 0; i < (m_numberOfSegments - 1); ++i)
        m_smoothList.append(isSmooth(i * 3 + 2));

}

void SplineEditor::invalidateSegmentProperties()
{
    for (int i = 0; i < m_numberOfSegments; ++i) {
        SegmentProperties *segmentProperties = m_segmentProperties.at(i);
        bool smooth = false;
        if (i < (m_numberOfSegments - 1)) {
            smooth = m_smoothList.at(i);
        }
        segmentProperties->setSegment(i, m_controlPoints.mid(i * 3, 3), smooth, i == (m_numberOfSegments - 1));
    }
}

QHash<QString, QEasingCurve> SplineEditor::presets() const
{
    return m_presets;
}

QString SplineEditor::generateCode()
{
    QString s = QLatin1String("[");
    for (const QPointF &point : qAsConst(m_controlPoints)) {
        s += QString::number(point.x(), 'g', 2) + QLatin1Char(',')
             + QString::number(point.y(), 'g', 3) + QLatin1Char(',');
    }
    s.chop(1); //removing last ","
    s += QLatin1Char(']');

    return s;
}

QStringList SplineEditor::presetNames() const
{
    return m_presets.keys();
}

QWidget *SplineEditor::pointListWidget()
{
    if (!m_pointListWidget) {
        setupPointListWidget();
    }

    return m_pointListWidget;
}

int SplineEditor::findControlPoint(const QPoint &point)
{
    int pointIndex = -1;
    qreal distance = -1;
    for (int i = 0; i<m_controlPoints.size() - 1; ++i) {
        qreal d = QLineF(point, mapToCanvas(m_controlPoints.at(i))).length();
        if ((distance < 0 && d < 10) || d < distance) {
            distance = d;
            pointIndex = i;
        }
    }
    return pointIndex;
}

static inline bool veryFuzzyCompare(qreal r1, qreal r2)
{
    if (qFuzzyCompare(r1, 2))
        return true;

    int r1i = qRound(r1 * 20);
    int r2i = qRound(r2 * 20);

    if (qFuzzyCompare(qreal(r1i) / 20, qreal(r2i) / 20))
        return true;

    return false;
}

bool SplineEditor::isSmooth(int i) const
{
    if (i == 0)
        return false;

    QPointF p = m_controlPoints.at(i);
    QPointF p_before = m_controlPoints.at(i - 1);
    QPointF p_after = m_controlPoints.at(i + 1);

    QPointF v1 = p_after - p;
    v1 = v1 / v1.manhattanLength(); //normalize

    QPointF v2 = p - p_before;
    v2 = v2 / v2.manhattanLength(); //normalize

    return veryFuzzyCompare(v1.x(), v2.x()) && veryFuzzyCompare(v1.y(), v2.y());
}

void SplineEditor::smoothPoint(int index)
{
    if (m_smoothAction->isChecked()) {

        QPointF before = QPointF(0,0);
        if (index > 3)
            before = m_controlPoints.at(index - 3);

        QPointF after = QPointF(1.0, 1.0);
        if ((index + 3) < m_controlPoints.count())
            after = m_controlPoints.at(index + 3);

        QPointF tangent = (after - before) / 6;

        QPointF thisPoint =  m_controlPoints.at(index);

        if (index > 0)
            m_controlPoints[index - 1] = thisPoint - tangent;

        if (index + 1  < m_controlPoints.count())
            m_controlPoints[index + 1] = thisPoint + tangent;

        m_smoothList[index / 3] = true;
    } else {
        m_smoothList[index / 3] = false;
    }
    invalidate();
    update();
}

void SplineEditor::cornerPoint(int index)
{
    QPointF before = QPointF(0,0);
    if (index > 3)
        before = m_controlPoints.at(index - 3);

    QPointF after = QPointF(1.0, 1.0);
    if ((index + 3) < m_controlPoints.count())
        after = m_controlPoints.at(index + 3);

    QPointF thisPoint =  m_controlPoints.at(index);

    if (index > 0)
        m_controlPoints[index - 1] = (before - thisPoint) / 3 + thisPoint;

    if (index + 1  < m_controlPoints.count())
        m_controlPoints[index + 1] = (after - thisPoint) / 3 + thisPoint;

    m_smoothList[(index) / 3] = false;
    invalidate();
}

void SplineEditor::deletePoint(int index)
{
    m_controlPoints.remove(index - 1, 3);
    m_numberOfSegments--;

    invalidateSmoothList();
    setupPointListWidget();
    invalidate();
}

void SplineEditor::addPoint(const QPointF point)
{
    QPointF newPos = mapFromCanvas(point);
    int splitIndex = 0;
    for (int i=0; i < m_controlPoints.size() - 1; ++i) {
        if (indexIsRealPoint(i) && m_controlPoints.at(i).x() > newPos.x()) {
            break;
        } else if (indexIsRealPoint(i))
            splitIndex = i;
    }
    QPointF before = QPointF(0,0);
    if (splitIndex > 0)
        before = m_controlPoints.at(splitIndex);

    QPointF after = QPointF(1.0, 1.0);
    if ((splitIndex + 3) < m_controlPoints.count())
        after = m_controlPoints.at(splitIndex + 3);

    if (splitIndex > 0) {
        m_controlPoints.insert(splitIndex + 2, (newPos + after) / 2);
        m_controlPoints.insert(splitIndex + 2, newPos);
        m_controlPoints.insert(splitIndex + 2, (newPos + before) / 2);
    } else {
        m_controlPoints.insert(splitIndex + 1, (newPos + after) / 2);
        m_controlPoints.insert(splitIndex + 1, newPos);
        m_controlPoints.insert(splitIndex + 1, (newPos + before) / 2);
    }
    m_numberOfSegments++;

    invalidateSmoothList();
    setupPointListWidget();
    invalidate();
}

void SplineEditor::initPresets()
{
    const QPointF endPoint(1.0, 1.0);
    {
        QEasingCurve easingCurve(QEasingCurve::BezierSpline);
        easingCurve.addCubicBezierSegment(QPointF(0.4, 0.075), QPointF(0.45, 0.24), QPointF(0.5, 0.5));
        easingCurve.addCubicBezierSegment(QPointF(0.55, 0.76), QPointF(0.7, 0.9), endPoint);
        m_presets.insert(tr("Standard Easing"), easingCurve);
    }

    {
        QEasingCurve easingCurve(QEasingCurve::BezierSpline);
        easingCurve.addCubicBezierSegment(QPointF(0.43, 0.0025), QPointF(0.65, 1), endPoint);
        m_presets.insert(tr("Simple"), easingCurve);
    }

    {
        QEasingCurve easingCurve(QEasingCurve::BezierSpline);
        easingCurve.addCubicBezierSegment(QPointF(0.43, 0.0025), QPointF(0.38, 0.51), QPointF(0.57, 0.99));
        easingCurve.addCubicBezierSegment(QPointF(0.8, 0.69), QPointF(0.65, 1), endPoint);
        m_presets.insert(tr("Simple Bounce"), easingCurve);
    }

    {
        QEasingCurve easingCurve(QEasingCurve::BezierSpline);
        easingCurve.addCubicBezierSegment(QPointF(0.4, 0.075), QPointF(0.64, -0.0025), QPointF(0.74, 0.23));
        easingCurve.addCubicBezierSegment(QPointF(0.84, 0.46), QPointF(0.91, 0.77), endPoint);
        m_presets.insert(tr("Slow in fast out"), easingCurve);
    }

    {
        QEasingCurve easingCurve(QEasingCurve::BezierSpline);
        easingCurve.addCubicBezierSegment(QPointF(0.43, 0.0025), QPointF(0.47, 0.51), QPointF(0.59, 0.94));
        easingCurve.addCubicBezierSegment(QPointF(0.84, 0.95), QPointF( 0.99, 0.94), endPoint);
        m_presets.insert(tr("Snapping"), easingCurve);
    }

    {
        QEasingCurve easingCurve(QEasingCurve::BezierSpline);
        easingCurve.addCubicBezierSegment(QPointF( 0.38, 0.35),QPointF(0.38, 0.7), QPointF(0.45, 0.99));
        easingCurve.addCubicBezierSegment(QPointF(0.48, 0.66), QPointF(0.62, 0.62), QPointF(0.66, 0.99));
        easingCurve.addCubicBezierSegment(QPointF(0.69, 0.76), QPointF(0.77, 0.76), QPointF(0.79, 0.99));
        easingCurve.addCubicBezierSegment(QPointF(0.83, 0.91), QPointF(0.87, 0.92), QPointF(0.91, 0.99));
        easingCurve.addCubicBezierSegment(QPointF(0.95, 0.95), QPointF(0.97, 0.94), endPoint);
        m_presets.insert(tr("Complex Bounce"), easingCurve);
    }

    {
        QEasingCurve easingCurve4(QEasingCurve::BezierSpline);
        easingCurve4.addCubicBezierSegment(QPointF(0.12, -0.12),QPointF(0.23, -0.19), QPointF( 0.35, -0.09));
        easingCurve4.addCubicBezierSegment(QPointF(0.47, 0.005), QPointF(0.52, 1), QPointF(0.62, 1.1));
        easingCurve4.addCubicBezierSegment(QPointF(0.73, 1.2), QPointF(0.91,1 ), endPoint);
        m_presets.insert(tr("Overshoot"), easingCurve4);
    }
}

void SplineEditor::setupPointListWidget()
{
    if (!m_pointListWidget)
        m_pointListWidget = new QScrollArea(this);

    if (m_pointListWidget->widget())
        delete m_pointListWidget->widget();

    m_pointListWidget->setFrameStyle(QFrame::NoFrame);
    m_pointListWidget->setWidgetResizable(true);
    m_pointListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_pointListWidget->setWidget(new QWidget(m_pointListWidget));
    QVBoxLayout *layout = new QVBoxLayout(m_pointListWidget->widget());
    layout->setMargin(0);
    layout->setSpacing(2);
    m_pointListWidget->widget()->setLayout(layout);

    m_segmentProperties.clear();

    { //implicit 0,0
        QWidget *widget = new QWidget(m_pointListWidget->widget());
        Ui_Pane pane;
        pane.setupUi(widget);
        pane.p1_x->setValue(0);
        pane.p1_y->setValue(0);
        layout->addWidget(widget);
        pane.label->setText("p0");
        widget->setEnabled(false);
    }

    for (int i = 0; i < m_numberOfSegments; ++i) {
        SegmentProperties *segmentProperties = new SegmentProperties(m_pointListWidget->widget());
        layout->addWidget(segmentProperties);
        bool smooth = false;
        if (i < (m_numberOfSegments - 1)) {
            smooth = m_smoothList.at(i);
        }
        segmentProperties->setSegment(i, m_controlPoints.mid(i * 3, 3), smooth, i == (m_numberOfSegments - 1));
        segmentProperties->setSplineEditor(this);
        m_segmentProperties << segmentProperties;
    }
    layout->addSpacerItem(new QSpacerItem(10, 10, QSizePolicy::Expanding, QSizePolicy::Expanding));

    m_pointListWidget->viewport()->show();
    m_pointListWidget->viewport()->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_pointListWidget->show();
}

bool SplineEditor::isControlPointSmooth(int i) const
{
    if (i == 0)
        return false;

    if (i == m_controlPoints.count() - 1)
        return false;

    if (m_numberOfSegments == 1)
        return false;

    int index = pointForControlPoint(i);

    if (index == 0)
        return false;

    if (index == m_controlPoints.count() - 1)
        return false;

    return m_smoothList.at(index / 3);
}

QPointF limitToCanvas(const QPointF point)
{
    qreal left = -qreal( canvasMargin) / qreal(canvasWidth);
    qreal width = 1.0 - 2.0 * left;

    qreal top = -qreal( canvasMargin) / qreal(canvasHeight);
    qreal height = 1.0 - 2.0 * top;

    QPointF p = point;
    QRectF r(left, top, width, height);

    if (p.x() > r.right()) {
        p.setX(r.right());
    }
    if (p.x() < r.left()) {
        p.setX(r.left());
    }
    if (p.y() < r.top()) {
        p.setY(r.top());
    }
    if (p.y() > r.bottom()) {
        p.setY(r.bottom());
    }
    return p;
}

void SplineEditor::mouseMoveEvent(QMouseEvent *e)
{
    // If we've moved more then 25 pixels, assume user is dragging
    if (!m_mouseDrag && QPoint(m_mousePress - e->pos()).manhattanLength() > qApp->startDragDistance())
        m_mouseDrag = true;

    QPointF p = mapFromCanvas(e->pos());

    if (m_mouseDrag && m_activeControlPoint >= 0 && m_activeControlPoint < m_controlPoints.size()) {
        p = limitToCanvas(p);
        if (indexIsRealPoint(m_activeControlPoint)) {
            //move also the tangents
            QPointF targetPoint = p;
            QPointF distance = targetPoint - m_controlPoints[m_activeControlPoint];
            m_controlPoints[m_activeControlPoint] = targetPoint;
            m_controlPoints[m_activeControlPoint - 1] += distance;
            m_controlPoints[m_activeControlPoint + 1] += distance;
        } else {
            if (!isControlPointSmooth(m_activeControlPoint)) {
                m_controlPoints[m_activeControlPoint] = p;
            } else {
                QPointF targetPoint = p;
                QPointF distance = targetPoint - m_controlPoints[m_activeControlPoint];
                m_controlPoints[m_activeControlPoint] = p;

                if ((m_activeControlPoint > 1) && (m_activeControlPoint % 3) == 0) { //right control point
                    m_controlPoints[m_activeControlPoint - 2] -= distance;
                } else if ((m_activeControlPoint < (m_controlPoints.count() - 2)) //left control point
                           && (m_activeControlPoint % 3) == 1) {
                    m_controlPoints[m_activeControlPoint + 2] -= distance;
                }
            }
        }
        invalidate();
    }
}

void SplineEditor::setEasingCurve(const QEasingCurve &easingCurve)
{
    if (m_easingCurve == easingCurve)
        return;
    m_block = true;
    m_easingCurve = easingCurve;
    m_controlPoints = m_easingCurve.toCubicSpline();
    m_numberOfSegments = m_controlPoints.count() / 3;
    update();
    emit easingCurveChanged();

    const QString code = generateCode();
    emit easingCurveCodeChanged(code);

    m_block = false;
}

void SplineEditor::setPreset(const QString &name)
{
    setEasingCurve(m_presets.value(name));
    invalidateSmoothList();
    setupPointListWidget();
}

void SplineEditor::setEasingCurve(const QString &code)
{
    if (m_block)
        return;
    if (code.startsWith(QLatin1Char('[')) && code.endsWith(QLatin1Char(']'))) {
        const QStringRef cleanCode(&code, 1, code.size() - 2);
        const auto stringList = cleanCode.split(QLatin1Char(','), QString::SkipEmptyParts);
        if (stringList.count() >= 6 && (stringList.count() % 6 == 0)) {
            QVector<qreal> realList;
            realList.reserve(stringList.count());
            for (const QStringRef &string : stringList) {
                bool ok;
                realList.append(string.toDouble(&ok));
                if (!ok)
                    return;
            }
            QVector<QPointF> points;
            const int count = realList.count() / 2;
            points.reserve(count);
            for (int i = 0; i < count; ++i)
                points.append(QPointF(realList.at(i * 2), realList.at(i * 2 + 1)));
            if (points.constLast() == QPointF(1.0, 1.0)) {
                QEasingCurve easingCurve(QEasingCurve::BezierSpline);

                for (int i = 0; i < points.count() / 3; ++i) {
                    easingCurve.addCubicBezierSegment(points.at(i * 3),
                                                      points.at(i * 3 + 1),
                                                      points.at(i * 3 + 2));
                }
                setEasingCurve(easingCurve);
                invalidateSmoothList();
                setupPointListWidget();
            }
        }
    }
}
