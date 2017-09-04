/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Linguist of the Qt Toolkit.
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

#include "statistics.h"

QT_BEGIN_NAMESPACE

Statistics::Statistics(QWidget* parent, Qt::WindowFlags fl)
  : QDialog(parent, fl)
{
    setupUi(this);
}

void Statistics::languageChange()
{
    retranslateUi(this);
}

void Statistics::updateStats(int sW,int sC,int sCS,int trW,int trC,int trCS)
{
    untrWords->setText(QString::number(sW));
    untrChars->setText(QString::number(sC));
    untrCharsSpc->setText(QString::number(sCS));
    trWords->setText(QString::number(trW));
    trChars->setText(QString::number(trC));
    trCharsSpc->setText(QString::number(trCS));
}

QT_END_NAMESPACE
