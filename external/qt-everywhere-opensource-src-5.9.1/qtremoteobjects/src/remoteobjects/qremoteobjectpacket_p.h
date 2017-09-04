/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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

#ifndef QTREMOTEOBJECTPACKET_P_H
#define QTREMOTEOBJECTPACKET_P_H

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

#include "qtremoteobjectglobal.h"
#include "qremoteobjectsource.h"
#include "qconnectionfactories_p.h"

#include <QtCore/QHash>
#include <QtCore/QMap>
#include <QtCore/QPair>
#include <QtCore/QUrl>
#include <QtCore/QVariant>
#include <QtCore/QLoggingCategory>

#include <cstdlib>

QT_BEGIN_NAMESPACE

class QMetaObjectBuilder;
class QRemoteObjectSource;

namespace QRemoteObjectPackets {

class DataStreamPacket;

struct ObjectInfo
{
    QString name;
    QString typeName;
    QByteArray signature;
};

inline QDebug operator<<(QDebug dbg, const ObjectInfo &info)
{
    dbg.nospace() << "ObjectInfo(" << info.name << ", " << info.typeName << ", " << info.signature <<")";
    return dbg.space();
}

inline QDataStream& operator<<(QDataStream &stream, const ObjectInfo &info)
{
    return stream << info.name << info.typeName << info.signature;
}

inline QDataStream& operator>>(QDataStream &stream, ObjectInfo &info)
{
    return stream >> info.name >> info.typeName >> info.signature;
}

typedef QVector<ObjectInfo> ObjectInfoList;

void serializeObjectListPacket(DataStreamPacket&, const ObjectInfoList&);
void deserializeObjectListPacket(QDataStream&, ObjectInfoList&);

//Helper class for creating a QByteArray from a QRemoteObjectPacket
class DataStreamPacket : public QDataStream
{
public:
    DataStreamPacket(quint16 id = QtRemoteObjects::InvokePacket)
        : QDataStream(&array, QIODevice::WriteOnly)
        , baseAddress(0)
        , size(0)
    {
        this->setVersion(QtRemoteObjects::dataStreamVersion);
        *this << quint32(0);
        *this << id;
    }
    void setId(quint16 id)
    {
        device()->seek(baseAddress);
        *this << quint32(0);
        *this << id;
    }

    void finishPacket()
    {
        size = device()->pos();
        device()->seek(baseAddress);
        *this << quint32(size - baseAddress - sizeof(quint32));
    }
    QByteArray array;
    int baseAddress;
    int size;

private:
    Q_DISABLE_COPY(DataStreamPacket)
};

QVariant serializedProperty(const QMetaProperty &property, const QObject *object);
QVariant deserializedProperty(const QVariant &in, const QMetaProperty &property);

void serializeInitPacket(DataStreamPacket&, const QRemoteObjectSource*);
void deserializeInitPacket(QDataStream&, QVariantList&);

void serializeInitDynamicPacket(DataStreamPacket&, const QRemoteObjectSource*);
void deserializeInitDynamicPacket(QDataStream&, QMetaObjectBuilder&, QVariantList&);

void serializeAddObjectPacket(DataStreamPacket&, const QString &name, bool isDynamic);
void deserializeAddObjectPacket(QDataStream &ds, bool &isDynamic);

void serializeRemoveObjectPacket(DataStreamPacket&, const QString &name);
//There is no deserializeRemoveObjectPacket - no parameters other than id and name

void serializeInvokePacket(DataStreamPacket&, const QString &name, int call, int index, const QVariantList &args, int serialId = -1, int propertyIndex = -1);
void deserializeInvokePacket(QDataStream& in, int &call, int &index, QVariantList &args, int &serialId, int &propertyIndex);

void serializeInvokeReplyPacket(DataStreamPacket&, const QString &name, int ackedSerialId, const QVariant &value);
void deserializeInvokeReplyPacket(QDataStream& in, int &ackedSerialId, QVariant &value);

//TODO do we need the object name or could we go with an id in backend code, this could be a costly allocation
void serializePropertyChangePacket(DataStreamPacket&, const QString &name, int index, const QVariant &value);
void deserializePropertyChangePacket(QDataStream& in, int &index, QVariant &value);

} // namespace QRemoteObjectPackets

QT_END_NAMESPACE

#endif
