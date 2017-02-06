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

#include "globalinspector.h"
#include "highlight.h"
#include "inspecttool.h"
#include "qqmldebugpacket.h"

#include <private/qqmldebugserviceinterfaces_p.h>
#include <private/qabstractanimation_p.h>
#include <private/qqmlcomponent_p.h>

#include <QtGui/qwindow.h>

//INSPECTOR SERVICE PROTOCOL
// <HEADER><COMMAND><DATA>
// <HEADER> : <type{request, response, event}><requestId/eventId>[<response_success_bool>]
// <COMMAND> : {"enable", "disable", "select", "setAnimationSpeed",
//              "showAppOnTop", "createObject", "destroyObject", "moveObject"}
// <DATA> : select: <debugIds_int_list>
//          setAnimationSpeed: <speed_real>
//          showAppOnTop: <set_bool>
//          createObject: <qml_string><parentId_int><imports_string_list><filename_string>
//          destroyObject: <debugId_int>
//          moveObject: <debugId_int><newParentId_int>
// Response for "destroyObject" carries the <debugId_int> of the destroyed object.

QT_BEGIN_NAMESPACE

const char REQUEST[] = "request";
const char RESPONSE[] = "response";
const char EVENT[] = "event";
const char ENABLE[] = "enable";
const char DISABLE[] = "disable";
const char SELECT[] = "select";
const char SET_ANIMATION_SPEED[] = "setAnimationSpeed";
const char SHOW_APP_ON_TOP[] = "showAppOnTop";
const char CREATE_OBJECT[] = "createObject";
const char DESTROY_OBJECT[] = "destroyObject";
const char MOVE_OBJECT[] = "moveObject";

namespace QmlJSDebugger {

void GlobalInspector::removeFromSelectedItems(QObject *object)
{
    if (QQuickItem *item = qobject_cast<QQuickItem*>(object)) {
        if (m_selectedItems.removeOne(item))
            delete m_highlightItems.take(item);
    }
}

void GlobalInspector::setSelectedItems(const QList<QQuickItem *> &items)
{
    if (!syncSelectedItems(items))
        return;

    QList<QObject*> objectList;
    objectList.reserve(items.count());
    foreach (QQuickItem *item, items)
        objectList << item;

    sendCurrentObjects(objectList);
}

void GlobalInspector::showSelectedItemName(QQuickItem *item, const QPointF &point)
{
    SelectionHighlight *highlightItem = m_highlightItems.value(item, 0);
    if (highlightItem)
        highlightItem->showName(point);
}

void GlobalInspector::sendCurrentObjects(const QList<QObject*> &objects)
{
    QQmlDebugPacket ds;

    ds << QByteArray(EVENT) << m_eventId++ << QByteArray(SELECT);

    QList<int> debugIds;
    debugIds.reserve(objects.count());
    foreach (QObject *object, objects)
        debugIds << QQmlDebugService::idForObject(object);
    ds << debugIds;

    emit messageToClient(QQmlInspectorService::s_key, ds.data());
}

static bool reparentQmlObject(QObject *object, QObject *newParent)
{
    if (!newParent)
        return false;

    object->setParent(newParent);
    QQuickItem *newParentItem = qobject_cast<QQuickItem*>(newParent);
    QQuickItem *item = qobject_cast<QQuickItem*>(object);
    if (newParentItem && item)
        item->setParentItem(newParentItem);
    return true;
}

class ObjectCreator : public QObject
{
    Q_OBJECT
public:
    ObjectCreator(int requestId, QQmlEngine *engine, QObject *parent) :
        QObject(parent), m_component(engine), m_requestId(requestId)
    {
        connect(&m_component, &QQmlComponent::statusChanged, this, &ObjectCreator::tryCreateObject);
    }

    void run(const QByteArray &qml, const QUrl &filename)
    {
        m_component.setData(qml, filename);
    }

    void tryCreateObject(QQmlComponent::Status status)
    {
        switch (status) {
        case QQmlComponent::Error:
            emit result(m_requestId, false);
            delete this;
            return;
        case QQmlComponent::Ready: {
            // Stuff might have changed. We have to lookup the parentContext again.
            QQmlContext *parentContext = QQmlEngine::contextForObject(parent());
            if (!parentContext) {
                emit result(m_requestId, false);
            } else {
                QObject *newObject = m_component.create(parentContext);
                if (newObject && reparentQmlObject(newObject, parent()))
                    emit result(m_requestId, true);
                else
                    emit result(m_requestId, false);
            }
            deleteLater(); // The component might send more signals
            return;
        }
        default:
            break;
        }
    }

signals:
    void result(int requestId, bool success);

private:
    QQmlComponent m_component;
    int m_requestId;
};

bool GlobalInspector::createQmlObject(int requestId, const QString &qml, QObject *parent,
                                      const QStringList &importList, const QString &filename)
{
    if (!parent)
        return false;

    QQmlContext *parentContext = QQmlEngine::contextForObject(parent);
    if (!parentContext)
        return false;

    QString imports;
    foreach (const QString &s, importList)
        imports += s + QLatin1Char('\n');

    ObjectCreator *objectCreator = new ObjectCreator(requestId, parentContext->engine(), parent);
    connect(objectCreator, &ObjectCreator::result, this, &GlobalInspector::sendResult);
    objectCreator->run((imports + qml).toUtf8(), QUrl::fromLocalFile(filename));
    return true;
}

void GlobalInspector::addWindow(QQuickWindow *window)
{
    m_windowInspectors.append(new QQuickWindowInspector(window, this));
}

void GlobalInspector::removeWindow(QQuickWindow *window)
{
    for (QList<QmlJSDebugger::QQuickWindowInspector *>::Iterator i = m_windowInspectors.begin();
         i != m_windowInspectors.end();) {
        if ((*i)->quickWindow() == window) {
            delete *i;
            i = m_windowInspectors.erase(i);
        } else {
            ++i;
        }
    }
}

void GlobalInspector::setParentWindow(QQuickWindow *window, QWindow *parentWindow)
{
    foreach (QmlJSDebugger::QQuickWindowInspector *inspector, m_windowInspectors) {
        if (inspector->quickWindow() == window)
            inspector->setParentWindow(parentWindow);
    }
}

bool GlobalInspector::syncSelectedItems(const QList<QQuickItem *> &items)
{
    bool selectionChanged = false;

    // Disconnect and remove items that are no longer selected
    foreach (const QPointer<QQuickItem> &item, m_selectedItems) {
        if (!item) // Don't see how this can happen due to handling of destroyed()
            continue;
        if (items.contains(item))
            continue;

        selectionChanged = true;
        item->disconnect(this);
        m_selectedItems.removeOne(item);
        delete m_highlightItems.take(item);
    }

    // Connect and add newly selected items
    foreach (QQuickItem *item, items) {
        if (m_selectedItems.contains(item))
            continue;

        selectionChanged = true;
        connect(item, &QObject::destroyed, this, &GlobalInspector::removeFromSelectedItems);
        m_selectedItems.append(item);
        foreach (QQuickWindowInspector *inspector, m_windowInspectors) {
            if (inspector->isEnabled() && inspector->quickWindow() == item->window()) {
                m_highlightItems.insert(item, new SelectionHighlight(titleForItem(item), item,
                                                                     inspector->overlay()));
                break;
            }
        }
    }

    return selectionChanged;
}

QString GlobalInspector::titleForItem(QQuickItem *item) const
{
    QString className = QLatin1String(item->metaObject()->className());
    QString objectStringId = idStringForObject(item);

    className.remove(QRegExp(QLatin1String("_QMLTYPE_\\d+")));
    className.remove(QRegExp(QLatin1String("_QML_\\d+")));
    if (className.startsWith(QLatin1String("QQuick")))
        className = className.mid(6);

    QString constructedName;

    if (!objectStringId.isEmpty()) {
        constructedName = objectStringId + QLatin1String(" (") + className + QLatin1Char(')');
    } else if (!item->objectName().isEmpty()) {
        constructedName = item->objectName() + QLatin1String(" (") + className + QLatin1Char(')');
    } else {
        constructedName = className;
    }

    return constructedName;
}

QString GlobalInspector::idStringForObject(QObject *obj) const
{
    QQmlContext *context = qmlContext(obj);
    if (context) {
        QQmlContextData *cdata = QQmlContextData::get(context);
        if (cdata)
            return cdata->findObjectId(obj);
    }
    return QString();
}

void GlobalInspector::processMessage(const QByteArray &message)
{
    bool success = true;
    QQmlDebugPacket ds(message);

    QByteArray type;
    ds >> type;

    int requestId = -1;
    if (type == REQUEST) {
        QByteArray command;
        ds >> requestId >> command;

        if (command == ENABLE) {
            foreach (QQuickWindowInspector *inspector, m_windowInspectors)
                inspector->setEnabled(true);
            success = !m_windowInspectors.isEmpty();
        } else if (command == DISABLE) {
            setSelectedItems(QList<QQuickItem*>());
            foreach (QQuickWindowInspector *inspector, m_windowInspectors)
                inspector->setEnabled(false);
            success = !m_windowInspectors.isEmpty();
        } else if (command == SELECT) {
            QList<int> debugIds;
            ds >> debugIds;

            QList<QQuickItem *> selectedObjects;
            foreach (int debugId, debugIds) {
                if (QQuickItem *obj =
                        qobject_cast<QQuickItem *>(QQmlDebugService::objectForId(debugId)))
                    selectedObjects << obj;
            }
            syncSelectedItems(selectedObjects);
        } else if (command == SET_ANIMATION_SPEED) {
            qreal speed;
            ds >> speed;
            QUnifiedTimer::instance()->setSlowModeEnabled(speed != 1.0);
            QUnifiedTimer::instance()->setSlowdownFactor(speed);
        } else if (command == SHOW_APP_ON_TOP) {
            bool showOnTop;
            ds >> showOnTop;
            foreach (QmlJSDebugger::QQuickWindowInspector *inspector, m_windowInspectors)
                inspector->setShowAppOnTop(showOnTop);
            success = !m_windowInspectors.isEmpty();
        } else if (command == CREATE_OBJECT) {
            QString qml;
            int parentId;
            QString filename;
            QStringList imports;
            ds >> qml >> parentId >> imports >> filename;
            if (QObject *parent = QQmlDebugService::objectForId(parentId)) {
                if (createQmlObject(requestId, qml, parent, imports, filename))
                    return; // will callback for result
                else {
                    success = false;
                }
            } else {
                success = false;
            }

        } else if (command == DESTROY_OBJECT) {
            int debugId;
            ds >> debugId;
            if (QObject *obj = QQmlDebugService::objectForId(debugId))
                delete obj;
            else
                success = false;

        } else if (command == MOVE_OBJECT) {
            int debugId, newParent;
            ds >> debugId >> newParent;
            success = reparentQmlObject(QQmlDebugService::objectForId(debugId),
                                        QQmlDebugService::objectForId(newParent));
        } else {
            qWarning() << "Warning: Not handling command:" << command;
            success = false;
        }
    } else {
        qWarning() << "Warning: Not handling type:" << type << REQUEST;
        success = false;
    }

    sendResult(requestId, success);
}

void GlobalInspector::sendResult(int requestId, bool success)
{
    QQmlDebugPacket rs;
    rs << QByteArray(RESPONSE) << requestId << success;
    emit messageToClient(QQmlInspectorService::s_key, rs.data());
}

GlobalInspector::~GlobalInspector()
{
    // Everything else is parented
    qDeleteAll(m_highlightItems);
}

}

QT_END_NAMESPACE

#include <globalinspector.moc>
