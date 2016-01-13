/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QQUICKXMLLISTMODEL_H
#define QQUICKXMLLISTMODEL_H

#include <qqml.h>
#include <qqmlinfo.h>

#include <QtCore/qurl.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qabstractitemmodel.h>
#include <private/qv8engine_p.h>

QT_BEGIN_NAMESPACE


class QQmlContext;
class QQuickXmlListModelRole;
class QQuickXmlListModelPrivate;

struct QQuickXmlQueryResult {
    int queryId;
    int size;
    QList<QList<QVariant> > data;
    QList<QPair<int, int> > inserted;
    QList<QPair<int, int> > removed;
    QStringList keyRoleResultsCache;
};

class QQuickXmlListModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_ENUMS(Status)

    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QString xml READ xml WRITE setXml NOTIFY xmlChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QString namespaceDeclarations READ namespaceDeclarations WRITE setNamespaceDeclarations NOTIFY namespaceDeclarationsChanged)
    Q_PROPERTY(QQmlListProperty<QQuickXmlListModelRole> roles READ roleObjects)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_CLASSINFO("DefaultProperty", "roles")

public:
    QQuickXmlListModel(QObject *parent = 0);
    ~QQuickXmlListModel();

    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    int rowCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    QHash<int, QByteArray> roleNames() const;

    int count() const;

    QQmlListProperty<QQuickXmlListModelRole> roleObjects();

    QUrl source() const;
    void setSource(const QUrl&);

    QString xml() const;
    void setXml(const QString&);

    QString query() const;
    void setQuery(const QString&);

    QString namespaceDeclarations() const;
    void setNamespaceDeclarations(const QString&);

    Q_INVOKABLE QQmlV4Handle get(int index) const;

    enum Status { Null, Ready, Loading, Error };
    Status status() const;
    qreal progress() const;

    Q_INVOKABLE QString errorString() const;

    virtual void classBegin();
    virtual void componentComplete();

Q_SIGNALS:
    void statusChanged(QQuickXmlListModel::Status);
    void progressChanged(qreal progress);
    void countChanged();
    void sourceChanged();
    void xmlChanged();
    void queryChanged();
    void namespaceDeclarationsChanged();

public Q_SLOTS:
    // ### need to use/expose Expiry to guess when to call this?
    // ### property to auto-call this on reasonable Expiry?
    // ### LastModified/Age also useful to guess.
    // ### Probably also applies to other network-requesting types.
    void reload();

private Q_SLOTS:
    void requestFinished();
    void requestProgress(qint64,qint64);
    void dataCleared();
    void queryCompleted(const QQuickXmlQueryResult &);
    void queryError(void* object, const QString& error);

private:
    Q_DECLARE_PRIVATE(QQuickXmlListModel)
    Q_DISABLE_COPY(QQuickXmlListModel)
};

class QQuickXmlListModelRole : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(bool isKey READ isKey WRITE setIsKey NOTIFY isKeyChanged)
public:
    QQuickXmlListModelRole() : m_isKey(false) {}
    ~QQuickXmlListModelRole() {}

    QString name() const { return m_name; }
    void setName(const QString &name) {
        if (name == m_name)
            return;
        m_name = name;
        Q_EMIT nameChanged();
    }

    QString query() const { return m_query; }
    void setQuery(const QString &query)
    {
        if (query.startsWith(QLatin1Char('/'))) {
            qmlInfo(this) << tr("An XmlRole query must not start with '/'");
            return;
        }
        if (m_query == query)
            return;
        m_query = query;
        Q_EMIT queryChanged();
    }

    bool isKey() const { return m_isKey; }
    void setIsKey(bool b) {
        if (m_isKey == b)
            return;
        m_isKey = b;
        Q_EMIT isKeyChanged();
    }

    bool isValid() {
        return !m_name.isEmpty() && !m_query.isEmpty();
    }

Q_SIGNALS:
    void nameChanged();
    void queryChanged();
    void isKeyChanged();

private:
    QString m_name;
    QString m_query;
    bool m_isKey;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickXmlListModel)
QML_DECLARE_TYPE(QQuickXmlListModelRole)

#endif // QQUICKXMLLISTMODEL_H
