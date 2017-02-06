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

#ifndef DATASERIEDIALOG_H
#define DATASERIEDIALOG_H

#include <QtWidgets/QDialog>

QT_BEGIN_NAMESPACE
class QGroupBox;
class QCheckBox;
QT_END_NAMESPACE

class DataSerieDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DataSerieDialog(QWidget *parent = 0);

signals:
    void accepted(QString series, int columnCount, int rowCount, QString dataCharacteristics, bool labelsDefined);

public slots:
    void accept();

private:
    QGroupBox *seriesTypeSelector();
    QGroupBox *columnCountSelector();
    QGroupBox *rowCountSelector();
    QGroupBox *dataCharacteristicsSelector();
    void selectRadio(QGroupBox *groupBox, int defaultSelection);
    QString radioSelection(QGroupBox *groupBox);
    QGroupBox *m_seriesTypeSelector;
    QGroupBox *m_columnCountSelector;
    QGroupBox *m_rowCountSelector;
    QCheckBox *m_labelsSelector;
    QGroupBox *m_dataCharacteristicsSelector;
};

#endif // DATASERIEDIALOG_H
