/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#include <QJSValue>
#include <QQmlEngine>

#include "quick3dbuffer_p.h"
#include <QtQml/private/qqmlengine_p.h>
#include <QtQml/private/qjsvalue_p.h>
#include <QtQml/private/qv4typedarray_p.h>
#include <QtQml/private/qv4arraybuffer_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

namespace Quick {

namespace {
const int jsValueTypeId = qMetaTypeId<QJSValue>();
}

Quick3DBuffer::Quick3DBuffer(Qt3DCore::QNode *parent)
    : Qt3DRender::QBuffer(QBuffer::VertexBuffer, parent)
    , m_engine(nullptr)
    , m_v4engine(nullptr)
{
    QObject::connect(this, &Qt3DRender::QBuffer::dataChanged, this, &Quick3DBuffer::bufferDataChanged);
}

QByteArray Quick3DBuffer::convertToRawData(const QJSValue &jsValue)
{
    initEngines();
    Q_ASSERT(m_v4engine);
    QV4::Scope scope(m_v4engine);
    QV4::Scoped<QV4::TypedArray> typedArray(scope,
                                            QJSValuePrivate::convertedToValue(m_v4engine, jsValue));
    if (!typedArray)
        return QByteArray();

    char *dataPtr = reinterpret_cast<char *>(typedArray->arrayData()->data());
    dataPtr += typedArray->d()->byteOffset;
    uint byteLength = typedArray->byteLength();
    return QByteArray(dataPtr, byteLength);
}

QVariant Quick3DBuffer::bufferData() const
{
    return QVariant::fromValue(data());
}

void Quick3DBuffer::setBufferData(const QVariant &bufferData)
{
    if (bufferData.userType() == QMetaType::QByteArray) {
        QBuffer::setData(bufferData.toByteArray());
    } else if (bufferData.userType() == jsValueTypeId) {
        QJSValue jsValue = bufferData.value<QJSValue>();
        QBuffer::setData(convertToRawData(jsValue));
    }
}

void Quick3DBuffer::updateData(int offset, const QVariant &bufferData)
{
    if (bufferData.userType() == QMetaType::QByteArray) {
        QBuffer::updateData(offset, bufferData.toByteArray());
    } else if (bufferData.userType() == jsValueTypeId) {
        QJSValue jsValue = bufferData.value<QJSValue>();
        QBuffer::updateData(offset, convertToRawData(jsValue));
    }
}

void Quick3DBuffer::initEngines()
{
    if (m_engine == nullptr) {
        m_engine = qmlEngine(parent());
        m_v4engine = QQmlEnginePrivate::getV4Engine(m_engine);
    }
}

} // Quick

} // Render

} // Qt3DRender

QT_END_NAMESPACE
