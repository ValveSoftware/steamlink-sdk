/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "quick3dentityloader_p_p.h"

#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlIncubator>

#include <QtQml/private/qqmlengine_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {
namespace Quick {

namespace {
struct Quick3DQmlOwner
{
    Quick3DQmlOwner(QQmlEngine *e, QObject *o)
        : engine(e)
        , object(o)
    {}

    QQmlEngine *engine;
    QObject *object;

    QQmlContext *context() const
    {
        return engine->contextForObject(object);
    }
};

Quick3DQmlOwner _q_findQmlOwner(QObject *object)
{
    auto o = object;
    while (!qmlEngine(o) && o->parent())
        o = o->parent();
    return Quick3DQmlOwner(qmlEngine(o), o);
}
}

class Quick3DEntityLoaderIncubator : public QQmlIncubator
{
public:
    Quick3DEntityLoaderIncubator(Quick3DEntityLoader *loader)
        : QQmlIncubator(AsynchronousIfNested),
          m_loader(loader)
    {
    }

protected:
    void statusChanged(Status status) Q_DECL_FINAL
    {
        Quick3DEntityLoaderPrivate *priv = Quick3DEntityLoaderPrivate::get(m_loader);

        switch (status) {
        case Ready: {
            Q_ASSERT(priv->m_entity == nullptr);
            priv->m_entity = qobject_cast<QEntity *>(object());
            Q_ASSERT(priv->m_entity != nullptr);
            priv->m_entity->setParent(m_loader);
            emit m_loader->entityChanged();
            priv->setStatus(Quick3DEntityLoader::Ready);
            break;
        }

        case Loading: {
            priv->setStatus(Quick3DEntityLoader::Loading);
            break;
        }

        case Error: {
            QQmlEnginePrivate::warning(_q_findQmlOwner(m_loader).engine, errors());
            priv->clear();
            emit m_loader->entityChanged();
            priv->setStatus(Quick3DEntityLoader::Error);
            break;
        }

        default:
            break;
        }
    }

private:
    Quick3DEntityLoader *m_loader;
};

/*!
    \qmltype EntityLoader
    \inqmlmodule Qt3D.Core
    \inherits Entity
    \since 5.5
    \brief Provides a way to dynamically load an Entity subtree

    An EntityLoader provides the facitily to load predefined set of entities
    from qml source file. EntityLoader itself is an entity and the loaded entity
    tree is set as a child of the loader. The loaded entity tree root can be
    accessed with EntityLoader::entity property.

    \badcode
        EntityLoader {
            id: loader
            source: "./SphereEntity.qml"
        }
    \endcode
*/

Quick3DEntityLoader::Quick3DEntityLoader(QNode *parent)
    : QEntity(*new Quick3DEntityLoaderPrivate, parent)
{
}

Quick3DEntityLoader::~Quick3DEntityLoader()
{
    Q_D(Quick3DEntityLoader);
    d->clear();
}

/*!
    \qmlproperty QtQml::QtObject EntityLoader::entity
    Holds the loaded entity tree root.
    \readonly

    This property allows access to the content of the loader. It references
    either a valid Entity object if the status property equals
    EntityLoader.Ready, it is equal to null otherwise.
*/
QObject *Quick3DEntityLoader::entity() const
{
    Q_D(const Quick3DEntityLoader);
    return d->m_entity;
}

/*!
    \qmlproperty url Qt3DCore::EntityLoader::source
    Holds the source url.
*/
QUrl Quick3DEntityLoader::source() const
{
    Q_D(const Quick3DEntityLoader);
    return d->m_source;
}

void Quick3DEntityLoader::setSource(const QUrl &url)
{
    Q_D(Quick3DEntityLoader);

    if (url == d->m_source)
        return;

    d->clear();
    d->m_source = url;
    emit sourceChanged();
    d->loadFromSource();
}

/*!
    \qmlproperty Status Qt3DCore::EntityLoader::status

    Holds the status of the entity loader.
    \list
    \li EntityLoader.Null
    \li EntityLoader.Loading
    \li EntityLoader.Ready
    \li EntityLoader.Error
    \endlist
 */
Quick3DEntityLoader::Status Quick3DEntityLoader::status() const
{
    Q_D(const Quick3DEntityLoader);
    return d->m_status;
}

Quick3DEntityLoaderPrivate::Quick3DEntityLoaderPrivate()
    : QEntityPrivate(),
      m_incubator(nullptr),
      m_context(nullptr),
      m_component(nullptr),
      m_entity(nullptr),
      m_status(Quick3DEntityLoader::Null)
{
}

void Quick3DEntityLoaderPrivate::clear()
{
    if (m_incubator) {
        m_incubator->clear();
        delete m_incubator;
        m_incubator = nullptr;
    }

    if (m_entity) {
        m_entity->setParent(Q_NODE_NULLPTR);
        delete m_entity;
        m_entity = nullptr;
    }

    if (m_component) {
        delete m_component;
        m_component = nullptr;
    }

    if (m_context) {
        delete m_context;
        m_context = nullptr;
    }
}

void Quick3DEntityLoaderPrivate::loadFromSource()
{
    Q_Q(Quick3DEntityLoader);

    if (m_source.isEmpty()) {
        emit q->entityChanged();
        return;
    }

    loadComponent(m_source);
}

void Quick3DEntityLoaderPrivate::loadComponent(const QUrl &source)
{
    Q_Q(Quick3DEntityLoader);

    Q_ASSERT(m_entity == nullptr);
    Q_ASSERT(m_component == nullptr);
    Q_ASSERT(m_context == nullptr);

    auto owner = _q_findQmlOwner(q);
    m_component = new QQmlComponent(owner.engine, owner.object);
    QObject::connect(m_component, SIGNAL(statusChanged(QQmlComponent::Status)),
                     q, SLOT(_q_componentStatusChanged(QQmlComponent::Status)));
    m_component->loadUrl(source, QQmlComponent::Asynchronous);
}

void Quick3DEntityLoaderPrivate::_q_componentStatusChanged(QQmlComponent::Status status)
{
    Q_Q(Quick3DEntityLoader);

    Q_ASSERT(m_entity == nullptr);
    Q_ASSERT(m_component != nullptr);
    Q_ASSERT(m_context == nullptr);
    Q_ASSERT(m_incubator == nullptr);

    auto owner = _q_findQmlOwner(q);

    if (!m_component->errors().isEmpty()) {
        QQmlEnginePrivate::warning(owner.engine, m_component->errors());
        clear();
        emit q->entityChanged();
        return;
    }

    // Still loading
    if (status != QQmlComponent::Ready)
        return;

    m_context = new QQmlContext(owner.context());
    m_context->setContextObject(owner.object);

    m_incubator = new Quick3DEntityLoaderIncubator(q);
    m_component->create(*m_incubator, m_context);
}

void Quick3DEntityLoaderPrivate::setStatus(Quick3DEntityLoader::Status status)
{
    Q_Q(Quick3DEntityLoader);
    if (status != m_status) {
        m_status = status;
        const bool blocked = q->blockNotifications(true);
        emit q->statusChanged(m_status);
        q->blockNotifications(blocked);
    }
}

} // namespace Quick
} // namespace Qt3DCore

QT_END_NAMESPACE

#include "moc_quick3dentityloader_p.cpp"
