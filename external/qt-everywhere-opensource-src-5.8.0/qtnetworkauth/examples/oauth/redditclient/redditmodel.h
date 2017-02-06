/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Network Auth module of the Qt Toolkit.
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

#ifndef REDDITMODEL_H
#define REDDITMODEL_H

#include "redditwrapper.h"

#include <QtCore>

class QNetworkReply;

class RedditModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    RedditModel(QObject *parent = nullptr);
    RedditModel(const QString &clientId, QObject *parent = nullptr);

    virtual int rowCount(const QModelIndex &parent) const override;
    virtual int columnCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;

    void grant();

signals:
    void error(const QString &errorString);

private slots:
    void update();

private:
    RedditWrapper redditWrapper;
    QPointer<QNetworkReply> liveThreadReply;
    QList<QJsonObject> threads;
};

#endif // REDDITMODEL_H
