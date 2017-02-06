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

#include "qandroidjniobject.h"
#include <QtCore/private/qjni_p.h>
#include <QtCore/qhash.h>

QT_BEGIN_NAMESPACE

/*!
    \class QAndroidJniObject
    \inmodule QtAndroidExtras
    \brief Provides APIs to call Java code from C++.
    \since 5.2

    \sa QAndroidJniEnvironment

    \section1 General Notes

    \list
    \li Class names needs to contain the fully-qualified class name, for example: \b"java/lang/String".
    \li Method signatures are written as \b"(Arguments)ReturnType"
    \li All object types are returned as a QAndroidJniObject.
    \endlist

    \section1 Method Signatures

    For functions that take no arguments, QAndroidJniObject provides convenience functions that will use
    the correct signature based on the provided template type. For example:

    \code
    jint x = QAndroidJniObject::callMethod<jint>("getSize");
    QAndroidJniObject::callMethod<void>("touch");
    \endcode

    In other cases you will need to supply the signature yourself, and it is important that the
    signature matches the function you want to call. The signature structure is \b \(A\)R, where \b A
    is the type of the argument\(s\) and \b R is the return type. Array types in the signature must
    have the \b\[ suffix and the fully-qualified type names must have the \b L prefix and \b ; suffix.

    The example below demonstrates how to call two different static functions.
    \code
    // Java class
    package org.qtproject.qt5;
    class TestClass
    {
       static String fromNumber(int x) { ... }
       static String[] stringArray(String s1, String s2) { ... }
    }
    \endcode

    The signature for the first function is \b"\(I\)Ljava/lang/String;"

    \code
    // C++ code
    QAndroidJniObject stringNumber = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/TestClass",
                                                                               "fromNumber"
                                                                               "(I)Ljava/lang/String;",
                                                                               10);
    \endcode

    and the signature for the second function is \b"\(Ljava/lang/String;Ljava/lang/String;\)\[Ljava/lang/String;"

    \code
    // C++ code
    QAndroidJniObject string1 = QAndroidJniObject::fromString("String1");
    QAndroidJniObject string2 = QAndroidJniObject::fromString("String2");
    QAndroidJniObject stringArray = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/TestClass",
                                                                              "stringArray"
                                                                              "(Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;"
                                                                               string1.object<jstring>(),
                                                                               string2.object<jstring>());
    \endcode


    \section1 Handling Java Exception

    When calling Java functions that might throw an exception, it is important that you check, handle
    and clear out the exception before continuing.

    \note It is unsafe to make a JNI call when there are exceptions pending.

    \snippet code/src_androidextras_qandroidjniobject.cpp Check for exceptions

    \section1 Java Native Methods

    Java native methods makes it possible to call native code from Java, this is done by creating a
    function declaration in Java and prefixing it with the \b native keyword.
    Before a native function can be called from Java, you need to map the Java native function to a
    native function in your code. Mapping functions can be done by calling the RegisterNatives() function
    through the \l{QAndroidJniEnvironment}{JNI environment pointer}.

    The example below demonstrates how this could be done.

    Java implementation:
    \snippet code/src_androidextras_qandroidjniobject.cpp Java native methods

    C++ Implementation:
    \snippet code/src_androidextras_qandroidjniobject.cpp Registering native methods

    \section1 The Lifetime of a Java Object

    Most \l{Object types}{objects} received from Java will be local references and will only stay valid
    in the scope you received them. After that, the object becomes eligible for garbage collection. If you
    want to keep a Java object alive you need to either create a new global reference to the object and
    release it when you are done, or construct a new QAndroidJniObject and let it manage the lifetime of the Java object.
    \sa object()

    \note The QAndroidJniObject does only manage its own references, if you construct a QAndroidJniObject from a
          global or local reference that reference will not be released by the QAndroidJniObject.

    \section1 JNI Types

    \section2 Object Types
    \table
    \header
        \li Type
        \li Signature
    \row
        \li jobject
        \li Ljava/lang/Object;
    \row
        \li jclass
        \li Ljava/lang/Class;
    \row
        \li jstring
        \li Ljava/lang/String;
    \row
        \li jthrowable
        \li Ljava/lang/Throwable;
    \row
        \li jobjectArray
        \li [Ljava/lang/Object;
    \row
        \li jarray
        \li [\e<type>
    \row
        \li jbooleanArray
        \li [Z
    \row
        \li jbyteArray
        \li [B
    \row
        \li jcharArray
        \li [C
    \row
        \li jshortArray
        \li [S
    \row
        \li jintArray
        \li [I
    \row
        \li jlongArray
        \li [J
    \row
        \li jfloatArray
        \li [F
    \row
        \li jdoubleArray
        \li [D
    \endtable


    \section2 Primitive Types
    \table
    \header
        \li Type
        \li Signature
    \row
        \li jboolean
        \li Z
    \row
        \li jbyte
        \li B
    \row
        \li jchar
        \li C
    \row
       \li jshort
       \li S
    \row
        \li jint
        \li I
    \row
        \li jlong
        \li J
    \row
        \li jfloat
        \li F
    \row
        \li jdouble
        \li D
    \endtable

    \section3 Other
    \table
    \header
        \li Type
        \li Signature
    \row
        \li void
        \li V
    \row
        \li \e{Custom type}
        \li L\e<fully-qualified-name>;
    \endtable

    For more information about JNI see: \l http://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/jniTOC.html
*/

/*!
    \fn QAndroidJniObject::QAndroidJniObject()

    Constructs an invalid QAndroidJniObject.

    \sa isValid()
*/

/*!
    \fn QAndroidJniObject::QAndroidJniObject(const char *className)

    Constructs a new QAndroidJniObject by calling the default constructor of \a className.

    \code

    ...
    QAndroidJniObject myJavaString("java/lang/String");
    ...

    \endcode
*/

/*!
    \fn QAndroidJniObject::QAndroidJniObject(const char *className, const char *signature, ...)

    Constructs a new QAndroidJniObject by calling the constructor of \a className with \a signature
    and arguments.

    \code

    ...
    jstring myJStringArg = ...;
    QAndroidJniObject myNewJavaString("java/lang/String", "(Ljava/lang/String;)V", myJStringArg);
    ...

    \endcode
*/

/*!
    \fn QAndroidJniObject::QAndroidJniObject(jclass clazz)

    Constructs a new QAndroidJniObject by calling the default constructor of \a clazz.

    Note: The QAndroidJniObject will create a new reference to the class \a clazz
          and releases it again when it is destroyed. References to the class created
          outside the QAndroidJniObject needs to be managed by the caller.

*/

/*!
    \fn QAndroidJniObject::QAndroidJniObject(jclass clazz, const char *signature, ...)

    Constructs a new QAndroidJniObject from \a clazz by calling the constructor with \a signature
    and arguments.

    \code
    jclass myClazz = ...;
    QAndroidJniObject::QAndroidJniObject(myClazz, "(I)V", 3);
    \endcode
*/

/*!
    \fn QAndroidJniObject::QAndroidJniObject(jobject object)

    Constructs a new QAndroidJniObject around the Java object \a object.

    Note: The QAndroidJniObject will hold a reference to the Java object \a object
          and release it when destroyed. Any references to the Java object \a object
          outside QAndroidJniObject needs to be managed by the caller.

    \sa fromLocalRef()
*/

/*!
    Destroys the QAndroidJniObject and releases any references held by the QAndroidJniObject.
*/
QAndroidJniObject::~QAndroidJniObject()
{
    // must be empty until ### Qt 6
}

/*!
    \fn T QAndroidJniObject::callMethod(const char *methodName, const char *signature, ...) const

    Calls the method \a methodName with \a signature and returns the value.

    \code
    QAndroidJniObject myJavaString = ...;
    jint index = myJavaString.callMethod<jint>("indexOf", "(I)I", 0x0051);
    \endcode

*/

/*!
    \fn T QAndroidJniObject::callMethod(const char *methodName) const

    Calls the method \a methodName and returns the value.

    \code
    QAndroidJniObject myJavaString = ...;
    jint size = myJavaString.callMethod<jint>("length");
    \endcode
*/

/*!
    \fn QAndroidJniObject QAndroidJniObject::callObjectMethod(const char *methodName) const

    Calls the Java objects method \a methodName and returns a new QAndroidJniObject for
    the returned Java object.

    \code
    ...
    QAndroidJniObject myJavaString1 = ...;
    QAndroidJniObject myJavaString2 = myJavaString1.callObjectMethod<jstring>("toString");
    ...
    \endcode
*/

/*!
    \fn QAndroidJniObject QAndroidJniObject::callObjectMethod(const char *methodName, const char *signature, ...) const

    Calls the Java object's method \a methodName with the signature \a signature and arguments

    \code
    QAndroidJniObject myJavaString; ==> "Hello, Java"
    QAndroidJniObject mySubstring = myJavaString.callObjectMethod("substring", "(II)Ljava/lang/String;", 7, 10);
    \endcode
*/

/*!
    \fn T QAndroidJniObject::callStaticMethod(jclass clazz, const char *methodName)

    Calls the static method \a methodName on \a clazz and returns the value.

    \code
    ...
    jclass javaMathClass = ...; // ("java/lang/Math")
    jdouble randNr = QAndroidJniObject::callStaticMethod<jdouble>(javaMathClass, "random");
    ...
    \endcode
*/

/*!
    \fn T QAndroidJniObject::callStaticMethod(const char *className, const char *methodName)

    Calls the static method \a methodName on class \a className and returns the value.

    \code
    jint value = QAndroidJniObject::callStaticMethod<jint>("MyClass", "staticMethod");
    \endcode
*/

/*!
    \fn T QAndroidJniObject::callStaticMethod(const char *className, const char *methodName, const char *signature, ...)

    Calls the static method with \a methodName with \a signature on class \a className with optional arguments.

    \code
    ...
    jint a = 2;
    jint b = 4;
    jint max = QAndroidJniObject::callStaticMethod<jint>("java/lang/Math", "max", "(II)I", a, b);
    ...
    \endcode
*/

/*!
    \fn T QAndroidJniObject::callStaticMethod(jclass clazz, const char *methodName, const char *signature, ...)

    Calls the static method \a methodName with \a signature on \a clazz and returns the value.

    \code
    ...
    jclass javaMathClass = ...; // ("java/lang/Math")
    jint a = 2;
    jint b = 4;
    jint max = QAndroidJniObject::callStaticMethod<jint>(javaMathClass, "max", "(II)I", a, b);
    ...
    \endcode
*/

/*!
    \fn QAndroidJniObject QAndroidJniObject::callStaticObjectMethod(const char *className, const char *methodName)

    Calls the static method with \a methodName on the class \a className.

    \code
    QAndroidJniObject string = QAndroidJniObject::callStaticObjectMethod<jstring>("CustomClass", "getClassName");
    \endcode
*/

/*!
    \fn QAndroidJniObject QAndroidJniObject::callStaticObjectMethod(const char *className, const char *methodName, const char *signature, ...)

    Calls the static method with \a methodName and \a signature on the class \a className.

    \code
    QAndroidJniObject thread = QAndroidJniObject::callStaticObjectMethod("java/lang/Thread", "currentThread", "()Ljava/lang/Thread;");
    QAndroidJniObject string = QAndroidJniObject::callStaticObjectMethod("java/lang/String", "valueOf", "(I)Ljava/lang/String;", 10);
    \endcode
*/

/*!
    \fn QAndroidJniObject QAndroidJniObject::callStaticObjectMethod(jclass clazz, const char *methodName)

    Calls the static method with \a methodName on \a clazz.

*/

/*!
    \fn QAndroidJniObject QAndroidJniObject::callStaticObjectMethod(jclass clazz, const char *methodName, const char *signature, ...)

    Calls the static method with \a methodName and \a signature on class \a clazz.
*/

/*!
    \fn T QAndroidJniObject::getField(const char *fieldName) const

    Retrieves the value of the field \a fieldName.

    \code
    QAndroidJniObject volumeControl = ...;
    jint fieldValue = volumeControl.getField<jint>("MAX_VOLUME");
    \endcode
*/

/*!
    \fn QAndroidJniObject QAndroidJniObject::getObjectField(const char *fieldName) const

    Retrieves the object of field \a fieldName.

    \code
    QAndroidJniObject field = jniObject.getObjectField<jstring>("FIELD_NAME");
    \endcode
*/

/*!
    \fn QAndroidJniObject QAndroidJniObject::getObjectField(const char *fieldName, const char *signature) const

    Retrieves the object from the field with \a signature and \a fieldName.

    \note Since \b{Qt 5.3} this function can be used without a template type.

    \code
    QAndroidJniObject field = jniObject.getObjectField("FIELD_NAME", "Ljava/lang/String;");
    \endcode
*/

/*!
    \fn T QAndroidJniObject::getStaticField(const char *className, const char *fieldName)

    Retrieves the value from the static field \a fieldName on the class \a className.
*/

/*!
    \fn T QAndroidJniObject::getStaticField(jclass clazz, const char *fieldName)

    Retrieves the value from the static field \a fieldName on \a clazz.
*/

/*!
    \fn QAndroidJniObject QAndroidJniObject::getStaticObjectField(const char *className, const char *fieldName)

    Retrieves the object from the field \a fieldName on the class \a className.

    \code
    QAndroidJniObject jobj = QAndroidJniObject::getStaticObjectField<jstring>("class/with/Fields", "FIELD_NAME");
    \endcode
*/

/*!
    \fn QAndroidJniObject QAndroidJniObject::getStaticObjectField(const char *className, const char *fieldName, const char *signature)
    Retrieves the object from the field with \a signature and \a fieldName on class \a className.

    \note Since \b{Qt 5.3} this function can be used without a template type.

    \code
    QAndroidJniObject jobj = QAndroidJniObject::getStaticObjectField("class/with/Fields", "FIELD_NAME", "Ljava/lang/String;");
    \endcode
*/

/*!
    \fn QAndroidJniObject QAndroidJniObject::getStaticObjectField(jclass clazz, const char *fieldName)

    Retrieves the object from the field \a fieldName on \a clazz.

    \code
    QAndroidJniObject jobj = QAndroidJniObject::getStaticObjectField<jstring>(clazz, "FIELD_NAME");
    \endcode
*/

/*!
    \fn QAndroidJniObject QAndroidJniObject::getStaticObjectField(jclass clazz, const char *fieldName, const char *signature)
    Retrieves the object from the field with \a signature and \a fieldName on \a clazz.

    \note Since \b{Qt 5.3} this function can be used without a template type.

    \code
    QAndroidJniObject jobj = QAndroidJniObject::getStaticObjectField(clazz, "FIELD_NAME", "Ljava/lang/String;");
    \endcode
*/

/*!
    \fn void QAndroidJniObject::setField(const char *fieldName, T value)

    Sets the value of \a fieldName to \a value.

    \code
    ...
    QAndroidJniObject obj;
    obj.setField<jint>("AN_INT_FIELD", 10);
    jstring myString = ...
    obj.setField<jstring>("A_STRING_FIELD", myString);
    ...
    \endcode
*/

/*!
    \fn void QAndroidJniObject::setField(const char *fieldName, const char *signature, T value)

    Sets the value of \a fieldName with \a signature to \a value.

    \code
    QAndroidJniObject stringArray = ...;
    QAndroidJniObject obj = ...;
    obj.setField<jobjectArray>("KEY_VALUES", "([Ljava/lang/String;)V", stringArray.object<jobjectArray>())
    \endcode
*/

/*!
    \fn void QAndroidJniObject::setStaticField(const char *className, const char *fieldName, T value)

    Sets the value of the static field \a fieldName in class \a className to \a value.
*/

/*!
    \fn void QAndroidJniObject::setStaticField(const char *className, const char *fieldName, const char *signature, T value);

    Sets the static field with \a fieldName and \a signature to \a value on class \a className.
*/

/*!
    \fn void QAndroidJniObject::setStaticField(jclass clazz, const char *fieldName, T value)

    Sets the static field \a fieldName of the class \a clazz to \a value.
*/

/*!
    \fn void QAndroidJniObject::setStaticField(jclass clazz, const char *fieldName, const char *signature, T value);

    Sets the static field with \a fieldName and \a signature to \a value on class \a clazz.
*/

/*!
    \fn bool QAndroidJniObject::isClassAvailable(const char *className)

    Returns true if the Java class \a className is available.

    \code
    ...
    if (QAndroidJniObject::isClassAvailable("java/lang/String")) {
       ...
    }
    ...
    \endcode
*/

/*!
    \fn bool QAndroidJniObject::isValid() const

    Returns true if this instance holds a valid Java object.

    \code
    ...
    QAndroidJniObject qjniObject;                        ==> isValid() == false
    QAndroidJniObject qjniObject(0)                      ==> isValid() == false
    QAndroidJniObject qjniObject("could/not/find/Class") ==> isValid() == false
    ...
    \endcode
*/

/*!
    \fn T QAndroidJniObject::object() const

    Returns the object held by the QAndroidJniObject as type T.

    \code
    QAndroidJniObject string = QAndroidJniObject::fromString("Hello, JNI");
    jstring jstring = string.object<jstring>();
    \endcode

    \note The returned object is still owned by the QAndroidJniObject. If you want to keep the object valid
    you should create a new QAndroidJniObject or make a new global reference to the object and
    free it yourself.

    \snippet code/src_androidextras_qandroidjniobject.cpp QAndroidJniObject scope

    \note Since \b{Qt 5.3} this function can be used without a template type, if the returned type
    is a \c jobject.

    \code
    jobject object = jniObject.object();
    \endcode
*/

/*!
    \fn QAndroidJniObject &QAndroidJniObject::operator=(T object)

    Replace the current object with \a object. The old Java object will be released.
*/

/*!
    \fn QString QAndroidJniObject::toString() const

    Returns a QString with a string representation of the java object.
    Calling this function on a Java String object is a convenient way of getting the actual string
    data.

    \code
    QAndroidJniObject string = ...; //  "Hello Java"
    QString qstring = string.toString(); // "Hello Java"
    \endcode

    \sa fromString()
*/

/*!
    \fn QAndroidJniObject QAndroidJniObject::fromString(const QString &string)

    Creates a Java string from the QString \a string and returns a QAndroidJniObject holding that string.

    \code
    ...
    QString myQString = "QString";
    QAndroidJniObject myJavaString = QAndroidJniObject::fromString(myQString);
    ...
    \endcode

    \sa toString()
*/

/*!
    \fn QAndroidJniObject QAndroidJniObject::fromLocalRef(jobject localRef)
    \since 5.7

    Creates a QAndroidJniObject from the local JNI reference \a localRef.
    This function takes ownership of \a localRef and frees it before returning.

    \note Only call this function with a local JNI reference. For example, most raw JNI calls, through
    the JNI environment, returns local references to a java object.

    \code
    jobject localRef = env->GetObjectArrayElement(array, index);
    QAndroidJniObject element = QAndroidJniObject::fromLocalRef(localRef);
    \endcode
*/

/*!
    \fn bool operator==(const QAndroidJniObject &o1, const QAndroidJniObject &o2)
    \relates QAndroidJniObject

    Returns true if both objects, \a o1 and \a o2, are referencing the same Java object, or if both
    are NULL. In any other cases false will be returned.
*/

/*!
    \fn bool operator!=(const QAndroidJniObject &o1, const QAndroidJniObject &o2)
    \relates QAndroidJniObject

    Returns true if \a o1 holds a reference to a different object then \a o2.
*/


QAndroidJniObject::QAndroidJniObject(const char *className, const char *sig, ...)
{
    va_list args;
    QJNIObjectPrivate::QVaListPrivate vargs = { args };
    va_start(args, sig);
    d = QSharedPointer<QJNIObjectPrivate>(new QJNIObjectPrivate(className, sig, vargs));
    va_end(args);
}

QAndroidJniObject::QAndroidJniObject(jclass clazz, const char *sig, ...)
{
    va_list args;
    QJNIObjectPrivate::QVaListPrivate vargs = { args };
    va_start(args, sig);
    d = QSharedPointer<QJNIObjectPrivate>(new QJNIObjectPrivate(clazz, sig, vargs));
    va_end(args);
}


QAndroidJniObject::QAndroidJniObject() : d(new QJNIObjectPrivate)
{

}

QAndroidJniObject::QAndroidJniObject(const char *className) : d(new QJNIObjectPrivate(className))
{
}

QAndroidJniObject::QAndroidJniObject(jclass clazz) : d(new QJNIObjectPrivate(clazz))
{
}

QAndroidJniObject::QAndroidJniObject(jobject obj) : d(new QJNIObjectPrivate(obj))
{
}

QAndroidJniObject::QAndroidJniObject(const QJNIObjectPrivate &o) : d(new QJNIObjectPrivate(o))
{

}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::callMethod<void>(const char *methodName, const char *sig, ...) const
{
    va_list args;
    va_start(args, sig);
    d->callMethodV<void>(methodName, sig, args);
    va_end(args);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jboolean QAndroidJniObject::callMethod<jboolean>(const char *methodName, const char *sig, ...) const
{
    va_list args;
    va_start(args, sig);
    jboolean res = d->callMethodV<jboolean>(methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jbyte QAndroidJniObject::callMethod<jbyte>(const char *methodName, const char *sig, ...) const
{
    va_list args;
    va_start(args, sig);
    jbyte res = d->callMethodV<jbyte>(methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jchar QAndroidJniObject::callMethod<jchar>(const char *methodName, const char *sig, ...) const
{
    va_list args;
    va_start(args, sig);
    jchar res = d->callMethodV<jchar>(methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jshort QAndroidJniObject::callMethod<jshort>(const char *methodName, const char *sig, ...) const
{
    va_list args;
    va_start(args, sig);
    jshort res = d->callMethodV<jshort>(methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jint QAndroidJniObject::callMethod<jint>(const char *methodName, const char *sig, ...) const
{
    va_list args;
    va_start(args, sig);
    jint res = d->callMethodV<jint>(methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jlong QAndroidJniObject::callMethod<jlong>(const char *methodName, const char *sig, ...) const
{
    va_list args;
    va_start(args, sig);
    jlong res = d->callMethodV<jlong>(methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jfloat QAndroidJniObject::callMethod<jfloat>(const char *methodName, const char *sig, ...) const
{
    va_list args;
    va_start(args, sig);
    jfloat res = d->callMethodV<jfloat>(methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jdouble QAndroidJniObject::callMethod<jdouble>(const char *methodName, const char *sig, ...) const
{
    va_list args;
    va_start(args, sig);
    jdouble res = d->callMethodV<jdouble>(methodName, sig, args);
    va_end(args);
    return res;
}

QAndroidJniObject QAndroidJniObject::callObjectMethod(const char *methodName,
                                        const char *sig,
                                        ...) const
{
    va_list args;
    va_start(args, sig);
    QJNIObjectPrivate res = d->callObjectMethodV(methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::callMethod<void>(const char *methodName) const
{
    callMethod<void>(methodName, "()V");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jboolean QAndroidJniObject::callMethod<jboolean>(const char *methodName) const
{
    return callMethod<jboolean>(methodName, "()Z");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jbyte QAndroidJniObject::callMethod<jbyte>(const char *methodName) const
{
    return callMethod<jbyte>(methodName, "()B");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jchar QAndroidJniObject::callMethod<jchar>(const char *methodName) const
{
    return callMethod<jchar>(methodName, "()C");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jshort QAndroidJniObject::callMethod<jshort>(const char *methodName) const
{
    return callMethod<jshort>(methodName, "()S");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jint QAndroidJniObject::callMethod<jint>(const char *methodName) const
{
    return callMethod<jint>(methodName, "()I");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jlong QAndroidJniObject::callMethod<jlong>(const char *methodName) const
{
    return callMethod<jlong>(methodName, "()J");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jfloat QAndroidJniObject::callMethod<jfloat>(const char *methodName) const
{
    return callMethod<jfloat>(methodName, "()F");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jdouble QAndroidJniObject::callMethod<jdouble>(const char *methodName) const
{
    return callMethod<jdouble>(methodName, "()D");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callObjectMethod<jobject>(const char *methodName) const
{
    return d->callObjectMethod(methodName, "()Ljava/lang/Object;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callObjectMethod<jclass>(const char *methodName) const
{
    return d->callObjectMethod(methodName, "()Ljava/lang/Class;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callObjectMethod<jstring>(const char *methodName) const
{
    return d->callObjectMethod(methodName, "()Ljava/lang/String;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callObjectMethod<jobjectArray>(const char *methodName) const
{
    return d->callObjectMethod(methodName, "()[Ljava/lang/Object;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callObjectMethod<jbooleanArray>(const char *methodName) const
{
    return d->callObjectMethod(methodName, "()[Z");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callObjectMethod<jbyteArray>(const char *methodName) const
{
    return d->callObjectMethod(methodName, "()[B");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callObjectMethod<jcharArray>(const char *methodName) const
{
    return d->callObjectMethod(methodName, "()[C");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callObjectMethod<jshortArray>(const char *methodName) const
{
    return d->callObjectMethod(methodName, "()[S");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callObjectMethod<jintArray>(const char *methodName) const
{
    return d->callObjectMethod(methodName, "()[I");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callObjectMethod<jlongArray>(const char *methodName) const
{
    return d->callObjectMethod(methodName, "()[J");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callObjectMethod<jfloatArray>(const char *methodName) const
{
    return d->callObjectMethod(methodName, "()[F");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callObjectMethod<jdoubleArray>(const char *methodName) const
{
    return d->callObjectMethod(methodName, "()[D");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callObjectMethod<jthrowable>(const char *methodName) const
{
    return d->callObjectMethod(methodName, "()Ljava/lang/Throwable;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::callStaticMethod<void>(const char *className,
                                        const char *methodName,
                                        const char *sig,
                                        ...)
{
    va_list args;
    va_start(args, sig);
    QJNIObjectPrivate::callStaticMethodV<void>(className, methodName, sig, args);
    va_end(args);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::callStaticMethod<void>(jclass clazz,
                                        const char *methodName,
                                        const char *sig,
                                        ...)
{
    va_list args;
    va_start(args, sig);
    QJNIObjectPrivate::callStaticMethodV<void>(clazz, methodName, sig, args);
    va_end(args);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jboolean QAndroidJniObject::callStaticMethod<jboolean>(const char *className,
                                                const char *methodName,
                                                const char *sig,
                                                ...)
{
    va_list args;
    va_start(args, sig);
    jboolean res = QJNIObjectPrivate::callStaticMethodV<jboolean>(className, methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jboolean QAndroidJniObject::callStaticMethod<jboolean>(jclass clazz,
                                                const char *methodName,
                                                const char *sig,
                                                ...)
{
    va_list args;
    va_start(args, sig);
    jboolean res = QJNIObjectPrivate::callStaticMethodV<jboolean>(clazz, methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jbyte QAndroidJniObject::callStaticMethod<jbyte>(const char *className,
                                          const char *methodName,
                                          const char *sig,
                                          ...)
{
    va_list args;
    va_start(args, sig);
    jbyte res = QJNIObjectPrivate::callStaticMethodV<jbyte>(className, methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jbyte QAndroidJniObject::callStaticMethod<jbyte>(jclass clazz,
                                          const char *methodName,
                                          const char *sig,
                                          ...)
{
    va_list args;
    va_start(args, sig);
    jbyte res = QJNIObjectPrivate::callStaticMethodV<jbyte>(clazz, methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jchar QAndroidJniObject::callStaticMethod<jchar>(const char *className,
                                          const char *methodName,
                                          const char *sig,
                                          ...)
{
    va_list args;
    va_start(args, sig);
    jchar res = QJNIObjectPrivate::callStaticMethodV<jchar>(className, methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jchar QAndroidJniObject::callStaticMethod<jchar>(jclass clazz,
                                          const char *methodName,
                                          const char *sig,
                                          ...)
{
    va_list args;
    va_start(args, sig);
    jchar res = QJNIObjectPrivate::callStaticMethodV<jchar>(clazz, methodName, sig, args);
    va_end(args);
    return res;
}


template <>
Q_ANDROIDEXTRAS_EXPORT jshort QAndroidJniObject::callStaticMethod<jshort>(const char *className,
                                            const char *methodName,
                                            const char *sig,
                                            ...)
{
    va_list args;
    va_start(args, sig);
    jshort res = QJNIObjectPrivate::callStaticMethodV<jshort>(className, methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jshort QAndroidJniObject::callStaticMethod<jshort>(jclass clazz,
                                            const char *methodName,
                                            const char *sig,
                                            ...)
{
    va_list args;
    va_start(args, sig);
    jshort res = QJNIObjectPrivate::callStaticMethodV<jshort>(clazz, methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jint QAndroidJniObject::callStaticMethod<jint>(const char *className,
                                        const char *methodName,
                                        const char *sig,
                                        ...)
{
    va_list args;
    va_start(args, sig);
    jint res = QJNIObjectPrivate::callStaticMethodV<jint>(className, methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jint QAndroidJniObject::callStaticMethod<jint>(jclass clazz,
                                        const char *methodName,
                                        const char *sig,
                                        ...)
{
    va_list args;
    va_start(args, sig);
    jint res = QJNIObjectPrivate::callStaticMethodV<jint>(clazz, methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jlong QAndroidJniObject::callStaticMethod<jlong>(const char *className,
                                          const char *methodName,
                                          const char *sig,
                                          ...)
{
    va_list args;
    va_start(args, sig);
    jlong res = QJNIObjectPrivate::callStaticMethodV<jlong>(className, methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jlong QAndroidJniObject::callStaticMethod<jlong>(jclass clazz,
                                          const char *methodName,
                                          const char *sig,
                                          ...)
{
    va_list args;
    va_start(args, sig);
    jlong res = QJNIObjectPrivate::callStaticMethodV<jlong>(clazz, methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jfloat QAndroidJniObject::callStaticMethod<jfloat>(const char *className,
                                            const char *methodName,
                                            const char *sig,
                                            ...)
{
    va_list args;
    va_start(args, sig);
    jfloat res = QJNIObjectPrivate::callStaticMethodV<jfloat>(className, methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jfloat QAndroidJniObject::callStaticMethod<jfloat>(jclass clazz,
                                            const char *methodName,
                                            const char *sig,
                                            ...)
{
    va_list args;
    va_start(args, sig);
    jfloat res = QJNIObjectPrivate::callStaticMethodV<jfloat>(clazz, methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jdouble QAndroidJniObject::callStaticMethod<jdouble>(const char *className,
                                              const char *methodName,
                                              const char *sig,
                                              ...)
{
    va_list args;
    va_start(args, sig);
    jdouble res = QJNIObjectPrivate::callStaticMethodV<jdouble>(className, methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT jdouble QAndroidJniObject::callStaticMethod<jdouble>(jclass clazz,
                                              const char *methodName,
                                              const char *sig,
                                              ...)
{
    va_list args;
    va_start(args, sig);
    jdouble res = QJNIObjectPrivate::callStaticMethodV<jdouble>(clazz, methodName, sig, args);
    va_end(args);
    return res;
}

QAndroidJniObject QAndroidJniObject::callStaticObjectMethod(const char *className,
                                              const char *methodName,
                                              const char *sig,
                                              ...)
{
    va_list args;
    va_start(args, sig);
    QJNIObjectPrivate res = QJNIObjectPrivate::callStaticObjectMethodV(className,
                                                                      methodName,
                                                                      sig,
                                                                      args);
    va_end(args);
    return res;
}

QAndroidJniObject QAndroidJniObject::callStaticObjectMethod(jclass clazz,
                                              const char *methodName,
                                              const char *sig,
                                              ...)
{
    va_list args;
    va_start(args, sig);
    QJNIObjectPrivate res = QJNIObjectPrivate::callStaticObjectMethodV(clazz, methodName, sig, args);
    va_end(args);
    return res;
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::callStaticMethod<void>(const char *className, const char *methodName)
{
    callStaticMethod<void>(className, methodName, "()V");
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::callStaticMethod<void>(jclass clazz, const char *methodName)
{
    callStaticMethod<void>(clazz, methodName, "()V");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jboolean QAndroidJniObject::callStaticMethod<jboolean>(const char *className, const char *methodName)
{
    return callStaticMethod<jboolean>(className, methodName, "()Z");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jboolean QAndroidJniObject::callStaticMethod<jboolean>(jclass clazz, const char *methodName)
{
    return callStaticMethod<jboolean>(clazz, methodName, "()Z");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jbyte QAndroidJniObject::callStaticMethod<jbyte>(const char *className, const char *methodName)
{
    return callStaticMethod<jbyte>(className, methodName, "()B");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jbyte QAndroidJniObject::callStaticMethod<jbyte>(jclass clazz, const char *methodName)
{
    return callStaticMethod<jbyte>(clazz, methodName, "()B");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jchar QAndroidJniObject::callStaticMethod<jchar>(const char *className, const char *methodName)
{
    return callStaticMethod<jchar>(className, methodName, "()C");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jchar QAndroidJniObject::callStaticMethod<jchar>(jclass clazz, const char *methodName)
{
    return callStaticMethod<jchar>(clazz, methodName, "()C");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jshort QAndroidJniObject::callStaticMethod<jshort>(const char *className, const char *methodName)
{
    return callStaticMethod<jshort>(className, methodName, "()S");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jshort QAndroidJniObject::callStaticMethod<jshort>(jclass clazz, const char *methodName)
{
    return callStaticMethod<jshort>(clazz, methodName, "()S");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jint QAndroidJniObject::callStaticMethod<jint>(const char *className, const char *methodName)
{
    return callStaticMethod<jint>(className, methodName, "()I");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jint QAndroidJniObject::callStaticMethod<jint>(jclass clazz, const char *methodName)
{
    return callStaticMethod<jint>(clazz, methodName, "()I");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jlong QAndroidJniObject::callStaticMethod<jlong>(const char *className, const char *methodName)
{
    return callStaticMethod<jlong>(className, methodName, "()J");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jlong QAndroidJniObject::callStaticMethod<jlong>(jclass clazz, const char *methodName)
{
    return callStaticMethod<jlong>(clazz, methodName, "()J");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jfloat QAndroidJniObject::callStaticMethod<jfloat>(const char *className, const char *methodName)
{
    return callStaticMethod<jfloat>(className, methodName, "()F");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jfloat QAndroidJniObject::callStaticMethod<jfloat>(jclass clazz, const char *methodName)
{
    return callStaticMethod<jfloat>(clazz, methodName, "()F");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jdouble QAndroidJniObject::callStaticMethod<jdouble>(const char *className, const char *methodName)
{
    return callStaticMethod<jdouble>(className, methodName, "()D");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jdouble QAndroidJniObject::callStaticMethod<jdouble>(jclass clazz, const char *methodName)
{
    return callStaticMethod<jdouble>(clazz, methodName, "()D");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jobject>(const char *className,
                                                                     const char *methodName)
{
    return callStaticObjectMethod(className, methodName, "()Ljava/lang/Object;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jobject>(jclass clazz,
                                                                     const char *methodName)
{
    return callStaticObjectMethod(clazz, methodName, "()Ljava/lang/Object;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jclass>(const char *className,
                                                                    const char *methodName)
{
    return callStaticObjectMethod(className, methodName, "()Ljava/lang/Class;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jclass>(jclass clazz,
                                                                    const char *methodName)
{
    return callStaticObjectMethod(clazz, methodName, "()Ljava/lang/Class;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jstring>(const char *className,
                                                       const char *methodName)
{
    return callStaticObjectMethod(className, methodName, "()Ljava/lang/String;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jstring>(jclass clazz,
                                                       const char *methodName)
{
    return callStaticObjectMethod(clazz, methodName, "()Ljava/lang/String;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jobjectArray>(const char *className,
                                                                          const char *methodName)
{
    return callStaticObjectMethod(className, methodName, "()[Ljava/lang/Object;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jobjectArray>(jclass clazz,
                                                                          const char *methodName)
{
    return callStaticObjectMethod(clazz, methodName, "()[Ljava/lang/Object;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jbooleanArray>(const char *className,
                                                             const char *methodName)
{
    return callStaticObjectMethod(className, methodName, "()[Z");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jbooleanArray>(jclass clazz,
                                                             const char *methodName)
{
    return callStaticObjectMethod(clazz, methodName, "()[Z");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jbyteArray>(const char *className,
                                                          const char *methodName)
{
    return callStaticObjectMethod(className, methodName, "()[B");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jbyteArray>(jclass clazz,
                                                          const char *methodName)
{
    return callStaticObjectMethod(clazz, methodName, "()[B");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jcharArray>(const char *className,
                                                          const char *methodName)
{
    return callStaticObjectMethod(className, methodName, "()[C");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jcharArray>(jclass clazz,
                                                          const char *methodName)
{
    return callStaticObjectMethod(clazz, methodName, "()[C");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jshortArray>(const char *className,
                                                           const char *methodName)
{
    return callStaticObjectMethod(className, methodName, "()[S");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jshortArray>(jclass clazz,
                                                           const char *methodName)
{
    return callStaticObjectMethod(clazz, methodName, "()[S");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jintArray>(const char *className,
                                                         const char *methodName)
{
    return callStaticObjectMethod(className, methodName, "()[I");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jintArray>(jclass clazz,
                                                         const char *methodName)
{
    return callStaticObjectMethod(clazz, methodName, "()[I");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jlongArray>(const char *className,
                                                          const char *methodName)
{
    return callStaticObjectMethod(className, methodName, "()[J");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jlongArray>(jclass clazz,
                                                          const char *methodName)
{
    return callStaticObjectMethod(clazz, methodName, "()[J");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jfloatArray>(const char *className,
                                                           const char *methodName)
{
    return callStaticObjectMethod(className, methodName, "()[F");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jfloatArray>(jclass clazz,
                                                           const char *methodName)
{
    return callStaticObjectMethod(clazz, methodName, "()[F");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jdoubleArray>(const char *className,
                                                            const char *methodName)
{
    return callStaticObjectMethod(className, methodName, "()[D");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jdoubleArray>(jclass clazz,
                                                            const char *methodName)
{
    return callStaticObjectMethod(clazz, methodName, "()[D");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jthrowable>(const char *className,
                                                                        const char *methodName)
{
    return callStaticObjectMethod(className, methodName, "()Ljava/lang/Throwable;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::callStaticObjectMethod<jthrowable>(jclass clazz,
                                                                        const char *methodName)
{
    return callStaticObjectMethod(clazz, methodName, "()Ljava/lang/Throwable;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT jboolean QAndroidJniObject::getField<jboolean>(const char *fieldName) const
{
    return d->getField<jboolean>(fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jbyte QAndroidJniObject::getField<jbyte>(const char *fieldName) const
{
    return d->getField<jbyte>(fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jchar QAndroidJniObject::getField<jchar>(const char *fieldName) const
{
    return d->getField<jchar>(fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jshort QAndroidJniObject::getField<jshort>(const char *fieldName) const
{
    return d->getField<jshort>(fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jint QAndroidJniObject::getField<jint>(const char *fieldName) const
{
    return d->getField<jint>(fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jlong QAndroidJniObject::getField<jlong>(const char *fieldName) const
{
    return d->getField<jlong>(fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jfloat QAndroidJniObject::getField<jfloat>(const char *fieldName) const
{
    return d->getField<jfloat>(fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jdouble QAndroidJniObject::getField<jdouble>(const char *fieldName) const
{
    return d->getField<jdouble>(fieldName);
}

QAndroidJniObject QAndroidJniObject::getObjectField(const char *fieldName, const char *sig) const
{
    return d->getObjectField(fieldName, sig);
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getObjectField<jobject>(const char *fieldName, const char *sig) const
{
    return d->getObjectField(fieldName, sig);
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getObjectField<jobject>(const char *fieldName) const
{
    return d->getObjectField(fieldName, "Ljava/lang/Object;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getObjectField<jclass>(const char *fieldName) const
{
    return d->getObjectField(fieldName, "Ljava/lang/Class;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getObjectField<jbooleanArray>(const char *fieldName) const
{
    return d->getObjectField(fieldName, "[Z");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getObjectField<jbyteArray>(const char *fieldName) const
{
    return d->getObjectField(fieldName, "[B");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getObjectField<jcharArray>(const char *fieldName) const
{
    return d->getObjectField(fieldName, "[C");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getObjectField<jshortArray>(const char *fieldName) const
{
    return d->getObjectField(fieldName, "[S");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getObjectField<jintArray>(const char *fieldName) const
{
    return d->getObjectField(fieldName, "[I");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getObjectField<jlongArray>(const char *fieldName) const
{
    return d->getObjectField(fieldName, "[J");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getObjectField<jfloatArray>(const char *fieldName) const
{
    return d->getObjectField(fieldName, "[F");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getObjectField<jdoubleArray>(const char *fieldName) const
{
    return d->getObjectField(fieldName, "[D");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getObjectField<jstring>(const char *fieldName) const
{
    return d->getObjectField(fieldName, "Ljava/lang/String;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getObjectField<jthrowable>(const char *fieldName) const
{
    return d->getObjectField(fieldName, "Ljava/lang/Throwable;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getObjectField<jobjectArray>(const char *fieldName,
                                                    const char *sig) const
{
    return d->getObjectField(fieldName, sig);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jboolean>(const char *fieldName, jboolean value)
{
    d->setField<jboolean>(fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jbyte>(const char *fieldName, jbyte value)
{
    d->setField<jbyte>(fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jchar>(const char *fieldName, jchar value)
{
    d->setField<jchar>(fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jshort>(const char *fieldName, jshort value)
{
    d->setField<jshort>(fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jint>(const char *fieldName, jint value)
{
    d->setField<jint>(fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jlong>(const char *fieldName, jlong value)
{
    d->setField<jlong>(fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jfloat>(const char *fieldName, jfloat value)
{
    d->setField<jfloat>(fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jdouble>(const char *fieldName, jdouble value)
{
    d->setField<jdouble>(fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jobject>(const char *fieldName,
                                          const char *sig,
                                          jobject value)
{
    d->setField<jobject>(fieldName, sig, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jobjectArray>(const char *fieldName,
                                        const char *sig,
                                        jobjectArray value)
{
    d->setField<jobjectArray>(fieldName, sig, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jobject>(const char *fieldName,
                                          jobject value)
{
    setField<jobject>(fieldName, "Ljava/lang/Object;", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jclass>(const char *fieldName,
                                         jclass value)
{
    setField<jobject>(fieldName, "Ljava/lang/Class;", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jstring>(const char *fieldName, jstring value)
{
    d->setField<jstring>(fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jbooleanArray>(const char *fieldName, jbooleanArray value)
{
    d->setField<jbooleanArray>(fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jbyteArray>(const char *fieldName, jbyteArray value)
{
    d->setField<jbyteArray>(fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jcharArray>(const char *fieldName, jcharArray value)
{
    d->setField<jcharArray>(fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jshortArray>(const char *fieldName, jshortArray value)
{
    d->setField<jshortArray>(fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jintArray>(const char *fieldName, jintArray value)
{
    d->setField<jintArray>(fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jlongArray>(const char *fieldName, jlongArray value)
{
    d->setField<jlongArray>(fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jfloatArray>(const char *fieldName, jfloatArray value)
{
    d->setField<jfloatArray>(fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jdoubleArray>(const char *fieldName, jdoubleArray value)
{
    d->setField<jdoubleArray>(fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setField<jthrowable>(const char *fieldName, jthrowable value)
{
    d->setField<jobject>(fieldName, "Ljava/lang/Throwable;", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jboolean QAndroidJniObject::getStaticField<jboolean>(jclass clazz, const char *fieldName)
{
    return QJNIObjectPrivate::getStaticField<jboolean>(clazz, fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jboolean QAndroidJniObject::getStaticField<jboolean>(const char *className, const char *fieldName)
{
    return QJNIObjectPrivate::getStaticField<jboolean>(className, fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jbyte QAndroidJniObject::getStaticField<jbyte>(jclass clazz, const char *fieldName)
{
    return QJNIObjectPrivate::getStaticField<jbyte>(clazz, fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jbyte QAndroidJniObject::getStaticField<jbyte>(const char *className, const char *fieldName)
{
    return QJNIObjectPrivate::getStaticField<jbyte>(className, fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jchar QAndroidJniObject::getStaticField<jchar>(jclass clazz, const char *fieldName)
{
    return QJNIObjectPrivate::getStaticField<jchar>(clazz, fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jchar QAndroidJniObject::getStaticField<jchar>(const char *className, const char *fieldName)
{
    return QJNIObjectPrivate::getStaticField<jchar>(className, fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jshort QAndroidJniObject::getStaticField<jshort>(jclass clazz, const char *fieldName)
{
    return QJNIObjectPrivate::getStaticField<jshort>(clazz, fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jshort QAndroidJniObject::getStaticField<jshort>(const char *className, const char *fieldName)
{
    return QJNIObjectPrivate::getStaticField<jshort>(className, fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jint QAndroidJniObject::getStaticField<jint>(jclass clazz, const char *fieldName)
{
    return QJNIObjectPrivate::getStaticField<jint>(clazz, fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jint QAndroidJniObject::getStaticField<jint>(const char *className, const char *fieldName)
{
    return QJNIObjectPrivate::getStaticField<jint>(className, fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jlong QAndroidJniObject::getStaticField<jlong>(jclass clazz, const char *fieldName)
{
    return QJNIObjectPrivate::getStaticField<jlong>(clazz, fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jlong QAndroidJniObject::getStaticField<jlong>(const char *className, const char *fieldName)
{
    return QJNIObjectPrivate::getStaticField<jlong>(className, fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jfloat QAndroidJniObject::getStaticField<jfloat>(jclass clazz, const char *fieldName)
{
    return QJNIObjectPrivate::getStaticField<jfloat>(clazz, fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jfloat QAndroidJniObject::getStaticField<jfloat>(const char *className, const char *fieldName)
{
    return QJNIObjectPrivate::getStaticField<jfloat>(className, fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jdouble QAndroidJniObject::getStaticField<jdouble>(jclass clazz, const char *fieldName)
{
    return QJNIObjectPrivate::getStaticField<jdouble>(clazz, fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT jdouble QAndroidJniObject::getStaticField<jdouble>(const char *className, const char *fieldName)
{
    return QJNIObjectPrivate::getStaticField<jdouble>(className, fieldName);
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jobject>(jclass clazz,
                                                     const char *fieldName,
                                                     const char *sig)
{
    return QJNIObjectPrivate::getStaticObjectField(clazz, fieldName, sig);
}

QAndroidJniObject QAndroidJniObject::getStaticObjectField(jclass clazz,
                                                          const char *fieldName,
                                                          const char *sig)
{
    return QJNIObjectPrivate::getStaticObjectField(clazz, fieldName, sig);
}

QAndroidJniObject QAndroidJniObject::getStaticObjectField(const char *className,
                                                          const char *fieldName,
                                                          const char *sig)
{
    return QJNIObjectPrivate::getStaticObjectField(className, fieldName, sig);
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jobject>(const char *className,
                                                     const char *fieldName,
                                                     const char *sig)
{
    return QJNIObjectPrivate::getStaticObjectField(className, fieldName, sig);
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jobjectArray>(jclass clazz,
                                                          const char *fieldName,
                                                          const char *sig)
{
    return QJNIObjectPrivate::getStaticObjectField(clazz, fieldName, sig);
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jobjectArray>(const char *className,
                                                          const char *fieldName,
                                                          const char *sig)
{
    return QJNIObjectPrivate::getStaticObjectField(className, fieldName, sig);
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jobject>(jclass clazz,
                                                                   const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(clazz, fieldName, "Ljava/lang/Object;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jobject>(const char *className,
                                                                   const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(className, fieldName, "Ljava/lang/Object;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jclass>(jclass clazz,
                                                                   const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(clazz, fieldName, "Ljava/lang/Class;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jclass>(const char *className,
                                                                  const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(className, fieldName, "Ljava/lang/Class;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jstring>(jclass clazz,
                                                     const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(clazz, fieldName, "Ljava/lang/String;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jstring>(const char *className,
                                                     const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(className, fieldName, "Ljava/lang/String;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jobjectArray>(jclass clazz,
                                                                        const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(clazz, fieldName, "[Ljava/lang/Object;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jobjectArray>(const char *className,
                                                                        const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(className, fieldName, "[Ljava/lang/Object;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jbooleanArray>(jclass clazz,
                                                           const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(clazz, fieldName, "[Z");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jbooleanArray>(const char *className,
                                                           const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(className, fieldName, "[Z");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jbyteArray>(jclass clazz,
                                                        const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(clazz, fieldName, "[B");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jbyteArray>(const char *className,
                                                        const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(className, fieldName, "[B");;
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jcharArray>(jclass clazz,
                                                        const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(clazz, fieldName, "[C");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jcharArray>(const char *className,
                                                        const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(className, fieldName, "[C");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jshortArray>(jclass clazz,
                                                         const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(clazz, fieldName, "[S");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jshortArray>(const char *className,
                                                         const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(className, fieldName, "[S");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jintArray>(jclass clazz,
                                                       const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(clazz, fieldName, "[I");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jintArray>(const char *className,
                                                       const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(className, fieldName, "[I");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jlongArray>(jclass clazz,
                                                        const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(clazz, fieldName, "[J");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jlongArray>(const char *className,
                                                        const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(className, fieldName, "[J");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jfloatArray>(jclass clazz,
                                                         const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(clazz, fieldName, "[F");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jfloatArray>(const char *className,
                                                         const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(className, fieldName, "[F");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jdoubleArray>(jclass clazz,
                                                          const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(clazz, fieldName, "[D");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jdoubleArray>(const char *className,
                                                          const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(className, fieldName, "[D");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jthrowable>(jclass clazz,
                                                                      const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(clazz, fieldName, "Ljava/lang/Throwable;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT QAndroidJniObject QAndroidJniObject::getStaticObjectField<jthrowable>(const char *className,
                                                                      const char *fieldName)
{
    return QJNIObjectPrivate::getStaticObjectField(className, fieldName, "Ljava/lang/Throwable;");
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jboolean>(jclass clazz, const char *fieldName, jboolean value)
{
    QJNIObjectPrivate::setStaticField<jboolean>(clazz, fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jboolean>(const char *className,
                                          const char *fieldName,
                                          jboolean value)
{
    QJNIObjectPrivate::setStaticField<jboolean>(className, fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jbyte>(jclass clazz, const char *fieldName, jbyte value)
{
    QJNIObjectPrivate::setStaticField<jbyte>(clazz, fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jbyte>(const char *className,
                                       const char *fieldName,
                                       jbyte value)
{
    QJNIObjectPrivate::setStaticField<jbyte>(className, fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jchar>(jclass clazz, const char *fieldName, jchar value)
{
    QJNIObjectPrivate::setStaticField<jchar>(clazz, fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jchar>(const char *className,
                                       const char *fieldName,
                                       jchar value)
{
    QJNIObjectPrivate::setStaticField<jchar>(className, fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jshort>(jclass clazz, const char *fieldName, jshort value)
{
    QJNIObjectPrivate::setStaticField<jshort>(clazz, fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jshort>(const char *className,
                                        const char *fieldName,
                                        jshort value)
{
    QJNIObjectPrivate::setStaticField<jshort>(className, fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jint>(jclass clazz, const char *fieldName, jint value)
{
    QJNIObjectPrivate::setStaticField<jint>(clazz, fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jint>(const char *className, const char *fieldName, jint value)
{
    QJNIObjectPrivate::setStaticField<jint>(className, fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jlong>(jclass clazz, const char *fieldName, jlong value)
{
    QJNIObjectPrivate::setStaticField<jlong>(clazz, fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jlong>(const char *className,
                                       const char *fieldName,
                                       jlong value)
{
    QJNIObjectPrivate::setStaticField<jlong>(className, fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jfloat>(jclass clazz, const char *fieldName, jfloat value)
{
    QJNIObjectPrivate::setStaticField<jfloat>(clazz, fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jfloat>(const char *className,
                                        const char *fieldName,
                                        jfloat value)
{
    QJNIObjectPrivate::setStaticField<jfloat>(className, fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jdouble>(jclass clazz, const char *fieldName, jdouble value)
{
    QJNIObjectPrivate::setStaticField<jdouble>(clazz, fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jdouble>(const char *className,
                                         const char *fieldName,
                                         jdouble value)
{
    QJNIObjectPrivate::setStaticField<jdouble>(className, fieldName, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jobject>(jclass clazz,
                                         const char *fieldName,
                                         const char *sig,
                                         jobject value)
{
    QJNIObjectPrivate::setStaticField<jobject>(clazz, fieldName, sig, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jobject>(const char *className,
                                         const char *fieldName,
                                         const char *sig,
                                         jobject value)
{
    QJNIObjectPrivate::setStaticField<jobject>(className, fieldName, sig, value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jclass>(const char *className,
                                               const char *fieldName,
                                               jclass value)
{
    QJNIObjectPrivate::setStaticField<jobject>(className, fieldName, "Ljava/lang/Class;", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jclass>(jclass clazz, const char *fieldName, jclass value)
{
    QJNIObjectPrivate::setStaticField<jobject>(clazz, fieldName, "Ljava/lang/Class;", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jstring>(const char *className,
                                         const char *fieldName,
                                         jstring value)
{
    QJNIObjectPrivate::setStaticField<jobject>(className, fieldName, "Ljava/lang/String;", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jstring>(jclass clazz, const char *fieldName, jstring value)
{
    QJNIObjectPrivate::setStaticField<jobject>(clazz, fieldName, "Ljava/lang/String;", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jobjectArray>(const char *className,
                                                     const char *fieldName,
                                                     jobjectArray value)
{
    QJNIObjectPrivate::setStaticField<jobject>(className, fieldName, "[Ljava/lang/Object;", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jobjectArray>(jclass clazz,
                                                     const char *fieldName,
                                                     jobjectArray value)
{
    QJNIObjectPrivate::setStaticField<jobject>(clazz, fieldName, "[Ljava/lang/Object;", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jbooleanArray>(const char *className,
                                               const char *fieldName,
                                               jbooleanArray value)
{
    QJNIObjectPrivate::setStaticField<jobject>(className, fieldName, "[Z", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jbooleanArray>(jclass clazz,
                                               const char *fieldName,
                                               jbooleanArray value)
{
    QJNIObjectPrivate::setStaticField<jobject>(clazz, fieldName, "[Z", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jbyteArray>(const char *className,
                                            const char *fieldName,
                                            jbyteArray value)
{
    QJNIObjectPrivate::setStaticField<jobject>(className, fieldName, "[B", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jbyteArray>(jclass clazz,
                                            const char *fieldName,
                                            jbyteArray value)
{
    QJNIObjectPrivate::setStaticField<jobject>(clazz, fieldName, "[B", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jcharArray>(const char *className,
                                            const char *fieldName,
                                            jcharArray value)
{
    QJNIObjectPrivate::setStaticField<jobject>(className, fieldName, "[C", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jcharArray>(jclass clazz,
                                            const char *fieldName,
                                            jcharArray value)
{
    QJNIObjectPrivate::setStaticField<jobject>(clazz, fieldName, "[C", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jshortArray>(const char *className,
                                             const char *fieldName,
                                             jshortArray value)
{
    QJNIObjectPrivate::setStaticField<jobject>(className, fieldName, "[S", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jshortArray>(jclass clazz,
                                             const char *fieldName,
                                             jshortArray value)
{
    QJNIObjectPrivate::setStaticField<jobject>(clazz, fieldName, "[S", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jintArray>(const char *className,
                                           const char *fieldName,
                                           jintArray value)
{
    QJNIObjectPrivate::setStaticField<jobject>(className, fieldName, "[I", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jintArray>(jclass clazz,
                                           const char *fieldName,
                                           jintArray value)
{
    QJNIObjectPrivate::setStaticField<jobject>(clazz, fieldName, "[I", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jlongArray>(const char *className,
                                            const char *fieldName,
                                            jlongArray value)
{
    QJNIObjectPrivate::setStaticField<jobject>(className, fieldName, "[J", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jlongArray>(jclass clazz,
                                            const char *fieldName,
                                            jlongArray value)
{
    QJNIObjectPrivate::setStaticField<jobject>(clazz, fieldName, "[J", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jfloatArray>(const char *className,
                                             const char *fieldName,
                                             jfloatArray value)
{
    QJNIObjectPrivate::setStaticField<jobject>(className, fieldName, "[F", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jfloatArray>(jclass clazz,
                                             const char *fieldName,
                                             jfloatArray value)
{
    QJNIObjectPrivate::setStaticField<jobject>(clazz, fieldName, "[F", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jdoubleArray>(const char *className,
                                              const char *fieldName,
                                              jdoubleArray value)
{
    QJNIObjectPrivate::setStaticField<jobject>(className, fieldName, "[D", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jdoubleArray>(jclass clazz,
                                              const char *fieldName,
                                              jdoubleArray value)
{
    QJNIObjectPrivate::setStaticField<jobject>(clazz, fieldName, "[D", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jthrowable>(const char *className,
                                                   const char *fieldName,
                                                   jthrowable value)
{
    QJNIObjectPrivate::setStaticField<jobject>(className, fieldName, "Ljava/lang/Throwable;", value);
}

template <>
Q_ANDROIDEXTRAS_EXPORT void QAndroidJniObject::setStaticField<jthrowable>(jclass clazz,
                                                   const char *fieldName,
                                                   jthrowable value)
{
    QJNIObjectPrivate::setStaticField<jobject>(clazz, fieldName, "Ljava/lang/Throwable;", value);
}

QAndroidJniObject QAndroidJniObject::fromString(const QString &string)
{
    return QJNIObjectPrivate::fromString(string);
}

QString QAndroidJniObject::toString() const
{
    return d->toString();
}

bool QAndroidJniObject::isClassAvailable(const char *className)
{
    return QJNIObjectPrivate::isClassAvailable(className);
}

bool QAndroidJniObject::isValid() const
{
    return d->isValid();
}

QAndroidJniObject QAndroidJniObject::fromLocalRef(jobject obj)
{
    return QJNIObjectPrivate::fromLocalRef(obj);
}

jobject QAndroidJniObject::javaObject() const
{
    return d->object();
}

bool QAndroidJniObject::isSameObject(jobject obj) const
{
    return d->isSameObject(obj);
}

bool QAndroidJniObject::isSameObject(const QAndroidJniObject &obj) const
{
    return d->isSameObject(*obj.d);
}

void QAndroidJniObject::assign(jobject o)
{
    if (d->isSameObject(o))
        return;

    d = QSharedPointer<QJNIObjectPrivate>(new QJNIObjectPrivate(o));
}

QT_END_NAMESPACE
