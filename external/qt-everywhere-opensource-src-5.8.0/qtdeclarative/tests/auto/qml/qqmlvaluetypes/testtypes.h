/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef TESTTYPES_H
#define TESTTYPES_H

#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QSizeF>
#include <QRect>
#include <QRectF>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QQuaternion>
#include <QMatrix4x4>
#include <QFont>
#include <QColor>
#include <qqml.h>
#include <QQmlPropertyValueSource>
#include <QQmlProperty>
#include <private/qqmlproperty_p.h>
#include <private/qqmlpropertyvalueinterceptor_p.h>

class MyTypeObject : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QPoint point READ point WRITE setPoint NOTIFY changed)
    Q_PROPERTY(QPointF pointf READ pointf WRITE setPointf NOTIFY changed)
    Q_PROPERTY(QPointF pointfpoint READ pointfpoint WRITE setPointfpoint NOTIFY changed)
    Q_PROPERTY(QSize size READ size WRITE setSize NOTIFY changed)
    Q_PROPERTY(QSizeF sizef READ sizef WRITE setSizef NOTIFY changed)
    Q_PROPERTY(QSizeF sizefsize READ sizefsize WRITE setSizefsize NOTIFY changed)
    Q_PROPERTY(QSize sizereadonly READ size NOTIFY changed)
    Q_PROPERTY(QRect rect READ rect WRITE setRect NOTIFY changed)
    Q_PROPERTY(QRectF rectf READ rectf WRITE setRectf NOTIFY changed)
    Q_PROPERTY(QRectF rectfrect READ rectfrect WRITE setRectfrect NOTIFY changed)
    Q_PROPERTY(QVector2D vector2 READ vector2 WRITE setVector2 NOTIFY changed)
    Q_PROPERTY(QVector3D vector READ vector WRITE setVector NOTIFY changed)
    Q_PROPERTY(QVector4D vector4 READ vector4 WRITE setVector4 NOTIFY changed)
    Q_PROPERTY(QQuaternion quaternion READ quaternion WRITE setQuaternion NOTIFY changed)
    Q_PROPERTY(QMatrix4x4 matrix READ matrix WRITE setMatrix NOTIFY changed)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY changed)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY changed)
    Q_PROPERTY(QVariant variant READ variant NOTIFY changed)

public:
    MyTypeObject() :
        m_point(10, 4),
        m_pointf(11.3, -10.9),
        m_pointfpoint(10.0, 4.0),
        m_size(1912, 1913),
        m_sizef(0.1, 100923.2),
        m_sizefsize(1912.0, 1913.0),
        m_rect(2, 3, 109, 102),
        m_rectf(103.8, 99.2, 88.1, 77.6),
        m_rectfrect(2.0, 3.0, 109.0, 102.0),
        m_vector2(32.88f, 1.3f),
        m_vector(23.88f, 3.1f, 4.3f),
        m_vector4(54.2f, 23.88f, 3.1f, 4.3f),
        m_quaternion(4.3f, 54.2f, 23.88f, 3.1f),
        m_matrix(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)
    {
        m_font.setFamily("Arial");
        m_font.setBold(true);
        m_font.setWeight(QFont::DemiBold);
        m_font.setItalic(true);
        m_font.setUnderline(true);
        m_font.setOverline(true);
        m_font.setStrikeOut(true);
        m_font.setCapitalization(QFont::AllLowercase);
        m_font.setLetterSpacing(QFont::AbsoluteSpacing, 10.2);
        m_font.setWordSpacing(19.7);
        m_color.setRedF(0.2);
        m_color.setGreenF(0.88);
        m_color.setBlueF(0.6);
        m_color.setAlphaF(0.34);
    }

    QPoint m_point;
    QPoint point() const { return m_point; }
    void setPoint(const QPoint &v) { m_point = v; emit changed(); }

    QPointF m_pointf;
    QPointF pointf() const { return m_pointf; }
    void setPointf(const QPointF &v) { m_pointf = v; emit changed(); }

    QPointF m_pointfpoint;
    QPointF pointfpoint() const { return m_pointfpoint; }
    void setPointfpoint(const QPointF &v) { m_pointfpoint = v; emit changed(); }

    QSize m_size;
    QSize size() const { return m_size; }
    void setSize(const QSize &v) { m_size = v; emit changed(); }

    QSizeF m_sizef;
    QSizeF sizef() const { return m_sizef; }
    void setSizef(const QSizeF &v) { m_sizef = v; emit changed(); }

    QSizeF m_sizefsize;
    QSizeF sizefsize() const { return m_sizefsize; }
    void setSizefsize(const QSizeF &v) { m_sizefsize = v; emit changed(); }

    QRect m_rect;
    QRect rect() const { return m_rect; }
    void setRect(const QRect &v) { m_rect = v; emit changed(); }

    QRectF m_rectf;
    QRectF rectf() const { return m_rectf; }
    void setRectf(const QRectF &v) { m_rectf = v; emit changed(); }

    QRectF m_rectfrect;
    QRectF rectfrect() const { return m_rectfrect; }
    void setRectfrect(const QRectF &v) { m_rectfrect = v; emit changed(); }

    QVector2D m_vector2;
    QVector2D vector2() const { return m_vector2; }
    void setVector2(const QVector2D &v) { m_vector2 = v; emit changed(); }

    QVector3D m_vector;
    QVector3D vector() const { return m_vector; }
    void setVector(const QVector3D &v) { m_vector = v; emit changed(); }

    QVector4D m_vector4;
    QVector4D vector4() const { return m_vector4; }
    void setVector4(const QVector4D &v) { m_vector4 = v; emit changed(); }

    QQuaternion m_quaternion;
    QQuaternion quaternion() const { return m_quaternion; }
    void setQuaternion(const QQuaternion &v) { m_quaternion = v; emit changed(); }

    QMatrix4x4 m_matrix;
    QMatrix4x4 matrix() const { return m_matrix; }
    void setMatrix(const QMatrix4x4 &v) { m_matrix = v; emit changed(); }

    QFont m_font;
    QFont font() const { return m_font; }
    void setFont(const QFont &v) { m_font = v; emit changed(); }

    QColor m_color;
    QColor color() const { return m_color; }
    void setColor(const QColor &v) { m_color = v; emit changed(); }

    QVariant variant() const { return sizef(); }

    void emitRunScript() { emit runScript(); }

signals:
    void changed();
    void runScript();

public slots:
    QSize method() { return QSize(13, 14); }
};

class MyConstantValueSource : public QObject, public QQmlPropertyValueSource
{
    Q_OBJECT
    Q_INTERFACES(QQmlPropertyValueSource)
public:
    virtual void setTarget(const QQmlProperty &p) { p.write(3345); }
};

class MyOffsetValueInterceptor : public QObject, public QQmlPropertyValueInterceptor
{
    Q_OBJECT
    Q_INTERFACES(QQmlPropertyValueInterceptor)
public:
    virtual void setTarget(const QQmlProperty &p) { prop = p; }
    virtual void write(const QVariant &value) { QQmlPropertyPrivate::write(prop, value.toInt() + 13, QQmlPropertyData::BypassInterceptor); }

private:
    QQmlProperty prop;
};

// This test interceptor deliberately swizzles RGBA -> ABGR
class MyColorInterceptor : public QObject, public QQmlPropertyValueInterceptor
{
    Q_OBJECT
    Q_INTERFACES(QQmlPropertyValueInterceptor)
public:
    virtual void setTarget(const QQmlProperty &p) { prop = p; }
    virtual void write(const QVariant &v)
    {
        QColor c = v.value<QColor>();

        int r, g, b, a;
        c.getRgb(&r, &g, &b, &a);
        c.setRgb(a, b, g, r);

        QQmlPropertyPrivate::write(prop, c, QQmlPropertyData::BypassInterceptor);
    }

private:
    QQmlProperty prop;
};

class MyFloatSetInterceptor : public QObject, public QQmlPropertyValueInterceptor
{
    Q_OBJECT
    Q_INTERFACES(QQmlPropertyValueInterceptor)
public:
    virtual void setTarget(const QQmlProperty &p) { prop = p; }
    virtual void write(const QVariant &)
    {
        QQmlPropertyPrivate::write(prop, 0.0f, QQmlPropertyData::BypassInterceptor);
    }

private:
    QQmlProperty prop;
};

class MyFloatIgnoreInterceptor : public QObject, public QQmlPropertyValueInterceptor
{
    Q_OBJECT
    Q_INTERFACES(QQmlPropertyValueInterceptor)
public:
    virtual void setTarget(const QQmlProperty &) {}
    virtual void write(const QVariant &) {}
};

class MyColorObject : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QColor color READ color WRITE setColor)

public:
    MyColorObject() {}

    QColor m_color;
    QColor color() const { return m_color; }
    void setColor(const QColor &v) { m_color = v; }
};

void registerTypes();

#endif // TESTTYPES_H
