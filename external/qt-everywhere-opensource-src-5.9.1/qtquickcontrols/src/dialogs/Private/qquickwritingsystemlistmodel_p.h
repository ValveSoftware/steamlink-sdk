/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Dialogs module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
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
