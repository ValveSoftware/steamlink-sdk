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

#ifndef ERRORSVIEW_H
#define ERRORSVIEW_H

#include <QListView>

QT_BEGIN_NAMESPACE

class QStandardItemModel;

class MultiDataModel;

class ErrorsView : public QListView
{
    Q_OBJECT
public:
    enum ErrorType {
        SuperfluousAccelerator,
        MissingAccelerator,
        SurroundingWhitespaceDiffers,
        PunctuationDiffers,
        IgnoredPhrasebook,
        PlaceMarkersDiffer,
        NumerusMarkerMissing
    };

    ErrorsView(MultiDataModel *dataModel, QWidget *parent = 0);
    void clear();
    void addError(int model, const ErrorType type, const QString &arg = QString());
    QString firstError();
private:
    void addError(int model, const QString &error);
    QStandardItemModel *m_list;
    MultiDataModel *m_dataModel;
};

QT_END_NAMESPACE

#endif // ERRORSVIEW_H
