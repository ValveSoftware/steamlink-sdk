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

#include "qshaderdata.h"
#include "qshaderdata_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

PropertyReaderInterface::~PropertyReaderInterface()
{
}

QShaderDataPrivate::QShaderDataPrivate()
    : QComponentPrivate()
    , m_propertyReader(PropertyReaderInterfacePtr(new QShaderDataPropertyReader()))
{
}

QShaderDataPrivate::QShaderDataPrivate(PropertyReaderInterfacePtr reader)
    : QComponentPrivate()
    , m_propertyReader(reader)
{
}

/*!
 * \class Qt3DRender::QShaderData
 * \inmodule Qt3DRender
 *
 * \brief Provides a way of specifying values of a Uniform Block or a shader
 * structure.
 *
 * \note When subclassing and adding properties to QShaderData, note that if
 * you need to nest an inner Qt3DRender::QShaderData, the data type of the
 * property should be Qt3DRender::QShaderData* instead of the name of your
 * subclass.
 *
 * \since 5.5
 */

/*!
  \fn Qt3DRender::PropertyReaderInterface::readProperty(const QVariant &v)
  \return the property identifies by \a v
 */

/*!
 *   Constructs a new QShaderData with the specified \a parent.
 */
QShaderData::QShaderData(QNode *parent)
    : QComponent(*new QShaderDataPrivate, parent)
{
}

/*! \internal */
QShaderData::~QShaderData()
{
}
/*!
 * \brief QShaderData::propertyReader
 * \return PropertyReaderInterfacePtr for this shader data
 */
PropertyReaderInterfacePtr QShaderData::propertyReader() const
{
    Q_D(const QShaderData);
    return d->m_propertyReader;
}

/*! \internal */
QShaderData::QShaderData(QShaderDataPrivate &dd, QNode *parent)
    : QComponent(dd, parent)
{
}

/*! \internal */
bool QShaderData::event(QEvent *event)
{
    Q_D(QShaderData);

    if (event->type() == QEvent::DynamicPropertyChange) {
        auto e = static_cast<QDynamicPropertyChangeEvent*>(event);
        const auto propertyName = e->propertyName();

        const QVariant data = property(propertyName);
        if (data.canConvert<Qt3DCore::QNode*>()) {
            const auto node = data.value<Qt3DCore::QNode*>();
            const auto id = node ? node->id() : Qt3DCore::QNodeId();
            d->notifyDynamicPropertyChange(propertyName, QVariant::fromValue(id));
        } else {
            d->notifyDynamicPropertyChange(propertyName, data);
        }
    }

    return QComponent::event(event);
}

Qt3DCore::QNodeCreatedChangeBasePtr QShaderData::createNodeCreationChange() const
{
    Q_D(const QShaderData);

    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QShaderDataData>::create(this);
    QShaderDataData &data = creationChange->data;

    data.propertyReader = d->m_propertyReader;

    const QMetaObject *metaObj = metaObject();
    const int propertyOffset = QShaderData::staticMetaObject.propertyOffset();
    const int propertyCount = metaObj->propertyCount();

    const auto propertyNames = dynamicPropertyNames();
    data.properties.reserve(propertyCount - propertyOffset + propertyNames.size());

    for (int i = propertyOffset; i < propertyCount; ++i) {
        const QMetaProperty pro = metaObj->property(i);
        if (pro.isWritable()) {
            data.properties.push_back(qMakePair(pro.name(),
                                                propertyReader()->readProperty(property(pro.name()))));
        }
    }

    for (const QByteArray &propertyName : propertyNames) {
        data.properties.push_back(qMakePair(propertyName,
                                            propertyReader()->readProperty(property(propertyName))));
    }

    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
