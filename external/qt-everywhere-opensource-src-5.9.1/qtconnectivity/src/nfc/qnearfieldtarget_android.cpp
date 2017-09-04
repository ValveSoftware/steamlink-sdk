/****************************************************************************
**
** Copyright (C) 2016 Centria research and development
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNfc module of the Qt Toolkit.
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

#include "qnearfieldtarget_android_p.h"
#include "android/androidjninfc_p.h"
#include "qdebug.h"

#define NDEFTECHNOLOGY              QStringLiteral("android.nfc.tech.Ndef")
#define NDEFFORMATABLETECHNOLOGY    QStringLiteral("android.nfc.tech.NdefFormatable")
#define ISODEPTECHNOLOGY            QStringLiteral("android.nfc.tech.IsoDep")
#define NFCATECHNOLOGY              QStringLiteral("android.nfc.tech.NfcA")
#define NFCBTECHNOLOGY              QStringLiteral("android.nfc.tech.NfcB")
#define NFCFTECHNOLOGY              QStringLiteral("android.nfc.tech.NfcF")
#define NFCVTECHNOLOGY              QStringLiteral("android.nfc.tech.NfcV")
#define MIFARECLASSICTECHNOLOGY     QStringLiteral("android.nfc.tech.MifareClassic")
#define MIFARECULTRALIGHTTECHNOLOGY QStringLiteral("android.nfc.tech.MifareUltralight")

#define MIFARETAG   QStringLiteral("com.nxp.ndef.mifareclassic")
#define NFCTAGTYPE1 QStringLiteral("org.nfcforum.ndef.type1")
#define NFCTAGTYPE2 QStringLiteral("org.nfcforum.ndef.type2")
#define NFCTAGTYPE3 QStringLiteral("org.nfcforum.ndef.type3")
#define NFCTAGTYPE4 QStringLiteral("org.nfcforum.ndef.type4")

NearFieldTarget::NearFieldTarget(QAndroidJniObject intent, const QByteArray uid, QObject *parent) :
    QNearFieldTarget(parent),
    m_intent(intent),
    m_uid(uid),
    m_keepConnection(false)
{
    updateTechList();
    updateType();
    setupTargetCheckTimer();
}

NearFieldTarget::~NearFieldTarget()
{
    releaseIntent();
    emit targetDestroyed(m_uid);
}

QByteArray NearFieldTarget::uid() const
{
    return m_uid;
}

QNearFieldTarget::Type NearFieldTarget::type() const
{
    return m_type;
}

QNearFieldTarget::AccessMethods NearFieldTarget::accessMethods() const
{
    AccessMethods result = UnknownAccess;

    if (m_techList.contains(NDEFTECHNOLOGY)
            || m_techList.contains(NDEFFORMATABLETECHNOLOGY))
        result |= NdefAccess;

    if (m_techList.contains(ISODEPTECHNOLOGY)
            || m_techList.contains(NFCATECHNOLOGY)
            || m_techList.contains(NFCBTECHNOLOGY)
            || m_techList.contains(NFCFTECHNOLOGY)
            || m_techList.contains(NFCVTECHNOLOGY))
        result |= TagTypeSpecificAccess;

    return result;
}

bool NearFieldTarget::keepConnection() const
{
    return m_keepConnection;
}

bool NearFieldTarget::setKeepConnection(bool isPersistent)
{
    m_keepConnection = isPersistent;

    if (!m_keepConnection)
        disconnect();

    return true;
}

bool NearFieldTarget::disconnect()
{
    if (!m_tagTech.isValid())
        return false;

    bool connected = m_tagTech.callMethod<jboolean>("isConnected");
    if (catchJavaExceptions())
        return false;

    if (!connected)
        return false;

    m_tagTech.callMethod<void>("close");
    return !catchJavaExceptions();
}

bool NearFieldTarget::hasNdefMessage()
{
    return m_techList.contains(NDEFTECHNOLOGY);
}

QNearFieldTarget::RequestId NearFieldTarget::readNdefMessages()
{
    // Making sure that target has NDEF messages
    if (!hasNdefMessage())
        return QNearFieldTarget::RequestId();

    // Making sure that target is still in range
    QNearFieldTarget::RequestId requestId(new QNearFieldTarget::RequestIdPrivate);
    if (!m_intent.isValid()) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QNearFieldTarget::Error, QNearFieldTarget::TargetOutOfRangeError),
                                  Q_ARG(const QNearFieldTarget::RequestId&, requestId));
        return requestId;
    }

    // Getting Ndef technology object
    if (!setTagTechnology({NDEFTECHNOLOGY})) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QNearFieldTarget::Error, QNearFieldTarget::UnsupportedError),
                                  Q_ARG(const QNearFieldTarget::RequestId&, requestId));
        return requestId;
    }

    // Connect
    if (!connect()) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QNearFieldTarget::Error, QNearFieldTarget::TargetOutOfRangeError),
                                  Q_ARG(const QNearFieldTarget::RequestId&, requestId));
        return requestId;
    }

    // Get NdefMessage object
    QAndroidJniObject ndefMessage = m_tagTech.callObjectMethod("getNdefMessage", "()Landroid/nfc/NdefMessage;");
    if (catchJavaExceptions())
        ndefMessage = QAndroidJniObject();
    if (!ndefMessage.isValid()) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QNearFieldTarget::Error, QNearFieldTarget::NdefReadError),
                                  Q_ARG(const QNearFieldTarget::RequestId&, requestId));
        return requestId;
    }

    // Convert to byte array
    QAndroidJniObject ndefMessageBA = ndefMessage.callObjectMethod("toByteArray", "()[B");
    QByteArray ndefMessageQBA = jbyteArrayToQByteArray(ndefMessageBA.object<jbyteArray>());

    if (!m_keepConnection) {
        // Closing connection
        disconnect();   // IOException at this point does not matter anymore.
    }

    // Sending QNdefMessage, requestCompleted and exit.
    QNdefMessage qNdefMessage = QNdefMessage::fromByteArray(ndefMessageQBA);
    QMetaObject::invokeMethod(this, "ndefMessageRead", Qt::QueuedConnection,
                              Q_ARG(const QNdefMessage&, qNdefMessage));
    QMetaObject::invokeMethod(this, "requestCompleted", Qt::QueuedConnection,
                              Q_ARG(const QNearFieldTarget::RequestId&, requestId));
    QMetaObject::invokeMethod(this, "ndefMessageRead", Qt::QueuedConnection,
                              Q_ARG(const QNdefMessage&, qNdefMessage),
                              Q_ARG(const QNearFieldTarget::RequestId&, requestId));
    return requestId;
}

int NearFieldTarget::maxCommandLength() const
{
    QAndroidJniObject tagTech;
    if (m_techList.contains(ISODEPTECHNOLOGY))
        tagTech = getTagTechnology(ISODEPTECHNOLOGY);
    else if (m_techList.contains(NFCATECHNOLOGY))
        tagTech = getTagTechnology(NFCATECHNOLOGY);
    else if (m_techList.contains(NFCBTECHNOLOGY))
        tagTech = getTagTechnology(NFCBTECHNOLOGY);
    else if (m_techList.contains(NFCFTECHNOLOGY))
        tagTech = getTagTechnology(NFCFTECHNOLOGY);
    else if (m_techList.contains(NFCVTECHNOLOGY))
        tagTech = getTagTechnology(NFCVTECHNOLOGY);
    else
        return 0;

    int returnVal = tagTech.callMethod<jint>("getMaxTransceiveLength");
    if (catchJavaExceptions())
        return 0;

    return returnVal;
}

QNearFieldTarget::RequestId NearFieldTarget::sendCommand(const QByteArray &command)
{
    if (command.size() == 0 || command.size() > maxCommandLength()) {
        Q_EMIT QNearFieldTarget::error(QNearFieldTarget::InvalidParametersError, QNearFieldTarget::RequestId());
        return QNearFieldTarget::RequestId();
    }

    // Making sure that target has commands
    if (!(accessMethods() & TagTypeSpecificAccess))
        return QNearFieldTarget::RequestId();

    QAndroidJniEnvironment env;

    if (!setTagTechnology({ISODEPTECHNOLOGY, NFCATECHNOLOGY, NFCBTECHNOLOGY, NFCFTECHNOLOGY, NFCVTECHNOLOGY})) {
        Q_EMIT QNearFieldTarget::error(QNearFieldTarget::UnsupportedError, QNearFieldTarget::RequestId());
        return QNearFieldTarget::RequestId();
    }

    // Connecting
    QNearFieldTarget::RequestId requestId = QNearFieldTarget::RequestId(new QNearFieldTarget::RequestIdPrivate());
    if (!connect()) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QNearFieldTarget::Error, QNearFieldTarget::TargetOutOfRangeError),
                                  Q_ARG(const QNearFieldTarget::RequestId&, requestId));
        return requestId;
    }

    // Making QByteArray
    QByteArray ba(command);
    jbyteArray jba = env->NewByteArray(ba.size());
    env->SetByteArrayRegion(jba, 0, ba.size(), reinterpret_cast<jbyte*>(ba.data()));

    // Writing
    QAndroidJniObject myNewVal = m_tagTech.callObjectMethod("transceive", "([B)[B", jba);
    if (catchJavaExceptions()) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QNearFieldTarget::Error, QNearFieldTarget::CommandError),
                                  Q_ARG(const QNearFieldTarget::RequestId&, requestId));
        return requestId;
    }
    QByteArray result = jbyteArrayToQByteArray(myNewVal.object<jbyteArray>());
    env->DeleteLocalRef(jba);

    handleResponse(requestId, result);

    if (!m_keepConnection) {
        // Closing connection
        disconnect();   // IOException at this point does not matter anymore.
    }
    QMetaObject::invokeMethod(this, "requestCompleted", Qt::QueuedConnection,
                              Q_ARG(const QNearFieldTarget::RequestId&, requestId));

    return requestId;
}

QNearFieldTarget::RequestId NearFieldTarget::sendCommands(const QList<QByteArray> &commands)
{
    QNearFieldTarget::RequestId requestId;
    for (int i=0; i < commands.size(); i++)
        requestId = sendCommand(commands.at(i));
    return requestId;
}

QNearFieldTarget::RequestId NearFieldTarget::writeNdefMessages(const QList<QNdefMessage> &messages)
{
    if (messages.size() == 0)
        return QNearFieldTarget::RequestId();

    if (messages.size() > 1)
        qWarning("QNearFieldTarget::writeNdefMessages: Android supports writing only one NDEF message per tag.");

    QAndroidJniEnvironment env;
    const char *writeMethod;
    QAndroidJniObject tagTechnology;

    if (!setTagTechnology({NDEFFORMATABLETECHNOLOGY, NDEFTECHNOLOGY}))
        return QNearFieldTarget::RequestId();

    // Getting write method
    if (m_tech == NDEFFORMATABLETECHNOLOGY)
        writeMethod = "format";
    else
        writeMethod = "writeNdefMessage";

    // Connecting
    QNearFieldTarget::RequestId requestId = QNearFieldTarget::RequestId(new QNearFieldTarget::RequestIdPrivate());
    if (!connect()) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QNearFieldTarget::Error, QNearFieldTarget::TargetOutOfRangeError),
                                  Q_ARG(const QNearFieldTarget::RequestId&, requestId));
        return requestId;
    }

    // Making NdefMessage object
    const QNdefMessage &message = messages.first();
    QByteArray ba = message.toByteArray();
    QAndroidJniObject jba = env->NewByteArray(ba.size());
    env->SetByteArrayRegion(jba.object<jbyteArray>(), 0, ba.size(), reinterpret_cast<jbyte*>(ba.data()));
    QAndroidJniObject jmessage = QAndroidJniObject("android/nfc/NdefMessage", "([B)V", jba.object<jbyteArray>());
    if (catchJavaExceptions()) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QNearFieldTarget::Error, QNearFieldTarget::UnknownError),
                                  Q_ARG(const QNearFieldTarget::RequestId&, requestId));
        return requestId;
    }

    // Writing
    tagTechnology.callMethod<void>(writeMethod, "(Landroid/nfc/NdefMessage;)V", jmessage.object<jobject>());
    if (catchJavaExceptions()) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QNearFieldTarget::Error, QNearFieldTarget::NdefWriteError),
                                  Q_ARG(const QNearFieldTarget::RequestId&, requestId));
        return requestId;
    }

    if (!m_keepConnection)
        disconnect();   // IOException at this point does not matter anymore.
    QMetaObject::invokeMethod(this, "ndefMessagesWritten", Qt::QueuedConnection);
    return requestId;
}

void NearFieldTarget::setIntent(QAndroidJniObject intent)
{
    if (m_intent == intent)
        return;

    releaseIntent();
    m_intent = intent;
    if (m_intent.isValid()) {
        // Updating tech list and type in case of there is another tag with same UID as one before.
        updateTechList();
        updateType();
        m_targetCheckTimer->start();
    }
}

void NearFieldTarget::checkIsTargetLost()
{
    if (!m_intent.isValid() || !setTagTechnology(m_techList)) {
        handleTargetLost();
        return;
    }

    bool connected = m_tagTech.callMethod<jboolean>("isConnected");
    if (catchJavaExceptions()) {
        handleTargetLost();
        return;
    }

    if (connected)
        return;

    m_tagTech.callMethod<void>("connect");
    if (catchJavaExceptions(false)) {
        handleTargetLost();
        return;
    }
    m_tagTech.callMethod<void>("close");
    if (catchJavaExceptions(false))
        handleTargetLost();
}

void NearFieldTarget::releaseIntent()
{
    m_targetCheckTimer->stop();

    m_intent = QAndroidJniObject();
}

void NearFieldTarget::updateTechList()
{
    if (!m_intent.isValid())
        return;

    // Getting tech list
    QAndroidJniEnvironment env;
    QAndroidJniObject tag = AndroidNfc::getTag(m_intent);
    Q_ASSERT_X(tag.isValid(), "updateTechList", "could not get Tag object");

    QAndroidJniObject techListArray = tag.callObjectMethod("getTechList", "()[Ljava/lang/String;");
    if (!techListArray.isValid()) {
        handleTargetLost();
        return;
    }

    // Converting tech list array to QStringList.
    m_techList.clear();
    jsize techCount = env->GetArrayLength(techListArray.object<jobjectArray>());
    for (jsize i = 0; i < techCount; ++i) {
        QAndroidJniObject tech = env->GetObjectArrayElement(techListArray.object<jobjectArray>(), i);
        m_techList.append(tech.callObjectMethod<jstring>("toString").toString());
    }
}

void NearFieldTarget::updateType()
{
    m_type = getTagType();
}

QNearFieldTarget::Type NearFieldTarget::getTagType() const
{
    QAndroidJniEnvironment env;

    if (m_techList.contains(NDEFTECHNOLOGY)) {
        QAndroidJniObject ndef = getTagTechnology(NDEFTECHNOLOGY);
        QString qtype = ndef.callObjectMethod("getType", "()Ljava/lang/String;").toString();

        if (qtype.compare(MIFARETAG) == 0)
            return MifareTag;
        if (qtype.compare(NFCTAGTYPE1) == 0)
            return NfcTagType1;
        if (qtype.compare(NFCTAGTYPE2) == 0)
            return NfcTagType2;
        if (qtype.compare(NFCTAGTYPE3) == 0)
            return NfcTagType3;
        if (qtype.compare(NFCTAGTYPE4) == 0)
            return NfcTagType4;
        return ProprietaryTag;
    } else if (m_techList.contains(NFCATECHNOLOGY)) {
        if (m_techList.contains(MIFARECLASSICTECHNOLOGY))
            return MifareTag;

        // Checking ATQA/SENS_RES
        // xxx0 0000  xxxx xxxx: Identifies tag Type 1 platform
        QAndroidJniObject nfca = getTagTechnology(NFCATECHNOLOGY);
        QAndroidJniObject atqaBA = nfca.callObjectMethod("getAtqa", "()[B");
        QByteArray atqaQBA = jbyteArrayToQByteArray(atqaBA.object<jbyteArray>());
        if (atqaQBA.isEmpty())
            return ProprietaryTag;
        if ((atqaQBA[0] & 0x1F) == 0x00)
            return NfcTagType1;

        // Checking SAK/SEL_RES
        // xxxx xxxx  x00x x0xx: Identifies tag Type 2 platform
        // xxxx xxxx  x01x x0xx: Identifies tag Type 4 platform
        jshort sakS = nfca.callMethod<jshort>("getSak");
        if ((sakS & 0x0064) == 0x0000)
            return NfcTagType2;
        else if ((sakS & 0x0064) == 0x0020)
            return NfcTagType4;
        return ProprietaryTag;
    } else if (m_techList.contains(NFCBTECHNOLOGY)) {
        return NfcTagType4;
    } else if (m_techList.contains(NFCFTECHNOLOGY)) {
        return NfcTagType3;
    }

    return ProprietaryTag;
}

void NearFieldTarget::setupTargetCheckTimer()
{
    m_targetCheckTimer = new QTimer(this);
    m_targetCheckTimer->setInterval(1000);
    QObject::connect(m_targetCheckTimer, &QTimer::timeout, this, &NearFieldTarget::checkIsTargetLost);
    m_targetCheckTimer->start();
}

void NearFieldTarget::handleTargetLost()
{
    releaseIntent();
    emit targetLost(this);
}

QAndroidJniObject NearFieldTarget::getTagTechnology(const QString &tech) const
{
    QString techClass(tech);
    techClass.replace(QLatin1Char('.'), QLatin1Char('/'));

    // Getting requested technology
    QAndroidJniObject tag = AndroidNfc::getTag(m_intent);
    Q_ASSERT_X(tag.isValid(), "getTagTechnology", "could not get Tag object");

    const QString sig = QString::fromUtf8("(Landroid/nfc/Tag;)L%1;");
    QAndroidJniObject tagTech = QAndroidJniObject::callStaticObjectMethod(techClass.toUtf8().constData(), "get",
            sig.arg(techClass).toUtf8().constData(), tag.object<jobject>());

    return tagTech;
}

bool NearFieldTarget::setTagTechnology(const QStringList &techList)
{
    for (const QString &tech : techList) {
        if (m_techList.contains(tech)) {
            if (m_tech == tech) {
                return true;
            }
            m_tech = tech;
            m_tagTech = getTagTechnology(tech);
            return m_tagTech.isValid();
        }
    }

    return false;
}

bool NearFieldTarget::connect()
{
    if (!m_tagTech.isValid())
        return false;

    bool connected = m_tagTech.callMethod<jboolean>("isConnected");
    if (catchJavaExceptions())
        return false;

    if (connected)
        return true;

    m_tagTech.callMethod<void>("connect");
    return !catchJavaExceptions();
}

QByteArray NearFieldTarget::jbyteArrayToQByteArray(const jbyteArray &byteArray) const
{
    QAndroidJniEnvironment env;
    QByteArray resultArray;
    jsize len = env->GetArrayLength(byteArray);
    resultArray.resize(len);
    env->GetByteArrayRegion(byteArray, 0, len, reinterpret_cast<jbyte*>(resultArray.data()));
    return resultArray;
}

bool NearFieldTarget::catchJavaExceptions(bool verbose) const
{
    QAndroidJniEnvironment env;
    if (env->ExceptionCheck()) {
        if (verbose)
            env->ExceptionDescribe();
        env->ExceptionClear();
        return true;
    }
    return false;
}
