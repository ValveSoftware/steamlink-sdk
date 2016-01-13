/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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
    QAndroidJniObject(const char *className, const char *sig, ...);
    explicit QAndroidJniObject(jclass clazz);
    QAndroidJniObject(jclass clazz, const char *sig, ...);
    QAndroidJniObject(jobject obj);
    ~QAndroidJniObject() { }

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
#ifndef Q_QDOC
    // ### Qt 6 remove templated version
    template <typename T>
    QAndroidJniObject getObjectField(const char *fieldName, const char *sig) const;
#endif // Q_QDOC
    QAndroidJniObject getObjectField(const char *fieldName, const char *sig) const;
    template <typename T>
    void setField(const char *fieldName, T value);
    template <typename T>
    void setField(const char *fieldName, const char *sig, T value);
    template <typename T>
    static QAndroidJniObject getStaticObjectField(const char *className, const char *fieldName);
#ifndef Q_QDOC
    // ### Qt 6 remove templated version
    template <typename T>
    static QAndroidJniObject getStaticObjectField(const char *className,
                                           const char *fieldName,
                                           const char *sig);
#endif // Q_QDOC
    static QAndroidJniObject getStaticObjectField(const char *className,
                                           const char *fieldName,
                                           const char *sig);
    template <typename T>
    static T getStaticField(const char *className, const char *fieldName);
    template <typename T>
    static QAndroidJniObject getStaticObjectField(jclass clazz, const char *fieldName);
#ifndef Q_QDOC
    // ### Qt 6 remove templated version
    template <typename T>
    static QAndroidJniObject getStaticObjectField(jclass clazz,
                                           const char *fieldName,
                                           const char *sig);
#endif // Q_QDOC
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

    template <typename T>
    inline QAndroidJniObject &operator=(T o)
    {
        assign(static_cast<jobject>(o));
        return *this;
    }

private:
    friend bool operator==(const QAndroidJniObject &, const QAndroidJniObject &);
    friend bool operator!=(const QAndroidJniObject &, const QAndroidJniObject &);
    template <typename T> friend bool operator!=(const QAndroidJniObject &, T);
    template <typename T> friend bool operator==(const QAndroidJniObject &, T);
    template <typename T> friend bool operator!=(T, const QAndroidJniObject &);
    template <typename T> friend bool operator==(T, const QAndroidJniObject &);

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

template <typename T>
inline bool operator==(const QAndroidJniObject &obj1, T obj2)
{
    return obj1.isSameObject(static_cast<jobject>(obj2));
}

template <typename T>
inline bool operator==(T obj1, const QAndroidJniObject &obj2)
{
    return obj2.isSameObject(static_cast<jobject>(obj1));
}

template <typename T>
inline bool operator!=(const QAndroidJniObject &obj1, T obj2)
{
    return !obj1.isSameObject(static_cast<jobject>(obj2));
}

template <typename T>
inline bool operator!=(T obj1, const QAndroidJniObject &obj2)
{
    return !obj2.isSameObject(static_cast<jobject>(obj1));
}

QT_END_NAMESPACE

#endif // QANDROIDJNIOBJECT_H
