/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd and/or its subsidiary(-ies).
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

#include "stlgeometryloader.h"

#include <QtCore/QDataStream>
#include <QtCore/QLoggingCategory>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

Q_LOGGING_CATEGORY(StlGeometryLoaderLog, "Qt3D.StlGeometryLoader", QtWarningMsg)

bool StlGeometryLoader::doLoad(QIODevice *ioDev, const QString &subMesh)
{
    char signature[5];

    Q_UNUSED(subMesh);

    if (ioDev->peek(signature, sizeof(signature)) != sizeof(signature))
        return false;

    if (!qstrncmp(signature, "solid", 5))
        return loadAscii(ioDev);
    else
        return loadBinary(ioDev);

    return true;
}

bool StlGeometryLoader::loadAscii(QIODevice *ioDev)
{
    // TODO stricter syntax checking

    while (!ioDev->atEnd()) {
        QByteArray lineBuffer = ioDev->readLine();

        const char *begin = lineBuffer.constData();
        const char *end = begin + lineBuffer.size();

        const ByteArraySplitter tokens(begin, end, ' ', QString::SkipEmptyParts);

        if (qstrncmp(tokens.charPtrAt(0), "vertex ", 7) == 0) {
            if (tokens.size() < 4) {
                qCWarning(StlGeometryLoaderLog) << "Unsupported number of components in vertex";
            } else {
                const float x = tokens.floatAt(1);
                const float y = tokens.floatAt(2);
                const float z = tokens.floatAt(3);
                m_points.append(QVector3D(x, y, z));
                m_indices.append(m_indices.size());
            }
        }
    }

    return true;
}

bool StlGeometryLoader::loadBinary(QIODevice *ioDev)
{
    static const int headerSize = 80;

    if (ioDev->read(headerSize).size() != headerSize)
        return false;

    ioDev->setTextModeEnabled(false);

    QDataStream stream(ioDev);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

    quint32 triangleCount;
    stream >> triangleCount;

    m_points.reserve(triangleCount * 3);
    m_indices.reserve(triangleCount * 3);

    for (unsigned i = 0; i < triangleCount; ++i) {
        QVector3D normal;
        stream >> normal;

        for (int j = 0; j < 3; ++j) {
            QVector3D point;
            stream >> point;
            m_points.append(point);

            m_indices.append((i * 3) + j);
        }

        quint16 attributeCount;
        stream >> attributeCount;
    }

    return true;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
