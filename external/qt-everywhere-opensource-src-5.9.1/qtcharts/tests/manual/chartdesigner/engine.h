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

#ifndef ENGINE_H
#define ENGINE_H

#include <QtCore/QObject>
#include <QtCharts/QAbstractSeries>
#include <QtCore/QModelIndex>

class QStandardItemModel;
class QItemSelectionModel;


QT_CHARTS_BEGIN_NAMESPACE
class QChart;
class QXYSeries;
class QAbstractBarSeries;
class QPieSeries;
class QAreaSeries;
QT_CHARTS_END_NAMESPACE

QT_CHARTS_USE_NAMESPACE

class Engine : public QObject
{
    Q_OBJECT
public:
    explicit Engine(QObject *parent = 0);
   ~Engine();

    int modelCount() { return m_count; }
    QStandardItemModel *model() const { return m_model; }
    QItemSelectionModel *selectionModel() const { return m_selection; }
    QChart* chart() const { return m_chart; }
    void clearModels();
    QList<QAbstractSeries*> addSeries(QAbstractSeries::SeriesType type);
    void removeSeries(QAbstractSeries* series);
    bool save(const QString &filename) const;
    bool load(const QString &filename);
signals:
    void selected();

private:
    void createModels();
    void setupXYSeries(QXYSeries *xyseries, const QList<int>& columns, int column, int minRow, int maxRow);
    void setupBarSeries(QAbstractBarSeries *series, const QList<int>& columns, int minRow, int maxRow);
    void setupPieSeries(QPieSeries *pie, const QList<int>& columns, int minRow, int maxRow);
    void setupAreaSeries(QAreaSeries *series, const QList<int>& columns, int minRow, int maxRow);

private:
    int m_count;
    QChart *m_chart;
    QStandardItemModel *m_model;
    QItemSelectionModel *m_selection;
    QMap<QAbstractSeries*, QList<QModelIndex> > m_seriesModelIndex;
};

#endif /* ENGINE_H */
