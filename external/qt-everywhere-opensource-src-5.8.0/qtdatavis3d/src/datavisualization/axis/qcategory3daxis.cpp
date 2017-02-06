/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
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

#include "qcategory3daxis_p.h"
#include "bars3dcontroller_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class QCategory3DAxis
 * \inmodule QtDataVisualization
 * \brief The QCategory3DAxis class is used for manipulating an axis of a graph.
 * \since QtDataVisualization 1.0
 *
 * QCategory3DAxis provides an axis that can be given labels. The axis is divided into equal-sized
 * categories based on the data window size defined by setting the axis range.
 *
 * Grid lines are drawn between categories, if visible. Labels are drawn to positions of categories
 * if provided.
 */

/*!
 * \qmltype CategoryAxis3D
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.0
 * \ingroup datavisualization_qml
 * \instantiates QCategory3DAxis
 * \inherits AbstractAxis3D
 * \brief The CategoryAxis3D type is used for manipulating an axis of a graph.
 *
 * This type provides an axis that can be given labels.
 */

/*!
 * \qmlproperty list CategoryAxis3D::labels
 *
 * Defines labels for axis applied to categories. If there are fewer labels than categories, the
 * remaining ones do not have a label. If category labels are not defined explicitly, labels are
 * generated from the data row (or column) labels of the primary series of the graph.
 */

/*!
 * Constructs QCategory3DAxis with \a parent.
 */
QCategory3DAxis::QCategory3DAxis(QObject *parent) :
    QAbstract3DAxis(new QCategory3DAxisPrivate(this), parent)
{
    connect(this, &QCategory3DAxis::labelsChanged, this, &QAbstract3DAxis::labelsChanged);
}

/*!
 * Destroys QCategory3DAxis.
 */
QCategory3DAxis::~QCategory3DAxis()
{
}

/*!
 * \property QCategory3DAxis::labels
 *
 * Defines \a labels for axis applied to categories. If there are fewer labels than categories, the
 * remaining ones do not have a label. If category \a labels are not defined explicitly, labels are
 * generated from the data row (or column) labels of the primary series of the graph.
 */
QStringList QCategory3DAxis::labels() const
{
    return QAbstract3DAxis::labels();
}

void QCategory3DAxis::setLabels(const QStringList &labels)
{
    dptr()->m_labelsExplicitlySet = !labels.isEmpty();
    bool labelsFromData = false;

    // Get labels from data proxy if axis is attached to a bar controller and an active axis there
    if (labels.isEmpty()) {
        Bars3DController *controller = qobject_cast<Bars3DController *>(parent());
        if (controller) {
            if (controller->axisX() == this) {
                controller->handleDataRowLabelsChanged();
                labelsFromData = true;
            } else if (controller->axisZ() == this) {
                controller->handleDataColumnLabelsChanged();
                labelsFromData = true;
            }
        }
    }

    if (!labelsFromData && d_ptr->m_labels != labels) {
        d_ptr->m_labels = labels;
        emit labelsChanged();
    }
}

/*!
 * \internal
 */
QCategory3DAxisPrivate *QCategory3DAxis::dptr()
{
    return static_cast<QCategory3DAxisPrivate *>(d_ptr.data());
}

QCategory3DAxisPrivate::QCategory3DAxisPrivate(QCategory3DAxis *q)
    : QAbstract3DAxisPrivate(q, QAbstract3DAxis::AxisTypeCategory),
      m_labelsExplicitlySet(false)
{
}

QCategory3DAxisPrivate::~QCategory3DAxisPrivate()
{
}

/*!
 * \internal
 * Controller uses this function to set labels from data proxy as category labels.
 * If the labels have been set explicitly by the user, data proxy labels are not used.
 */
void QCategory3DAxisPrivate::setDataLabels(const QStringList &labels)
{
    if (!m_labelsExplicitlySet && m_labels != labels) {
        m_labels = labels;
        emit qptr()->labelsChanged();
    }
}

bool QCategory3DAxisPrivate::allowZero()
{
    return true;
}

bool QCategory3DAxisPrivate::allowNegatives()
{
    return false;
}

bool QCategory3DAxisPrivate::allowMinMaxSame()
{
    return true;
}

QCategory3DAxis *QCategory3DAxisPrivate::qptr()
{
    return static_cast<QCategory3DAxis *>(q_ptr);
}

QT_END_NAMESPACE_DATAVISUALIZATION
