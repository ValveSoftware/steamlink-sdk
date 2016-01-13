/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QQMLVALUETYPE_P_H
#define QQMLVALUETYPE_P_H

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

#include "qqml.h"
#include "qqmlproperty.h"
#include "qqmlproperty_p.h"
#include "qqmlnullablevalue_p_p.h"

#include <QtCore/qobject.h>
#include <QtCore/qrect.h>
#include <QtCore/qeasingcurve.h>
#include <QtCore/qvariant.h>

QT_BEGIN_NAMESPACE

class Q_QML_PRIVATE_EXPORT QQmlValueType : public QObject
{
    Q_OBJECT
public:
    QQmlValueType(int userType, QObject *parent = 0);
    virtual void read(QObject *, int) = 0;
    virtual void readVariantValue(QObject *, int, QVariant *) = 0;
    virtual void write(QObject *, int, QQmlPropertyPrivate::WriteFlags flags) = 0;
    virtual void writeVariantValue(QObject *, int, QQmlPropertyPrivate::WriteFlags, QVariant *) = 0;
    virtual QVariant value() = 0;
    virtual void setValue(const QVariant &) = 0;

    virtual QString toString() const = 0;
    virtual bool isEqual(const QVariant &value) const = 0;

    virtual void onLoad() {}

    inline int userType() const
    {
        return m_userType;
    }

protected:
    inline void readProperty(QObject *obj, int idx, void *p)
    {
        void *a[] = { p, 0 };
        QMetaObject::metacall(obj, QMetaObject::ReadProperty, idx, a);
        onLoad();
    }

    inline void writeProperty(QObject *obj, int idx, QQmlPropertyPrivate::WriteFlags flags, void *p)
    {
        int status = -1;
        void *a[] = { p, 0, &status, &flags };
        QMetaObject::metacall(obj, QMetaObject::WriteProperty, idx, a);
    }

private:
    int m_userType;
};

template <typename T>
class QQmlValueTypeBase : public QQmlValueType
{
public:
    typedef T ValueType;

    QQmlValueTypeBase(int userType, QObject *parent)
        : QQmlValueType(userType, parent)
    {
    }

    virtual void read(QObject *obj, int idx)
    {
        readProperty(obj, idx, &v);
    }

    virtual void readVariantValue(QObject *obj, int idx, QVariant *into)
    {
        // important: must not change the userType of the variant
        readProperty(obj, idx, into);
    }

    virtual void write(QObject *obj, int idx, QQmlPropertyPrivate::WriteFlags flags)
    {
        writeProperty(obj, idx, flags, &v);
    }

    virtual void writeVariantValue(QObject *obj, int idx, QQmlPropertyPrivate::WriteFlags flags, QVariant *from)
    {
        writeProperty(obj, idx, flags, from);
    }

    virtual QVariant value()
    {
        return QVariant::fromValue(v);
    }

    virtual void setValue(const QVariant &value)
    {
        v = qvariant_cast<T>(value);
        onLoad();
    }

    virtual bool isEqual(const QVariant &other) const
    {
        return QVariant::fromValue(v) == other;
    }

protected:
    ValueType v;
};

class Q_QML_PRIVATE_EXPORT QQmlValueTypeFactory
{
public:
    static bool isValueType(int);
    static QQmlValueType *valueType(int idx);

    static void registerValueTypes(const char *uri, int versionMajor, int versionMinor);
};

class Q_QML_PRIVATE_EXPORT QQmlPointFValueType : public QQmlValueTypeBase<QPointF>
{
    Q_PROPERTY(qreal x READ x WRITE setX FINAL)
    Q_PROPERTY(qreal y READ y WRITE setY FINAL)
    Q_OBJECT
public:
    QQmlPointFValueType(QObject *parent = 0);

    virtual QString toString() const;

    qreal x() const;
    qreal y() const;
    void setX(qreal);
    void setY(qreal);
};

class Q_QML_PRIVATE_EXPORT QQmlPointValueType : public QQmlValueTypeBase<QPoint>
{
    Q_PROPERTY(int x READ x WRITE setX FINAL)
    Q_PROPERTY(int y READ y WRITE setY FINAL)
    Q_OBJECT
public:
    QQmlPointValueType(QObject *parent = 0);

    virtual QString toString() const;

    int x() const;
    int y() const;
    void setX(int);
    void setY(int);
};

class Q_QML_PRIVATE_EXPORT QQmlSizeFValueType : public QQmlValueTypeBase<QSizeF>
{
    Q_PROPERTY(qreal width READ width WRITE setWidth FINAL)
    Q_PROPERTY(qreal height READ height WRITE setHeight FINAL)
    Q_OBJECT
public:
    QQmlSizeFValueType(QObject *parent = 0);

    virtual QString toString() const;

    qreal width() const;
    qreal height() const;
    void setWidth(qreal);
    void setHeight(qreal);
};

class Q_QML_PRIVATE_EXPORT QQmlSizeValueType : public QQmlValueTypeBase<QSize>
{
    Q_PROPERTY(int width READ width WRITE setWidth FINAL)
    Q_PROPERTY(int height READ height WRITE setHeight FINAL)
    Q_OBJECT
public:
    QQmlSizeValueType(QObject *parent = 0);

    virtual QString toString() const;

    int width() const;
    int height() const;
    void setWidth(int);
    void setHeight(int);
};

class Q_QML_PRIVATE_EXPORT QQmlRectFValueType : public QQmlValueTypeBase<QRectF>
{
    Q_PROPERTY(qreal x READ x WRITE setX FINAL)
    Q_PROPERTY(qreal y READ y WRITE setY FINAL)
    Q_PROPERTY(qreal width READ width WRITE setWidth FINAL)
    Q_PROPERTY(qreal height READ height WRITE setHeight FINAL)
    Q_OBJECT
public:
    QQmlRectFValueType(QObject *parent = 0);

    virtual QString toString() const;

    qreal x() const;
    qreal y() const;
    void setX(qreal);
    void setY(qreal);

    qreal width() const;
    qreal height() const;
    void setWidth(qreal);
    void setHeight(qreal);
};

class Q_QML_PRIVATE_EXPORT QQmlRectValueType : public QQmlValueTypeBase<QRect>
{
    Q_PROPERTY(int x READ x WRITE setX FINAL)
    Q_PROPERTY(int y READ y WRITE setY FINAL)
    Q_PROPERTY(int width READ width WRITE setWidth FINAL)
    Q_PROPERTY(int height READ height WRITE setHeight FINAL)
    Q_OBJECT
public:
    QQmlRectValueType(QObject *parent = 0);

    virtual QString toString() const;

    int x() const;
    int y() const;
    void setX(int);
    void setY(int);

    int width() const;
    int height() const;
    void setWidth(int);
    void setHeight(int);
};

class Q_QML_PRIVATE_EXPORT QQmlEasingValueType : public QQmlValueTypeBase<QEasingCurve>
{
    Q_OBJECT
    Q_ENUMS(Type)

    Q_PROPERTY(QQmlEasingValueType::Type type READ type WRITE setType FINAL)
    Q_PROPERTY(qreal amplitude READ amplitude WRITE setAmplitude FINAL)
    Q_PROPERTY(qreal overshoot READ overshoot WRITE setOvershoot FINAL)
    Q_PROPERTY(qreal period READ period WRITE setPeriod FINAL)
    Q_PROPERTY(QVariantList bezierCurve READ bezierCurve WRITE setBezierCurve FINAL)
public:
    enum Type {
        Linear = QEasingCurve::Linear,
        InQuad = QEasingCurve::InQuad, OutQuad = QEasingCurve::OutQuad,
        InOutQuad = QEasingCurve::InOutQuad, OutInQuad = QEasingCurve::OutInQuad,
        InCubic = QEasingCurve::InCubic, OutCubic = QEasingCurve::OutCubic,
        InOutCubic = QEasingCurve::InOutCubic, OutInCubic = QEasingCurve::OutInCubic,
        InQuart = QEasingCurve::InQuart, OutQuart = QEasingCurve::OutQuart,
        InOutQuart = QEasingCurve::InOutQuart, OutInQuart = QEasingCurve::OutInQuart,
        InQuint = QEasingCurve::InQuint, OutQuint = QEasingCurve::OutQuint,
        InOutQuint = QEasingCurve::InOutQuint, OutInQuint = QEasingCurve::OutInQuint,
        InSine = QEasingCurve::InSine, OutSine = QEasingCurve::OutSine,
        InOutSine = QEasingCurve::InOutSine, OutInSine = QEasingCurve::OutInSine,
        InExpo = QEasingCurve::InExpo, OutExpo = QEasingCurve::OutExpo,
        InOutExpo = QEasingCurve::InOutExpo, OutInExpo = QEasingCurve::OutInExpo,
        InCirc = QEasingCurve::InCirc, OutCirc = QEasingCurve::OutCirc,
        InOutCirc = QEasingCurve::InOutCirc, OutInCirc = QEasingCurve::OutInCirc,
        InElastic = QEasingCurve::InElastic, OutElastic = QEasingCurve::OutElastic,
        InOutElastic = QEasingCurve::InOutElastic, OutInElastic = QEasingCurve::OutInElastic,
        InBack = QEasingCurve::InBack, OutBack = QEasingCurve::OutBack,
        InOutBack = QEasingCurve::InOutBack, OutInBack = QEasingCurve::OutInBack,
        InBounce = QEasingCurve::InBounce, OutBounce = QEasingCurve::OutBounce,
        InOutBounce = QEasingCurve::InOutBounce, OutInBounce = QEasingCurve::OutInBounce,
        InCurve = QEasingCurve::InCurve, OutCurve = QEasingCurve::OutCurve,
        SineCurve = QEasingCurve::SineCurve, CosineCurve = QEasingCurve::CosineCurve,
        Bezier = QEasingCurve::BezierSpline
    };

    QQmlEasingValueType(QObject *parent = 0);

    virtual QString toString() const;

    Type type() const;
    qreal amplitude() const;
    qreal overshoot() const;
    qreal period() const;
    void setType(Type);
    void setAmplitude(qreal);
    void setOvershoot(qreal);
    void setPeriod(qreal);
    void setBezierCurve(const QVariantList &);
    QVariantList bezierCurve() const;
};

template<typename T>
int qmlRegisterValueTypeEnums(const char *uri, int versionMajor, int versionMinor, const char *qmlName)
{
    QByteArray name(T::staticMetaObject.className());

    QByteArray pointerName(name + '*');

    QQmlPrivate::RegisterType type = {
        0,

        qRegisterNormalizedMetaType<T *>(pointerName.constData()), 0, 0, 0,

        QString(),

        uri, versionMajor, versionMinor, qmlName, &T::staticMetaObject,

        0, 0,

        0, 0, 0,

        0, 0,

        0,
        0
    };

    return QQmlPrivate::qmlregister(QQmlPrivate::TypeRegistration, &type);
}

QT_END_NAMESPACE

#endif  // QQMLVALUETYPE_P_H
