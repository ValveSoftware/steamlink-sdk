/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#ifndef QANDROIDJNIOBJECT_H
#define QANDROIDJNIOBJECT_H

#include <jni.h>
#include <QtAndroidExtras/qandroidextrasglobal.h>
#include <QtCore/qglobal.h>
#include <QtCore/qsharedpointer.h>

QT_BEGIN_NAMESPACE

class QJNIObjectPrivate;

class Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject
{
public:
    QAndroidJniObject();
    explicit QAndroidJniObject(const char *className);
    explicit QAndroidJniObject(const char *className, const char *sig, ...);
    explicit QAndroidJniObject(jclass clazz);
    explicit QAndroidJniObject(jclass clazz, const char *sig, ...);
    QAndroidJniObject(jobject obj);
    ~QAndroidJniObject();

    template <typename T>
    inline T object() const { return static_cast<T>(javaObject()); }
#ifndef Q_QDOC
    inline jobject object() const { return javaObject(); }
#endif // Q_QDOC

    template <typename T>
    T callMethod(const char *methodName) const;
    template <typename T>
    T callMethod(const char *methodName, const char *sig, ...) const;
    template <typename T>
    QAndroidJniObject callObjectMethod(const char *methodName) const;
    QAndroidJniObject callObjectMethod(const char *methodName,
                                const char *sig,
                                ...) const;

    template <typename T>
    static T callStaticMethod(const char *className, const char *methodName);
    template <typename T>
    static T callStaticMethod(const char *className, const char *methodName, const char *sig, ...);
    template <typename T>
    static QAndroidJniObject callStaticObjectMethod(const char *className, const char *methodName);
    static QAndroidJniObject callStaticObjectMethod(const char *className,
                                             const char *methodName,
                                             const char *sig, ...);
    template <typename T>
    static T callStaticMethod(jclass clazz, const char *methodName);
    template <typename T>
    static T callStaticMethod(jclass clazz, const char *methodName, const char *sig, ...);
    template <typename T>
    static QAndroidJniObject callStaticObjectMethod(jclass clazz, const char *methodName);
    static QAndroidJniObject callStaticObjectMethod(jclass clazz,
                                             const char *methodName,
                                             const char *sig, ...);
    template <typename T>
    T getField(const char *fieldName) const;
    template <typename T>
    QAndroidJniObject getObjectField(const char *fieldName) const;
#if QT_DEPRECATED_SINCE(5, 5)
# ifndef Q_QDOC
    template <typename T>
    QT_DEPRECATED_X("Use the non-template version instead")
    QAndroidJniObject getObjectField(const char *fieldName, const char *sig) const;
# endif // Q_QDOC
#endif // QT_DEPRECATED_SINCE
    QAndroidJniObject getObjectField(const char *fieldName, const char *sig) const;
    template <typename T>
    void setField(const char *fieldName, T value);
    template <typename T>
    void setField(const char *fieldName, const char *sig, T value);
    template <typename T>
    static QAndroidJniObject getStaticObjectField(const char *className, const char *fieldName);
#if QT_DEPRECATED_SINCE(5, 5)
# ifndef Q_QDOC
    template <typename T>
    QT_DEPRECATED_X("Use the non-template version instead")
    static QAndroidJniObject getStaticObjectField(const char *className,
                                                  const char *fieldName,
                                                  const char *sig);
# endif // Q_QDOC
#endif // QT_DEPRECATED_SINCE
    static QAndroidJniObject getStaticObjectField(const char *className,
                                           const char *fieldName,
                                           const char *sig);
    template <typename T>
    static T getStaticField(const char *className, const char *fieldName);
    template <typename T>
    static QAndroidJniObject getStaticObjectField(jclass clazz, const char *fieldName);
#if QT_DEPRECATED_SINCE(5, 5)
# ifndef Q_QDOC
    template <typename T>
    QT_DEPRECATED_X("Use the non-template version instead")
    static QAndroidJniObject getStaticObjectField(jclass clazz,
                                                  const char *fieldName,
                                                  const char *sig);
# endif // Q_QDOC
#endif // QT_DEPRECATED_SINCE
    static QAndroidJniObject getStaticObjectField(jclass clazz,
                                           const char *fieldName,
                                           const char *sig);
    template <typename T>
    static T getStaticField(jclass clazz, const char *fieldName);

    template <typename T>
    static void setStaticField(const char *className,
                               const char *fieldName,
                               const char *sig,
                               T value);
    template <typename T>
    static void setStaticField(const char *className, const char *fieldName, T value);
    template <typename T>
    static void setStaticField(jclass clazz, const char *fieldName, const char *sig, T value);
    template <typename T>
    static void setStaticField(jclass clazz, const char *fieldName, T value);

    static QAndroidJniObject fromString(const QString &string);
    QString toString() const;

    static bool isClassAvailable(const char *className);
    bool isValid() const;

    static QAndroidJniObject fromLocalRef(jobject obj);

    template <typename T>
    inline QAndroidJniObject &operator=(T o)
    {
        assign(static_cast<jobject>(o));
        return *this;
    }

private:
    friend bool operator==(const QAndroidJniObject &, const QAndroidJniObject &);
    friend bool operator!=(const QAndroidJniObject &, const QAndroidJniObject &);
    template <typename T> friend bool operator!=(const QAndroidJniObject &, const QAndroidJniObject &);
    template <typename T> friend bool operator==(const QAndroidJniObject &, const QAndroidJniObject &);

    QAndroidJniObject(const QJNIObjectPrivate &o);

    void assign(jobject o);
    jobject javaObject() const;
    bool isSameObject(jobject obj) const;
    bool isSameObject(const QAndroidJniObject &obj) const;

    QSharedPointer<QJNIObjectPrivate> d;
};

inline bool operator==(const QAndroidJniObject &obj1, const QAndroidJniObject &obj2)
{
    return obj1.isSameObject(obj2);
}

inline bool operator!=(const QAndroidJniObject &obj1, const QAndroidJniObject &obj2)
{
    return !obj1.isSameObject(obj2);
}

#if QT_DEPRECATED_SINCE(5, 5)
template<typename T>
QT_DEPRECATED inline bool operator==(const QAndroidJniObject &obj1, const QAndroidJniObject &obj2)
{
    return obj1.isSameObject(obj2.object());
}

template <typename T>
QT_DEPRECATED inline bool operator!=(const QAndroidJniObject &obj1, const QAndroidJniObject &obj2)
{
    return !obj1.isSameObject(obj2.object());
}
#endif // QT_DEPRECATED_SINCE

QT_END_NAMESPACE

#endif // QANDROIDJNIOBJECT_H
