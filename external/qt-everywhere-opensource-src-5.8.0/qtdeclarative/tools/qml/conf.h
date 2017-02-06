/****************************************************************************
**
** Copyright (C) 2016 Research In Motion.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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
#ifndef CONF_H
#define CONF_H

#include <QtQml/QQmlContext>
#include <QtQml/QQmlListProperty>
#include <QObject>
#include <QUrl>

class PartialScene : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl container READ container WRITE setContainer NOTIFY containerChanged)
    Q_PROPERTY(QString itemType READ itemType WRITE setItemType NOTIFY itemTypeChanged)
public:
    PartialScene(QObject *parent = 0) : QObject(parent)
    {}

    const QUrl container() const { return m_container; }
    const QString itemType() const { return m_itemType; }

    void setContainer(const QUrl &a) {
        if (a==m_container)
            return;
        m_container = a;
        emit containerChanged();
    }
    void setItemType(const QString &a) {
        if (a==m_itemType)
            return;
        m_itemType = a;
        emit itemTypeChanged();
    }

signals:
    void containerChanged();
    void itemTypeChanged();

private:
    QUrl m_container;
    QString m_itemType;
};

class Config : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<PartialScene> sceneCompleters READ sceneCompleters)
    Q_CLASSINFO("DefaultProperty", "sceneCompleters")
public:
    Config (QObject* parent=0) : QObject(parent)
    {}

    QQmlListProperty<PartialScene> sceneCompleters()
    {
        return QQmlListProperty<PartialScene>(this, completers);
    }

    QList<PartialScene*> completers;
};

#endif
