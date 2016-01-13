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

#include "qqmlbuiltinfunctions_p.h"

#include <QtQml/qqmlcomponent.h>
#include <private/qqmlengine_p.h>
#include <private/qqmlcomponent_p.h>
#include <private/qqmlstringconverters_p.h>
#include <private/qqmllocale_p.h>
#include <private/qv8engine_p.h>
#include <QFileInfo>

#include <private/qqmlprofilerservice_p.h>
#include <private/qqmlglobal_p.h>

#include <private/qqmlplatform_p.h>

#include <private/qv4engine_p.h>
#include <private/qv4functionobject_p.h>
#include <private/qv4include_p.h>
#include <private/qv4context_p.h>
#include <private/qv4stringobject_p.h>
#include <private/qv4mm_p.h>
#include <private/qv4jsonobject_p.h>
#include <private/qv4objectproto_p.h>

#include <QtCore/qstring.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qcryptographichash.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qpoint.h>
#include <QtCore/qurl.h>
#include <QtCore/qfile.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qloggingcategory.h>

QT_BEGIN_NAMESPACE

using namespace QV4;

DEFINE_OBJECT_VTABLE(QtObject);

struct StaticQtMetaObject : public QObject
{
    static const QMetaObject *get()
        { return &staticQtMetaObject; }
};

QV4::QtObject::Data::Data(ExecutionEngine *v4, QQmlEngine *qmlEngine)
    : Object::Data(v4)
{
    setVTable(staticVTable());

    Scope scope(v4);
    ScopedObject o(scope, this);

    // Set all the enums from the "Qt" namespace
    const QMetaObject *qtMetaObject = StaticQtMetaObject::get();
    ScopedString str(scope);
    ScopedValue v(scope);
    for (int ii = 0; ii < qtMetaObject->enumeratorCount(); ++ii) {
        QMetaEnum enumerator = qtMetaObject->enumerator(ii);
        for (int jj = 0; jj < enumerator.keyCount(); ++jj) {
            o->put((str = v4->newString(QString::fromUtf8(enumerator.key(jj)))).getPointer(), (v = QV4::Primitive::fromInt32(enumerator.value(jj))));
        }
    }
    o->put((str = v4->newString(QStringLiteral("Asynchronous"))).getPointer(), (v = QV4::Primitive::fromInt32(0)));
    o->put((str = v4->newString(QStringLiteral("Synchronous"))).getPointer(), (v = QV4::Primitive::fromInt32(1)));

    o->defineDefaultProperty(QStringLiteral("include"), QV4Include::method_include);
    o->defineDefaultProperty(QStringLiteral("isQtObject"), method_isQtObject);
    o->defineDefaultProperty(QStringLiteral("rgba"), method_rgba);
    o->defineDefaultProperty(QStringLiteral("hsla"), method_hsla);
    o->defineDefaultProperty(QStringLiteral("colorEqual"), method_colorEqual);
    o->defineDefaultProperty(QStringLiteral("rect"), method_rect);
    o->defineDefaultProperty(QStringLiteral("point"), method_point);
    o->defineDefaultProperty(QStringLiteral("size"), method_size);
    o->defineDefaultProperty(QStringLiteral("font"), method_font);

    o->defineDefaultProperty(QStringLiteral("vector2d"), method_vector2d);
    o->defineDefaultProperty(QStringLiteral("vector3d"), method_vector3d);
    o->defineDefaultProperty(QStringLiteral("vector4d"), method_vector4d);
    o->defineDefaultProperty(QStringLiteral("quaternion"), method_quaternion);
    o->defineDefaultProperty(QStringLiteral("matrix4x4"), method_matrix4x4);

    o->defineDefaultProperty(QStringLiteral("formatDate"), method_formatDate);
    o->defineDefaultProperty(QStringLiteral("formatTime"), method_formatTime);
    o->defineDefaultProperty(QStringLiteral("formatDateTime"), method_formatDateTime);

    o->defineDefaultProperty(QStringLiteral("openUrlExternally"), method_openUrlExternally);
    o->defineDefaultProperty(QStringLiteral("fontFamilies"), method_fontFamilies);
    o->defineDefaultProperty(QStringLiteral("md5"), method_md5);
    o->defineDefaultProperty(QStringLiteral("btoa"), method_btoa);
    o->defineDefaultProperty(QStringLiteral("atob"), method_atob);
    o->defineDefaultProperty(QStringLiteral("resolvedUrl"), method_resolvedUrl);
    o->defineDefaultProperty(QStringLiteral("locale"), method_locale);
    o->defineDefaultProperty(QStringLiteral("binding"), method_binding);

    if (qmlEngine) {
        o->defineDefaultProperty(QStringLiteral("lighter"), method_lighter);
        o->defineDefaultProperty(QStringLiteral("darker"), method_darker);
        o->defineDefaultProperty(QStringLiteral("tint"), method_tint);
        o->defineDefaultProperty(QStringLiteral("quit"), method_quit);
        o->defineDefaultProperty(QStringLiteral("createQmlObject"), method_createQmlObject);
        o->defineDefaultProperty(QStringLiteral("createComponent"), method_createComponent);
    }

    o->defineAccessorProperty(QStringLiteral("platform"), method_get_platform, 0);
    o->defineAccessorProperty(QStringLiteral("application"), method_get_application, 0);
#ifndef QT_NO_IM
    o->defineAccessorProperty(QStringLiteral("inputMethod"), method_get_inputMethod, 0);
#endif
}


/*!
\qmlmethod bool Qt::isQtObject(object)
Returns true if \c object is a valid reference to a Qt or QML object, otherwise false.
*/
ReturnedValue QtObject::method_isQtObject(QV4::CallContext *ctx)
{
    if (ctx->d()->callData->argc == 0)
        return QV4::Encode(false);

    return QV4::Encode(ctx->d()->callData->args[0].as<QV4::QObjectWrapper>() != 0);
}

/*!
\qmlmethod color Qt::rgba(real red, real green, real blue, real alpha)

Returns a color with the specified \c red, \c green, \c blue and \c alpha components.
All components should be in the range 0-1 inclusive.
*/
ReturnedValue QtObject::method_rgba(QV4::CallContext *ctx)
{
    int argCount = ctx->d()->callData->argc;
    if (argCount < 3 || argCount > 4)
        V4THROW_ERROR("Qt.rgba(): Invalid arguments");

    double r = ctx->d()->callData->args[0].toNumber();
    double g = ctx->d()->callData->args[1].toNumber();
    double b = ctx->d()->callData->args[2].toNumber();
    double a = (argCount == 4) ? ctx->d()->callData->args[3].toNumber() : 1;

    if (r < 0.0) r=0.0;
    if (r > 1.0) r=1.0;
    if (g < 0.0) g=0.0;
    if (g > 1.0) g=1.0;
    if (b < 0.0) b=0.0;
    if (b > 1.0) b=1.0;
    if (a < 0.0) a=0.0;
    if (a > 1.0) a=1.0;

    return ctx->d()->engine->v8Engine->fromVariant(QQml_colorProvider()->fromRgbF(r, g, b, a));
}

/*!
\qmlmethod color Qt::hsla(real hue, real saturation, real lightness, real alpha)

Returns a color with the specified \c hue, \c saturation, \c lightness and \c alpha components.
All components should be in the range 0-1 inclusive.
*/
ReturnedValue QtObject::method_hsla(QV4::CallContext *ctx)
{
    int argCount = ctx->d()->callData->argc;
    if (argCount < 3 || argCount > 4)
        V4THROW_ERROR("Qt.hsla(): Invalid arguments");

    double h = ctx->d()->callData->args[0].toNumber();
    double s = ctx->d()->callData->args[1].toNumber();
    double l = ctx->d()->callData->args[2].toNumber();
    double a = (argCount == 4) ? ctx->d()->callData->args[3].toNumber() : 1;

    if (h < 0.0) h=0.0;
    if (h > 1.0) h=1.0;
    if (s < 0.0) s=0.0;
    if (s > 1.0) s=1.0;
    if (l < 0.0) l=0.0;
    if (l > 1.0) l=1.0;
    if (a < 0.0) a=0.0;
    if (a > 1.0) a=1.0;

    return ctx->d()->engine->v8Engine->fromVariant(QQml_colorProvider()->fromHslF(h, s, l, a));
}

/*!
\qmlmethod color Qt::colorEqual(color lhs, string rhs)

Returns true if both \c lhs and \c rhs yield equal color values.  Both arguments
may be either color values or string values.  If a string value is supplied it
must be convertible to a color, as described for the \l{colorbasictypedocs}{color}
basic type.
*/
ReturnedValue QtObject::method_colorEqual(QV4::CallContext *ctx)
{
    if (ctx->d()->callData->argc != 2)
        V4THROW_ERROR("Qt.colorEqual(): Invalid arguments");

    bool ok = false;

    QV8Engine *v8engine = ctx->d()->engine->v8Engine;

    QVariant lhs = v8engine->toVariant(ctx->d()->callData->args[0], -1);
    if (lhs.userType() == QVariant::String) {
        lhs = QQmlStringConverters::colorFromString(lhs.toString(), &ok);
        if (!ok) {
            V4THROW_ERROR("Qt.colorEqual(): Invalid color name");
        }
    } else if (lhs.userType() != QVariant::Color) {
        V4THROW_ERROR("Qt.colorEqual(): Invalid arguments");
    }

    QVariant rhs = v8engine->toVariant(ctx->d()->callData->args[1], -1);
    if (rhs.userType() == QVariant::String) {
        rhs = QQmlStringConverters::colorFromString(rhs.toString(), &ok);
        if (!ok) {
            V4THROW_ERROR("Qt.colorEqual(): Invalid color name");
        }
    } else if (rhs.userType() != QVariant::Color) {
        V4THROW_ERROR("Qt.colorEqual(): Invalid arguments");
    }

    bool equal = (lhs == rhs);
    return QV4::Encode(equal);
}

/*!
\qmlmethod rect Qt::rect(int x, int y, int width, int height)

Returns a \c rect with the top-left corner at \c x, \c y and the specified \c width and \c height.

The returned object has \c x, \c y, \c width and \c height attributes with the given values.
*/
ReturnedValue QtObject::method_rect(QV4::CallContext *ctx)
{
    if (ctx->d()->callData->argc != 4)
        V4THROW_ERROR("Qt.rect(): Invalid arguments");

    double x = ctx->d()->callData->args[0].toNumber();
    double y = ctx->d()->callData->args[1].toNumber();
    double w = ctx->d()->callData->args[2].toNumber();
    double h = ctx->d()->callData->args[3].toNumber();

    return ctx->d()->engine->v8Engine->fromVariant(QVariant::fromValue(QRectF(x, y, w, h)));
}

/*!
\qmlmethod point Qt::point(int x, int y)
Returns a Point with the specified \c x and \c y coordinates.
*/
ReturnedValue QtObject::method_point(QV4::CallContext *ctx)
{
    if (ctx->d()->callData->argc != 2)
        V4THROW_ERROR("Qt.point(): Invalid arguments");

    double x = ctx->d()->callData->args[0].toNumber();
    double y = ctx->d()->callData->args[1].toNumber();

    return ctx->d()->engine->v8Engine->fromVariant(QVariant::fromValue(QPointF(x, y)));
}

/*!
\qmlmethod Qt::size(int width, int height)
Returns a Size with the specified \c width and \c height.
*/
ReturnedValue QtObject::method_size(QV4::CallContext *ctx)
{
    if (ctx->d()->callData->argc != 2)
        V4THROW_ERROR("Qt.size(): Invalid arguments");

    double w = ctx->d()->callData->args[0].toNumber();
    double h = ctx->d()->callData->args[1].toNumber();

    return ctx->d()->engine->v8Engine->fromVariant(QVariant::fromValue(QSizeF(w, h)));
}

/*!
\qmlmethod Qt::font(object fontSpecifier)
Returns a Font with the properties specified in the \c fontSpecifier object
or the nearest matching font.  The \c fontSpecifier object should contain
key-value pairs where valid keys are the \l{fontbasictypedocs}{font} type's
subproperty names, and the values are valid values for each subproperty.
Invalid keys will be ignored.
*/
ReturnedValue QtObject::method_font(QV4::CallContext *ctx)
{
    if (ctx->d()->callData->argc != 1 || !ctx->d()->callData->args[0].isObject())
        V4THROW_ERROR("Qt.font(): Invalid arguments");

    QV8Engine *v8engine = ctx->d()->engine->v8Engine;
    bool ok = false;
    QVariant v = QQml_valueTypeProvider()->createVariantFromJsObject(QMetaType::QFont, QQmlV4Handle(ctx->d()->callData->args[0]), v8engine, &ok);
    if (!ok)
        V4THROW_ERROR("Qt.font(): Invalid argument: no valid font subproperties specified");
    return v8engine->fromVariant(v);
}



/*!
\qmlmethod Qt::vector2d(real x, real y)
Returns a Vector2D with the specified \c x and \c y.
*/
ReturnedValue QtObject::method_vector2d(QV4::CallContext *ctx)
{
    if (ctx->d()->callData->argc != 2)
        V4THROW_ERROR("Qt.vector2d(): Invalid arguments");

    float xy[3]; // qvector2d uses float internally
    xy[0] = ctx->d()->callData->args[0].toNumber();
    xy[1] = ctx->d()->callData->args[1].toNumber();

    const void *params[] = { xy };
    QV8Engine *v8engine = ctx->d()->engine->v8Engine;
    return v8engine->fromVariant(QQml_valueTypeProvider()->createValueType(QMetaType::QVector2D, 1, params));
}

/*!
\qmlmethod Qt::vector3d(real x, real y, real z)
Returns a Vector3D with the specified \c x, \c y and \c z.
*/
ReturnedValue QtObject::method_vector3d(QV4::CallContext *ctx)
{
    if (ctx->d()->callData->argc != 3)
        V4THROW_ERROR("Qt.vector3d(): Invalid arguments");

    float xyz[3]; // qvector3d uses float internally
    xyz[0] = ctx->d()->callData->args[0].toNumber();
    xyz[1] = ctx->d()->callData->args[1].toNumber();
    xyz[2] = ctx->d()->callData->args[2].toNumber();

    const void *params[] = { xyz };
    QV8Engine *v8engine = ctx->d()->engine->v8Engine;
    return v8engine->fromVariant(QQml_valueTypeProvider()->createValueType(QMetaType::QVector3D, 1, params));
}

/*!
\qmlmethod Qt::vector4d(real x, real y, real z, real w)
Returns a Vector4D with the specified \c x, \c y, \c z and \c w.
*/
ReturnedValue QtObject::method_vector4d(QV4::CallContext *ctx)
{
    if (ctx->d()->callData->argc != 4)
        V4THROW_ERROR("Qt.vector4d(): Invalid arguments");

    float xyzw[4]; // qvector4d uses float internally
    xyzw[0] = ctx->d()->callData->args[0].toNumber();
    xyzw[1] = ctx->d()->callData->args[1].toNumber();
    xyzw[2] = ctx->d()->callData->args[2].toNumber();
    xyzw[3] = ctx->d()->callData->args[3].toNumber();

    const void *params[] = { xyzw };
    QV8Engine *v8engine = ctx->d()->engine->v8Engine;
    return v8engine->fromVariant(QQml_valueTypeProvider()->createValueType(QMetaType::QVector4D, 1, params));
}

/*!
\qmlmethod Qt::quaternion(real scalar, real x, real y, real z)
Returns a Quaternion with the specified \c scalar, \c x, \c y, and \c z.
*/
ReturnedValue QtObject::method_quaternion(QV4::CallContext *ctx)
{
    if (ctx->d()->callData->argc != 4)
        V4THROW_ERROR("Qt.quaternion(): Invalid arguments");

    qreal sxyz[4]; // qquaternion uses qreal internally
    sxyz[0] = ctx->d()->callData->args[0].toNumber();
    sxyz[1] = ctx->d()->callData->args[1].toNumber();
    sxyz[2] = ctx->d()->callData->args[2].toNumber();
    sxyz[3] = ctx->d()->callData->args[3].toNumber();

    const void *params[] = { sxyz };
    QV8Engine *v8engine = ctx->d()->engine->v8Engine;
    return v8engine->fromVariant(QQml_valueTypeProvider()->createValueType(QMetaType::QQuaternion, 1, params));
}

/*!
\qmlmethod Qt::matrix4x4(real m11, real m12, real m13, real m14, real m21, real m22, real m23, real m24, real m31, real m32, real m33, real m34, real m41, real m42, real m43, real m44)
Returns a Matrix4x4 with the specified values.
Alternatively, the function may be called with a single argument
where that argument is a JavaScript array which contains the sixteen
matrix values.
*/
ReturnedValue QtObject::method_matrix4x4(QV4::CallContext *ctx)
{
    QV8Engine *v8engine = ctx->d()->engine->v8Engine;

    if (ctx->d()->callData->argc == 1 && ctx->d()->callData->args[0].isObject()) {
        bool ok = false;
        QVariant v = QQml_valueTypeProvider()->createVariantFromJsObject(QMetaType::QMatrix4x4, QQmlV4Handle(ctx->d()->callData->args[0]), v8engine, &ok);
        if (!ok)
            V4THROW_ERROR("Qt.matrix4x4(): Invalid argument: not a valid matrix4x4 values array");
        return v8engine->fromVariant(v);
    }

    if (ctx->d()->callData->argc != 16)
        V4THROW_ERROR("Qt.matrix4x4(): Invalid arguments");

    qreal vals[16]; // qmatrix4x4 uses qreal internally
    vals[0] = ctx->d()->callData->args[0].toNumber();
    vals[1] = ctx->d()->callData->args[1].toNumber();
    vals[2] = ctx->d()->callData->args[2].toNumber();
    vals[3] = ctx->d()->callData->args[3].toNumber();
    vals[4] = ctx->d()->callData->args[4].toNumber();
    vals[5] = ctx->d()->callData->args[5].toNumber();
    vals[6] = ctx->d()->callData->args[6].toNumber();
    vals[7] = ctx->d()->callData->args[7].toNumber();
    vals[8] = ctx->d()->callData->args[8].toNumber();
    vals[9] = ctx->d()->callData->args[9].toNumber();
    vals[10] = ctx->d()->callData->args[10].toNumber();
    vals[11] = ctx->d()->callData->args[11].toNumber();
    vals[12] = ctx->d()->callData->args[12].toNumber();
    vals[13] = ctx->d()->callData->args[13].toNumber();
    vals[14] = ctx->d()->callData->args[14].toNumber();
    vals[15] = ctx->d()->callData->args[15].toNumber();

    const void *params[] = { vals };
    return v8engine->fromVariant(QQml_valueTypeProvider()->createValueType(QMetaType::QMatrix4x4, 1, params));
}

/*!
\qmlmethod color Qt::lighter(color baseColor, real factor)
Returns a color lighter than \c baseColor by the \c factor provided.

If the factor is greater than 1.0, this functions returns a lighter color.
Setting factor to 1.5 returns a color that is 50% brighter. If the factor is less than 1.0,
the return color is darker, but we recommend using the Qt.darker() function for this purpose.
If the factor is 0 or negative, the return value is unspecified.

The function converts the current RGB color to HSV, multiplies the value (V) component
by factor and converts the color back to RGB.

If \c factor is not supplied, returns a color 50% lighter than \c baseColor (factor 1.5).
*/
ReturnedValue QtObject::method_lighter(QV4::CallContext *ctx)
{
    if (ctx->d()->callData->argc != 1 && ctx->d()->callData->argc != 2)
        V4THROW_ERROR("Qt.lighter(): Invalid arguments");

    QV8Engine *v8engine = ctx->d()->engine->v8Engine;
    QVariant v = v8engine->toVariant(ctx->d()->callData->args[0], -1);
    if (v.userType() == QVariant::String) {
        bool ok = false;
        v = QQmlStringConverters::colorFromString(v.toString(), &ok);
        if (!ok) {
            return QV4::Encode::null();
        }
    } else if (v.userType() != QVariant::Color) {
        return QV4::Encode::null();
    }

    qreal factor = 1.5;
    if (ctx->d()->callData->argc == 2)
        factor = ctx->d()->callData->args[1].toNumber();

    return v8engine->fromVariant(QQml_colorProvider()->lighter(v, factor));
}

/*!
\qmlmethod color Qt::darker(color baseColor, real factor)
Returns a color darker than \c baseColor by the \c factor provided.

If the factor is greater than 1.0, this function returns a darker color.
Setting factor to 3.0 returns a color that has one-third the brightness.
If the factor is less than 1.0, the return color is lighter, but we recommend using
the Qt.lighter() function for this purpose. If the factor is 0 or negative, the return
value is unspecified.

The function converts the current RGB color to HSV, divides the value (V) component
by factor and converts the color back to RGB.

If \c factor is not supplied, returns a color 50% darker than \c baseColor (factor 2.0).
*/
ReturnedValue QtObject::method_darker(QV4::CallContext *ctx)
{
    if (ctx->d()->callData->argc != 1 && ctx->d()->callData->argc != 2)
        V4THROW_ERROR("Qt.darker(): Invalid arguments");

    QV8Engine *v8engine = ctx->d()->engine->v8Engine;
    QVariant v = v8engine->toVariant(ctx->d()->callData->args[0], -1);
    if (v.userType() == QVariant::String) {
        bool ok = false;
        v = QQmlStringConverters::colorFromString(v.toString(), &ok);
        if (!ok) {
            return QV4::Encode::null();
        }
    } else if (v.userType() != QVariant::Color) {
        return QV4::Encode::null();
    }

    qreal factor = 2.0;
    if (ctx->d()->callData->argc == 2)
        factor = ctx->d()->callData->args[1].toNumber();

    return v8engine->fromVariant(QQml_colorProvider()->darker(v, factor));
}

/*!
    \qmlmethod color Qt::tint(color baseColor, color tintColor)
    This function allows tinting one color with another.

    The tint color should usually be mostly transparent, or you will not be
    able to see the underlying color. The below example provides a slight red
    tint by having the tint color be pure red which is only 1/16th opaque.

    \qml
    Item {
        Rectangle {
            x: 0; width: 80; height: 80
            color: "lightsteelblue"
        }
        Rectangle {
            x: 100; width: 80; height: 80
            color: Qt.tint("lightsteelblue", "#10FF0000")
        }
    }
    \endqml
    \image declarative-rect_tint.png

    Tint is most useful when a subtle change is intended to be conveyed due to some event; you can then use tinting to more effectively tune the visible color.
*/
ReturnedValue QtObject::method_tint(QV4::CallContext *ctx)
{
    if (ctx->d()->callData->argc != 2)
        V4THROW_ERROR("Qt.tint(): Invalid arguments");

    QV8Engine *v8engine = ctx->d()->engine->v8Engine;

    // base color
    QVariant v1 = v8engine->toVariant(ctx->d()->callData->args[0], -1);
    if (v1.userType() == QVariant::String) {
        bool ok = false;
        v1 = QQmlStringConverters::colorFromString(v1.toString(), &ok);
        if (!ok) {
            return QV4::Encode::null();
        }
    } else if (v1.userType() != QVariant::Color) {
        return QV4::Encode::null();
    }

    // tint color
    QVariant v2 = v8engine->toVariant(ctx->d()->callData->args[1], -1);
    if (v2.userType() == QVariant::String) {
        bool ok = false;
        v2 = QQmlStringConverters::colorFromString(v2.toString(), &ok);
        if (!ok) {
            return QV4::Encode::null();
        }
    } else if (v2.userType() != QVariant::Color) {
        return QV4::Encode::null();
    }

    return v8engine->fromVariant(QQml_colorProvider()->tint(v1, v2));
}

/*!
\qmlmethod string Qt::formatDate(datetime date, variant format)

Returns a string representation of \c date, optionally formatted according
to \c format.

The \a date parameter may be a JavaScript \c Date object, a \l{date}{date}
property, a QDate, or QDateTime value. The \a format parameter may be any of
the possible format values as described for
\l{QtQml::Qt::formatDateTime()}{Qt.formatDateTime()}.

If \a format is not specified, \a date is formatted using
\l {Qt::DefaultLocaleShortDate}{Qt.DefaultLocaleShortDate}.

\sa Locale
*/
ReturnedValue QtObject::method_formatDate(QV4::CallContext *ctx)
{
    if (ctx->d()->callData->argc < 1 || ctx->d()->callData->argc > 2)
        V4THROW_ERROR("Qt.formatDate(): Invalid arguments");
    QV4::Scope scope(ctx);

    QV8Engine *v8engine = ctx->d()->engine->v8Engine;

    Qt::DateFormat enumFormat = Qt::DefaultLocaleShortDate;
    QDate date = v8engine->toVariant(ctx->d()->callData->args[0], -1).toDateTime().date();
    QString formattedDate;
    if (ctx->d()->callData->argc == 2) {
        QV4::ScopedString s(scope, ctx->d()->callData->args[1]);
        if (s) {
            QString format = s->toQString();
            formattedDate = date.toString(format);
        } else if (ctx->d()->callData->args[1].isNumber()) {
            quint32 intFormat = ctx->d()->callData->args[1].asDouble();
            Qt::DateFormat format = Qt::DateFormat(intFormat);
            formattedDate = date.toString(format);
        } else {
            V4THROW_ERROR("Qt.formatDate(): Invalid date format");
        }
    } else {
         formattedDate = date.toString(enumFormat);
    }

    return ctx->d()->engine->newString(formattedDate)->asReturnedValue();
}

/*!
\qmlmethod string Qt::formatTime(datetime time, variant format)

Returns a string representation of \c time, optionally formatted according to
\c format.

The \a time parameter may be a JavaScript \c Date object, a QTime, or QDateTime
value. The \a format parameter may be any of the possible format values as
described for \l{QtQml::Qt::formatDateTime()}{Qt.formatDateTime()}.

If \a format is not specified, \a time is formatted using
\l {Qt::DefaultLocaleShortDate}{Qt.DefaultLocaleShortDate}.

\sa Locale
*/
ReturnedValue QtObject::method_formatTime(QV4::CallContext *ctx)
{
    if (ctx->d()->callData->argc < 1 || ctx->d()->callData->argc > 2)
        V4THROW_ERROR("Qt.formatTime(): Invalid arguments");
    QV4::Scope scope(ctx);

    QV8Engine *v8engine = ctx->d()->engine->v8Engine;

    QVariant argVariant = v8engine->toVariant(ctx->d()->callData->args[0], -1);
    QTime time;
    if (ctx->d()->callData->args[0].asDateObject() || (argVariant.type() == QVariant::String))
        time = argVariant.toDateTime().time();
    else // if (argVariant.type() == QVariant::Time), or invalid.
        time = argVariant.toTime();

    Qt::DateFormat enumFormat = Qt::DefaultLocaleShortDate;
    QString formattedTime;
    if (ctx->d()->callData->argc == 2) {
        QV4::ScopedString s(scope, ctx->d()->callData->args[1]);
        if (s) {
            QString format = s->toQString();
            formattedTime = time.toString(format);
        } else if (ctx->d()->callData->args[1].isNumber()) {
            quint32 intFormat = ctx->d()->callData->args[1].asDouble();
            Qt::DateFormat format = Qt::DateFormat(intFormat);
            formattedTime = time.toString(format);
        } else {
            V4THROW_ERROR("Qt.formatTime(): Invalid time format");
        }
    } else {
         formattedTime = time.toString(enumFormat);
    }

    return ctx->d()->engine->newString(formattedTime)->asReturnedValue();
}

/*!
\qmlmethod string Qt::formatDateTime(datetime dateTime, variant format)

Returns a string representation of \c datetime, optionally formatted according to
\c format.

The \a date parameter may be a JavaScript \c Date object, a \l{date}{date}
property, a QDate, QTime, or QDateTime value.

If \a format is not provided, \a dateTime is formatted using
\l {Qt::DefaultLocaleShortDate}{Qt.DefaultLocaleShortDate}. Otherwise,
\a format should be either:

\list
\li One of the Qt::DateFormat enumeration values, such as
   \c Qt.DefaultLocaleShortDate or \c Qt.ISODate
\li A string that specifies the format of the returned string, as detailed below.
\endlist

If \a format specifies a format string, it should use the following expressions
to specify the date:

    \table
    \header \li Expression \li Output
    \row \li d \li the day as number without a leading zero (1 to 31)
    \row \li dd \li the day as number with a leading zero (01 to 31)
    \row \li ddd
            \li the abbreviated localized day name (e.g. 'Mon' to 'Sun').
            Uses QDate::shortDayName().
    \row \li dddd
            \li the long localized day name (e.g. 'Monday' to 'Qt::Sunday').
            Uses QDate::longDayName().
    \row \li M \li the month as number without a leading zero (1-12)
    \row \li MM \li the month as number with a leading zero (01-12)
    \row \li MMM
            \li the abbreviated localized month name (e.g. 'Jan' to 'Dec').
            Uses QDate::shortMonthName().
    \row \li MMMM
            \li the long localized month name (e.g. 'January' to 'December').
            Uses QDate::longMonthName().
    \row \li yy \li the year as two digit number (00-99)
    \row \li yyyy \li the year as four digit number
    \endtable

In addition the following expressions can be used to specify the time:

    \table
    \header \li Expression \li Output
    \row \li h
         \li the hour without a leading zero (0 to 23 or 1 to 12 if AM/PM display)
    \row \li hh
         \li the hour with a leading zero (00 to 23 or 01 to 12 if AM/PM display)
    \row \li m \li the minute without a leading zero (0 to 59)
    \row \li mm \li the minute with a leading zero (00 to 59)
    \row \li s \li the second without a leading zero (0 to 59)
    \row \li ss \li the second with a leading zero (00 to 59)
    \row \li z \li the milliseconds without leading zeroes (0 to 999)
    \row \li zzz \li the milliseconds with leading zeroes (000 to 999)
    \row \li AP
            \li use AM/PM display. \e AP will be replaced by either "AM" or "PM".
    \row \li ap
            \li use am/pm display. \e ap will be replaced by either "am" or "pm".
    \endtable

    All other input characters will be ignored. Any sequence of characters that
    are enclosed in single quotes will be treated as text and not be used as an
    expression. Two consecutive single quotes ("''") are replaced by a single quote
    in the output.

For example, if the following date/time value was specified:

    \code
    // 21 May 2001 14:13:09
    var dateTime = new Date(2001, 5, 21, 14, 13, 09)
    \endcode

This \a dateTime value could be passed to \c Qt.formatDateTime(),
\l {QtQml::Qt::formatDate()}{Qt.formatDate()} or \l {QtQml::Qt::formatTime()}{Qt.formatTime()}
with the \a format values below to produce the following results:

    \table
    \header \li Format \li Result
    \row \li "dd.MM.yyyy"      \li 21.05.2001
    \row \li "ddd MMMM d yy"   \li Tue May 21 01
    \row \li "hh:mm:ss.zzz"    \li 14:13:09.042
    \row \li "h:m:s ap"        \li 2:13:9 pm
    \endtable

    \sa Locale
*/
ReturnedValue QtObject::method_formatDateTime(QV4::CallContext *ctx)
{
    if (ctx->d()->callData->argc < 1 || ctx->d()->callData->argc > 2)
        V4THROW_ERROR("Qt.formatDateTime(): Invalid arguments");
    QV4::Scope scope(ctx);

    QV8Engine *v8engine = ctx->d()->engine->v8Engine;

    Qt::DateFormat enumFormat = Qt::DefaultLocaleShortDate;
    QDateTime dt = v8engine->toVariant(ctx->d()->callData->args[0], -1).toDateTime();
    QString formattedDt;
    if (ctx->d()->callData->argc == 2) {
        QV4::ScopedString s(scope, ctx->d()->callData->args[1]);
        if (s) {
            QString format = s->toQString();
            formattedDt = dt.toString(format);
        } else if (ctx->d()->callData->args[1].isNumber()) {
            quint32 intFormat = ctx->d()->callData->args[1].asDouble();
            Qt::DateFormat format = Qt::DateFormat(intFormat);
            formattedDt = dt.toString(format);
        } else {
            V4THROW_ERROR("Qt.formatDateTime(): Invalid datetime format");
        }
    } else {
         formattedDt = dt.toString(enumFormat);
    }

    return ctx->d()->engine->newString(formattedDt)->asReturnedValue();
}

/*!
\qmlmethod bool Qt::openUrlExternally(url target)
Attempts to open the specified \c target url in an external application, based on the user's desktop preferences. Returns true if it succeeds, and false otherwise.
*/
ReturnedValue QtObject::method_openUrlExternally(QV4::CallContext *ctx)
{
    if (ctx->d()->callData->argc != 1)
        return QV4::Encode(false);

    QV8Engine *v8engine = ctx->d()->engine->v8Engine;

    QUrl url(Value::fromReturnedValue(method_resolvedUrl(ctx)).toQStringNoThrow());
    return v8engine->fromVariant(QQml_guiProvider()->openUrlExternally(url));
}

/*!
  \qmlmethod url Qt::resolvedUrl(url url)
  Returns \a url resolved relative to the URL of the caller.
*/
ReturnedValue QtObject::method_resolvedUrl(QV4::CallContext *ctx)
{
    QV8Engine *v8engine = ctx->d()->engine->v8Engine;

    QUrl url = v8engine->toVariant(ctx->d()->callData->args[0], -1).toUrl();
    QQmlEngine *e = v8engine->engine();
    QQmlEnginePrivate *p = 0;
    if (e) p = QQmlEnginePrivate::get(e);
    if (p) {
        QQmlContextData *ctxt = v8engine->callingContext();
        if (ctxt)
            return ctx->d()->engine->newString(ctxt->resolvedUrl(url).toString())->asReturnedValue();
        else
            return ctx->d()->engine->newString(url.toString())->asReturnedValue();
    }

    return ctx->d()->engine->newString(e->baseUrl().resolved(url).toString())->asReturnedValue();
}

/*!
\qmlmethod list<string> Qt::fontFamilies()
Returns a list of the font families available to the application.
*/
ReturnedValue QtObject::method_fontFamilies(CallContext *ctx)
{
    if (ctx->d()->callData->argc != 0)
        V4THROW_ERROR("Qt.fontFamilies(): Invalid arguments");

    QV8Engine *v8engine = ctx->d()->engine->v8Engine;
    return v8engine->fromVariant(QVariant(QQml_guiProvider()->fontFamilies()));
}

/*!
\qmlmethod string Qt::md5(data)
Returns a hex string of the md5 hash of \c data.
*/
ReturnedValue QtObject::method_md5(CallContext *ctx)
{
    if (ctx->d()->callData->argc != 1)
        V4THROW_ERROR("Qt.md5(): Invalid arguments");

    QByteArray data = ctx->d()->callData->args[0].toQStringNoThrow().toUtf8();
    QByteArray result = QCryptographicHash::hash(data, QCryptographicHash::Md5);
    return ctx->d()->engine->newString(QLatin1String(result.toHex()))->asReturnedValue();
}

/*!
\qmlmethod string Qt::btoa(data)
Binary to ASCII - this function returns a base64 encoding of \c data.
*/
ReturnedValue QtObject::method_btoa(CallContext *ctx)
{
    if (ctx->d()->callData->argc != 1)
        V4THROW_ERROR("Qt.btoa(): Invalid arguments");

    QByteArray data = ctx->d()->callData->args[0].toQStringNoThrow().toUtf8();

    return ctx->d()->engine->newString(QLatin1String(data.toBase64()))->asReturnedValue();
}

/*!
\qmlmethod string Qt::atob(data)
ASCII to binary - this function returns a base64 decoding of \c data.
*/
ReturnedValue QtObject::method_atob(CallContext *ctx)
{
    if (ctx->d()->callData->argc != 1)
        V4THROW_ERROR("Qt.atob(): Invalid arguments");

    QByteArray data = ctx->d()->callData->args[0].toQStringNoThrow().toLatin1();

    return ctx->d()->engine->newString(QString::fromUtf8(QByteArray::fromBase64(data)))->asReturnedValue();
}

/*!
\qmlmethod Qt::quit()
This function causes the QQmlEngine::quit() signal to be emitted.
Within the \l {Prototyping with qmlscene}, this causes the launcher application to exit;
to quit a C++ application when this method is called, connect the
QQmlEngine::quit() signal to the QCoreApplication::quit() slot.
*/
ReturnedValue QtObject::method_quit(CallContext *ctx)
{
    QV8Engine *v8engine = ctx->d()->engine->v8Engine;

    QQmlEnginePrivate::get(v8engine->engine())->sendQuit();
    return QV4::Encode::undefined();
}

/*!
\qmlmethod object Qt::createQmlObject(string qml, object parent, string filepath)

Returns a new object created from the given \a string of QML which will have the specified \a parent,
or \c null if there was an error in creating the object.

If \a filepath is specified, it will be used for error reporting for the created object.

Example (where \c parentItem is the id of an existing QML item):

\snippet qml/createQmlObject.qml 0

In the case of an error, a \l {Qt Script} Error object is thrown. This object has an additional property,
\c qmlErrors, which is an array of the errors encountered.
Each object in this array has the members \c lineNumber, \c columnNumber, \c fileName and \c message.
For example, if the above snippet had misspelled color as 'colro' then the array would contain an object like the following:
{ "lineNumber" : 1, "columnNumber" : 32, "fileName" : "dynamicSnippet1", "message" : "Cannot assign to non-existent property \"colro\""}.

Note that this function returns immediately, and therefore may not work if
the \a qml string loads new components (that is, external QML files that have not yet been loaded).
If this is the case, consider using \l{QtQml::Qt::createComponent()}{Qt.createComponent()} instead.

See \l {Dynamic QML Object Creation from JavaScript} for more information on using this function.
*/
ReturnedValue QtObject::method_createQmlObject(CallContext *ctx)
{
    Scope scope(ctx);
    if (ctx->d()->callData->argc < 2 || ctx->d()->callData->argc > 3)
        V4THROW_ERROR("Qt.createQmlObject(): Invalid arguments");

    struct Error {
        static ReturnedValue create(QV4::ExecutionEngine *v4, const QList<QQmlError> &errors) {
            Scope scope(v4);
            QString errorstr = QLatin1String("Qt.createQmlObject(): failed to create object: ");

            QV4::Scoped<ArrayObject> qmlerrors(scope, v4->newArrayObject());
            QV4::ScopedObject qmlerror(scope);
            QV4::ScopedString s(scope);
            QV4::ScopedValue v(scope);
            for (int ii = 0; ii < errors.count(); ++ii) {
                const QQmlError &error = errors.at(ii);
                errorstr += QLatin1String("\n    ") + error.toString();
                qmlerror = v4->newObject();
                qmlerror->put((s = v4->newString(QStringLiteral("lineNumber"))).getPointer(), (v = QV4::Primitive::fromInt32(error.line())));
                qmlerror->put((s = v4->newString(QStringLiteral("columnNumber"))).getPointer(), (v = QV4::Primitive::fromInt32(error.column())));
                qmlerror->put((s = v4->newString(QStringLiteral("fileName"))).getPointer(), (v = v4->newString(error.url().toString())));
                qmlerror->put((s = v4->newString(QStringLiteral("message"))).getPointer(), (v = v4->newString(error.description())));
                qmlerrors->putIndexed(ii, qmlerror);
            }

            v = v4->newString(errorstr);
            Scoped<Object> errorObject(scope, v4->newErrorObject(v));
            errorObject->put((s = v4->newString(QStringLiteral("qmlErrors"))).getPointer(), qmlerrors);
            return errorObject.asReturnedValue();
        }
    };

    QV8Engine *v8engine = ctx->d()->engine->v8Engine;
    QQmlEngine *engine = v8engine->engine();

    QQmlContextData *context = v8engine->callingContext();
    Q_ASSERT(context);
    QQmlContext *effectiveContext = 0;
    if (context->isPragmaLibraryContext)
        effectiveContext = engine->rootContext();
    else
        effectiveContext = context->asQQmlContext();
    Q_ASSERT(effectiveContext);

    QString qml = ctx->d()->callData->args[0].toQStringNoThrow();
    if (qml.isEmpty())
        return QV4::Encode::null();

    QUrl url;
    if (ctx->d()->callData->argc > 2)
        url = QUrl(ctx->d()->callData->args[2].toQStringNoThrow());
    else
        url = QUrl(QLatin1String("inline"));

    if (url.isValid() && url.isRelative())
        url = context->resolvedUrl(url);

    QObject *parentArg = 0;
    QV4::Scoped<QV4::QObjectWrapper> qobjectWrapper(scope, ctx->d()->callData->args[1]);
    if (!!qobjectWrapper)
        parentArg = qobjectWrapper->object();
    if (!parentArg)
        V4THROW_ERROR("Qt.createQmlObject(): Missing parent object");

    QQmlComponent component(engine);
    component.setData(qml.toUtf8(), url);

    if (component.isError()) {
        ScopedValue v(scope, Error::create(ctx->d()->engine, component.errors()));
        return ctx->throwError(v);
    }

    if (!component.isReady())
        V4THROW_ERROR("Qt.createQmlObject(): Component is not ready");

    QObject *obj = component.beginCreate(effectiveContext);
    if (obj) {
        QQmlData::get(obj, true)->explicitIndestructibleSet = false;
        QQmlData::get(obj)->indestructible = false;


        obj->setParent(parentArg);

        QList<QQmlPrivate::AutoParentFunction> functions = QQmlMetaType::parentFunctions();
        for (int ii = 0; ii < functions.count(); ++ii) {
            if (QQmlPrivate::Parented == functions.at(ii)(obj, parentArg))
                break;
        }
    }
    component.completeCreate();

    if (component.isError()) {
        ScopedValue v(scope, Error::create(ctx->d()->engine, component.errors()));
        return ctx->throwError(v);
    }

    Q_ASSERT(obj);

    return QV4::QObjectWrapper::wrap(ctx->d()->engine, obj);
}

/*!
\qmlmethod object Qt::createComponent(url, mode, parent)

Returns a \l Component object created using the QML file at the specified \a url,
or \c null if an empty string was given.

The returned component's \l Component::status property indicates whether the
component was successfully created. If the status is \c Component.Error,
see \l Component::errorString() for an error description.

If the optional \a mode parameter is set to \c Component.Asynchronous, the
component will be loaded in a background thread.  The Component::status property
will be \c Component.Loading while it is loading.  The status will change to
\c Component.Ready if the component loads successfully, or \c Component.Error
if loading fails.

If the optional \a parent parameter is given, it should refer to the object
that will become the parent for the created \l Component object.

Call \l {Component::createObject()}{Component.createObject()} on the returned
component to create an object instance of the component.

For example:

\snippet qml/createComponent-simple.qml 0

See \l {Dynamic QML Object Creation from JavaScript} for more information on using this function.

To create a QML object from an arbitrary string of QML (instead of a file),
use \l{QtQml::Qt::createQmlObject()}{Qt.createQmlObject()}.
*/
ReturnedValue QtObject::method_createComponent(CallContext *ctx)
{
    if (ctx->d()->callData->argc < 1 || ctx->d()->callData->argc > 3)
        return ctx->throwError(QStringLiteral("Qt.createComponent(): Invalid arguments"));

    Scope scope(ctx);

    QV8Engine *v8engine = ctx->d()->engine->v8Engine;
    QQmlEngine *engine = v8engine->engine();

    QQmlContextData *context = v8engine->callingContext();
    Q_ASSERT(context);
    QQmlContextData *effectiveContext = context;
    if (context->isPragmaLibraryContext)
        effectiveContext = 0;

    QString arg = ctx->d()->callData->args[0].toQStringNoThrow();
    if (arg.isEmpty())
        return QV4::Encode::null();

    QQmlComponent::CompilationMode compileMode = QQmlComponent::PreferSynchronous;
    QObject *parentArg = 0;

    int consumedCount = 1;
    if (ctx->d()->callData->argc > 1) {
        ScopedValue lastArg(scope, ctx->d()->callData->args[ctx->d()->callData->argc-1]);

        // The second argument could be the mode enum
        if (ctx->d()->callData->args[1].isInteger()) {
            int mode = ctx->d()->callData->args[1].integerValue();
            if (mode != int(QQmlComponent::PreferSynchronous) && mode != int(QQmlComponent::Asynchronous))
                return ctx->throwError(QStringLiteral("Qt.createComponent(): Invalid arguments"));
            compileMode = QQmlComponent::CompilationMode(mode);
            consumedCount += 1;
        } else {
            // The second argument could be the parent only if there are exactly two args
            if ((ctx->d()->callData->argc != 2) || !(lastArg->isObject() || lastArg->isNull()))
                return ctx->throwError(QStringLiteral("Qt.createComponent(): Invalid arguments"));
        }

        if (consumedCount < ctx->d()->callData->argc) {
            if (lastArg->isObject()) {
                Scoped<QObjectWrapper> qobjectWrapper(scope, lastArg);
                if (qobjectWrapper)
                    parentArg = qobjectWrapper->object();
                if (!parentArg)
                    return ctx->throwError(QStringLiteral("Qt.createComponent(): Invalid parent object"));
            } else if (lastArg->isNull()) {
                parentArg = 0;
            } else {
                return ctx->throwError(QStringLiteral("Qt.createComponent(): Invalid parent object"));
            }
        }
    }

    QUrl url = context->resolvedUrl(QUrl(arg));
    QQmlComponent *c = new QQmlComponent(engine, url, compileMode, parentArg);
    QQmlComponentPrivate::get(c)->creationContext = effectiveContext;
    QQmlData::get(c, true)->explicitIndestructibleSet = false;
    QQmlData::get(c)->indestructible = false;

    return QV4::QObjectWrapper::wrap(ctx->d()->engine, c);
}

/*!
    \qmlmethod Qt::locale(name)

    Returns a JS object representing the locale with the specified
    name, which has the format "language[_territory][.codeset][@modifier]"
    or "C", where:

    \list
    \li language is a lowercase, two-letter, ISO 639 language code,
    \li territory is an uppercase, two-letter, ISO 3166 country code,
    \li and codeset and modifier are ignored.
    \endlist

    If the string violates the locale format, or language is not a
    valid ISO 369 code, the "C" locale is used instead. If country
    is not present, or is not a valid ISO 3166 code, the most
    appropriate country is chosen for the specified language.

    \sa Locale
*/
ReturnedValue QtObject::method_locale(CallContext *ctx)
{
    QString code;
    if (ctx->d()->callData->argc > 1)
        V4THROW_ERROR("locale() requires 0 or 1 argument");
    if (ctx->d()->callData->argc == 1 && !ctx->d()->callData->args[0].isString())
        V4THROW_TYPE("locale(): argument (locale code) must be a string");

    QV8Engine *v8engine = ctx->d()->engine->v8Engine;
    if (ctx->d()->callData->argc == 1)
        code = ctx->d()->callData->args[0].toQStringNoThrow();

    return QQmlLocale::locale(v8engine, code);
}

QQmlBindingFunction::Data::Data(FunctionObject *originalFunction)
    : QV4::FunctionObject::Data(originalFunction->scope(), originalFunction->name())
    , originalFunction(originalFunction)
{
    setVTable(staticVTable());
    bindingKeyFlag = true;
}

void QQmlBindingFunction::initBindingLocation()
{
    QV4::StackFrame frame = engine()->currentStackFrame();
    d()->bindingLocation.sourceFile = frame.source;
    d()->bindingLocation.line = frame.line;
}

ReturnedValue QQmlBindingFunction::call(Managed *that, CallData *callData)
{
    QQmlBindingFunction *This = static_cast<QQmlBindingFunction*>(that);
    return This->d()->originalFunction->call(callData);
}

void QQmlBindingFunction::markObjects(Managed *that, ExecutionEngine *e)
{
    QQmlBindingFunction *This = static_cast<QQmlBindingFunction*>(that);
    This->d()->originalFunction->mark(e);
    QV4::FunctionObject::markObjects(that, e);
}

DEFINE_OBJECT_VTABLE(QQmlBindingFunction);

/*!
    \qmlmethod Qt::binding(function)

    Returns a JavaScript object representing a \l{Property Binding}{property binding}.

    There are two main use-cases for the function: firstly, to apply a
    property binding imperatively from JavaScript code:

    \snippet qml/qtBinding.1.qml 0

    and secondly, to apply a property binding when initializing property values
    of dynamically constructed objects (via \l{Component::createObject()}
    {Component.createObject()} or \l{Loader::setSource()}{Loader.setSource()}).

    For example, assuming the existence of a DynamicText component:
    \snippet qml/DynamicText.qml 0

    the output from:
    \snippet qml/qtBinding.2.qml 0

    and from:
    \snippet qml/qtBinding.3.qml 0

    should both be:
    \code
    Root text extra text
    Modified root text extra text
    Dynamic text extra text
    Modified dynamic text extra text
    \endcode

    This function cannot be used in property binding declarations
    (see the documentation on \l{qml-javascript-assignment}{binding
    declarations and binding assignments}) except when the result is
    stored in an array bound to a var property.

    \snippet qml/qtBinding.4.qml 0

    \note In \l {Qt Quick 1}, all function assignments were treated as
    binding assignments. The Qt.binding() function is new to
    \l {Qt Quick}{Qt Quick 2}.

    \since 5.0
*/
ReturnedValue QtObject::method_binding(CallContext *ctx)
{
    if (ctx->d()->callData->argc != 1)
        V4THROW_ERROR("binding() requires 1 argument");
    QV4::FunctionObject *f = ctx->d()->callData->args[0].asFunctionObject();
    if (!f)
        V4THROW_TYPE("binding(): argument (binding expression) must be a function");

    return (ctx->d()->engine->memoryManager->alloc<QQmlBindingFunction>(f))->asReturnedValue();
}


ReturnedValue QtObject::method_get_platform(CallContext *ctx)
{
    // ### inefficient. Should be just a value based getter
    Object *o = ctx->d()->callData->thisObject.asObject();
    if (!o)
        return ctx->throwTypeError();
    QtObject *qt = o->as<QtObject>();
    if (!qt)
        return ctx->throwTypeError();

    if (!qt->d()->platform)
        // Only allocate a platform object once
        qt->d()->platform = new QQmlPlatform(ctx->d()->engine->v8Engine->publicEngine());

    return QV4::QObjectWrapper::wrap(ctx->d()->engine, qt->d()->platform);
}

ReturnedValue QtObject::method_get_application(CallContext *ctx)
{
    // ### inefficient. Should be just a value based getter
    Object *o = ctx->d()->callData->thisObject.asObject();
    if (!o)
        return ctx->throwTypeError();
    QtObject *qt = o->as<QtObject>();
    if (!qt)
        return ctx->throwTypeError();

    if (!qt->d()->application)
        // Only allocate an application object once
        qt->d()->application = QQml_guiProvider()->application(ctx->d()->engine->v8Engine->publicEngine());

    return QV4::QObjectWrapper::wrap(ctx->d()->engine, qt->d()->application);
}

#ifndef QT_NO_IM
ReturnedValue QtObject::method_get_inputMethod(CallContext *ctx)
{
    QObject *o = QQml_guiProvider()->inputMethod();
    QQmlEngine::setObjectOwnership(o, QQmlEngine::CppOwnership);
    return QV4::QObjectWrapper::wrap(ctx->d()->engine, o);
}
#endif


QV4::ConsoleObject::Data::Data(ExecutionEngine *v4)
    : Object::Data(v4)
{
    QV4::Scope scope(v4);
    QV4::ScopedObject o(scope, this);

    o->defineDefaultProperty(QStringLiteral("debug"), method_log);
    o->defineDefaultProperty(QStringLiteral("log"), method_log);
    o->defineDefaultProperty(QStringLiteral("info"), method_log);
    o->defineDefaultProperty(QStringLiteral("warn"), method_warn);
    o->defineDefaultProperty(QStringLiteral("error"), method_error);
    o->defineDefaultProperty(QStringLiteral("assert"), method_assert);

    o->defineDefaultProperty(QStringLiteral("count"), method_count);
    o->defineDefaultProperty(QStringLiteral("profile"), method_profile);
    o->defineDefaultProperty(QStringLiteral("profileEnd"), method_profileEnd);
    o->defineDefaultProperty(QStringLiteral("time"), method_time);
    o->defineDefaultProperty(QStringLiteral("timeEnd"), method_timeEnd);
    o->defineDefaultProperty(QStringLiteral("trace"), method_trace);
    o->defineDefaultProperty(QStringLiteral("exception"), method_exception);
}


enum ConsoleLogTypes {
    Log,
    Warn,
    Error
};

static QString jsStack(QV4::ExecutionEngine *engine) {
    QString stack;

    QVector<QV4::StackFrame> stackTrace = engine->stackTrace(10);

    for (int i = 0; i < stackTrace.count(); i++) {
        const QV4::StackFrame &frame = stackTrace.at(i);

        QString stackFrame;
        if (frame.column >= 0)
            stackFrame = QString::fromLatin1("%1 (%2:%3:%4)").arg(frame.function,
                                                             frame.source,
                                                             QString::number(frame.line),
                                                             QString::number(frame.column));
        else
            stackFrame = QString::fromLatin1("%1 (%2:%3)").arg(frame.function,
                                                             frame.source,
                                                             QString::number(frame.line));

        if (i)
            stack += QLatin1Char('\n');
        stack += stackFrame;
    }
    return stack;
}

static QV4::ReturnedValue writeToConsole(ConsoleLogTypes logType, CallContext *ctx,
                                         bool printStack = false)
{
    QString result;
    QV4::ExecutionEngine *v4 = ctx->d()->engine;

    for (int i = 0; i < ctx->d()->callData->argc; ++i) {
        if (i != 0)
            result.append(QLatin1Char(' '));

        if (ctx->d()->callData->args[i].asArrayObject())
            result.append(QStringLiteral("[") + ctx->d()->callData->args[i].toQStringNoThrow() + QStringLiteral("]"));
        else
            result.append(ctx->d()->callData->args[i].toQStringNoThrow());
    }

    if (printStack) {
        result.append(QLatin1String("\n"));
        result.append(jsStack(v4));
    }

    static QLoggingCategory loggingCategory("qml");
    QV4::StackFrame frame = v4->currentStackFrame();
    const QByteArray baSource = frame.source.toUtf8();
    const QByteArray baFunction = frame.function.toUtf8();
    QMessageLogger logger(baSource.constData(), frame.line, baFunction.constData(), loggingCategory.categoryName());

    switch (logType) {
    case Log:
        if (loggingCategory.isDebugEnabled())
            logger.debug("%s", result.toUtf8().constData());
        break;
    case Warn:
        if (loggingCategory.isWarningEnabled())
            logger.warning("%s", result.toUtf8().constData());
        break;
    case Error:
        if (loggingCategory.isCriticalEnabled())
            logger.critical("%s", result.toUtf8().constData());
        break;
    default:
        break;
    }

    return QV4::Encode::undefined();
}

QV4::ReturnedValue ConsoleObject::method_error(CallContext *ctx)
{
    return writeToConsole(Error, ctx);
}

QV4::ReturnedValue ConsoleObject::method_log(CallContext *ctx)
{
    //console.log
    //console.debug
    //console.info
    //print
    return writeToConsole(Log, ctx);
}

QV4::ReturnedValue ConsoleObject::method_profile(CallContext *ctx)
{
    QV4::ExecutionEngine *v4 = ctx->d()->engine;

    QV4::StackFrame frame = v4->currentStackFrame();
    const QByteArray baSource = frame.source.toUtf8();
    const QByteArray baFunction = frame.function.toUtf8();
    QMessageLogger logger(baSource.constData(), frame.line, baFunction.constData());
    if (!QQmlDebugService::isDebuggingEnabled()) {
        logger.warning("Cannot start profiling because debug service is disabled. Start with -qmljsdebugger=port:XXXXX.");
    } else {
        QQmlProfilerService::instance()->startProfiling(v4->v8Engine->engine());
        logger.debug("Profiling started.");
    }

    return QV4::Encode::undefined();
}

QV4::ReturnedValue ConsoleObject::method_profileEnd(CallContext *ctx)
{
    QV4::ExecutionEngine *v4 = ctx->d()->engine;

    QV4::StackFrame frame = v4->currentStackFrame();
    const QByteArray baSource = frame.source.toUtf8();
    const QByteArray baFunction = frame.function.toUtf8();
    QMessageLogger logger(baSource.constData(), frame.line, baFunction.constData());

    if (!QQmlDebugService::isDebuggingEnabled()) {
        logger.warning("Ignoring console.profileEnd(): the debug service is disabled.");
    } else {
        QQmlProfilerService::instance()->stopProfiling(v4->v8Engine->engine());
        logger.debug("Profiling ended.");
    }

    return QV4::Encode::undefined();
}

QV4::ReturnedValue ConsoleObject::method_time(CallContext *ctx)
{
    if (ctx->d()->callData->argc != 1)
        V4THROW_ERROR("console.time(): Invalid arguments");

    QV8Engine *v8engine = ctx->d()->engine->v8Engine;

    QString name = ctx->d()->callData->args[0].toQStringNoThrow();
    v8engine->startTimer(name);
    return QV4::Encode::undefined();
}

QV4::ReturnedValue ConsoleObject::method_timeEnd(CallContext *ctx)
{
    if (ctx->d()->callData->argc != 1)
        V4THROW_ERROR("console.time(): Invalid arguments");

    QV8Engine *v8engine = ctx->d()->engine->v8Engine;

    QString name = ctx->d()->callData->args[0].toQStringNoThrow();
    bool wasRunning;
    qint64 elapsed = v8engine->stopTimer(name, &wasRunning);
    if (wasRunning) {
        qDebug("%s: %llims", qPrintable(name), elapsed);
    }
    return QV4::Encode::undefined();
}

QV4::ReturnedValue ConsoleObject::method_count(CallContext *ctx)
{
    // first argument: name to print. Ignore any additional arguments
    QString name;
    if (ctx->d()->callData->argc > 0)
        name = ctx->d()->callData->args[0].toQStringNoThrow();

    QV4::ExecutionEngine *v4 = ctx->d()->engine;
    QV8Engine *v8engine = ctx->d()->engine->v8Engine;

    QV4::StackFrame frame = v4->currentStackFrame();

    QString scriptName = frame.source;

    int value = v8engine->consoleCountHelper(scriptName, frame.line, frame.column);
    QString message = name + QLatin1String(": ") + QString::number(value);

    QMessageLogger(qPrintable(scriptName), frame.line,
                   qPrintable(frame.function))
        .debug("%s", qPrintable(message));

    return QV4::Encode::undefined();
}

QV4::ReturnedValue ConsoleObject::method_trace(CallContext *ctx)
{
    if (ctx->d()->callData->argc != 0)
        V4THROW_ERROR("console.trace(): Invalid arguments");

    QV4::ExecutionEngine *v4 = ctx->d()->engine;

    QString stack = jsStack(v4);

    QV4::StackFrame frame = v4->currentStackFrame();
    QMessageLogger(frame.source.toUtf8().constData(), frame.line,
                   frame.function.toUtf8().constData())
        .debug("%s", qPrintable(stack));

    return QV4::Encode::undefined();
}

QV4::ReturnedValue ConsoleObject::method_warn(CallContext *ctx)
{
    return writeToConsole(Warn, ctx);
}

QV4::ReturnedValue ConsoleObject::method_assert(CallContext *ctx)
{
    if (ctx->d()->callData->argc == 0)
        V4THROW_ERROR("console.assert(): Missing argument");

    QV4::ExecutionEngine *v4 = ctx->d()->engine;

    if (!ctx->d()->callData->args[0].toBoolean()) {
        QString message;
        for (int i = 1; i < ctx->d()->callData->argc; ++i) {
            if (i != 1)
                message.append(QLatin1Char(' '));

            message.append(ctx->d()->callData->args[i].toQStringNoThrow());
        }

        QString stack = jsStack(v4);

        QV4::StackFrame frame = v4->currentStackFrame();
        QMessageLogger(frame.source.toUtf8().constData(), frame.line,
                       frame.function.toUtf8().constData())
            .critical("%s\n%s",qPrintable(message), qPrintable(stack));

    }
    return QV4::Encode::undefined();
}

QV4::ReturnedValue ConsoleObject::method_exception(CallContext *ctx)
{
    if (ctx->d()->callData->argc == 0)
        V4THROW_ERROR("console.exception(): Missing argument");

    writeToConsole(Error, ctx, true);

    return QV4::Encode::undefined();
}



void QV4::GlobalExtensions::init(QQmlEngine *qmlEngine, Object *globalObject)
{
    ExecutionEngine *v4 = globalObject->engine();
    Scope scope(v4);

#ifndef QT_NO_TRANSLATION
    globalObject->defineDefaultProperty(QStringLiteral("qsTranslate"), method_qsTranslate);
    globalObject->defineDefaultProperty(QStringLiteral("QT_TRANSLATE_NOOP"), method_qsTranslateNoOp);
    globalObject->defineDefaultProperty(QStringLiteral("qsTr"), method_qsTr);
    globalObject->defineDefaultProperty(QStringLiteral("QT_TR_NOOP"), method_qsTrNoOp);
    globalObject->defineDefaultProperty(QStringLiteral("qsTrId"), method_qsTrId);
    globalObject->defineDefaultProperty(QStringLiteral("QT_TRID_NOOP"), method_qsTrIdNoOp);
#endif

    globalObject->defineDefaultProperty(QStringLiteral("print"), ConsoleObject::method_log);
    globalObject->defineDefaultProperty(QStringLiteral("gc"), method_gc);

    ScopedObject console(scope, v4->memoryManager->alloc<QV4::ConsoleObject>(v4));
    globalObject->defineDefaultProperty(QStringLiteral("console"), console);

    ScopedObject qt(scope, v4->memoryManager->alloc<QV4::QtObject>(v4, qmlEngine));
    globalObject->defineDefaultProperty(QStringLiteral("Qt"), qt);

    // string prototype extension
    v4->stringObjectClass->prototype->defineDefaultProperty(QStringLiteral("arg"), method_string_arg);
}


#ifndef QT_NO_TRANSLATION
/*!
    \qmlmethod string Qt::qsTranslate(string context, string sourceText, string disambiguation, int n)

    Returns a translated version of \a sourceText within the given \a context, optionally based on a
    \a disambiguation string and value of \a n for strings containing plurals;
    otherwise returns \a sourceText itself if no appropriate translated string
    is available.

    If the same \a sourceText is used in different roles within the
    same translation \a context, an additional identifying string may be passed in
    for \a disambiguation.

    Example:
    \snippet qml/qsTranslate.qml 0

    \sa {Internationalization and Localization with Qt Quick}
*/
ReturnedValue GlobalExtensions::method_qsTranslate(CallContext *ctx)
{
    if (ctx->d()->callData->argc < 2)
        V4THROW_ERROR("qsTranslate() requires at least two arguments");
    if (!ctx->d()->callData->args[0].isString())
        V4THROW_ERROR("qsTranslate(): first argument (context) must be a string");
    if (!ctx->d()->callData->args[1].isString())
        V4THROW_ERROR("qsTranslate(): second argument (sourceText) must be a string");
    if ((ctx->d()->callData->argc > 2) && !ctx->d()->callData->args[2].isString())
        V4THROW_ERROR("qsTranslate(): third argument (disambiguation) must be a string");

    QString context = ctx->d()->callData->args[0].toQStringNoThrow();
    QString text = ctx->d()->callData->args[1].toQStringNoThrow();
    QString comment;
    if (ctx->d()->callData->argc > 2) comment = ctx->d()->callData->args[2].toQStringNoThrow();

    int i = 3;
    if (ctx->d()->callData->argc > i && ctx->d()->callData->args[i].isString()) {
        qWarning("qsTranslate(): specifying the encoding as fourth argument is deprecated");
        ++i;
    }

    int n = -1;
    if (ctx->d()->callData->argc > i)
        n = ctx->d()->callData->args[i].toInt32();

    QString result = QCoreApplication::translate(context.toUtf8().constData(),
                                                 text.toUtf8().constData(),
                                                 comment.toUtf8().constData(),
                                                 n);

    return ctx->d()->engine->newString(result)->asReturnedValue();
}

/*!
    \qmlmethod string Qt::qsTranslateNoOp(string context, string sourceText, string disambiguation)

    Marks \a sourceText for dynamic translation in the given \a context; i.e, the stored \a sourceText
    will not be altered.

    If the same \a sourceText is used in different roles within the
    same translation context, an additional identifying string may be passed in
    for \a disambiguation.

    Returns the \a sourceText.

    QT_TRANSLATE_NOOP is used in conjunction with the dynamic translation functions
    qsTr() and qsTranslate(). It identifies a string as requiring
    translation (so it can be identified by \c lupdate), but leaves the actual
    translation to the dynamic functions.

    Example:
    \snippet qml/qtTranslateNoOp.qml 0

    \sa {Internationalization and Localization with Qt Quick}
*/
ReturnedValue GlobalExtensions::method_qsTranslateNoOp(CallContext *ctx)
{
    if (ctx->d()->callData->argc < 2)
        return QV4::Encode::undefined();
    return ctx->d()->callData->args[1].asReturnedValue();
}

/*!
    \qmlmethod string Qt::qsTr(string sourceText, string disambiguation, int n)

    Returns a translated version of \a sourceText, optionally based on a
    \a disambiguation string and value of \a n for strings containing plurals;
    otherwise returns \a sourceText itself if no appropriate translated string
    is available.

    If the same \a sourceText is used in different roles within the
    same translation context, an additional identifying string may be passed in
    for \a disambiguation.

    Example:
    \snippet qml/qsTr.qml 0

    \sa {Internationalization and Localization with Qt Quick}
*/
ReturnedValue GlobalExtensions::method_qsTr(CallContext *ctx)
{
    if (ctx->d()->callData->argc < 1)
        V4THROW_ERROR("qsTr() requires at least one argument");
    if (!ctx->d()->callData->args[0].isString())
        V4THROW_ERROR("qsTr(): first argument (sourceText) must be a string");
    if ((ctx->d()->callData->argc > 1) && !ctx->d()->callData->args[1].isString())
        V4THROW_ERROR("qsTr(): second argument (disambiguation) must be a string");
    if ((ctx->d()->callData->argc > 2) && !ctx->d()->callData->args[2].isNumber())
        V4THROW_ERROR("qsTr(): third argument (n) must be a number");

    QV8Engine *v8engine = ctx->d()->engine->v8Engine;
    QString context;
    if (QQmlContextData *ctxt = v8engine->callingContext()) {
        QString path = ctxt->url.toString();
        int lastSlash = path.lastIndexOf(QLatin1Char('/'));
        int lastDot = path.lastIndexOf(QLatin1Char('.'));
        int length = lastDot - (lastSlash + 1);
        context = (lastSlash > -1) ? path.mid(lastSlash + 1, (length > -1) ? length : -1) : QString();
    } else if (QV4::ExecutionContext *parentCtx = ctx->d()->parent) {
        // The first non-empty source URL in the call stack determines the translation context.
        while (parentCtx && context.isEmpty()) {
            if (QV4::CompiledData::CompilationUnit *unit = parentCtx->d()->compilationUnit) {
                QString fileName = unit->fileName();
                QUrl url(unit->fileName());
                if (url.isValid() && url.isRelative()) {
                    context = url.fileName();
                } else {
                    context = QQmlFile::urlToLocalFileOrQrc(fileName);
                    if (context.isEmpty() && fileName.startsWith(QLatin1String(":/")))
                        context = fileName;
                }
                context = QFileInfo(context).baseName();
            }
            parentCtx = parentCtx->d()->parent;
        }
    }

    QString text = ctx->d()->callData->args[0].toQStringNoThrow();
    QString comment;
    if (ctx->d()->callData->argc > 1)
        comment = ctx->d()->callData->args[1].toQStringNoThrow();
    int n = -1;
    if (ctx->d()->callData->argc > 2)
        n = ctx->d()->callData->args[2].toInt32();

    QString result = QCoreApplication::translate(context.toUtf8().constData(), text.toUtf8().constData(),
                                                 comment.toUtf8().constData(), n);

    return ctx->d()->engine->newString(result)->asReturnedValue();
}

/*!
    \qmlmethod string Qt::qsTrNoOp(string sourceText, string disambiguation)

    Marks \a sourceText for dynamic translation; i.e, the stored \a sourceText
    will not be altered.

    If the same \a sourceText is used in different roles within the
    same translation context, an additional identifying string may be passed in
    for \a disambiguation.

    Returns the \a sourceText.

    QT_TR_NOOP is used in conjunction with the dynamic translation functions
    qsTr() and qsTranslate(). It identifies a string as requiring
    translation (so it can be identified by \c lupdate), but leaves the actual
    translation to the dynamic functions.

    Example:
    \snippet qml/qtTrNoOp.qml 0

    \sa {Internationalization and Localization with Qt Quick}
*/
ReturnedValue GlobalExtensions::method_qsTrNoOp(CallContext *ctx)
{
    if (ctx->d()->callData->argc < 1)
        return QV4::Encode::undefined();
    return ctx->d()->callData->args[0].asReturnedValue();
}

/*!
    \qmlmethod string Qt::qsTrId(string id, int n)

    Returns a translated string identified by \a id.
    If no matching string is found, the id itself is returned. This
    should not happen under normal conditions.

    If \a n >= 0, all occurrences of \c %n in the resulting string
    are replaced with a decimal representation of \a n. In addition,
    depending on \a n's value, the translation text may vary.

    Example:
    \snippet qml/qsTrId.qml 0

    It is possible to supply a source string template like:

    \tt{//% <string>}

    or

    \tt{\\begincomment% <string> \\endcomment}

    Example:
    \snippet qml/qsTrId.1.qml 0

    Creating binary translation (QM) files suitable for use with this function requires passing
    the \c -idbased option to the \c lrelease tool.

    \sa QT_TRID_NOOP(), {Internationalization and Localization with Qt Quick}
*/
ReturnedValue GlobalExtensions::method_qsTrId(CallContext *ctx)
{
    if (ctx->d()->callData->argc < 1)
        V4THROW_ERROR("qsTrId() requires at least one argument");
    if (!ctx->d()->callData->args[0].isString())
        V4THROW_TYPE("qsTrId(): first argument (id) must be a string");
    if (ctx->d()->callData->argc > 1 && !ctx->d()->callData->args[1].isNumber())
        V4THROW_TYPE("qsTrId(): second argument (n) must be a number");

    int n = -1;
    if (ctx->d()->callData->argc > 1)
        n = ctx->d()->callData->args[1].toInt32();

    return ctx->d()->engine->newString(qtTrId(ctx->d()->callData->args[0].toQStringNoThrow().toUtf8().constData(), n))->asReturnedValue();
}

/*!
    \qmlmethod string Qt::qsTrIdNoOp(string id)

    Marks \a id for dynamic translation.

    Returns the \a id.

    QT_TRID_NOOP is used in conjunction with the dynamic translation function
    qsTrId(). It identifies a string as requiring translation (so it can be identified
    by \c lupdate), but leaves the actual translation to qsTrId().

    Example:
    \snippet qml/qtTrIdNoOp.qml 0

    \sa qsTrId(), {Internationalization and Localization with Qt Quick}
*/
ReturnedValue GlobalExtensions::method_qsTrIdNoOp(CallContext *ctx)
{
    if (ctx->d()->callData->argc < 1)
        return QV4::Encode::undefined();
    return ctx->d()->callData->args[0].asReturnedValue();
}
#endif // QT_NO_TRANSLATION


QV4::ReturnedValue GlobalExtensions::method_gc(CallContext *ctx)
{
    ctx->d()->engine->memoryManager->runGC();

    return QV4::Encode::undefined();
}



ReturnedValue GlobalExtensions::method_string_arg(CallContext *ctx)
{
    if (ctx->d()->callData->argc != 1)
        V4THROW_ERROR("String.arg(): Invalid arguments");

    QString value = ctx->d()->callData->thisObject.toQString();

    QV4::Scope scope(ctx);
    QV4::ScopedValue arg(scope, ctx->d()->callData->args[0]);
    if (arg->isInteger())
        return ctx->d()->engine->newString(value.arg(arg->integerValue()))->asReturnedValue();
    else if (arg->isDouble())
        return ctx->d()->engine->newString(value.arg(arg->doubleValue()))->asReturnedValue();
    else if (arg->isBoolean())
        return ctx->d()->engine->newString(value.arg(arg->booleanValue()))->asReturnedValue();

    return ctx->d()->engine->newString(value.arg(arg->toQString()))->asReturnedValue();
}


QT_END_NAMESPACE

