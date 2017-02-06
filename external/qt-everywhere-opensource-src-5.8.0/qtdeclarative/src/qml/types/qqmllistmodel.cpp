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

#include "qqmllistmodel_p_p.h"
#include "qqmllistmodelworkeragent_p.h"
#include <private/qqmlopenmetaobject_p.h>
#include <private/qqmljsast_p.h>
#include <private/qqmljsengine_p.h>

#include <private/qqmlcustomparser_p.h>
#include <private/qqmlengine_p.h>
#include <private/qqmlnotifier_p.h>

#include <private/qv4object_p.h>
#include <private/qv4dateobject_p.h>
#include <private/qv4objectiterator_p.h>
#include <private/qv4alloca_p.h>

#include <qqmlcontext.h>
#include <qqmlinfo.h>

#include <QtCore/qdebug.h>
#include <QtCore/qstack.h>
#include <QXmlStreamReader>
#include <QtCore/qdatetime.h>
#include <QScopedValueRollback>

QT_BEGIN_NAMESPACE

// Set to 1024 as a debugging aid - easier to distinguish uids from indices of elements/models.
enum { MIN_LISTMODEL_UID = 1024 };

static QAtomicInt uidCounter(MIN_LISTMODEL_UID);

template <typename T>
static bool isMemoryUsed(const char *mem)
{
    for (size_t i=0 ; i < sizeof(T) ; ++i) {
        if (mem[i] != 0)
            return true;
    }

    return false;
}

static QString roleTypeName(ListLayout::Role::DataType t)
{
    static const QString roleTypeNames[] = {
        QStringLiteral("String"), QStringLiteral("Number"), QStringLiteral("Bool"),
        QStringLiteral("List"), QStringLiteral("QObject"), QStringLiteral("VariantMap"),
        QStringLiteral("DateTime")
    };

    if (t > ListLayout::Role::Invalid && t < ListLayout::Role::MaxDataType)
        return roleTypeNames[t];

    return QString();
}

const ListLayout::Role &ListLayout::getRoleOrCreate(const QString &key, Role::DataType type)
{
    QStringHash<Role *>::Node *node = roleHash.findNode(key);
    if (node) {
        const Role &r = *node->value;
        if (type != r.type)
            qmlInfo(0) << QStringLiteral("Can't assign to existing role '%1' of different type [%2 -> %3]").arg(r.name).arg(roleTypeName(type)).arg(roleTypeName(r.type));
        return r;
    }

    return createRole(key, type);
}

const ListLayout::Role &ListLayout::getRoleOrCreate(QV4::String *key, Role::DataType type)
{
    QStringHash<Role *>::Node *node = roleHash.findNode(key);
    if (node) {
        const Role &r = *node->value;
        if (type != r.type)
            qmlInfo(0) << QStringLiteral("Can't assign to existing role '%1' of different type [%2 -> %3]").arg(r.name).arg(roleTypeName(type)).arg(roleTypeName(r.type));
        return r;
    }

    QString qkey = key->toQString();

    return createRole(qkey, type);
}

const ListLayout::Role &ListLayout::createRole(const QString &key, ListLayout::Role::DataType type)
{
    const int dataSizes[] = { sizeof(QString), sizeof(double), sizeof(bool), sizeof(ListModel *), sizeof(QPointer<QObject>), sizeof(QVariantMap), sizeof(QDateTime) };
    const int dataAlignments[] = { sizeof(QString), sizeof(double), sizeof(bool), sizeof(ListModel *), sizeof(QObject *), sizeof(QVariantMap), sizeof(QDateTime) };

    Role *r = new Role;
    r->name = key;
    r->type = type;

    if (type == Role::List) {
        r->subLayout = new ListLayout;
    } else {
        r->subLayout = 0;
    }

    int dataSize = dataSizes[type];
    int dataAlignment = dataAlignments[type];

    int dataOffset = (currentBlockOffset + dataAlignment-1) & ~(dataAlignment-1);
    if (dataOffset + dataSize > ListElement::BLOCK_SIZE) {
        r->blockIndex = ++currentBlock;
        r->blockOffset = 0;
        currentBlockOffset = dataSize;
    } else {
        r->blockIndex = currentBlock;
        r->blockOffset = dataOffset;
        currentBlockOffset = dataOffset + dataSize;
    }

    int roleIndex = roles.count();
    r->index = roleIndex;

    roles.append(r);
    roleHash.insert(key, r);

    return *r;
}

ListLayout::ListLayout(const ListLayout *other) : currentBlock(0), currentBlockOffset(0)
{
    const int otherRolesCount = other->roles.count();
    roles.reserve(otherRolesCount);
    for (int i=0 ; i < otherRolesCount; ++i) {
        Role *role = new Role(other->roles[i]);
        roles.append(role);
        roleHash.insert(role->name, role);
    }
    currentBlockOffset = other->currentBlockOffset;
    currentBlock = other->currentBlock;
}

ListLayout::~ListLayout()
{
    qDeleteAll(roles);
}

void ListLayout::sync(ListLayout *src, ListLayout *target)
{
    int roleOffset = target->roles.count();
    int newRoleCount = src->roles.count() - roleOffset;

    for (int i=0 ; i < newRoleCount ; ++i) {
        Role *role = new Role(src->roles[roleOffset + i]);
        target->roles.append(role);
        target->roleHash.insert(role->name, role);
    }

    target->currentBlockOffset = src->currentBlockOffset;
    target->currentBlock = src->currentBlock;
}

ListLayout::Role::Role(const Role *other)
{
    name = other->name;
    type = other->type;
    blockIndex = other->blockIndex;
    blockOffset = other->blockOffset;
    index = other->index;
    if (other->subLayout)
        subLayout = new ListLayout(other->subLayout);
    else
        subLayout = 0;
}

ListLayout::Role::~Role()
{
    delete subLayout;
}

const ListLayout::Role *ListLayout::getRoleOrCreate(const QString &key, const QVariant &data)
{
    Role::DataType type;

    switch (data.type()) {
        case QVariant::Double:      type = Role::Number;      break;
        case QVariant::Int:         type = Role::Number;      break;
        case QVariant::UserType:    type = Role::List;        break;
        case QVariant::Bool:        type = Role::Bool;        break;
        case QVariant::String:      type = Role::String;      break;
        case QVariant::Map:         type = Role::VariantMap;  break;
        case QVariant::DateTime:    type = Role::DateTime;    break;
        default:                    type = Role::Invalid;     break;
    }

    if (type == Role::Invalid) {
        qmlInfo(0) << "Can't create role for unsupported data type";
        return 0;
    }

    return &getRoleOrCreate(key, type);
}

const ListLayout::Role *ListLayout::getExistingRole(const QString &key)
{
    Role *r = 0;
    QStringHash<Role *>::Node *node = roleHash.findNode(key);
    if (node)
        r = node->value;
    return r;
}

const ListLayout::Role *ListLayout::getExistingRole(QV4::String *key)
{
    Role *r = 0;
    QStringHash<Role *>::Node *node = roleHash.findNode(key);
    if (node)
        r = node->value;
    return r;
}

QObject *ListModel::getOrCreateModelObject(QQmlListModel *model, int elementIndex)
{
    ListElement *e = elements[elementIndex];
    if (e->m_objectCache == 0) {
        e->m_objectCache = new QObject;
        (void)new ModelNodeMetaObject(e->m_objectCache, model, elementIndex);
    }
    return e->m_objectCache;
}

void ListModel::sync(ListModel *src, ListModel *target, QHash<int, ListModel *> *targetModelHash)
{
    // Sanity check
    target->m_uid = src->m_uid;
    if (targetModelHash)
        targetModelHash->insert(target->m_uid, target);

    // Build hash of elements <-> uid for each of the lists
    QHash<int, ElementSync> elementHash;
    for (int i=0 ; i < target->elements.count() ; ++i) {
        ListElement *e = target->elements.at(i);
        int uid = e->getUid();
        ElementSync sync;
        sync.target = e;
        elementHash.insert(uid, sync);
    }
    for (int i=0 ; i < src->elements.count() ; ++i) {
        ListElement *e = src->elements.at(i);
        int uid = e->getUid();

        QHash<int, ElementSync>::iterator it = elementHash.find(uid);
        if (it == elementHash.end()) {
            ElementSync sync;
            sync.src = e;
            elementHash.insert(uid, sync);
        } else {
            ElementSync &sync = it.value();
            sync.src = e;
        }
    }

    // Get list of elements that are in the target but no longer in the source. These get deleted first.
    QHash<int, ElementSync>::iterator it = elementHash.begin();
    QHash<int, ElementSync>::iterator end = elementHash.end();
    while (it != end) {
        const ElementSync &s = it.value();
        if (s.src == 0) {
            s.target->destroy(target->m_layout);
            target->elements.removeOne(s.target);
            delete s.target;
        }
        ++it;
    }

    // Sync the layouts
    ListLayout::sync(src->m_layout, target->m_layout);

    // Clear the target list, and append in correct order from the source
    target->elements.clear();
    for (int i=0 ; i < src->elements.count() ; ++i) {
        ListElement *srcElement = src->elements.at(i);
        it = elementHash.find(srcElement->getUid());
        const ElementSync &s = it.value();
        ListElement *targetElement = s.target;
        if (targetElement == 0) {
            targetElement = new ListElement(srcElement->getUid());
        }
        ListElement::sync(srcElement, src->m_layout, targetElement, target->m_layout, targetModelHash);
        target->elements.append(targetElement);
    }

    target->updateCacheIndices();

    // Update values stored in target meta objects
    for (int i=0 ; i < target->elements.count() ; ++i) {
        ListElement *e = target->elements[i];
        if (ModelNodeMetaObject *mo = e->objectCache())
            mo->updateValues();
    }
}

ListModel::ListModel(ListLayout *layout, QQmlListModel *modelCache, int uid) : m_layout(layout), m_modelCache(modelCache)
{
    if (uid == -1)
        uid = uidCounter.fetchAndAddOrdered(1);
    m_uid = uid;
}

void ListModel::destroy()
{
    clear();
    m_uid = -1;
    m_layout = 0;
    if (m_modelCache && m_modelCache->m_primary == false)
        delete m_modelCache;
    m_modelCache = 0;
}

int ListModel::appendElement()
{
    int elementIndex = elements.count();
    newElement(elementIndex);
    return elementIndex;
}

void ListModel::insertElement(int index)
{
    newElement(index);
    updateCacheIndices();
}

void ListModel::move(int from, int to, int n)
{
    if (from > to) {
        // Only move forwards - flip if backwards moving
        int tfrom = from;
        int tto = to;
        from = tto;
        to = tto+n;
        n = tfrom-tto;
    }

    QPODVector<ListElement *, 4> store;
    for (int i=0 ; i < (to-from) ; ++i)
        store.append(elements[from+n+i]);
    for (int i=0 ; i < n ; ++i)
        store.append(elements[from+i]);
    for (int i=0 ; i < store.count() ; ++i)
        elements[from+i] = store[i];

    updateCacheIndices();
}

void ListModel::newElement(int index)
{
    ListElement *e = new ListElement;
    elements.insert(index, e);
}

void ListModel::updateCacheIndices()
{
    for (int i=0 ; i < elements.count() ; ++i) {
        ListElement *e = elements.at(i);
        if (ModelNodeMetaObject *mo = e->objectCache())
            mo->m_elementIndex = i;
    }
}

QVariant ListModel::getProperty(int elementIndex, int roleIndex, const QQmlListModel *owner, QV4::ExecutionEngine *eng)
{
    if (roleIndex >= m_layout->roleCount())
        return QVariant();
    ListElement *e = elements[elementIndex];
    const ListLayout::Role &r = m_layout->getExistingRole(roleIndex);
    return e->getProperty(r, owner, eng);
}

ListModel *ListModel::getListProperty(int elementIndex, const ListLayout::Role &role)
{
    ListElement *e = elements[elementIndex];
    return e->getListProperty(role);
}

void ListModel::set(int elementIndex, QV4::Object *object, QVector<int> *roles)
{
    ListElement *e = elements[elementIndex];

    QV4::ExecutionEngine *v4 = object->engine();
    QV4::Scope scope(v4);
    QV4::ScopedObject o(scope);

    QV4::ObjectIterator it(scope, object, QV4::ObjectIterator::WithProtoChain|QV4::ObjectIterator::EnumerableOnly);
    QV4::ScopedString propertyName(scope);
    QV4::ScopedValue propertyValue(scope);
    while (1) {
        propertyName = it.nextPropertyNameAsString(propertyValue);
        if (!propertyName)
            break;

        // Check if this key exists yet
        int roleIndex = -1;

        // Add the value now
        if (const QV4::String *s = propertyValue->as<QV4::String>()) {
            const ListLayout::Role &r = m_layout->getRoleOrCreate(propertyName, ListLayout::Role::String);
            roleIndex = e->setStringProperty(r, s->toQString());
        } else if (propertyValue->isNumber()) {
            const ListLayout::Role &r = m_layout->getRoleOrCreate(propertyName, ListLayout::Role::Number);
            roleIndex = e->setDoubleProperty(r, propertyValue->asDouble());
        } else if (QV4::ArrayObject *a = propertyValue->as<QV4::ArrayObject>()) {
            const ListLayout::Role &r = m_layout->getRoleOrCreate(propertyName, ListLayout::Role::List);
            ListModel *subModel = new ListModel(r.subLayout, 0, -1);

            int arrayLength = a->getLength();
            for (int j=0 ; j < arrayLength ; ++j) {
                o = a->getIndexed(j);
                subModel->append(o);
            }

            roleIndex = e->setListProperty(r, subModel);
        } else if (propertyValue->isBoolean()) {
            const ListLayout::Role &r = m_layout->getRoleOrCreate(propertyName, ListLayout::Role::Bool);
            roleIndex = e->setBoolProperty(r, propertyValue->booleanValue());
        } else if (QV4::DateObject *dd = propertyValue->as<QV4::DateObject>()) {
            const ListLayout::Role &r = m_layout->getRoleOrCreate(propertyName, ListLayout::Role::DateTime);
            QDateTime dt = dd->toQDateTime();
            roleIndex = e->setDateTimeProperty(r, dt);
        } else if (QV4::Object *o = propertyValue->as<QV4::Object>()) {
            if (QV4::QObjectWrapper *wrapper = o->as<QV4::QObjectWrapper>()) {
                QObject *o = wrapper->object();
                const ListLayout::Role &role = m_layout->getRoleOrCreate(propertyName, ListLayout::Role::QObject);
                if (role.type == ListLayout::Role::QObject)
                    roleIndex = e->setQObjectProperty(role, o);
            } else {
                const ListLayout::Role &role = m_layout->getRoleOrCreate(propertyName, ListLayout::Role::VariantMap);
                if (role.type == ListLayout::Role::VariantMap) {
                    QV4::ScopedObject obj(scope, o);
                    roleIndex = e->setVariantMapProperty(role, obj);
                }
            }
        } else if (propertyValue->isNullOrUndefined()) {
            const ListLayout::Role *r = m_layout->getExistingRole(propertyName);
            if (r)
                e->clearProperty(*r);
        }

        if (roleIndex != -1)
            roles->append(roleIndex);
    }

    if (ModelNodeMetaObject *mo = e->objectCache())
        mo->updateValues(*roles);
}

void ListModel::set(int elementIndex, QV4::Object *object)
{
    if (!object)
        return;

    ListElement *e = elements[elementIndex];

    QV4::ExecutionEngine *v4 = object->engine();
    QV4::Scope scope(v4);

    QV4::ObjectIterator it(scope, object, QV4::ObjectIterator::WithProtoChain|QV4::ObjectIterator::EnumerableOnly);
    QV4::ScopedString propertyName(scope);
    QV4::ScopedValue propertyValue(scope);
    QV4::ScopedObject o(scope);
    while (1) {
        propertyName = it.nextPropertyNameAsString(propertyValue);
        if (!propertyName)
            break;

        // Add the value now
        if (propertyValue->isString()) {
            const ListLayout::Role &r = m_layout->getRoleOrCreate(propertyName, ListLayout::Role::String);
            if (r.type == ListLayout::Role::String)
                e->setStringPropertyFast(r, propertyValue->stringValue()->toQString());
        } else if (propertyValue->isNumber()) {
            const ListLayout::Role &r = m_layout->getRoleOrCreate(propertyName, ListLayout::Role::Number);
            if (r.type == ListLayout::Role::Number) {
                e->setDoublePropertyFast(r, propertyValue->asDouble());
            }
        } else if (QV4::ArrayObject *a = propertyValue->as<QV4::ArrayObject>()) {
            const ListLayout::Role &r = m_layout->getRoleOrCreate(propertyName, ListLayout::Role::List);
            if (r.type == ListLayout::Role::List) {
                ListModel *subModel = new ListModel(r.subLayout, 0, -1);

                int arrayLength = a->getLength();
                for (int j=0 ; j < arrayLength ; ++j) {
                    o = a->getIndexed(j);
                    subModel->append(o);
                }

                e->setListPropertyFast(r, subModel);
            }
        } else if (propertyValue->isBoolean()) {
            const ListLayout::Role &r = m_layout->getRoleOrCreate(propertyName, ListLayout::Role::Bool);
            if (r.type == ListLayout::Role::Bool) {
                e->setBoolPropertyFast(r, propertyValue->booleanValue());
            }
        } else if (QV4::DateObject *date = propertyValue->as<QV4::DateObject>()) {
            const ListLayout::Role &r = m_layout->getRoleOrCreate(propertyName, ListLayout::Role::DateTime);
            if (r.type == ListLayout::Role::DateTime) {
                QDateTime dt = date->toQDateTime();;
                e->setDateTimePropertyFast(r, dt);
            }
        } else if (QV4::Object *o = propertyValue->as<QV4::Object>()) {
            if (QV4::QObjectWrapper *wrapper = o->as<QV4::QObjectWrapper>()) {
                QObject *o = wrapper->object();
                const ListLayout::Role &r = m_layout->getRoleOrCreate(propertyName, ListLayout::Role::QObject);
                if (r.type == ListLayout::Role::QObject)
                    e->setQObjectPropertyFast(r, o);
            } else {
                const ListLayout::Role &role = m_layout->getRoleOrCreate(propertyName, ListLayout::Role::VariantMap);
                if (role.type == ListLayout::Role::VariantMap)
                    e->setVariantMapFast(role, o);
            }
        } else if (propertyValue->isNullOrUndefined()) {
            const ListLayout::Role *r = m_layout->getExistingRole(propertyName);
            if (r)
                e->clearProperty(*r);
        }
    }
}

void ListModel::clear()
{
    int elementCount = elements.count();
    for (int i=0 ; i < elementCount ; ++i) {
        elements[i]->destroy(m_layout);
        delete elements[i];
    }
    elements.clear();
}

void ListModel::remove(int index, int count)
{
    for (int i=0 ; i < count ; ++i) {
        elements[index+i]->destroy(m_layout);
        delete elements[index+i];
    }
    elements.remove(index, count);
    updateCacheIndices();
}

void ListModel::insert(int elementIndex, QV4::Object *object)
{
    insertElement(elementIndex);
    set(elementIndex, object);
}

int ListModel::append(QV4::Object *object)
{
    int elementIndex = appendElement();
    set(elementIndex, object);
    return elementIndex;
}

int ListModel::setOrCreateProperty(int elementIndex, const QString &key, const QVariant &data)
{
    int roleIndex = -1;

    if (elementIndex >= 0 && elementIndex < elements.count()) {
        ListElement *e = elements[elementIndex];

        const ListLayout::Role *r = m_layout->getRoleOrCreate(key, data);
        if (r) {
            roleIndex = e->setVariantProperty(*r, data);

            ModelNodeMetaObject *cache = e->objectCache();

            if (roleIndex != -1 && cache) {
                QVector<int> roles;
                roles << roleIndex;
                cache->updateValues(roles);
            }
        }
    }

    return roleIndex;
}

int ListModel::setExistingProperty(int elementIndex, const QString &key, const QV4::Value &data, QV4::ExecutionEngine *eng)
{
    int roleIndex = -1;

    if (elementIndex >= 0 && elementIndex < elements.count()) {
        ListElement *e = elements[elementIndex];
        const ListLayout::Role *r = m_layout->getExistingRole(key);
        if (r)
            roleIndex = e->setJsProperty(*r, data, eng);
    }

    return roleIndex;
}

inline char *ListElement::getPropertyMemory(const ListLayout::Role &role)
{
    ListElement *e = this;
    int blockIndex = 0;
    while (blockIndex < role.blockIndex) {
        if (e->next == 0) {
            e->next = new ListElement;
            e->next->uid = uid;
        }
        e = e->next;
        ++blockIndex;
    }

    char *mem = &e->data[role.blockOffset];
    return mem;
}

ModelNodeMetaObject *ListElement::objectCache()
{
    if (!m_objectCache)
        return 0;
    return ModelNodeMetaObject::get(m_objectCache);
}

QString *ListElement::getStringProperty(const ListLayout::Role &role)
{
    char *mem = getPropertyMemory(role);
    QString *s = reinterpret_cast<QString *>(mem);
    return s->data_ptr() ? s : 0;
}

QObject *ListElement::getQObjectProperty(const ListLayout::Role &role)
{
    char *mem = getPropertyMemory(role);
    QPointer<QObject> *o = reinterpret_cast<QPointer<QObject> *>(mem);
    return o->data();
}

QVariantMap *ListElement::getVariantMapProperty(const ListLayout::Role &role)
{
    QVariantMap *map = 0;

    char *mem = getPropertyMemory(role);
    if (isMemoryUsed<QVariantMap>(mem))
        map = reinterpret_cast<QVariantMap *>(mem);

    return map;
}

QDateTime *ListElement::getDateTimeProperty(const ListLayout::Role &role)
{
    QDateTime *dt = 0;

    char *mem = getPropertyMemory(role);
    if (isMemoryUsed<QDateTime>(mem))
        dt = reinterpret_cast<QDateTime *>(mem);

    return dt;
}

QPointer<QObject> *ListElement::getGuardProperty(const ListLayout::Role &role)
{
    char *mem = getPropertyMemory(role);

    bool existingGuard = false;
    for (size_t i=0 ; i < sizeof(QPointer<QObject>) ; ++i) {
        if (mem[i] != 0) {
            existingGuard = true;
            break;
        }
    }

    QPointer<QObject> *o = 0;

    if (existingGuard)
        o = reinterpret_cast<QPointer<QObject> *>(mem);

    return o;
}

ListModel *ListElement::getListProperty(const ListLayout::Role &role)
{
    char *mem = getPropertyMemory(role);
    ListModel **value = reinterpret_cast<ListModel **>(mem);
    return *value;
}

QVariant ListElement::getProperty(const ListLayout::Role &role, const QQmlListModel *owner, QV4::ExecutionEngine *eng)
{
    char *mem = getPropertyMemory(role);

    QVariant data;

    switch (role.type) {
        case ListLayout::Role::Number:
            {
                double *value = reinterpret_cast<double *>(mem);
                data = *value;
            }
            break;
        case ListLayout::Role::String:
            {
                QString *value = reinterpret_cast<QString *>(mem);
                if (value->data_ptr() != 0)
                    data = *value;
            }
            break;
        case ListLayout::Role::Bool:
            {
                bool *value = reinterpret_cast<bool *>(mem);
                data = *value;
            }
            break;
        case ListLayout::Role::List:
            {
                ListModel **value = reinterpret_cast<ListModel **>(mem);
                ListModel *model = *value;

                if (model) {
                    if (model->m_modelCache == 0) {
                        model->m_modelCache = new QQmlListModel(owner, model, eng);
                        QQmlEngine::setContextForObject(model->m_modelCache, QQmlEngine::contextForObject(owner));
                    }

                    QObject *object = model->m_modelCache;
                    data = QVariant::fromValue(object);
                }
            }
            break;
        case ListLayout::Role::QObject:
            {
                QPointer<QObject> *guard = reinterpret_cast<QPointer<QObject> *>(mem);
                QObject *object = guard->data();
                if (object)
                    data = QVariant::fromValue(object);
            }
            break;
        case ListLayout::Role::VariantMap:
            {
                if (isMemoryUsed<QVariantMap>(mem)) {
                    QVariantMap *map = reinterpret_cast<QVariantMap *>(mem);
                    data = *map;
                }
            }
            break;
        case ListLayout::Role::DateTime:
            {
                if (isMemoryUsed<QDateTime>(mem)) {
                    QDateTime *dt = reinterpret_cast<QDateTime *>(mem);
                    data = *dt;
                }
            }
            break;
        default:
            break;
    }

    return data;
}

int ListElement::setStringProperty(const ListLayout::Role &role, const QString &s)
{
    int roleIndex = -1;

    if (role.type == ListLayout::Role::String) {
        char *mem = getPropertyMemory(role);
        QString *c = reinterpret_cast<QString *>(mem);
        bool changed;
        if (c->data_ptr() == 0) {
            new (mem) QString(s);
            changed = true;
        } else {
            changed = c->compare(s) != 0;
            *c = s;
        }
        if (changed)
            roleIndex = role.index;
    }

    return roleIndex;
}

int ListElement::setDoubleProperty(const ListLayout::Role &role, double d)
{
    int roleIndex = -1;

    if (role.type == ListLayout::Role::Number) {
        char *mem = getPropertyMemory(role);
        double *value = reinterpret_cast<double *>(mem);
        bool changed = *value != d;
        *value = d;
        if (changed)
            roleIndex = role.index;
    }

    return roleIndex;
}

int ListElement::setBoolProperty(const ListLayout::Role &role, bool b)
{
    int roleIndex = -1;

    if (role.type == ListLayout::Role::Bool) {
        char *mem = getPropertyMemory(role);
        bool *value = reinterpret_cast<bool *>(mem);
        bool changed = *value != b;
        *value = b;
        if (changed)
            roleIndex = role.index;
    }

    return roleIndex;
}

int ListElement::setListProperty(const ListLayout::Role &role, ListModel *m)
{
    int roleIndex = -1;

    if (role.type == ListLayout::Role::List) {
        char *mem = getPropertyMemory(role);
        ListModel **value = reinterpret_cast<ListModel **>(mem);
        if (*value && *value != m) {
            (*value)->destroy();
            delete *value;
        }
        *value = m;
        roleIndex = role.index;
    }

    return roleIndex;
}

int ListElement::setQObjectProperty(const ListLayout::Role &role, QObject *o)
{
    int roleIndex = -1;

    if (role.type == ListLayout::Role::QObject) {
        char *mem = getPropertyMemory(role);
        QPointer<QObject> *g = reinterpret_cast<QPointer<QObject> *>(mem);
        bool existingGuard = false;
        for (size_t i=0 ; i < sizeof(QPointer<QObject>) ; ++i) {
            if (mem[i] != 0) {
                existingGuard = true;
                break;
            }
        }
        bool changed;
        if (existingGuard) {
            changed = g->data() != o;
            g->~QPointer();
        } else {
            changed = true;
        }
        new (mem) QPointer<QObject>(o);
        if (changed)
            roleIndex = role.index;
    }

    return roleIndex;
}

int ListElement::setVariantMapProperty(const ListLayout::Role &role, QV4::Object *o)
{
    int roleIndex = -1;

    if (role.type == ListLayout::Role::VariantMap) {
        char *mem = getPropertyMemory(role);
        if (isMemoryUsed<QVariantMap>(mem)) {
            QVariantMap *map = reinterpret_cast<QVariantMap *>(mem);
            map->~QMap();
        }
        new (mem) QVariantMap(o->engine()->variantMapFromJS(o));
        roleIndex = role.index;
    }

    return roleIndex;
}

int ListElement::setVariantMapProperty(const ListLayout::Role &role, QVariantMap *m)
{
    int roleIndex = -1;

    if (role.type == ListLayout::Role::VariantMap) {
        char *mem = getPropertyMemory(role);
        if (isMemoryUsed<QVariantMap>(mem)) {
            QVariantMap *map = reinterpret_cast<QVariantMap *>(mem);
            map->~QMap();
        }
        if (m)
            new (mem) QVariantMap(*m);
        else
            new (mem) QVariantMap;
        roleIndex = role.index;
    }

    return roleIndex;
}

int ListElement::setDateTimeProperty(const ListLayout::Role &role, const QDateTime &dt)
{
    int roleIndex = -1;

    if (role.type == ListLayout::Role::DateTime) {
        char *mem = getPropertyMemory(role);
        if (isMemoryUsed<QDateTime>(mem)) {
            QDateTime *dt = reinterpret_cast<QDateTime *>(mem);
            dt->~QDateTime();
        }
        new (mem) QDateTime(dt);
        roleIndex = role.index;
    }

    return roleIndex;
}

void ListElement::setStringPropertyFast(const ListLayout::Role &role, const QString &s)
{
    char *mem = getPropertyMemory(role);
    new (mem) QString(s);
}

void ListElement::setDoublePropertyFast(const ListLayout::Role &role, double d)
{
    char *mem = getPropertyMemory(role);
    double *value = new (mem) double;
    *value = d;
}

void ListElement::setBoolPropertyFast(const ListLayout::Role &role, bool b)
{
    char *mem = getPropertyMemory(role);
    bool *value = new (mem) bool;
    *value = b;
}

void ListElement::setQObjectPropertyFast(const ListLayout::Role &role, QObject *o)
{
    char *mem = getPropertyMemory(role);
    new (mem) QPointer<QObject>(o);
}

void ListElement::setListPropertyFast(const ListLayout::Role &role, ListModel *m)
{
    char *mem = getPropertyMemory(role);
    ListModel **value = new (mem) ListModel *;
    *value = m;
}

void ListElement::setVariantMapFast(const ListLayout::Role &role, QV4::Object *o)
{
    char *mem = getPropertyMemory(role);
    QVariantMap *map = new (mem) QVariantMap;
    *map = o->engine()->variantMapFromJS(o);
}

void ListElement::setDateTimePropertyFast(const ListLayout::Role &role, const QDateTime &dt)
{
    char *mem = getPropertyMemory(role);
    new (mem) QDateTime(dt);
}

void ListElement::clearProperty(const ListLayout::Role &role)
{
    switch (role.type) {
    case ListLayout::Role::String:
        setStringProperty(role, QString());
        break;
    case ListLayout::Role::Number:
        setDoubleProperty(role, 0.0);
        break;
    case ListLayout::Role::Bool:
        setBoolProperty(role, false);
        break;
    case ListLayout::Role::List:
        setListProperty(role, 0);
        break;
    case ListLayout::Role::QObject:
        setQObjectProperty(role, 0);
        break;
    case ListLayout::Role::DateTime:
        setDateTimeProperty(role, QDateTime());
        break;
    case ListLayout::Role::VariantMap:
        setVariantMapProperty(role, (QVariantMap *)0);
        break;
    default:
        break;
    }
}

ListElement::ListElement()
{
    m_objectCache = 0;
    uid = uidCounter.fetchAndAddOrdered(1);
    next = 0;
    memset(data, 0, sizeof(data));
}

ListElement::ListElement(int existingUid)
{
    m_objectCache = 0;
    uid = existingUid;
    next = 0;
    memset(data, 0, sizeof(data));
}

ListElement::~ListElement()
{
    delete next;
}

void ListElement::sync(ListElement *src, ListLayout *srcLayout, ListElement *target, ListLayout *targetLayout, QHash<int, ListModel *> *targetModelHash)
{
    for (int i=0 ; i < srcLayout->roleCount() ; ++i) {
        const ListLayout::Role &srcRole = srcLayout->getExistingRole(i);
        const ListLayout::Role &targetRole = targetLayout->getExistingRole(i);

        switch (srcRole.type) {
            case ListLayout::Role::List:
                {
                    ListModel *srcSubModel = src->getListProperty(srcRole);
                    ListModel *targetSubModel = target->getListProperty(targetRole);

                    if (srcSubModel) {
                        if (targetSubModel == 0) {
                            targetSubModel = new ListModel(targetRole.subLayout, 0, srcSubModel->getUid());
                            target->setListPropertyFast(targetRole, targetSubModel);
                        }
                        ListModel::sync(srcSubModel, targetSubModel, targetModelHash);
                    }
                }
                break;
            case ListLayout::Role::QObject:
                {
                    QObject *object = src->getQObjectProperty(srcRole);
                    target->setQObjectProperty(targetRole, object);
                }
                break;
            case ListLayout::Role::String:
            case ListLayout::Role::Number:
            case ListLayout::Role::Bool:
            case ListLayout::Role::DateTime:
                {
                    QVariant v = src->getProperty(srcRole, 0, 0);
                    target->setVariantProperty(targetRole, v);
                }
            case ListLayout::Role::VariantMap:
                {
                    QVariantMap *map = src->getVariantMapProperty(srcRole);
                    target->setVariantMapProperty(targetRole, map);
                }
                break;
            default:
                break;
        }
    }

}

void ListElement::destroy(ListLayout *layout)
{
    if (layout) {
        for (int i=0 ; i < layout->roleCount() ; ++i) {
            const ListLayout::Role &r = layout->getExistingRole(i);

            switch (r.type) {
                case ListLayout::Role::String:
                    {
                        QString *string = getStringProperty(r);
                        if (string)
                            string->~QString();
                    }
                    break;
                case ListLayout::Role::List:
                    {
                        ListModel *model = getListProperty(r);
                        if (model) {
                            model->destroy();
                            delete model;
                        }
                    }
                    break;
                case ListLayout::Role::QObject:
                    {
                        QPointer<QObject> *guard = getGuardProperty(r);
                        if (guard)
                            guard->~QPointer();
                    }
                    break;
                case ListLayout::Role::VariantMap:
                    {
                        QVariantMap *map = getVariantMapProperty(r);
                        if (map)
                            map->~QMap();
                    }
                    break;
                case ListLayout::Role::DateTime:
                    {
                        QDateTime *dt = getDateTimeProperty(r);
                        if (dt)
                            dt->~QDateTime();
                    }
                    break;
                default:
                    // other types don't need explicit cleanup.
                    break;
            }
        }

        delete m_objectCache;
    }

    if (next)
        next->destroy(0);
    uid = -1;
}

int ListElement::setVariantProperty(const ListLayout::Role &role, const QVariant &d)
{
    int roleIndex = -1;

    switch (role.type) {
        case ListLayout::Role::Number:
            roleIndex = setDoubleProperty(role, d.toDouble());
            break;
        case ListLayout::Role::String:
            roleIndex = setStringProperty(role, d.toString());
            break;
        case ListLayout::Role::Bool:
            roleIndex = setBoolProperty(role, d.toBool());
            break;
        case ListLayout::Role::List:
            roleIndex = setListProperty(role, d.value<ListModel *>());
            break;
        case ListLayout::Role::VariantMap: {
                QVariantMap map = d.toMap();
                roleIndex = setVariantMapProperty(role, &map);
            }
            break;
        case ListLayout::Role::DateTime:
            roleIndex = setDateTimeProperty(role, d.toDateTime());
            break;
        default:
            break;
    }

    return roleIndex;
}

int ListElement::setJsProperty(const ListLayout::Role &role, const QV4::Value &d, QV4::ExecutionEngine *eng)
{
    // Check if this key exists yet
    int roleIndex = -1;

    QV4::Scope scope(eng);

    // Add the value now
    if (d.isString()) {
        QString qstr = d.toQString();
        roleIndex = setStringProperty(role, qstr);
    } else if (d.isNumber()) {
        roleIndex = setDoubleProperty(role, d.asDouble());
    } else if (d.as<QV4::ArrayObject>()) {
        QV4::ScopedArrayObject a(scope, d);
        if (role.type == ListLayout::Role::List) {
            QV4::Scope scope(a->engine());
            QV4::ScopedObject o(scope);

            ListModel *subModel = new ListModel(role.subLayout, 0, -1);
            int arrayLength = a->getLength();
            for (int j=0 ; j < arrayLength ; ++j) {
                o = a->getIndexed(j);
                subModel->append(o);
            }
            roleIndex = setListProperty(role, subModel);
        } else {
            qmlInfo(0) << QStringLiteral("Can't assign to existing role '%1' of different type [%2 -> %3]").arg(role.name).arg(roleTypeName(role.type)).arg(roleTypeName(ListLayout::Role::List));
        }
    } else if (d.isBoolean()) {
        roleIndex = setBoolProperty(role, d.booleanValue());
    } else if (d.as<QV4::DateObject>()) {
        QV4::Scoped<QV4::DateObject> dd(scope, d);
        QDateTime dt = dd->toQDateTime();
        roleIndex = setDateTimeProperty(role, dt);
    } else if (d.isObject()) {
        QV4::ScopedObject o(scope, d);
        QV4::QObjectWrapper *wrapper = o->as<QV4::QObjectWrapper>();
        if (role.type == ListLayout::Role::QObject && wrapper) {
            QObject *o = wrapper->object();
            roleIndex = setQObjectProperty(role, o);
        } else if (role.type == ListLayout::Role::VariantMap) {
            roleIndex = setVariantMapProperty(role, o);
        }
    } else if (d.isNullOrUndefined()) {
        clearProperty(role);
    }

    return roleIndex;
}

ModelNodeMetaObject::ModelNodeMetaObject(QObject *object, QQmlListModel *model, int elementIndex)
: QQmlOpenMetaObject(object), m_enabled(false), m_model(model), m_elementIndex(elementIndex), m_initialized(false)
{}

void ModelNodeMetaObject::initialize()
{
    const int roleCount = m_model->m_listModel->roleCount();
    QVector<QByteArray> properties;
    properties.reserve(roleCount);
    for (int i = 0 ; i < roleCount ; ++i) {
        const ListLayout::Role &role = m_model->m_listModel->getExistingRole(i);
        QByteArray name = role.name.toUtf8();
        properties << name;
    }
    type()->createProperties(properties);
    updateValues();
    m_enabled = true;
}

ModelNodeMetaObject::~ModelNodeMetaObject()
{
}

QAbstractDynamicMetaObject *ModelNodeMetaObject::toDynamicMetaObject(QObject *object)
{
    if (!m_initialized) {
        m_initialized = true;
        initialize();
    }
    return QQmlOpenMetaObject::toDynamicMetaObject(object);
}

ModelNodeMetaObject *ModelNodeMetaObject::get(QObject *obj)
{
    QObjectPrivate *op = QObjectPrivate::get(obj);
    return static_cast<ModelNodeMetaObject*>(op->metaObject);
}

void ModelNodeMetaObject::updateValues()
{
    const int roleCount = m_model->m_listModel->roleCount();
    if (!m_initialized) {
        int *changedRoles = reinterpret_cast<int *>(alloca(roleCount * sizeof(int)));
        for (int i = 0; i < roleCount; ++i)
            changedRoles[i] = i;
        emitDirectNotifies(changedRoles, roleCount);
        return;
    }
    for (int i=0 ; i < roleCount ; ++i) {
        const ListLayout::Role &role = m_model->m_listModel->getExistingRole(i);
        QByteArray name = role.name.toUtf8();
        const QVariant &data = m_model->data(m_elementIndex, i);
        setValue(name, data, role.type == ListLayout::Role::List);
    }
}

void ModelNodeMetaObject::updateValues(const QVector<int> &roles)
{
    if (!m_initialized) {
        emitDirectNotifies(roles.constData(), roles.count());
        return;
    }
    int roleCount = roles.count();
    for (int i=0 ; i < roleCount ; ++i) {
        int roleIndex = roles.at(i);
        const ListLayout::Role &role = m_model->m_listModel->getExistingRole(roleIndex);
        QByteArray name = role.name.toUtf8();
        const QVariant &data = m_model->data(m_elementIndex, roleIndex);
        setValue(name, data, role.type == ListLayout::Role::List);
    }
}

void ModelNodeMetaObject::propertyWritten(int index)
{
    if (!m_enabled)
        return;

    QString propName = QString::fromUtf8(name(index));
    QVariant value = operator[](index);

    QV4::Scope scope(m_model->engine());
    QV4::ScopedValue v(scope, scope.engine->fromVariant(value));

    int roleIndex = m_model->m_listModel->setExistingProperty(m_elementIndex, propName, v, scope.engine);
    if (roleIndex != -1) {
        QVector<int> roles;
        roles << roleIndex;
        m_model->emitItemsChanged(m_elementIndex, 1, roles);
    }
}

// Does the emission of the notifiers when we haven't created the meta-object yet
void ModelNodeMetaObject::emitDirectNotifies(const int *changedRoles, int roleCount)
{
    Q_ASSERT(!m_initialized);
    QQmlData *ddata = QQmlData::get(object(), /*create*/false);
    if (!ddata)
        return;
    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(qmlEngine(m_model));
    if (!ep)
        return;
    for (int i = 0; i < roleCount; ++i) {
        const int changedRole = changedRoles[i];
        QQmlNotifier::notify(ddata, changedRole);
    }
}

namespace QV4 {

void ModelObject::put(Managed *m, String *name, const Value &value)
{
    ModelObject *that = static_cast<ModelObject*>(m);

    ExecutionEngine *eng = that->engine();
    const int elementIndex = that->d()->m_elementIndex;
    const QString propName = name->toQString();
    int roleIndex = that->d()->m_model->m_listModel->setExistingProperty(elementIndex, propName, value, eng);
    if (roleIndex != -1) {
        QVector<int> roles;
        roles << roleIndex;
        that->d()->m_model->emitItemsChanged(elementIndex, 1, roles);
    }

    ModelNodeMetaObject *mo = ModelNodeMetaObject::get(that->object());
    if (mo->initialized())
        mo->emitPropertyNotification(name->toQString().toUtf8());
}

ReturnedValue ModelObject::get(const Managed *m, String *name, bool *hasProperty)
{
    const ModelObject *that = static_cast<const ModelObject*>(m);
    const ListLayout::Role *role = that->d()->m_model->m_listModel->getExistingRole(name);
    if (!role)
        return QObjectWrapper::get(m, name, hasProperty);
    if (hasProperty)
        *hasProperty = true;

    if (QQmlEngine *qmlEngine = that->engine()->qmlEngine()) {
        QQmlEnginePrivate *ep = QQmlEnginePrivate::get(qmlEngine);
        if (ep && ep->propertyCapture) {
            QObjectPrivate *op = QObjectPrivate::get(that->object());
            // Temporarily hide the dynamic meta-object, to prevent it from being created when the capture
            // triggers a QObject::connectNotify() by calling obj->metaObject().
            QScopedValueRollback<QDynamicMetaObjectData*> metaObjectBlocker(op->metaObject, 0);
            ep->propertyCapture->captureProperty(that->object(), -1, role->index);
        }
    }

    const int elementIndex = that->d()->m_elementIndex;
    QVariant value = that->d()->m_model->data(elementIndex, role->index);
    return that->engine()->fromVariant(value);
}

void ModelObject::advanceIterator(Managed *m, ObjectIterator *it, Value *name, uint *index, Property *p, PropertyAttributes *attributes)
{
    ModelObject *that = static_cast<ModelObject*>(m);
    ExecutionEngine *v4 = that->engine();
    name->setM(0);
    *index = UINT_MAX;
    if (it->arrayIndex < uint(that->d()->m_model->m_listModel->roleCount())) {
        Scope scope(that->engine());
        const ListLayout::Role &role = that->d()->m_model->m_listModel->getExistingRole(it->arrayIndex);
        ++it->arrayIndex;
        ScopedString roleName(scope, v4->newString(role.name));
        name->setM(roleName->d());
        *attributes = QV4::Attr_Data;
        QVariant value = that->d()->m_model->data(that->d()->m_elementIndex, role.index);
        p->value = v4->fromVariant(value);
        return;
    }
    QV4::QObjectWrapper::advanceIterator(m, it, name, index, p, attributes);
}

DEFINE_OBJECT_VTABLE(ModelObject);

} // namespace QV4

DynamicRoleModelNode::DynamicRoleModelNode(QQmlListModel *owner, int uid) : m_owner(owner), m_uid(uid), m_meta(new DynamicRoleModelNodeMetaObject(this))
{
    setNodeUpdatesEnabled(true);
}

DynamicRoleModelNode *DynamicRoleModelNode::create(const QVariantMap &obj, QQmlListModel *owner)
{
    DynamicRoleModelNode *object = new DynamicRoleModelNode(owner, uidCounter.fetchAndAddOrdered(1));
    QVector<int> roles;
    object->updateValues(obj, roles);
    return object;
}

void DynamicRoleModelNode::sync(DynamicRoleModelNode *src, DynamicRoleModelNode *target, QHash<int, QQmlListModel *> *targetModelHash)
{
    for (int i=0 ; i < src->m_meta->count() ; ++i) {
        const QByteArray &name = src->m_meta->name(i);
        QVariant value = src->m_meta->value(i);

        QQmlListModel *srcModel = qobject_cast<QQmlListModel *>(value.value<QObject *>());
        QQmlListModel *targetModel = qobject_cast<QQmlListModel *>(target->m_meta->value(i).value<QObject *>());

        if (srcModel) {
            if (targetModel == 0)
                targetModel = QQmlListModel::createWithOwner(target->m_owner);

            QQmlListModel::sync(srcModel, targetModel, targetModelHash);

            QObject *targetModelObject = targetModel;
            value = QVariant::fromValue(targetModelObject);
        } else if (targetModel) {
            delete targetModel;
        }

        target->setValue(name, value);
    }
}

void DynamicRoleModelNode::updateValues(const QVariantMap &object, QVector<int> &roles)
{
    for (auto it = object.cbegin(), end = object.cend(); it != end; ++it) {
        const QString &key = it.key();

        int roleIndex = m_owner->m_roles.indexOf(key);
        if (roleIndex == -1) {
            roleIndex = m_owner->m_roles.count();
            m_owner->m_roles.append(key);
        }

        QVariant value = it.value();

        // A JS array/object is translated into a (hierarchical) QQmlListModel,
        // so translate to a variant map/list first with toVariant().
        if (value.userType() == qMetaTypeId<QJSValue>())
            value = value.value<QJSValue>().toVariant();

        if (value.type() == QVariant::List) {
            QQmlListModel *subModel = QQmlListModel::createWithOwner(m_owner);

            QVariantList subArray = value.toList();
            QVariantList::const_iterator subIt = subArray.cbegin();
            QVariantList::const_iterator subEnd = subArray.cend();
            while (subIt != subEnd) {
                const QVariantMap &subObject = subIt->toMap();
                subModel->m_modelObjects.append(DynamicRoleModelNode::create(subObject, subModel));
                ++subIt;
            }

            QObject *subModelObject = subModel;
            value = QVariant::fromValue(subModelObject);
        }

        const QByteArray &keyUtf8 = key.toUtf8();

        QQmlListModel *existingModel = qobject_cast<QQmlListModel *>(m_meta->value(keyUtf8).value<QObject *>());
        delete existingModel;

        if (m_meta->setValue(keyUtf8, value))
            roles << roleIndex;
    }
}

DynamicRoleModelNodeMetaObject::DynamicRoleModelNodeMetaObject(DynamicRoleModelNode *object)
    : QQmlOpenMetaObject(object), m_enabled(false), m_owner(object)
{
}

DynamicRoleModelNodeMetaObject::~DynamicRoleModelNodeMetaObject()
{
    for (int i=0 ; i < count() ; ++i) {
        QQmlListModel *subModel = qobject_cast<QQmlListModel *>(value(i).value<QObject *>());
        delete subModel;
    }
}

void DynamicRoleModelNodeMetaObject::propertyWrite(int index)
{
    if (!m_enabled)
        return;

    QVariant v = value(index);
    QQmlListModel *model = qobject_cast<QQmlListModel *>(v.value<QObject *>());
    delete model;
}

void DynamicRoleModelNodeMetaObject::propertyWritten(int index)
{
    if (!m_enabled)
        return;

    QQmlListModel *parentModel = m_owner->m_owner;

    QVariant v = value(index);

    // A JS array/object is translated into a (hierarchical) QQmlListModel,
    // so translate to a variant map/list first with toVariant().
    if (v.userType() == qMetaTypeId<QJSValue>())
        v= v.value<QJSValue>().toVariant();

    if (v.type() == QVariant::List) {
        QQmlListModel *subModel = QQmlListModel::createWithOwner(parentModel);

        QVariantList subArray = v.toList();
        QVariantList::const_iterator subIt = subArray.cbegin();
        QVariantList::const_iterator subEnd = subArray.cend();
        while (subIt != subEnd) {
            const QVariantMap &subObject = subIt->toMap();
            subModel->m_modelObjects.append(DynamicRoleModelNode::create(subObject, subModel));
            ++subIt;
        }

        QObject *subModelObject = subModel;
        v = QVariant::fromValue(subModelObject);

        setValue(index, v);
    }

    int elementIndex = parentModel->m_modelObjects.indexOf(m_owner);
    int roleIndex = parentModel->m_roles.indexOf(QString::fromLatin1(name(index).constData()));

    if (elementIndex != -1 && roleIndex != -1) {

        QVector<int> roles;
        roles << roleIndex;

        parentModel->emitItemsChanged(elementIndex, 1, roles);
    }
}

/*!
    \qmltype ListModel
    \instantiates QQmlListModel
    \inqmlmodule QtQml.Models
    \ingroup qtquick-models
    \brief Defines a free-form list data source

    The ListModel is a simple container of ListElement definitions, each
    containing data roles. The contents can be defined dynamically, or
    explicitly in QML.

    The number of elements in the model can be obtained from its \l count property.
    A number of familiar methods are also provided to manipulate the contents of the
    model, including append(), insert(), move(), remove() and set(). These methods
    accept dictionaries as their arguments; these are translated to ListElement objects
    by the model.

    Elements can be manipulated via the model using the setProperty() method, which
    allows the roles of the specified element to be set and changed.

    \section1 Example Usage

    The following example shows a ListModel containing three elements, with the roles
    "name" and "cost".

    \div {class="float-right"}
    \inlineimage listmodel.png
    \enddiv

    \snippet qml/listmodel/listmodel.qml 0

    Roles (properties) in each element must begin with a lower-case letter and
    should be common to all elements in a model. The ListElement documentation
    provides more guidelines for how elements should be defined.

    Since the example model contains an \c id property, it can be referenced
    by views, such as the ListView in this example:

    \snippet qml/listmodel/listmodel-simple.qml 0
    \dots 8
    \snippet qml/listmodel/listmodel-simple.qml 1

    It is possible for roles to contain list data.  In the following example we
    create a list of fruit attributes:

    \snippet qml/listmodel/listmodel-nested.qml model

    The delegate displays all the fruit attributes:

    \div {class="float-right"}
    \inlineimage listmodel-nested.png
    \enddiv

    \snippet qml/listmodel/listmodel-nested.qml delegate

    \section1 Modifying List Models

    The content of a ListModel may be created and modified using the clear(),
    append(), set(), insert() and setProperty() methods.  For example:

    \snippet qml/listmodel/listmodel-modify.qml delegate

    Note that when creating content dynamically the set of available properties
    cannot be changed once set. Whatever properties are first added to the model
    are the only permitted properties in the model.

    \section1 Using Threaded List Models with WorkerScript

    ListModel can be used together with WorkerScript access a list model
    from multiple threads. This is useful if list modifications are
    synchronous and take some time: the list operations can be moved to a
    different thread to avoid blocking of the main GUI thread.

    Here is an example that uses WorkerScript to periodically append the
    current time to a list model:

    \snippet ../quick/threading/threadedlistmodel/timedisplay.qml 0

    The included file, \tt dataloader.js, looks like this:

    \snippet ../quick/threading/threadedlistmodel/dataloader.js 0

    The timer in the main example sends messages to the worker script by calling
    \l WorkerScript::sendMessage(). When this message is received,
    \c WorkerScript.onMessage() is invoked in \c dataloader.js,
    which appends the current time to the list model.

    Note the call to sync() from the external thread.
    You must call sync() or else the changes made to the list from that
    thread will not be reflected in the list model in the main thread.

    \sa {qml-data-models}{Data Models}, {Qt Quick Examples - Threading}, {Qt QML}
*/

QQmlListModel::QQmlListModel(QObject *parent)
: QAbstractListModel(parent)
{
    m_mainThread = true;
    m_primary = true;
    m_agent = 0;
    m_uid = uidCounter.fetchAndAddOrdered(1);
    m_dynamicRoles = false;

    m_layout = new ListLayout;
    m_listModel = new ListModel(m_layout, this, -1);

    m_engine = 0;
}

QQmlListModel::QQmlListModel(const QQmlListModel *owner, ListModel *data, QV4::ExecutionEngine *engine, QObject *parent)
: QAbstractListModel(parent)
{
    m_mainThread = owner->m_mainThread;
    m_primary = false;
    m_agent = owner->m_agent;

    Q_ASSERT(owner->m_dynamicRoles == false);
    m_dynamicRoles = false;
    m_layout = 0;
    m_listModel = data;

    m_engine = engine;
}

QQmlListModel::QQmlListModel(QQmlListModel *orig, QQmlListModelWorkerAgent *agent)
: QAbstractListModel(agent)
{
    m_mainThread = false;
    m_primary = true;
    m_agent = agent;
    m_dynamicRoles = orig->m_dynamicRoles;

    m_layout = new ListLayout(orig->m_layout);
    m_listModel = new ListModel(m_layout, this, orig->m_listModel->getUid());

    if (m_dynamicRoles)
        sync(orig, this, 0);
    else
        ListModel::sync(orig->m_listModel, m_listModel, 0);

    m_engine = 0;
}

QQmlListModel::~QQmlListModel()
{
    qDeleteAll(m_modelObjects);

    if (m_primary) {
        m_listModel->destroy();
        delete m_listModel;

        if (m_mainThread && m_agent) {
            m_agent->modelDestroyed();
            m_agent->release();
        }
    }

    m_listModel = 0;

    delete m_layout;
    m_layout = 0;
}

QQmlListModel *QQmlListModel::createWithOwner(QQmlListModel *newOwner)
{
    QQmlListModel *model = new QQmlListModel;

    model->m_mainThread = newOwner->m_mainThread;
    model->m_engine = newOwner->m_engine;
    model->m_agent = newOwner->m_agent;
    model->m_dynamicRoles = newOwner->m_dynamicRoles;

    if (model->m_mainThread && model->m_agent)
        model->m_agent->addref();

    QQmlEngine::setContextForObject(model, QQmlEngine::contextForObject(newOwner));

    return model;
}

QV4::ExecutionEngine *QQmlListModel::engine() const
{
    if (m_engine == 0) {
        m_engine  = QQmlEnginePrivate::get(qmlEngine(this))->v4engine();
    }

    return m_engine;
}

void QQmlListModel::sync(QQmlListModel *src, QQmlListModel *target, QHash<int, QQmlListModel *> *targetModelHash)
{
    Q_ASSERT(src->m_dynamicRoles && target->m_dynamicRoles);

    target->m_uid = src->m_uid;
    if (targetModelHash)
        targetModelHash->insert(target->m_uid, target);
    target->m_roles = src->m_roles;

    // Build hash of elements <-> uid for each of the lists
    QHash<int, ElementSync> elementHash;
    for (int i=0 ; i < target->m_modelObjects.count() ; ++i) {
        DynamicRoleModelNode *e = target->m_modelObjects.at(i);
        int uid = e->getUid();
        ElementSync sync;
        sync.target = e;
        elementHash.insert(uid, sync);
    }
    for (int i=0 ; i < src->m_modelObjects.count() ; ++i) {
        DynamicRoleModelNode *e = src->m_modelObjects.at(i);
        int uid = e->getUid();

        QHash<int, ElementSync>::iterator it = elementHash.find(uid);
        if (it == elementHash.end()) {
            ElementSync sync;
            sync.src = e;
            elementHash.insert(uid, sync);
        } else {
            ElementSync &sync = it.value();
            sync.src = e;
        }
    }

    // Get list of elements that are in the target but no longer in the source. These get deleted first.
    QHash<int, ElementSync>::iterator it = elementHash.begin();
    QHash<int, ElementSync>::iterator end = elementHash.end();
    while (it != end) {
        const ElementSync &s = it.value();
        if (s.src == 0) {
            int targetIndex = target->m_modelObjects.indexOf(s.target);
            target->m_modelObjects.remove(targetIndex, 1);
            delete s.target;
        }
        ++it;
    }

    // Clear the target list, and append in correct order from the source
    target->m_modelObjects.clear();
    for (int i=0 ; i < src->m_modelObjects.count() ; ++i) {
        DynamicRoleModelNode *srcElement = src->m_modelObjects.at(i);
        it = elementHash.find(srcElement->getUid());
        const ElementSync &s = it.value();
        DynamicRoleModelNode *targetElement = s.target;
        if (targetElement == 0) {
            targetElement = new DynamicRoleModelNode(target, srcElement->getUid());
        }
        DynamicRoleModelNode::sync(srcElement, targetElement, targetModelHash);
        target->m_modelObjects.append(targetElement);
    }
}

void QQmlListModel::emitItemsChanged(int index, int count, const QVector<int> &roles)
{
    if (count <= 0)
        return;

    if (m_mainThread) {
        emit dataChanged(createIndex(index, 0), createIndex(index + count - 1, 0), roles);;
    } else {
        int uid = m_dynamicRoles ? getUid() : m_listModel->getUid();
        m_agent->data.changedChange(uid, index, count, roles);
    }
}

void QQmlListModel::emitItemsAboutToBeRemoved(int index, int count)
{
    if (count <= 0 || !m_mainThread)
        return;

    beginRemoveRows(QModelIndex(), index, index + count - 1);
}

void QQmlListModel::emitItemsRemoved(int index, int count)
{
    if (count <= 0)
        return;

    if (m_mainThread) {
            endRemoveRows();
            emit countChanged();
    } else {
        int uid = m_dynamicRoles ? getUid() : m_listModel->getUid();
        if (index == 0 && count == this->count())
            m_agent->data.clearChange(uid);
        m_agent->data.removeChange(uid, index, count);
    }
}

void QQmlListModel::emitItemsAboutToBeInserted(int index, int count)
{
    if (count <= 0 || !m_mainThread)
        return;

    beginInsertRows(QModelIndex(), index, index + count - 1);
}

void QQmlListModel::emitItemsInserted(int index, int count)
{
    if (count <= 0)
        return;

    if (m_mainThread) {
        endInsertRows();
        emit countChanged();
    } else {
        int uid = m_dynamicRoles ? getUid() : m_listModel->getUid();
        m_agent->data.insertChange(uid, index, count);
    }
}

void QQmlListModel::emitItemsAboutToBeMoved(int from, int to, int n)
{
    if (n <= 0 || !m_mainThread)
        return;

    beginMoveRows(QModelIndex(), from, from + n - 1, QModelIndex(), to > from ? to + n : to);
}

void QQmlListModel::emitItemsMoved(int from, int to, int n)
{
    if (n <= 0)
        return;

    if (m_mainThread) {
        endMoveRows();
    } else {
        int uid = m_dynamicRoles ? getUid() : m_listModel->getUid();
        m_agent->data.moveChange(uid, from, n, to);
    }
}

QQmlListModelWorkerAgent *QQmlListModel::agent()
{
    if (m_agent)
        return m_agent;

    m_agent = new QQmlListModelWorkerAgent(this);
    return m_agent;
}

QModelIndex QQmlListModel::index(int row, int column, const QModelIndex &parent) const
{
    return row >= 0 && row < count() && column == 0 && !parent.isValid()
            ? createIndex(row, column)
            : QModelIndex();
}

int QQmlListModel::rowCount(const QModelIndex &parent) const
{
    return !parent.isValid() ? count() : 0;
}

QVariant QQmlListModel::data(const QModelIndex &index, int role) const
{
    return data(index.row(), role);
}

bool QQmlListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    const int row = index.row();
    if (row >= count() || row < 0)
        return false;

    if (m_dynamicRoles) {
        const QByteArray property = m_roles.at(role).toUtf8();
        if (m_modelObjects[row]->setValue(property, value)) {
            emitItemsChanged(row, 1, QVector<int>() << role);
            return true;
        }
    } else {
        const ListLayout::Role &r = m_listModel->getExistingRole(role);
        const int roleIndex = m_listModel->setOrCreateProperty(row, r.name, value);
        if (roleIndex != -1) {
            emitItemsChanged(row, 1, QVector<int>() << role);
            return true;
        }
    }

    return false;
}

QVariant QQmlListModel::data(int index, int role) const
{
    QVariant v;

    if (index >= count() || index < 0)
        return v;

    if (m_dynamicRoles)
        v = m_modelObjects[index]->getValue(m_roles[role]);
    else
        v = m_listModel->getProperty(index, role, this, engine());

    return v;
}

QHash<int, QByteArray> QQmlListModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;

    if (m_dynamicRoles) {
        for (int i = 0 ; i < m_roles.count() ; ++i)
            roleNames.insert(i, m_roles.at(i).toUtf8());
    } else {
        for (int i = 0 ; i < m_listModel->roleCount() ; ++i) {
            const ListLayout::Role &r = m_listModel->getExistingRole(i);
            roleNames.insert(i, r.name.toUtf8());
        }
    }

    return roleNames;
}

/*!
    \qmlproperty bool ListModel::dynamicRoles

    By default, the type of a role is fixed the first time
    the role is used. For example, if you create a role called
    "data" and assign a number to it, you can no longer assign
    a string to the "data" role. However, when the dynamicRoles
    property is enabled, the type of a given role is not fixed
    and can be different between elements.

    The dynamicRoles property must be set before any data is
    added to the ListModel, and must be set from the main
    thread.

    A ListModel that has data statically defined (via the
    ListElement QML syntax) cannot have the dynamicRoles
    property enabled.

    There is a significant performance cost to using a
    ListModel with dynamic roles enabled. The cost varies
    from platform to platform but is typically somewhere
    between 4-6x slower than using static role types.

    Due to the performance cost of using dynamic roles,
    they are disabled by default.
*/
void QQmlListModel::setDynamicRoles(bool enableDynamicRoles)
{
    if (m_mainThread && m_agent == 0) {
        if (enableDynamicRoles) {
            if (m_layout->roleCount())
                qmlInfo(this) << tr("unable to enable dynamic roles as this model is not empty!");
            else
                m_dynamicRoles = true;
        } else {
            if (m_roles.count()) {
                qmlInfo(this) << tr("unable to enable static roles as this model is not empty!");
            } else {
                m_dynamicRoles = false;
            }
        }
    } else {
        qmlInfo(this) << tr("dynamic role setting must be made from the main thread, before any worker scripts are created");
    }
}

/*!
    \qmlproperty int ListModel::count
    The number of data entries in the model.
*/
int QQmlListModel::count() const
{
    return m_dynamicRoles ? m_modelObjects.count() : m_listModel->elementCount();
}

/*!
    \qmlmethod ListModel::clear()

    Deletes all content from the model.

    \sa append(), remove()
*/
void QQmlListModel::clear()
{
    const int cleared = count();

    emitItemsAboutToBeRemoved(0, cleared);

    if (m_dynamicRoles) {
        qDeleteAll(m_modelObjects);
        m_modelObjects.clear();
    } else {
        m_listModel->clear();
    }

    emitItemsRemoved(0, cleared);
}

/*!
    \qmlmethod ListModel::remove(int index, int count = 1)

    Deletes the content at \a index from the model.

    \sa clear()
*/
void QQmlListModel::remove(QQmlV4Function *args)
{
    int argLength = args->length();

    if (argLength == 1 || argLength == 2) {
        QV4::Scope scope(args->v4engine());
        int index = QV4::ScopedValue(scope, (*args)[0])->toInt32();
        int removeCount = (argLength == 2 ? QV4::ScopedValue(scope, (*args)[1])->toInt32() : 1);

        if (index < 0 || index+removeCount > count() || removeCount <= 0) {
            qmlInfo(this) << tr("remove: indices [%1 - %2] out of range [0 - %3]").arg(index).arg(index+removeCount).arg(count());
            return;
        }

        emitItemsAboutToBeRemoved(index, removeCount);

        if (m_dynamicRoles) {
            for (int i=0 ; i < removeCount ; ++i)
                delete m_modelObjects[index+i];
            m_modelObjects.remove(index, removeCount);
        } else {
            m_listModel->remove(index, removeCount);
        }

        emitItemsRemoved(index, removeCount);
    } else {
        qmlInfo(this) << tr("remove: incorrect number of arguments");
    }
}

/*!
    \qmlmethod ListModel::insert(int index, jsobject dict)

    Adds a new item to the list model at position \a index, with the
    values in \a dict.

    \code
        fruitModel.insert(2, {"cost": 5.95, "name":"Pizza"})
    \endcode

    The \a index must be to an existing item in the list, or one past
    the end of the list (equivalent to append).

    \sa set(), append()
*/

void QQmlListModel::insert(QQmlV4Function *args)
{
    if (args->length() == 2) {
        QV4::Scope scope(args->v4engine());
        QV4::ScopedValue arg0(scope, (*args)[0]);
        int index = arg0->toInt32();

        if (index < 0 || index > count()) {
            qmlInfo(this) << tr("insert: index %1 out of range").arg(index);
            return;
        }

        QV4::ScopedObject argObject(scope, (*args)[1]);
        QV4::ScopedArrayObject objectArray(scope, (*args)[1]);
        if (objectArray) {
            QV4::ScopedObject argObject(scope);

            int objectArrayLength = objectArray->getLength();
            emitItemsAboutToBeInserted(index, objectArrayLength);
            for (int i=0 ; i < objectArrayLength ; ++i) {
                argObject = objectArray->getIndexed(i);

                if (m_dynamicRoles) {
                    m_modelObjects.insert(index+i, DynamicRoleModelNode::create(scope.engine->variantMapFromJS(argObject), this));
                } else {
                    m_listModel->insert(index+i, argObject);
                }
            }
            emitItemsInserted(index, objectArrayLength);
        } else if (argObject) {
            emitItemsAboutToBeInserted(index, 1);

            if (m_dynamicRoles) {
                m_modelObjects.insert(index, DynamicRoleModelNode::create(scope.engine->variantMapFromJS(argObject), this));
            } else {
                m_listModel->insert(index, argObject);
            }

            emitItemsInserted(index, 1);
        } else {
            qmlInfo(this) << tr("insert: value is not an object");
        }
    } else {
        qmlInfo(this) << tr("insert: value is not an object");
    }
}

/*!
    \qmlmethod ListModel::move(int from, int to, int n)

    Moves \a n items \a from one position \a to another.

    The from and to ranges must exist; for example, to move the first 3 items
    to the end of the list:

    \code
        fruitModel.move(0, fruitModel.count - 3, 3)
    \endcode

    \sa append()
*/
void QQmlListModel::move(int from, int to, int n)
{
    if (n==0 || from==to)
        return;
    if (!canMove(from, to, n)) {
        qmlInfo(this) << tr("move: out of range");
        return;
    }

    emitItemsAboutToBeMoved(from, to, n);

    if (m_dynamicRoles) {

        int realFrom = from;
        int realTo = to;
        int realN = n;

        if (from > to) {
            // Only move forwards - flip if backwards moving
            int tfrom = from;
            int tto = to;
            realFrom = tto;
            realTo = tto+n;
            realN = tfrom-tto;
        }

        QPODVector<DynamicRoleModelNode *, 4> store;
        for (int i=0 ; i < (realTo-realFrom) ; ++i)
            store.append(m_modelObjects[realFrom+realN+i]);
        for (int i=0 ; i < realN ; ++i)
            store.append(m_modelObjects[realFrom+i]);
        for (int i=0 ; i < store.count() ; ++i)
            m_modelObjects[realFrom+i] = store[i];

    } else {
        m_listModel->move(from, to, n);
    }

    emitItemsMoved(from, to, n);
}

/*!
    \qmlmethod ListModel::append(jsobject dict)

    Adds a new item to the end of the list model, with the
    values in \a dict.

    \code
        fruitModel.append({"cost": 5.95, "name":"Pizza"})
    \endcode

    \sa set(), remove()
*/
void QQmlListModel::append(QQmlV4Function *args)
{
    if (args->length() == 1) {
        QV4::Scope scope(args->v4engine());
        QV4::ScopedObject argObject(scope, (*args)[0]);
        QV4::ScopedArrayObject objectArray(scope, (*args)[0]);

        if (objectArray) {
            QV4::ScopedObject argObject(scope);

            int objectArrayLength = objectArray->getLength();

            int index = count();
            emitItemsAboutToBeInserted(index, objectArrayLength);

            for (int i=0 ; i < objectArrayLength ; ++i) {
                argObject = objectArray->getIndexed(i);

                if (m_dynamicRoles) {
                    m_modelObjects.append(DynamicRoleModelNode::create(scope.engine->variantMapFromJS(argObject), this));
                } else {
                    m_listModel->append(argObject);
                }
            }

            emitItemsInserted(index, objectArrayLength);
        } else if (argObject) {
            int index;

            if (m_dynamicRoles) {
                index = m_modelObjects.count();
                emitItemsAboutToBeInserted(index, 1);
                m_modelObjects.append(DynamicRoleModelNode::create(scope.engine->variantMapFromJS(argObject), this));
            } else {
                index = m_listModel->elementCount();
                emitItemsAboutToBeInserted(index, 1);
                m_listModel->append(argObject);
            }

            emitItemsInserted(index, 1);
        } else {
            qmlInfo(this) << tr("append: value is not an object");
        }
    } else {
        qmlInfo(this) << tr("append: value is not an object");
    }
}

/*!
    \qmlmethod object ListModel::get(int index)

    Returns the item at \a index in the list model. This allows the item
    data to be accessed or modified from JavaScript:

    \code
    Component.onCompleted: {
        fruitModel.append({"cost": 5.95, "name":"Jackfruit"});
        console.log(fruitModel.get(0).cost);
        fruitModel.get(0).cost = 10.95;
    }
    \endcode

    The \a index must be an element in the list.

    Note that properties of the returned object that are themselves objects
    will also be models, and this get() method is used to access elements:

    \code
        fruitModel.append(..., "attributes":
            [{"name":"spikes","value":"7mm"},
             {"name":"color","value":"green"}]);
        fruitModel.get(0).attributes.get(1).value; // == "green"
    \endcode

    \warning The returned object is not guaranteed to remain valid. It
    should not be used in \l{Property Binding}{property bindings}.

    \sa append()
*/
QQmlV4Handle QQmlListModel::get(int index) const
{
    QV4::Scope scope(engine());
    QV4::ScopedValue result(scope, QV4::Primitive::undefinedValue());

    if (index >= 0 && index < count()) {

        if (m_dynamicRoles) {
            DynamicRoleModelNode *object = m_modelObjects[index];
            result = QV4::QObjectWrapper::wrap(scope.engine, object);
        } else {
            QObject *object = m_listModel->getOrCreateModelObject(const_cast<QQmlListModel *>(this), index);
            result = scope.engine->memoryManager->allocObject<QV4::ModelObject>(object, const_cast<QQmlListModel *>(this), index);
            // Keep track of the QObjectWrapper in persistent value storage
            QV4::Value *val = scope.engine->memoryManager->m_weakValues->allocate();
            *val = result;
        }
    }

    return QQmlV4Handle(result);
}

/*!
    \qmlmethod ListModel::set(int index, jsobject dict)

    Changes the item at \a index in the list model with the
    values in \a dict. Properties not appearing in \a dict
    are left unchanged.

    \code
        fruitModel.set(3, {"cost": 5.95, "name":"Pizza"})
    \endcode

    If \a index is equal to count() then a new item is appended to the
    list. Otherwise, \a index must be an element in the list.

    \sa append()
*/
void QQmlListModel::set(int index, const QQmlV4Handle &handle)
{
    QV4::Scope scope(engine());
    QV4::ScopedObject object(scope, handle);

    if (!object) {
        qmlInfo(this) << tr("set: value is not an object");
        return;
    }
    if (index > count() || index < 0) {
        qmlInfo(this) << tr("set: index %1 out of range").arg(index);
        return;
    }


    if (index == count()) {
        emitItemsAboutToBeInserted(index, 1);

        if (m_dynamicRoles) {
            m_modelObjects.append(DynamicRoleModelNode::create(scope.engine->variantMapFromJS(object), this));
        } else {
            m_listModel->insert(index, object);
        }

        emitItemsInserted(index, 1);
    } else {

        QVector<int> roles;

        if (m_dynamicRoles) {
            m_modelObjects[index]->updateValues(scope.engine->variantMapFromJS(object), roles);
        } else {
            m_listModel->set(index, object, &roles);
        }

        if (roles.count())
            emitItemsChanged(index, 1, roles);
    }
}

/*!
    \qmlmethod ListModel::setProperty(int index, string property, variant value)

    Changes the \a property of the item at \a index in the list model to \a value.

    \code
        fruitModel.setProperty(3, "cost", 5.95)
    \endcode

    The \a index must be an element in the list.

    \sa append()
*/
void QQmlListModel::setProperty(int index, const QString& property, const QVariant& value)
{
    if (count() == 0 || index >= count() || index < 0) {
        qmlInfo(this) << tr("set: index %1 out of range").arg(index);
        return;
    }

    if (m_dynamicRoles) {
        int roleIndex = m_roles.indexOf(property);
        if (roleIndex == -1) {
            roleIndex = m_roles.count();
            m_roles.append(property);
        }
        if (m_modelObjects[index]->setValue(property.toUtf8(), value)) {
            QVector<int> roles;
            roles << roleIndex;
            emitItemsChanged(index, 1, roles);
        }
    } else {
        int roleIndex = m_listModel->setOrCreateProperty(index, property, value);
        if (roleIndex != -1) {

            QVector<int> roles;
            roles << roleIndex;

            emitItemsChanged(index, 1, roles);
        }
    }
}

/*!
    \qmlmethod ListModel::sync()

    Writes any unsaved changes to the list model after it has been modified
    from a worker script.
*/
void QQmlListModel::sync()
{
    // This is just a dummy method to make it look like sync() exists in
    // ListModel (and not just QQmlListModelWorkerAgent) and to let
    // us document sync().
    qmlInfo(this) << "List sync() can only be called from a WorkerScript";
}

bool QQmlListModelParser::verifyProperty(const QV4::CompiledData::Unit *qmlUnit, const QV4::CompiledData::Binding *binding)
{
    if (binding->type >= QV4::CompiledData::Binding::Type_Object) {
        const quint32 targetObjectIndex = binding->value.objectIndex;
        const QV4::CompiledData::Object *target = qmlUnit->objectAt(targetObjectIndex);
        QString objName = qmlUnit->stringAt(target->inheritedTypeNameIndex);
        if (objName != listElementTypeName) {
            const QMetaObject *mo = resolveType(objName);
            if (mo != &QQmlListElement::staticMetaObject) {
                error(target, QQmlListModel::tr("ListElement: cannot contain nested elements"));
                return false;
            }
            listElementTypeName = objName; // cache right name for next time
        }

        if (!qmlUnit->stringAt(target->idNameIndex).isEmpty()) {
            error(target->locationOfIdProperty, QQmlListModel::tr("ListElement: cannot use reserved \"id\" property"));
            return false;
        }

        const QV4::CompiledData::Binding *binding = target->bindingTable();
        for (quint32 i = 0; i < target->nBindings; ++i, ++binding) {
            QString propName = qmlUnit->stringAt(binding->propertyNameIndex);
            if (propName.isEmpty()) {
                error(binding, QQmlListModel::tr("ListElement: cannot contain nested elements"));
                return false;
            }
            if (!verifyProperty(qmlUnit, binding))
                return false;
        }
    } else if (binding->type == QV4::CompiledData::Binding::Type_Script) {
        QString scriptStr = binding->valueAsScriptString(qmlUnit);
        if (!definesEmptyList(scriptStr)) {
            QByteArray script = scriptStr.toUtf8();
            bool ok;
            evaluateEnum(script, &ok);
            if (!ok) {
                error(binding, QQmlListModel::tr("ListElement: cannot use script for property value"));
                return false;
            }
        }
    }

    return true;
}

bool QQmlListModelParser::applyProperty(const QV4::CompiledData::Unit *qmlUnit, const QV4::CompiledData::Binding *binding, ListModel *model, int outterElementIndex)
{
    const QString elementName = qmlUnit->stringAt(binding->propertyNameIndex);

    bool roleSet = false;
    if (binding->type >= QV4::CompiledData::Binding::Type_Object) {
        const quint32 targetObjectIndex = binding->value.objectIndex;
        const QV4::CompiledData::Object *target = qmlUnit->objectAt(targetObjectIndex);

        ListModel *subModel = 0;
        if (outterElementIndex == -1) {
            subModel = model;
        } else {
            const ListLayout::Role &role = model->getOrCreateListRole(elementName);
            if (role.type == ListLayout::Role::List) {
                subModel = model->getListProperty(outterElementIndex, role);
                if (subModel == 0) {
                    subModel = new ListModel(role.subLayout, 0, -1);
                    QVariant vModel = QVariant::fromValue(subModel);
                    model->setOrCreateProperty(outterElementIndex, elementName, vModel);
                }
            }
        }

        int elementIndex = subModel ? subModel->appendElement() : -1;

        const QV4::CompiledData::Binding *subBinding = target->bindingTable();
        for (quint32 i = 0; i < target->nBindings; ++i, ++subBinding) {
            roleSet |= applyProperty(qmlUnit, subBinding, subModel, elementIndex);
        }

    } else {
        QVariant value;

        if (binding->evaluatesToString()) {
            value = binding->valueAsString(qmlUnit);
        } else if (binding->type == QV4::CompiledData::Binding::Type_Number) {
            value = binding->valueAsNumber();
        } else if (binding->type == QV4::CompiledData::Binding::Type_Boolean) {
            value = binding->valueAsBoolean();
        } else if (binding->type == QV4::CompiledData::Binding::Type_Script) {
            QString scriptStr = binding->valueAsScriptString(qmlUnit);
            if (definesEmptyList(scriptStr)) {
                const ListLayout::Role &role = model->getOrCreateListRole(elementName);
                ListModel *emptyModel = new ListModel(role.subLayout, 0, -1);
                value = QVariant::fromValue(emptyModel);
            } else {
                QByteArray script = scriptStr.toUtf8();
                bool ok;
                value = evaluateEnum(script, &ok);
            }
        } else {
            Q_UNREACHABLE();
        }

        model->setOrCreateProperty(outterElementIndex, elementName, value);
        roleSet = true;
    }
    return roleSet;
}

void QQmlListModelParser::verifyBindings(const QV4::CompiledData::Unit *qmlUnit, const QList<const QV4::CompiledData::Binding *> &bindings)
{
    listElementTypeName = QString(); // unknown

    foreach (const QV4::CompiledData::Binding *binding, bindings) {
        QString propName = qmlUnit->stringAt(binding->propertyNameIndex);
        if (!propName.isEmpty()) { // isn't default property
            error(binding, QQmlListModel::tr("ListModel: undefined property '%1'").arg(propName));
            return;
        }
        if (!verifyProperty(qmlUnit, binding))
            return;
    }
}

void QQmlListModelParser::applyBindings(QObject *obj, QV4::CompiledData::CompilationUnit *compilationUnit, const QList<const QV4::CompiledData::Binding *> &bindings)
{
    QQmlListModel *rv = static_cast<QQmlListModel *>(obj);

    rv->m_engine = QV8Engine::getV4(qmlEngine(rv));

    const QV4::CompiledData::Unit *qmlUnit = compilationUnit->data;

    bool setRoles = false;

    foreach (const QV4::CompiledData::Binding *binding, bindings) {
        if (binding->type != QV4::CompiledData::Binding::Type_Object)
            continue;
        setRoles |= applyProperty(qmlUnit, binding, rv->m_listModel, /*outter element index*/-1);
    }

    if (setRoles == false)
        qmlInfo(obj) << "All ListElement declarations are empty, no roles can be created unless dynamicRoles is set.";
}

bool QQmlListModelParser::definesEmptyList(const QString &s)
{
    if (s.startsWith(QLatin1Char('[')) && s.endsWith(QLatin1Char(']'))) {
        for (int i=1; i<s.length()-1; i++) {
            if (!s[i].isSpace())
                return false;
        }
        return true;
    }
    return false;
}


/*!
    \qmltype ListElement
    \instantiates QQmlListElement
    \inqmlmodule QtQml.Models
    \brief Defines a data item in a ListModel
    \ingroup qtquick-models

    List elements are defined inside ListModel definitions, and represent items in a
    list that will be displayed using ListView or \l Repeater items.

    List elements are defined like other QML elements except that they contain
    a collection of \e role definitions instead of properties. Using the same
    syntax as property definitions, roles both define how the data is accessed
    and include the data itself.

    The names used for roles must begin with a lower-case letter and should be
    common to all elements in a given model. Values must be simple constants; either
    strings (quoted and optionally within a call to QT_TR_NOOP), boolean values
    (true, false), numbers, or enumeration values (such as AlignText.AlignHCenter).

    \section1 Referencing Roles

    The role names are used by delegates to obtain data from list elements.
    Each role name is accessible in the delegate's scope, and refers to the
    corresponding role in the current element. Where a role name would be
    ambiguous to use, it can be accessed via the \l{ListView::}{model}
    property (e.g., \c{model.cost} instead of \c{cost}).

    \section1 Example Usage

    The following model defines a series of list elements, each of which
    contain "name" and "cost" roles and their associated values.

    \snippet qml/listmodel/listelements.qml model

    The delegate obtains the name and cost for each element by simply referring
    to \c name and \c cost:

    \snippet qml/listmodel/listelements.qml view

    \sa ListModel
*/

QT_END_NAMESPACE
