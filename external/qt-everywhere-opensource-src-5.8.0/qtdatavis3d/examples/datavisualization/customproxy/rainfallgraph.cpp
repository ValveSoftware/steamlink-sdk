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

#include "rainfallgraph.h"
#include <QtDataVisualization/qcategory3daxis.h>
#include <QtDataVisualization/qvalue3daxis.h>
#include <QtDataVisualization/q3dscene.h>
#include <QtDataVisualization/q3dcamera.h>
#include <QtDataVisualization/qbar3dseries.h>
#include <QtDataVisualization/q3dtheme.h>
#include <QtGui/QGuiApplication>
#include <QtGui/QFont>
#include <QtCore/QDebug>
#include <QtCore/QTextStream>
#include <QtCore/QFile>

using namespace QtDataVisualization;

RainfallGraph::RainfallGraph(Q3DBars *rainfall)
    : m_graph(rainfall)
{
    // In data file the months are in numeric format, so create custom list
    for (int i = 1; i <= 12; i++)
        m_numericMonths << QString::number(i);

    m_columnCount = m_numericMonths.size();

    m_proxy = new VariantBarDataProxy;
    QBar3DSeries *series = new QBar3DSeries(m_proxy);
    m_graph->addSeries(series);

    updateYearsList(2000, 2012);

    // Set up bar specifications; make the bars as wide as they are deep,
    // and add a small space between the bars
    m_graph->setBarThickness(1.0f);
    m_graph->setBarSpacing(QSizeF(1.1, 1.1));

    // Set axis labels and titles
    QStringList months;
    months << "January" << "February" << "March" << "April" << "May" << "June" << "July" << "August" << "September" << "October" << "November" << "December";
    m_graph->rowAxis()->setTitle("Year");
    m_graph->columnAxis()->setTitle("Month");
    m_graph->valueAxis()->setTitle("rainfall");
    m_graph->valueAxis()->setLabelFormat("%d mm");
    m_graph->valueAxis()->setSegmentCount(5);
    m_graph->rowAxis()->setLabels(m_years);
    m_graph->columnAxis()->setLabels(months);

    // Set bar type to cylinder
    series->setMesh(QAbstract3DSeries::MeshCylinder);

    // Set shadows to medium
    m_graph->setShadowQuality(QAbstract3DGraph::ShadowQualityMedium);

    // Set selection mode to bar and column
    m_graph->setSelectionMode(QAbstract3DGraph::SelectionItemAndColumn | QAbstract3DGraph::SelectionSlice);

    // Set theme
    m_graph->activeTheme()->setType(Q3DTheme::ThemeArmyBlue);

    // Override font in theme
    m_graph->activeTheme()->setFont(QFont("Century Gothic", 30));

    // Override label background for theme
    m_graph->activeTheme()->setLabelBackgroundEnabled(false);

    // Set camera position and zoom
    m_graph->scene()->activeCamera()->setCameraPreset(Q3DCamera::CameraPresetIsometricRightHigh);

    // Set window title
    m_graph->setTitle(QStringLiteral("Monthly rainfall in Northern Finland"));

    // Set reflections on
    m_graph->setReflection(true);
}

RainfallGraph::~RainfallGraph()
{
    delete m_mapping;
    delete m_dataSet;
    delete m_graph;
}

void RainfallGraph::start()
{
    addDataSet();
}

void RainfallGraph::updateYearsList(int start, int end)
{
    m_years.clear();
    for (int i = start; i <= end; i++)
        m_years << QString::number(i);

    m_rowCount = m_years.size();
}

//! [0]
void RainfallGraph::addDataSet()
{
    // Create a new variant data set and data item list
    m_dataSet =  new VariantDataSet;
    VariantDataItemList *itemList = new VariantDataItemList;

    // Read data from a data file into the data item list
    QTextStream stream;
    QFile dataFile(":/data/raindata.txt");
    if (dataFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        stream.setDevice(&dataFile);
        while (!stream.atEnd()) {
            QString line = stream.readLine();
            if (line.startsWith("#")) // Ignore comments
                continue;
            QStringList strList = line.split(",", QString::SkipEmptyParts);
            // Each line has three data items: Year, month, and rainfall value
            if (strList.size() < 3) {
                qWarning() << "Invalid row read from data:" << line;
                continue;
            }
            // Store year and month as strings, and rainfall value as double
            // into a variant data item and add the item to the item list.
            VariantDataItem *newItem = new VariantDataItem;
            for (int i = 0; i < 2; i++)
                newItem->append(strList.at(i).trimmed());
            newItem->append(strList.at(2).trimmed().toDouble());
            itemList->append(newItem);
        }
    } else {
        qWarning() << "Unable to open data file:" << dataFile.fileName();
    }

    //! [1]
    // Add items to the data set and set it to the proxy
    m_dataSet->addItems(itemList);
    m_proxy->setDataSet(m_dataSet);

    // Create new mapping for the data and set it to the proxy
    m_mapping =  new VariantBarDataMapping(0, 1, 2, m_years, m_numericMonths);
    m_proxy->setMapping(m_mapping);
    //! [1]
}
//! [0]
