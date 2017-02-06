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

#include "twitter.h"

#include <QtCore>
#include <QtNetwork>

class TwitterTimelineModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    TwitterTimelineModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void authenticate(const QPair<QString, QString> &clientCredentials);
    QAbstractOAuth::Status status() const;

public slots:
    void updateTimeline();

signals:
    void authenticated();

private:
    Q_DISABLE_COPY(TwitterTimelineModel)

    void parseJson();

    struct Tweet {
        quint64 id;
        QDateTime createdAt;
        QString user;
        QString text;
    };

    QList<Tweet> tweets;
    Twitter twitter;
};
