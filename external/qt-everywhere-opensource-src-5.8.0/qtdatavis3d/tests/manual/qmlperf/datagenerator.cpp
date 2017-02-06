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

#include "datagenerator.h"
#include <QDebug>

using namespace QtDataVisualization;

Q_DECLARE_METATYPE(QScatter3DSeries *)

DataGenerator::DataGenerator(QObject *parent) :
    QObject(parent)
{
    qRegisterMetaType<QScatter3DSeries *>();

    m_file = new QFile("results.txt");
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Text)) {
        delete m_file;
        m_file = 0;
    }
}

DataGenerator::~DataGenerator()
{
    m_file->close();
    delete m_file;
}

void DataGenerator::generateData(QScatter3DSeries *series, uint count)
{
    QScatterDataArray *dataArray = new QScatterDataArray;
    dataArray->resize(count);
    QScatterDataItem *ptrToDataArray = &dataArray->first();

    float rand_max = float(RAND_MAX);
    for (uint i = 0; i < count; i++) {
            ptrToDataArray->setPosition(QVector3D(float(qrand()) / rand_max,
                                                  float(qrand()) / rand_max,
                                                  float(qrand()) / rand_max));
            ptrToDataArray++;
    }

    series->dataProxy()->resetArray(dataArray);
}

void DataGenerator::add(QScatter3DSeries *series, uint count)
{
    QScatterDataArray appendArray;
    appendArray.resize(count);

    float rand_max = float(RAND_MAX);
    for (uint i = 0; i < count; i++) {
            appendArray[i].setPosition(QVector3D(float(qrand()) / rand_max,
                                                 float(qrand()) / rand_max,
                                                 float(qrand()) / rand_max));
    }

    series->dataProxy()->addItems(appendArray);
}

void DataGenerator::writeLine(int itemCount, float fps)
{
    if (m_file) {
        QTextStream out(m_file);

        QString fpsFormatString(QStringLiteral("%1   %2\n"));
        QString fpsString = fpsFormatString.arg(itemCount).arg(fps);

        out << fpsString;
    }
}
