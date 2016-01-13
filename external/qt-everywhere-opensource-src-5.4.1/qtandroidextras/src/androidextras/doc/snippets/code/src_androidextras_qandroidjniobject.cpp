/****************************************************************************
 **
 ** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the documentation of the Qt Toolkit.
 **
 ** $QT_BEGIN_LICENSE:BSD$
 ** You may use this file under the terms of the BSD license as follows:
 **
 ** "Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions are
 ** met:
 **   * Redistributions of source code must retain the above copyright
 **     notice, this list of conditions and the following disclaimer.
 **   * Redistributions in binary form must reproduce the above copyright
 **     notice, this list of conditions and the following disclaimer in
 **     the documentation and/or other materials provided with the
 **     distribution.
 **   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
 **     of its contributors may be used to endorse or promote products derived
 **     from this software without specific prior written permission.
 **
 **
 ** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 ** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 ** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 ** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 ** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 ** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 ** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 ** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 **
 ** $QT_END_LICENSE$
 **
 ****************************************************************************/

//! [Working with lists]
QStringList getTrackTitles(const QAndroidJniObject &album) {
    QStringList stringList;
    QAndroidJniObject list = album.callObjectMethod("getTitles",
                                                      "()Ljava/util/List;");

    if (list.isValid()) {
        const int size = list.callMethod<jint>("size");
        for (int i = 0; i < size; ++i) {
            QAndroidJniObject title = list.callObjectMethod("get", "(I)Ljava/lang/Object;", i);
            stringList.append(title.toString());
        }
    }
    return stringList;
}
//! [Working with lists]

//! [QAndroidJniObject scope]
void functionScope()
{
    QString helloString("Hello");
    jstring myJString = 0;
    {
        QAndroidJniObject string = QAndroidJniObject::fromString(helloString);
        myJString = string.object<jstring>();
    }

   // Ops! myJString is no longer valid.
}
//! [QAndroidJniObject scope]

//! [Check for exceptions]
void functionException()
{
    QAndroidJniObject myString = QAndroidJniObject::fromString("Hello");
    jchar c = myString.callMethod<jchar>("charAt", "(I)C", 1000);
    QAndroidJniEnvironment env;
    if (env->ExceptionCheck()) {
        // Handle exception here.
        env->ExceptionClear();
    }
}
//! [Check for exceptions]

//! [Registering native methods]
static void fromJavaOne(JNIEnv *env, jobject thiz, jint x)
{
    Q_UNUSED(env)
    Q_UNUSED(thiz)
    qDebug() << x << "< 100";
}

static void fromJavaTwo(JNIEnv *env, jobject thiz, jint x)
{
    Q_UNUSED(env)
    Q_UNUSED(thiz)
    qDebug() << x << ">= 100";
}

void registerNativeMethods() {
    JNINativeMethod methods[] {{"callNativeOne", "(I)V", reinterpret_cast<void *>(fromJavaOne)},
                               {"callNativeTwo", "(I)V", reinterpret_cast<void *>(fromJavaTwo)}};

    QAndroidJniObject javaClass("my/java/project/FooJavaClass");
    QAndroidJniEnvironment env;
    jclass objectClass = env->GetObjectClass(javaClass.object<jobject>());
    env->RegisterNatives(objectClass,
                         methods,
                         sizeof(methods) / sizeof(methods[0]));
    env->DeleteLocalRef(objectClass);
}

void foo()
{
    QAndroidJniObject::callStaticMethod<void>("my/java/project/FooJavaClass", "foo", "(I)V", 10);  // Output: 10 < 100
    QAndroidJniObject::callStaticMethod<void>("my/java/project/FooJavaClass", "foo", "(I)V", 100); // Output: 100 >= 100
}

//! [Registering native methods]

//! [Java native methods]
class FooJavaClass
{
    public static void foo(int x)
    {
        if (x < 100)
            callNativeOne(x);
        else
            callNativeTwo(x);
    }

private static native void callNativeOne(int x);
private static native void callNativeTwo(int x);

}
//! [Java native methods]
