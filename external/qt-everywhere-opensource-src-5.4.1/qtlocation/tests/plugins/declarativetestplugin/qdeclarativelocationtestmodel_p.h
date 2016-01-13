/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QDECLARATIVELOCATIONTESTMODEL_H
#define QDECLARATIVELOCATIONTESTMODEL_H

#include <QtQuick/QQuickItem>
#include <QAbstractListModel>
#include <QTimer>
#include <QDebug>
#include <QList>
#include <QtPositioning/QGeoCoordinate>

class DataObject: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QGeoCoordinate coordinate READ coordinate CONSTANT)

public:
    DataObject() {}
    ~DataObject() {}

    QGeoCoordinate coordinate_;
    QGeoCoordinate coordinate() const {return coordinate_;}
};

class QDeclarativeLocationTestModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_PROPERTY(int datacount READ datacount WRITE setDatacount NOTIFY datacountChanged)
    Q_PROPERTY(int delay READ delay WRITE setDelay NOTIFY delayChanged)
    Q_PROPERTY(bool crazyMode READ crazyMode WRITE setCrazyMode NOTIFY crazyModeChanged)
    Q_PROPERTY(int crazyLevel READ crazyLevel WRITE setCrazyLevel NOTIFY crazyLevelChanged)
    Q_PROPERTY(QString datatype READ datatype WRITE setDatatype NOTIFY datatypeChanged)
    Q_INTERFACES(QQmlParserStatus)

public:
    QDeclarativeLocationTestModel(QObject* parent = 0);
    ~QDeclarativeLocationTestModel();

    enum Roles {
        TestDataRole = Qt::UserRole + 500
    };

    // from QQmlParserStatus
    virtual void componentComplete();
    virtual void classBegin() {}

    // From QAbstractListModel
    virtual int rowCount(const QModelIndex &parent) const;
    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QHash<int, QByteArray> roleNames() const;

    int datacount() const;
    void setDatacount(int datacount);

    int delay() const;
    void setDelay(int delay);

    int crazyLevel() const;
    void setCrazyLevel(int level);

    bool crazyMode() const;
    void setCrazyMode(bool mode);

    QString datatype() const;
    void setDatatype(QString datatype);

    //Q_INVOKABLE void clear();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void update();
    //Q_INVOKABLE void reset();

signals:
    void countChanged();
    void datacountChanged();
    void datatypeChanged();
    void delayChanged();
    void modelChanged();
    void crazyLevelChanged();
    void crazyModeChanged();

private slots:
    void repopulate();
    void timerFired();

private:
    void scheduleRepopulation();

private:
    int delay_;
    int datacount_;
    bool componentCompleted_;
    QString datatype_;
    QTimer timer_;
    QList<DataObject*> dataobjects_;
    int crazyLevel_;
    bool crazyMode_;
};

#endif
