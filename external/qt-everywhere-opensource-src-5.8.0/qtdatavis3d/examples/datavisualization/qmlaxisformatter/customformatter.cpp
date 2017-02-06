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

#include "customformatter.h"
#include <QtDataVisualization/QValue3DAxis>
#include <QtQml/QQmlExtensionPlugin>
#include <QtCore/QDebug>

using namespace QtDataVisualization;

Q_DECLARE_METATYPE(QValue3DAxisFormatter *)

static const qreal oneDayMs = 60.0 * 60.0 * 24.0 * 1000.0;

CustomFormatter::CustomFormatter(QObject *parent) :
    QValue3DAxisFormatter(parent)
{
    qRegisterMetaType<QValue3DAxisFormatter *>();
}

CustomFormatter::~CustomFormatter()
{
}

//! [1]
QValue3DAxisFormatter *CustomFormatter::createNewInstance() const
{
    return new CustomFormatter();
}

void CustomFormatter::populateCopy(QValue3DAxisFormatter &copy) const
{
    QValue3DAxisFormatter::populateCopy(copy);

    CustomFormatter *customFormatter = static_cast<CustomFormatter *>(&copy);
    customFormatter->m_originDate = m_originDate;
    customFormatter->m_selectionFormat = m_selectionFormat;
}
//! [1]

//! [2]
void CustomFormatter::recalculate()
{
    // We want our axis to always have gridlines at date breaks

    // Convert range into QDateTimes
    QDateTime minTime = valueToDateTime(qreal(axis()->min()));
    QDateTime maxTime = valueToDateTime(qreal(axis()->max()));

    // Find out the grid counts
    QTime midnight(0, 0);
    QDateTime minFullDate(minTime.date(), midnight);
    int gridCount = 0;
    if (minFullDate != minTime)
        minFullDate = minFullDate.addDays(1);
    QDateTime maxFullDate(maxTime.date(), midnight);

    gridCount += minFullDate.daysTo(maxFullDate) + 1;
    int subGridCount = axis()->subSegmentCount() - 1;

    // Reserve space for position arrays and label strings
    gridPositions().resize(gridCount);
    subGridPositions().resize((gridCount + 1) * subGridCount);
    labelPositions().resize(gridCount);
    labelStrings().reserve(gridCount);

    // Calculate positions and format labels
    qint64 startMs = minTime.toMSecsSinceEpoch();
    qint64 endMs = maxTime.toMSecsSinceEpoch();
    qreal dateNormalizer = endMs - startMs;
    qreal firstLineOffset = (minFullDate.toMSecsSinceEpoch() - startMs) / dateNormalizer;
    qreal segmentStep = oneDayMs / dateNormalizer;
    qreal subSegmentStep = 0;
    if (subGridCount > 0)
        subSegmentStep = segmentStep / qreal(subGridCount + 1);

    for (int i = 0; i < gridCount; i++) {
        qreal gridValue = firstLineOffset + (segmentStep * qreal(i));
        gridPositions()[i] = float(gridValue);
        labelPositions()[i] = float(gridValue);
        labelStrings() << minFullDate.addDays(i).toString(axis()->labelFormat());
    }

    for (int i = 0; i <= gridCount; i++) {
        if (subGridPositions().size()) {
            for (int j = 0; j < subGridCount; j++) {
                float position;
                if (i)
                    position =  gridPositions().at(i - 1) + subSegmentStep * (j + 1);
                else
                    position =  gridPositions().at(0) - segmentStep + subSegmentStep * (j + 1);
                if (position > 1.0f || position < 0.0f)
                    position = gridPositions().at(0);
                subGridPositions()[i * subGridCount + j] = position;
            }
        }
    }
}
//! [2]

//! [3]
QString CustomFormatter::stringForValue(qreal value, const QString &format) const
{
    Q_UNUSED(format)

    return valueToDateTime(value).toString(m_selectionFormat);
}
//! [3]

QDate CustomFormatter::originDate() const
{
    return m_originDate;
}

QString CustomFormatter::selectionFormat() const
{
    return m_selectionFormat;
}

void CustomFormatter::setOriginDate(const QDate &date)
{
    if (m_originDate != date) {
        m_originDate = date;
        markDirty(true);
        emit originDateChanged(date);
    }
}

void CustomFormatter::setSelectionFormat(const QString &format)
{
    if (m_selectionFormat != format) {
        m_selectionFormat = format;
        markDirty(true); // Necessary to regenerate already visible selection label
        emit selectionFormatChanged(format);
    }
}

//! [0]
QDateTime CustomFormatter::valueToDateTime(qreal value) const
{
    return QDateTime(m_originDate).addMSecs(qint64(oneDayMs * value));
}
//! [0]
