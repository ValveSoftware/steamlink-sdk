/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#ifndef LOCATIONVALUETYPEPROVIDER_H
#define LOCATIONVALUETYPEPROVIDER_H

#include <QtQml/private/qqmlglobal_p.h>

QT_BEGIN_NAMESPACE

class LocationValueTypeProvider : public QQmlValueTypeProvider
{
public:
    LocationValueTypeProvider();

private:
    template<typename T>
    bool typedCreate(QQmlValueType *&v)
    {
        v = new T;
        return true;
    }

    bool create(int type, QQmlValueType *&v) Q_DECL_OVERRIDE;

    template<typename T>
    bool typedInit(void *data, size_t dataSize)
    {
        Q_ASSERT(dataSize >= sizeof(T));
        Q_UNUSED(dataSize);
        T *t = reinterpret_cast<T *>(data);
        new (t) T();
        return true;
    }

    bool init(int type, void *data, size_t dataSize) Q_DECL_OVERRIDE;

    template<typename T>
    bool typedDestroy(void *data, size_t dataSize)
    {
        Q_ASSERT(dataSize >= sizeof(T));
        Q_UNUSED(dataSize);
        T *t = reinterpret_cast<T *>(data);
        t->~T();
        return true;
    }

    bool destroy(int type, void *data, size_t dataSize) Q_DECL_OVERRIDE;

    template<typename T>
    bool typedCopyConstruct(const void *src, void *dst, size_t dstSize)
    {
        Q_ASSERT(dstSize >= sizeof(T));
        Q_UNUSED(dstSize);
        const T *srcT = reinterpret_cast<const T *>(src);
        T *dstT = reinterpret_cast<T *>(dst);
        new (dstT) T(*srcT);
        return true;
    }

    bool copy(int type, const void *src, void *dst, size_t dstSize) Q_DECL_OVERRIDE;

    bool create(int type, int argc, const void *argv[], QVariant *v) Q_DECL_OVERRIDE;
    bool createFromString(int type, const QString &s, void *data, size_t dataSize) Q_DECL_OVERRIDE;
    bool createStringFrom(int type, const void *data, QString *s) Q_DECL_OVERRIDE;

    bool variantFromJsObject(int type, QQmlV4Handle h, QV8Engine *e, QVariant *v) Q_DECL_OVERRIDE;

    template<typename T>
    bool typedEqual(const void *lhs, const void *rhs)
    {
        return *reinterpret_cast<const T *>(lhs) == *reinterpret_cast<const T *>(rhs);
    }

    bool equal(int type, const void *lhs, const void *rhs, size_t rhsSize) Q_DECL_OVERRIDE;

    template<typename T>
    bool typedStore(const void *src, void *dst, size_t dstSize)
    {
        Q_ASSERT(dstSize >= sizeof(T));
        Q_UNUSED(dstSize);
        const T *srcT = reinterpret_cast<const T *>(src);
        T *dstT = reinterpret_cast<T *>(dst);
        new (dstT) T(*srcT);
        return true;
    }

    bool store(int type, const void *src, void *dst, size_t dstSize) Q_DECL_OVERRIDE;

    template<typename T>
    bool typedRead(int srcType, const void *src, size_t srcSize, int dstType, void *dst)
    {
        T *dstT = reinterpret_cast<T *>(dst);
        if (srcType == dstType) {
            Q_ASSERT(srcSize >= sizeof(T));
            Q_UNUSED(srcSize);
            const T *srcT = reinterpret_cast<const T *>(src);
            *dstT = *srcT;
        } else {
            *dstT = T();
        }
        return true;
    }

    bool read(int srcType, const void *src, size_t srcSize, int dstType, void *dst) Q_DECL_OVERRIDE;

    template<typename T>
    bool typedWrite(const void *src, void *dst, size_t dstSize)
    {
        Q_ASSERT(dstSize >= sizeof(T));
        Q_UNUSED(dstSize);
        const T *srcT = reinterpret_cast<const T *>(src);
        T *dstT = reinterpret_cast<T *>(dst);
        if (*dstT != *srcT) {
            *dstT = *srcT;
            return true;
        }
        return false;
    }

    bool write(int type, const void *src, void *dst, size_t dstSize) Q_DECL_OVERRIDE;
};

QT_END_NAMESPACE

#endif // LOCATIONVALUETYPEPROVIDER_H
