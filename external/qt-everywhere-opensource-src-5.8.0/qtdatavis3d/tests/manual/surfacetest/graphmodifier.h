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

#ifndef GRAPHMODIFIER_H
#define GRAPHMODIFIER_H

#include <QtDataVisualization/Q3DSurface>
#include <QtDataVisualization/QSurfaceDataProxy>
#include <QtDataVisualization/QSurface3DSeries>
#include <QSlider>
#include <QTimer>
#include <QLabel>
#include <QCheckBox>

#define MULTI_SERIES

using namespace QtDataVisualization;

class GraphModifier : public QObject
{
    Q_OBJECT
public:
    enum Samples {
        SqrtSin = 1,
        Plane,
        Map
    };

    explicit GraphModifier(Q3DSurface *graph, QWidget *parentWidget);
    ~GraphModifier();

    void toggleSeries1(bool enabled);
    void toggleSeries2(bool enabled);
    void toggleSeries3(bool enabled);
    void toggleSeries4(bool enabled);
    void toggleSmooth(bool enabled);
    void toggleSurfaceGrid(bool enable);
    void toggleSurface(bool enable);
    void toggleSeriesVisible(bool enable);
    void toggleSmoothS2(bool enabled);
    void toggleSurfaceGridS2(bool enable);
    void toggleSurfaceS2(bool enable);
    void toggleSeries2Visible(bool enable);
    void toggleSmoothS3(bool enabled);
    void toggleSurfaceGridS3(bool enable);
    void toggleSurfaceS3(bool enable);
    void toggleSeries3Visible(bool enable);
    void toggleSmoothS4(bool enabled);
    void toggleSurfaceGridS4(bool enable);
    void toggleSurfaceS4(bool enable);
    void toggleSeries4Visible(bool enable);

    void toggleSqrtSin(bool enable);
    void togglePlane(bool enable);
    void setHeightMapData(bool enable);
    void toggleGridSliderLock(bool enable);
    void setGridSliderX(QSlider *slider) { m_gridSliderX = slider; }
    void setGridSliderZ(QSlider *slider) { m_gridSliderZ = slider; }
    void setAxisRangeSliderX(QSlider *slider) { m_axisRangeSliderX = slider; }
    void setAxisRangeSliderZ(QSlider *slider) { m_axisRangeSliderZ = slider; }
    void setAxisMinSliderX(QSlider *slider) { m_axisMinSliderX = slider; }
    void setAxisMinSliderZ(QSlider *slider) { m_axisMinSliderZ = slider; }
    void setSeries1CB(QCheckBox *cb) { m_series1CB = cb; }
    void setSeries2CB(QCheckBox *cb) { m_series2CB = cb; }
    void setSeries3CB(QCheckBox *cb) { m_series3CB = cb; }
    void setSeries4CB(QCheckBox *cb) { m_series4CB = cb; }
    void adjustXCount(int count);
    void adjustZCount(int count);
    void adjustXRange(int range);
    void adjustYRange(int range);
    void adjustZRange(int range);
    void adjustXMin(int min);
    void adjustYMin(int min);
    void adjustZMin(int min);
    void updateSamples();
    void gradientPressed();
    void changeFont(const QFont &font);
    void changeStyle();
    void selectButtonClicked();
    void setSelectionInfoLabel(QLabel *label) {m_selectionInfoLabel = label; }
    void selectedPointChanged(const QPoint &point);
    void changeRow();
    void changeRows();
    void changeMesh();
    void changeItem();
    void changeMultipleItem();
    void changeMultipleRows();
    void addRow();
    void addRows();
    void insertRow();
    void insertRows();
    void removeRow();
    void resetArray();
    void resetArrayEmpty();
    void massiveDataTest();
    void massiveTestScroll();
    void massiveTestAppendAndScroll();
    void testAxisReverse();
    void testDataOrdering();
    void setAspectRatio(int ratio);
    void setHorizontalAspectRatio(int ratio);
    void setSurfaceTexture(bool enabled);

public Q_SLOTS:
    void changeShadowQuality(int quality);
    void changeTheme(int theme);
    void flipViews();
    void changeSelectionMode(int mode);
    void timeout();
    void graphQueryTimeout();

    void handleAxisXChanged(QValue3DAxis *axis);
    void handleAxisYChanged(QValue3DAxis *axis);
    void handleAxisZChanged(QValue3DAxis *axis);
    void handleFpsChange(qreal fps);
    void changeLabelRotation(int rotation);
    void toggleAxisTitleVisibility(bool enabled);
    void toggleAxisTitleFixed(bool enabled);
    void toggleXAscending(bool enabled);
    void toggleZAscending(bool enabled);
    void togglePolar(bool enabled);
    void setCameraTargetX(int value);
    void setCameraTargetY(int value);
    void setCameraTargetZ(int value);
    void setGraphMargin(int value);

private:
    void fillSeries();
    void resetArrayAndSliders(QSurfaceDataArray *array, float minZ, float maxZ, float minX,
                              float maxX);
    QSurfaceDataRow *createMultiRow(int row, int series, bool change);
    void populateRisingSeries(QSurface3DSeries *series, int rows, int columns, float minValue,
                              float maxValue, bool ascendingX, bool ascendingZ);

    Q3DSurface *m_graph;
    QSurface3DSeries *m_multiseries[4];
    QSurface3DSeries *m_series1;
    QSurface3DSeries *m_series2;
    QSurface3DSeries *m_series3;
    QSurface3DSeries *m_series4;
    QSlider *m_gridSliderX;
    QSlider *m_gridSliderZ;
    QSlider *m_axisRangeSliderX;
    QSlider *m_axisRangeSliderZ;
    QSlider *m_axisMinSliderX;
    QSlider *m_axisMinSliderZ;
    QCheckBox *m_series1CB;
    QCheckBox *m_series2CB;
    QCheckBox *m_series3CB;
    QCheckBox *m_series4CB;
    bool m_gridSlidersLocked;
    int m_xCount;
    int m_zCount;
    int m_activeSample;
    int m_fontSize;
    float m_rangeX;
    float m_rangeY;
    float m_rangeZ;
    float m_minX;
    float m_minY;
    float m_minZ;
    int m_addRowCounter;
    int m_insertTestZPos;
    int m_insertTestIndexPos;
    QTimer m_timer;
    QSurfaceDataArray *m_planeArray;
    QLabel *m_selectionInfoLabel;
    QSurface3DSeries *m_theSeries;
    QSurface3DSeries::DrawFlags m_drawMode;
    QSurface3DSeries::DrawFlags m_drawMode2;
    QSurface3DSeries::DrawFlags m_drawMode3;
    QSurface3DSeries::DrawFlags m_drawMode4;
    float m_limitX;
    float m_limitZ;
    float m_offset;
    float m_multiSampleOffsetX[4];
    float m_multiSampleOffsetZ[4];
    QSurfaceDataArray m_massiveTestCacheArray;
    QVector3D m_cameraTarget;
    QWidget *m_parentWidget;
    QTimer m_graphPositionQueryTimer;
    bool m_ascendingX;
    bool m_ascendingZ;
};

#endif
