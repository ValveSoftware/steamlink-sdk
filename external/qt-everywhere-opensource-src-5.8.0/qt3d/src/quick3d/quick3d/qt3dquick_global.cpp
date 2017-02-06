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

#include "qt3dquick_global_p.h"
#include "qt3dquicknodefactory_p.h"
#include <private/qqmlglobal_p.h>
#include <private/qt3dquickvaluetypes_p.h>
#include <private/qv8engine_p.h>
#include <private/qv4engine_p.h>
#include <private/qv4object_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {
namespace Quick {

class Quick3DColorProvider : public QQmlColorProvider
{
public:
    QVariant colorFromString(const QString &s, bool *ok)
    {
        QColor c(s);
        if (c.isValid()) {
            if (ok) *ok = true;
            return QVariant(c);
        }

        if (ok) *ok = false;
        return QVariant();
    }

    unsigned rgbaFromString(const QString &s, bool *ok)
    {
        QColor c(s);
        if (c.isValid()) {
            if (ok) *ok = true;
            return c.rgba();
        }

        if (ok) *ok = false;
        return 0;
    }

    QString stringFromRgba(unsigned rgba)
    {
        QColor c(QColor::fromRgba(rgba));
        if (c.isValid()) {
            return QVariant(c).toString();
        }

        return QString();
    }

    QVariant fromRgbF(double r, double g, double b, double a)
    {
        return QVariant(QColor::fromRgbF(r, g, b, a));
    }

    QVariant fromHslF(double h, double s, double l, double a)
    {
        return QVariant(QColor::fromHslF(h, s, l, a));
    }

    QVariant lighter(const QVariant &var, qreal factor)
    {
        QColor color = var.value<QColor>();
        color = color.lighter(int(qRound(factor*100.)));
        return QVariant::fromValue(color);
    }

    QVariant darker(const QVariant &var, qreal factor)
    {
        QColor color = var.value<QColor>();
        color = color.darker(int(qRound(factor*100.)));
        return QVariant::fromValue(color);
    }

    QVariant tint(const QVariant &baseVar, const QVariant &tintVar)
    {
        QColor tintColor = tintVar.value<QColor>();

        int tintAlpha = tintColor.alpha();
        if (tintAlpha == 0xFF) {
            return tintVar;
        } else if (tintAlpha == 0x00) {
            return baseVar;
        }

        // tint the base color and return the final color
        QColor baseColor = baseVar.value<QColor>();
        qreal a = tintColor.alphaF();
        qreal inv_a = 1.0 - a;

        qreal r = tintColor.redF() * a + baseColor.redF() * inv_a;
        qreal g = tintColor.greenF() * a + baseColor.greenF() * inv_a;
        qreal b = tintColor.blueF() * a + baseColor.blueF() * inv_a;

        return QVariant::fromValue(QColor::fromRgbF(r, g, b, a + inv_a * baseColor.alphaF()));
    }
};


// Note: The functions in this class provide handling only for the types
// that the QML engine will currently actually call them for, so many
// appear incompletely implemented.  For some functions, the implementation
// would be obvious, but for others (particularly create and createFromString)
// the exact semantics are unknown.  For this reason unused functionality
// has been omitted.

class Quick3DValueTypeProvider : public QQmlValueTypeProvider
{
public:

#if defined(QT_NO_DEBUG) && !defined(QT_FORCE_ASSERTS)
    #define ASSERT_VALID_SIZE(size, min) Q_UNUSED(size)
#else
    #define ASSERT_VALID_SIZE(size, min) Q_ASSERT(size >= min)
#endif

    static QVector2D vector2DFromString(const QString &s, bool *ok)
    {
        if (s.count(QLatin1Char(',')) == 1) {
            int index = s.indexOf(QLatin1Char(','));

            bool xGood, yGood;
            float xCoord = s.leftRef(index).toFloat(&xGood);
            float yCoord = s.midRef(index+1).toFloat(&yGood);

            if (xGood && yGood) {
                if (ok) *ok = true;
                return QVector2D(xCoord, yCoord);
            }
        }

        if (ok) *ok = false;
        return QVector2D();
    }

    static QVector3D vector3DFromString(const QString &s, bool *ok)
    {
        if (s.count(QLatin1Char(',')) == 2) {
            int index = s.indexOf(QLatin1Char(','));
            int index2 = s.indexOf(QLatin1Char(','), index+1);

            bool xGood, yGood, zGood;
            float xCoord = s.leftRef(index).toFloat(&xGood);
            float yCoord = s.midRef(index+1, index2-index-1).toFloat(&yGood);
            float zCoord = s.midRef(index2+1).toFloat(&zGood);

            if (xGood && yGood && zGood) {
                if (ok) *ok = true;
                return QVector3D(xCoord, yCoord, zCoord);
            }
        }

        if (ok) *ok = false;
        return QVector3D();
    }

    static QVector4D vector4DFromString(const QString &s, bool *ok)
    {
        if (s.count(QLatin1Char(',')) == 3) {
            int index = s.indexOf(QLatin1Char(','));
            int index2 = s.indexOf(QLatin1Char(','), index+1);
            int index3 = s.indexOf(QLatin1Char(','), index2+1);

            bool xGood, yGood, zGood, wGood;
            float xCoord = s.leftRef(index).toFloat(&xGood);
            float yCoord = s.midRef(index+1, index2-index-1).toFloat(&yGood);
            float zCoord = s.midRef(index2+1, index3-index2-1).toFloat(&zGood);
            float wCoord = s.midRef(index3+1).toFloat(&wGood);

            if (xGood && yGood && zGood && wGood) {
                if (ok) *ok = true;
                return QVector4D(xCoord, yCoord, zCoord, wCoord);
            }
        }

        if (ok) *ok = false;
        return QVector4D();
    }

    static QQuaternion quaternionFromString(const QString &s, bool *ok)
    {
        if (s.count(QLatin1Char(',')) == 3) {
            int index = s.indexOf(QLatin1Char(','));
            int index2 = s.indexOf(QLatin1Char(','), index+1);
            int index3 = s.indexOf(QLatin1Char(','), index2+1);

            bool sGood, xGood, yGood, zGood;
            qreal sCoord = s.leftRef(index).toDouble(&sGood);
            qreal xCoord = s.midRef(index+1, index2-index-1).toDouble(&xGood);
            qreal yCoord = s.midRef(index2+1, index3-index2-1).toDouble(&yGood);
            qreal zCoord = s.midRef(index3+1).toDouble(&zGood);

            if (sGood && xGood && yGood && zGood) {
                if (ok) *ok = true;
                return QQuaternion(sCoord, xCoord, yCoord, zCoord);
            }
        }

        if (ok) *ok = false;
        return QQuaternion();
    }

    static QMatrix4x4 matrix4x4FromString(const QString &s, bool *ok)
    {
        if (s.count(QLatin1Char(',')) == 15) {
            float matValues[16];
            bool vOK = true;
            QStringRef mutableStr(&s);
            for (int i = 0; vOK && i < 16; ++i) {
                int cidx = mutableStr.indexOf(QLatin1Char(','));
                matValues[i] = mutableStr.left(cidx).toDouble(&vOK);
                mutableStr = mutableStr.mid(cidx + 1);
            }

            if (vOK) {
                if (ok) *ok = true;
                return QMatrix4x4(matValues);
            }
        }

        if (ok) *ok = false;
        return QMatrix4x4();
    }

    static QMatrix4x4 matrix4x4FromObject(QQmlV4Handle object, QV4::ExecutionEngine *v4, bool *ok)
    {
        if (ok) *ok = false;
        QV4::Scope scope(v4);
        QV4::ScopedArrayObject array(scope, object);
        if (!array)
            return QMatrix4x4();

        if (array->getLength() != 16)
            return QMatrix4x4();

        float matVals[16];
        QV4::ScopedValue v(scope);
        for (quint32 i = 0; i < 16; ++i) {
            v = array->getIndexed(i);
            if (!v->isNumber())
                return QMatrix4x4();
            matVals[i] = v->asDouble();
        }

        if (ok) *ok = true;
        return QMatrix4x4(matVals);
    }

    const QMetaObject *getMetaObjectForMetaType(int type) Q_DECL_OVERRIDE
    {
        switch (type) {
        case QMetaType::QColor:
            return &Quick3DColorValueType::staticMetaObject;
        case QMetaType::QVector2D:
            return &Quick3DVector2DValueType::staticMetaObject;
        case QMetaType::QVector3D:
            return &Quick3DVector3DValueType::staticMetaObject;
        case QMetaType::QVector4D:
            return &Quick3DVector4DValueType::staticMetaObject;
        case QMetaType::QQuaternion:
            return &Quick3DQuaternionValueType::staticMetaObject;
        case QMetaType::QMatrix4x4:
            return &Quick3DMatrix4x4ValueType::staticMetaObject;
        default:
            break;
        }

        return nullptr;
    }

    bool init(int type, QVariant& dst) Q_DECL_OVERRIDE
    {
        switch (type) {
        case QMetaType::QColor:
            dst.setValue<QColor>(QColor());
            return true;
        case QMetaType::QVector2D:
            dst.setValue<QVector2D>(QVector2D());
            return true;
        case QMetaType::QVector3D:
            dst.setValue<QVector3D>(QVector3D());
            return true;
        case QMetaType::QVector4D:
            dst.setValue<QVector4D>(QVector4D());
            return true;
        case QMetaType::QQuaternion:
            dst.setValue<QQuaternion>(QQuaternion());
            return true;
        case QMetaType::QMatrix4x4:
            dst.setValue<QMatrix4x4>(QMatrix4x4());
            return true;
        default: break;
        }

        return false;
    }

    bool create(int type, int argc, const void *argv[], QVariant *v) Q_DECL_OVERRIDE
    {
        switch (type) {
        case QMetaType::QVector2D:
            if (argc == 1) {
                const float *xy = reinterpret_cast<const float*>(argv[0]);
                QVector2D v2(xy[0], xy[1]);
                *v = QVariant(v2);
                return true;
            }
            break;
        case QMetaType::QVector3D:
            if (argc == 1) {
                const float *xyz = reinterpret_cast<const float*>(argv[0]);
                QVector3D v3(xyz[0], xyz[1], xyz[2]);
                *v = QVariant(v3);
                return true;
            }
            break;
        case QMetaType::QVector4D:
            if (argc == 1) {
                const float *xyzw = reinterpret_cast<const float*>(argv[0]);
                QVector4D v4(xyzw[0], xyzw[1], xyzw[2], xyzw[3]);
                *v = QVariant(v4);
                return true;
            }
            break;
        case QMetaType::QQuaternion:
            if (argc == 1) {
                const qreal *sxyz = reinterpret_cast<const qreal*>(argv[0]);
                QQuaternion q(sxyz[0], sxyz[1], sxyz[2], sxyz[3]);
                *v = QVariant(q);
                return true;
            }
            break;
        case QMetaType::QMatrix4x4:
            if (argc == 0) {
                QMatrix4x4 m;
                *v = QVariant(m);
                return true;
            } else if (argc == 1) {
                const qreal *vals = reinterpret_cast<const qreal*>(argv[0]);
                QMatrix4x4 m(vals[0], vals[1], vals[2], vals[3],
                             vals[4], vals[5], vals[6], vals[7],
                             vals[8], vals[9], vals[10], vals[11],
                             vals[12], vals[13], vals[14], vals[15]);
                *v = QVariant(m);
                return true;
            }
            break;
        default: break;
        }

        return false;
    }

    template<typename T>
    bool createFromStringTyped(void *data, size_t dataSize, T initValue)
    {
        ASSERT_VALID_SIZE(dataSize, sizeof(T));
        T *t = reinterpret_cast<T *>(data);
        new (t) T(initValue);
        return true;
    }

    bool createFromString(int type, const QString &s, void *data, size_t dataSize) Q_DECL_OVERRIDE
    {
        bool ok = false;

        switch (type) {
        case QMetaType::QColor:
            return createFromStringTyped<QColor>(data, dataSize, QColor(s));
        case QMetaType::QVector2D:
            return createFromStringTyped<QVector2D>(data, dataSize, vector2DFromString(s, &ok));
        case QMetaType::QVector3D:
            return createFromStringTyped<QVector3D>(data, dataSize, vector3DFromString(s, &ok));
        case QMetaType::QVector4D:
            return createFromStringTyped<QVector4D>(data, dataSize, vector4DFromString(s, &ok));
        case QMetaType::QQuaternion:
            return createFromStringTyped<QQuaternion>(data, dataSize, quaternionFromString(s, &ok));
        case QMetaType::QMatrix4x4:
            return createFromStringTyped<QMatrix4x4>(data, dataSize, matrix4x4FromString(s, &ok));
        default: break;
        }

        return false;
    }

    bool createStringFrom(int type, const void *data, QString *s) Q_DECL_OVERRIDE
    {
        if (type == QMetaType::QColor) {
            const QColor *color = reinterpret_cast<const QColor *>(data);
            new (s) QString(QVariant(*color).toString());
            return true;
        }

        return false;
    }

    bool variantFromString(const QString &s, QVariant *v) Q_DECL_OVERRIDE
    {
        QColor c(s);
        if (c.isValid()) {
            *v = QVariant::fromValue(c);
            return true;
        }

        bool ok = false;

        QVector2D v2 = vector2DFromString(s, &ok);
        if (ok) {
            *v = QVariant::fromValue(v2);
            return true;
        }

        QVector3D v3 = vector3DFromString(s, &ok);
        if (ok) {
            *v = QVariant::fromValue(v3);
            return true;
        }

        QVector4D v4 = vector4DFromString(s, &ok);
        if (ok) {
            *v = QVariant::fromValue(v4);
            return true;
        }

        QQuaternion q = quaternionFromString(s, &ok);
        if (ok) {
            *v = QVariant::fromValue(q);
            return true;
        }

        QMatrix4x4 m = matrix4x4FromString(s, &ok);
        if (ok) {
            *v = QVariant::fromValue(m);
            return true;
        }

        return false;
    }

    bool variantFromString(int type, const QString &s, QVariant *v) Q_DECL_OVERRIDE
    {
        bool ok = false;

        switch (type) {
        case QMetaType::QColor:
            {
            QColor c(s);
            *v = QVariant::fromValue(c);
            return true;
            }
        case QMetaType::QVector2D:
            {
            *v = QVariant::fromValue(vector2DFromString(s, &ok));
            return true;
            }
        case QMetaType::QVector3D:
            {
            *v = QVariant::fromValue(vector3DFromString(s, &ok));
            return true;
            }
        case QMetaType::QVector4D:
            {
            *v = QVariant::fromValue(vector4DFromString(s, &ok));
            return true;
            }
        case QMetaType::QQuaternion:
            {
            *v = QVariant::fromValue(quaternionFromString(s, &ok));
            return true;
            }
        case QMetaType::QMatrix4x4:
            {
            *v = QVariant::fromValue(matrix4x4FromString(s, &ok));
            return true;
            }
        default:
            break;
        }

        return false;
    }

    bool variantFromJsObject(int type, QQmlV4Handle object, QV4::ExecutionEngine *v4, QVariant *v) Q_DECL_OVERRIDE
    {
        QV4::Scope scope(v4);
#ifndef QT_NO_DEBUG
        QV4::ScopedObject obj(scope, object);
        Q_ASSERT(obj);
#endif
        bool ok = false;
        switch (type) {
        case QMetaType::QMatrix4x4:
            *v = QVariant::fromValue(matrix4x4FromObject(object, v4, &ok));
        default: break;
        }

        return ok;
    }

    template<typename T>
    bool typedEqual(const void *lhs, const QVariant& rhs)
    {
        return (*(reinterpret_cast<const T *>(lhs)) == rhs.value<T>());
    }

    bool equal(int type, const void *lhs, const QVariant &rhs) Q_DECL_OVERRIDE
    {
        switch (type) {
        case QMetaType::QColor:
            return typedEqual<QColor>(lhs, rhs);
        case QMetaType::QVector2D:
            return typedEqual<QVector2D>(lhs, rhs);
        case QMetaType::QVector3D:
            return typedEqual<QVector3D>(lhs, rhs);
        case QMetaType::QVector4D:
            return typedEqual<QVector4D>(lhs, rhs);
        case QMetaType::QQuaternion:
            return typedEqual<QQuaternion>(lhs, rhs);
        case QMetaType::QMatrix4x4:
            return typedEqual<QMatrix4x4>(lhs, rhs);
        default: break;
        }

        return false;
    }

    template<typename T>
    bool typedStore(const void *src, void *dst, size_t dstSize)
    {
        ASSERT_VALID_SIZE(dstSize, sizeof(T));
        const T *srcT = reinterpret_cast<const T *>(src);
        T *dstT = reinterpret_cast<T *>(dst);
        new (dstT) T(*srcT);
        return true;
    }

    bool store(int type, const void *src, void *dst, size_t dstSize) Q_DECL_OVERRIDE
    {
        switch (type) {
        case QMetaType::QColor:
            {
            Q_ASSERT(dstSize >= sizeof(QColor));
            const QRgb *rgb = reinterpret_cast<const QRgb *>(src);
            QColor *color = reinterpret_cast<QColor *>(dst);
            new (color) QColor(QColor::fromRgba(*rgb));
            return true;
            }
        default: break;
        }

        return false;
    }

    template<typename T>
    bool typedRead(const QVariant& src, int dstType, void *dst)
    {
        T *dstT = reinterpret_cast<T *>(dst);
        if (src.type() == QVariant::Type(dstType)) {
            *dstT = src.value<T>();
        } else {
            *dstT = T();
        }
        return true;
    }

    bool read(const QVariant &src, void *dst, int dstType) Q_DECL_OVERRIDE
    {
        switch (dstType) {
        case QMetaType::QColor:
            return typedRead<QColor>(src, dstType, dst);
        case QMetaType::QVector2D:
            return typedRead<QVector2D>(src, dstType, dst);
        case QMetaType::QVector3D:
            return typedRead<QVector3D>(src, dstType, dst);
        case QMetaType::QVector4D:
            return typedRead<QVector4D>(src, dstType, dst);
        case QMetaType::QQuaternion:
            return typedRead<QQuaternion>(src, dstType, dst);
        case QMetaType::QMatrix4x4:
            return typedRead<QMatrix4x4>(src, dstType, dst);
        default: break;
        }

        return false;
    }

    template<typename T>
    bool typedWrite(const void *src, QVariant& dst)
    {
        const T *srcT = reinterpret_cast<const T *>(src);
        if (dst.value<T>() != *srcT) {
            dst = *srcT;
            return true;
        }
        return false;
    }

    bool write(int type, const void *src, QVariant& dst) Q_DECL_OVERRIDE
    {
        switch (type) {
        case QMetaType::QColor:
            return typedWrite<QColor>(src, dst);
        case QMetaType::QVector2D:
            return typedWrite<QVector2D>(src, dst);
        case QMetaType::QVector3D:
            return typedWrite<QVector3D>(src, dst);
        case QMetaType::QVector4D:
            return typedWrite<QVector4D>(src, dst);
        case QMetaType::QQuaternion:
            return typedWrite<QQuaternion>(src, dst);
        case QMetaType::QMatrix4x4:
            return typedWrite<QMatrix4x4>(src, dst);
        default: break;
        }

        return false;
    }
#undef ASSERT_VALID_SIZE
};

static Quick3DValueTypeProvider *getValueTypeProvider()
{
    static Quick3DValueTypeProvider valueTypeProvider;
    return &valueTypeProvider;
}

static Quick3DColorProvider *getColorProvider()
{
    static Quick3DColorProvider colorProvider;
    return &colorProvider;
}

static QQmlPrivate::AutoParentResult qquick3ditem_autoParent(QObject *obj, QObject *parent)
{
    // When setting a parent (especially during dynamic object creation) in QML,
    // also try to set up the analogous item/window relationship.
    auto parentNode = qmlobject_cast<Qt3DCore::QNode *>(parent);
    if (parentNode) {
        auto node = qmlobject_cast<Qt3DCore::QNode *>(obj);
        if (node) {
            // A QNode has another QNode child
            node->setParent(parentNode);
            return QQmlPrivate::Parented;
        }
    } else {
        return QQmlPrivate::IncompatibleParent;
    }
    return QQmlPrivate::IncompatibleObject;
}

void Quick3D_initialize()
{
    Qt3DCore::Quick::Quick3DValueTypes::registerValueTypes();
    QQml_addValueTypeProvider(getValueTypeProvider());
    QQml_setColorProvider(getColorProvider());
    QAbstractNodeFactory::registerNodeFactory(QuickNodeFactory::instance());

    // Register a hook called when we do component.create() that sets the
    // parent. We need this as QObject::setParent() is insufficient to propagate
    // the arbiter and scene to the children (see QNode::setParent(QNode *).
    // TODO: Replace this with virtual void QObjectPrivate::setParent(QObject *)
    // that can be called from QObject ctor and QObject::setParent(). That would
    // allow removal of this hook here and in QtQuick.
    QQmlPrivate::RegisterAutoParent autoparent = { 0, &qquick3ditem_autoParent };
    QQmlPrivate::qmlregister(QQmlPrivate::AutoParentRegistration, &autoparent);
}

void Quick3D_registerType(const char *className, const char *quickName, int major, int minor)
{
    QuickNodeFactory::instance()->registerType(className, quickName, major, minor);
}

} // namespace Quick
} // namespace Qt3DCore

QT_END_NAMESPACE
