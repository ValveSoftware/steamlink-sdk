/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Graphical Effects module.
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

#ifndef IMAGECOMPARE_H
#define IMAGECOMPARE_H

#include <QObject>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtGui/QImage>
#include <math.h>
#include <QDebug>

class ImageCompare : public QObject
{
    Q_OBJECT

public:
    explicit ImageCompare(QObject *parent = 0);
    bool ComparePixels(QImage actual, QImage expected, int tolerance = 0, QString filename = "difference.png");
    bool CompareSizes(QImage actual, QImage expected);
    
signals:
    
public slots:
    
};

#endif // IMAGECOMPARE_H
