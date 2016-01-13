/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick.Dialogs module of the Qt Toolkit.
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

#ifndef QQUICKWRITINGSYSTEMLISTMODEL_P_H
#define QQUICKWRITINGSYSTEMLISTMODEL_P_H

#include <QtCore/qstringlist.h>
#include <QtCore/QAbstractListModel>
#include <QtQml/qqmlparserstatus.h>
#include <QtQml/qjsvalue.h>

QT_BEGIN_NAMESPACE

class QModelIndex;

class QQuickWritingSystemListModelPrivate;

class QQuickWritingSystemListModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QStringList writingSystems READ writingSystems NOTIFY writingSystemsChanged)

    Q_PROPERTY(int count READ count NOTIFY rowCountChanged)

public:
    QQuickWritingSystemListModel(QObject *parent = 0);
    ~QQuickWritingSystemListModel();

    enum Roles {
        WritingSystemNameRole = Qt::UserRole + 1,
        WritingSystemSampleRole = Qt::UserRole + 2
    };

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    virtual QHash<int, QByteArray> roleNames() const;

    int count() const { return rowCount(QModelIndex()); }

    QStringList writingSystems() const;

    Q_INVOKABLE QJSValue get(int index) const;

    virtual void classBegin();
    virtual void componentComplete();

Q_SIGNALS:
    void writingSystemsChanged();
    void rowCountChanged() const;

private:
    Q_DISABLE_COPY(QQuickWritingSystemListModel)
    Q_DECLARE_PRIVATE(QQuickWritingSystemListModel)
    QScopedPointer<QQuickWritingSystemListModelPrivate> d_ptr;

};

QT_END_NAMESPACE

#endif // QQUICKWRITINGSYSTEMLISTMODEL_P_H
