/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QQMLENGINEDEBUGSERVICE_H
#define QQMLENGINEDEBUGSERVICE_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qqmldebugservice_p.h>
#include <private/qqmldebugserviceinterfaces_p.h>

#include <QtCore/qurl.h>
#include <QtCore/qvariant.h>
#include <QtCore/QPointer>

QT_BEGIN_NAMESPACE

class QQmlEngine;
class QQmlContext;
class QQmlWatcher;
class QDataStream;
class QQmlDebugStatesDelegate;

class QQmlEngineDebugServiceImpl : public QQmlEngineDebugService
{
    Q_OBJECT
public:
    QQmlEngineDebugServiceImpl(QObject * = 0);
    ~QQmlEngineDebugServiceImpl();

    struct QQmlObjectData {
        QUrl url;
        int lineNumber;
        int columnNumber;
        QString idString;
        QString objectName;
        QString objectType;
        int objectId;
        int contextId;
        int parentId;
    };

    struct QQmlObjectProperty {
        enum Type { Unknown, Basic, Object, List, SignalProperty, Variant };
        Type type;
        QString name;
        QVariant value;
        QString valueTypeName;
        QString binding;
        bool hasNotifySignal;
    };

    void engineAboutToBeAdded(QJSEngine *) Q_DECL_OVERRIDE;
    void engineAboutToBeRemoved(QJSEngine *) Q_DECL_OVERRIDE;
    void objectCreated(QJSEngine *, QObject *) Q_DECL_OVERRIDE;

    void setStatesDelegate(QQmlDebugStatesDelegate *) Q_DECL_OVERRIDE;

signals:
    void scheduleMessage(const QByteArray &);

protected:
    virtual void messageReceived(const QByteArray &) Q_DECL_OVERRIDE;

private:
    friend class QQmlDebuggerServiceFactory;

    void processMessage(const QByteArray &msg);
    void propertyChanged(int id, int objectId, const QMetaProperty &property, const QVariant &value);

    void prepareDeferredObjects(QObject *);
    void buildObjectList(QDataStream &, QQmlContext *,
                         const QList<QPointer<QObject> > &instances);
    void buildObjectDump(QDataStream &, QObject *, bool, bool);
    void buildStatesList(bool cleanList, const QList<QPointer<QObject> > &instances);
    QQmlObjectData objectData(QObject *);
    QQmlObjectProperty propertyData(QObject *, int);
    QVariant valueContents(QVariant defaultValue) const;
    bool setBinding(int objectId, const QString &propertyName, const QVariant &expression, bool isLiteralValue, QString filename = QString(), int line = -1, int column = 0);
    bool resetBinding(int objectId, const QString &propertyName);
    bool setMethodBody(int objectId, const QString &method, const QString &body);
    void storeObjectIds(QObject *co);
    QList<QObject *> objectForLocationInfo(const QString &filename, int lineNumber,
                                           int columnNumber);

    QList<QJSEngine *> m_engines;
    QQmlWatcher *m_watch;
    QQmlDebugStatesDelegate *m_statesDelegate;
};
QDataStream &operator<<(QDataStream &, const QQmlEngineDebugServiceImpl::QQmlObjectData &);
QDataStream &operator>>(QDataStream &, QQmlEngineDebugServiceImpl::QQmlObjectData &);
QDataStream &operator<<(QDataStream &, const QQmlEngineDebugServiceImpl::QQmlObjectProperty &);
QDataStream &operator>>(QDataStream &, QQmlEngineDebugServiceImpl::QQmlObjectProperty &);

QT_END_NAMESPACE

#endif // QQMLENGINEDEBUGSERVICE_H

