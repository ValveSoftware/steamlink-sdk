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

#ifndef DATA_H
#define DATA_H

#include "axesinputhandler.h"

#include <QtDataVisualization/q3dscatter.h>
#include <QtGui/QVector3D>

using namespace QtDataVisualization;

class Data : public QObject
{
    Q_OBJECT
public:
    explicit Data(Q3DScatter *scatter);
    ~Data();

    void toggleRanges();

private:
    void addData();
    QVector3D randVector();

private:
    Q3DScatter *m_graph;
    AxesInputHandler *m_inputHandler;
    bool m_autoAdjust;
};

#endif
