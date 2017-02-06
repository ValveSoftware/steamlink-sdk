/****************************************************************************
**
** Copyright (C) 2016 Lauri Laanmets (Proekspert AS) <lauri.laanmets@eesti.ee>
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include "android/devicediscoverybroadcastreceiver_p.h"
#include <QtCore/QtEndian>
#include <QtCore/QLoggingCategory>
#include <QtBluetooth/QBluetoothAddress>
#include <QtBluetooth/QBluetoothDeviceInfo>
#include <QtBluetooth/QBluetoothUuid>
#include "android/jni_android_p.h"
#include <QtCore/private/qjnihelpers_p.h>
#include <QtCore/QHash>
#include <QtCore/qbitarray.h>
#include <algorithm>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_ANDROID)

typedef QHash<jint, QBluetoothDeviceInfo::CoreConfigurations> JCachedBtTypes;
Q_GLOBAL_STATIC(JCachedBtTypes, cachedBtTypes)
typedef QHash<jint, QBluetoothDeviceInfo::MajorDeviceClass> JCachedMajorTypes;
Q_GLOBAL_STATIC(JCachedMajorTypes, cachedMajorTypes)

typedef QHash<jint, quint8> JCachedMinorTypes;
Q_GLOBAL_STATIC(JCachedMinorTypes, cachedMinorTypes)

static QBitArray initializeMinorCaches()
{
    const int numberOfMajorDeviceClasses = 11; // count QBluetoothDeviceInfo::MajorDeviceClass values

    // switch below used to ensure that we notice additions to MajorDeviceClass enum
    const QBluetoothDeviceInfo::MajorDeviceClass classes = QBluetoothDeviceInfo::ComputerDevice;
    switch (classes) {
    case QBluetoothDeviceInfo::MiscellaneousDevice:
    case QBluetoothDeviceInfo::ComputerDevice:
    case QBluetoothDeviceInfo::PhoneDevice:
    case QBluetoothDeviceInfo::LANAccessDevice:
    case QBluetoothDeviceInfo::AudioVideoDevice:
    case QBluetoothDeviceInfo::PeripheralDevice:
    case QBluetoothDeviceInfo::ImagingDevice:
    case QBluetoothDeviceInfo::WearableDevice:
    case QBluetoothDeviceInfo::ToyDevice:
    case QBluetoothDeviceInfo::HealthDevice:
    case QBluetoothDeviceInfo::UncategorizedDevice:
        break;
    default:
        qCWarning(QT_BT_ANDROID) << "Unknown category of major device class:" << classes;
    }

    return QBitArray(numberOfMajorDeviceClasses, false);
}

Q_GLOBAL_STATIC_WITH_ARGS(QBitArray, initializedCacheTracker, (initializeMinorCaches()))


// class names
static const char * const javaBluetoothDeviceClassName = "android/bluetooth/BluetoothDevice";
static const char * const javaBluetoothClassDeviceMajorClassName = "android/bluetooth/BluetoothClass$Device$Major";
static const char * const javaBluetoothClassDeviceClassName = "android/bluetooth/BluetoothClass$Device";

// field names device type (LE vs classic)
static const char * const javaDeviceTypeClassic = "DEVICE_TYPE_CLASSIC";
static const char * const javaDeviceTypeDual = "DEVICE_TYPE_DUAL";
static const char * const javaDeviceTypeLE = "DEVICE_TYPE_LE";
static const char * const javaDeviceTypeUnknown = "DEVICE_TYPE_UNKNOWN";

struct MajorClassJavaToQtMapping
{
    char const * javaFieldName;
    QBluetoothDeviceInfo::MajorDeviceClass qtMajor;
};

static const MajorClassJavaToQtMapping majorMappings[] = {
    { "AUDIO_VIDEO", QBluetoothDeviceInfo::AudioVideoDevice },
    { "COMPUTER", QBluetoothDeviceInfo::ComputerDevice },
    { "HEALTH", QBluetoothDeviceInfo::HealthDevice },
    { "IMAGING", QBluetoothDeviceInfo::ImagingDevice },
    { "MISC", QBluetoothDeviceInfo::MiscellaneousDevice },
    { "NETWORKING", QBluetoothDeviceInfo::LANAccessDevice },
    { "PERIPHERAL", QBluetoothDeviceInfo::PeripheralDevice },
    { "PHONE", QBluetoothDeviceInfo::PhoneDevice },
    { "TOY", QBluetoothDeviceInfo::ToyDevice },
    { "UNCATEGORIZED", QBluetoothDeviceInfo::UncategorizedDevice },
    { "WEARABLE", QBluetoothDeviceInfo::WearableDevice },
    { Q_NULLPTR, QBluetoothDeviceInfo::UncategorizedDevice } //end of list
};

// QBluetoothDeviceInfo::MajorDeviceClass value plus 1 matches index
// UncategorizedDevice shifts to index 0
static const int minorIndexSizes[] = {
  64,  // QBluetoothDevice::UncategorizedDevice
  61,  // QBluetoothDevice::MiscellaneousDevice
  18,  // QBluetoothDevice::ComputerDevice
  35,  // QBluetoothDevice::PhoneDevice
  62,  // QBluetoothDevice::LANAccessDevice
  0,  // QBluetoothDevice::AudioVideoDevice
  56,  // QBluetoothDevice::PeripheralDevice
  63,  // QBluetoothDevice::ImagingDEvice
  49,  // QBluetoothDevice::WearableDevice
  42,  // QBluetoothDevice::ToyDevice
  26,  // QBluetoothDevice::HealthDevice
};

struct MinorClassJavaToQtMapping
{
    char const * javaFieldName;
    quint8 qtMinor;
};

static const MinorClassJavaToQtMapping minorMappings[] = {
    // QBluetoothDevice::AudioVideoDevice -> 17 entries
    { "AUDIO_VIDEO_CAMCORDER", QBluetoothDeviceInfo::Camcorder }, //index 0
    { "AUDIO_VIDEO_CAR_AUDIO", QBluetoothDeviceInfo::CarAudio },
    { "AUDIO_VIDEO_HANDSFREE", QBluetoothDeviceInfo::HandsFreeDevice },
    { "AUDIO_VIDEO_HEADPHONES", QBluetoothDeviceInfo::Headphones },
    { "AUDIO_VIDEO_HIFI_AUDIO", QBluetoothDeviceInfo::HiFiAudioDevice },
    { "AUDIO_VIDEO_LOUDSPEAKER", QBluetoothDeviceInfo::Loudspeaker },
    { "AUDIO_VIDEO_MICROPHONE", QBluetoothDeviceInfo::Microphone },
    { "AUDIO_VIDEO_PORTABLE_AUDIO", QBluetoothDeviceInfo::PortableAudioDevice },
    { "AUDIO_VIDEO_SET_TOP_BOX", QBluetoothDeviceInfo::SetTopBox },
    { "AUDIO_VIDEO_UNCATEGORIZED", QBluetoothDeviceInfo::UncategorizedAudioVideoDevice },
    { "AUDIO_VIDEO_VCR", QBluetoothDeviceInfo::Vcr },
    { "AUDIO_VIDEO_VIDEO_CAMERA", QBluetoothDeviceInfo::VideoCamera },
    { "AUDIO_VIDEO_VIDEO_CONFERENCING", QBluetoothDeviceInfo::VideoConferencing },
    { "AUDIO_VIDEO_VIDEO_DISPLAY_AND_LOUDSPEAKER", QBluetoothDeviceInfo::VideoDisplayAndLoudspeaker },
    { "AUDIO_VIDEO_VIDEO_GAMING_TOY", QBluetoothDeviceInfo::GamingDevice },
    { "AUDIO_VIDEO_VIDEO_MONITOR", QBluetoothDeviceInfo::VideoMonitor },
    { "AUDIO_VIDEO_WEARABLE_HEADSET", QBluetoothDeviceInfo::WearableHeadsetDevice },
    { Q_NULLPTR, 0 }, // separator

    // QBluetoothDevice::ComputerDevice -> 7 entries
    { "COMPUTER_UNCATEGORIZED", QBluetoothDeviceInfo::UncategorizedComputer }, // index 18
    { "COMPUTER_DESKTOP", QBluetoothDeviceInfo::DesktopComputer },
    { "COMPUTER_HANDHELD_PC_PDA", QBluetoothDeviceInfo::HandheldComputer },
    { "COMPUTER_LAPTOP", QBluetoothDeviceInfo::LaptopComputer },
    { "COMPUTER_PALM_SIZE_PC_PDA", QBluetoothDeviceInfo::HandheldClamShellComputer },
    { "COMPUTER_SERVER", QBluetoothDeviceInfo::ServerComputer },
    { "COMPUTER_WEARABLE", QBluetoothDeviceInfo::WearableComputer },
    { Q_NULLPTR, 0 },  // separator

    // QBluetoothDevice::HealthDevice -> 8 entries
    { "HEALTH_BLOOD_PRESSURE", QBluetoothDeviceInfo::HealthBloodPressureMonitor }, // index 26
    { "HEALTH_DATA_DISPLAY", QBluetoothDeviceInfo::HealthDataDisplay },
    { "HEALTH_GLUCOSE", QBluetoothDeviceInfo::HealthGlucoseMeter },
    { "HEALTH_PULSE_OXIMETER", QBluetoothDeviceInfo::HealthPulseOximeter },
    { "HEALTH_PULSE_RATE", QBluetoothDeviceInfo::HealthStepCounter },
    { "HEALTH_THERMOMETER", QBluetoothDeviceInfo::HealthThermometer },
    { "HEALTH_UNCATEGORIZED", QBluetoothDeviceInfo::UncategorizedHealthDevice },
    { "HEALTH_WEIGHING", QBluetoothDeviceInfo::HealthWeightScale },
    { Q_NULLPTR, 0 }, // separator

    // QBluetoothDevice::PhoneDevice -> 6 entries
    { "PHONE_CELLULAR", QBluetoothDeviceInfo::CellularPhone }, // index 35
    { "PHONE_CORDLESS", QBluetoothDeviceInfo::CordlessPhone },
    { "PHONE_ISDN", QBluetoothDeviceInfo::CommonIsdnAccessPhone },
    { "PHONE_MODEM_OR_GATEWAY", QBluetoothDeviceInfo::WiredModemOrVoiceGatewayPhone },
    { "PHONE_SMART", QBluetoothDeviceInfo::SmartPhone },
    { "PHONE_UNCATEGORIZED", QBluetoothDeviceInfo::UncategorizedPhone },
    { Q_NULLPTR, 0 }, // separator

    // QBluetoothDevice::ToyDevice -> 6 entries
    { "TOY_CONTROLLER", QBluetoothDeviceInfo::ToyController }, // index 42
    { "TOY_DOLL_ACTION_FIGURE", QBluetoothDeviceInfo::ToyDoll },
    { "TOY_GAME", QBluetoothDeviceInfo::ToyGame },
    { "TOY_ROBOT", QBluetoothDeviceInfo::ToyRobot },
    { "TOY_UNCATEGORIZED", QBluetoothDeviceInfo::UncategorizedToy },
    { "TOY_VEHICLE", QBluetoothDeviceInfo::ToyVehicle },
    { Q_NULLPTR, 0 }, // separator

    // QBluetoothDevice::WearableDevice -> 6 entries
    { "WEARABLE_GLASSES", QBluetoothDeviceInfo::WearableGlasses }, // index 49
    { "WEARABLE_HELMET", QBluetoothDeviceInfo::WearableHelmet },
    { "WEARABLE_JACKET", QBluetoothDeviceInfo::WearableJacket },
    { "WEARABLE_PAGER", QBluetoothDeviceInfo::WearablePager },
    { "WEARABLE_UNCATEGORIZED", QBluetoothDeviceInfo::UncategorizedWearableDevice },
    { "WEARABLE_WRIST_WATCH", QBluetoothDeviceInfo::WearableWristWatch },
    { Q_NULLPTR, 0 }, // separator

    // QBluetoothDevice::PeripheralDevice -> 3 entries
    // For some reason these are not mentioned in Android docs but still exist
    { "PERIPHERAL_NON_KEYBOARD_NON_POINTING", QBluetoothDeviceInfo::UncategorizedPeripheral }, // index 56
    { "PERIPHERAL_KEYBOARD", QBluetoothDeviceInfo::KeyboardPeripheral },
    { "PERIPHERAL_POINTING", QBluetoothDeviceInfo::PointingDevicePeripheral },
    { "PERIPHERAL_KEYBOARD_POINTING", QBluetoothDeviceInfo::KeyboardWithPointingDevicePeripheral },
    { Q_NULLPTR, 0 }, // separator

    // the following entries do not exist on Android
    // we map them to the unknown minor version case
    // QBluetoothDevice::Miscellaneous
    { Q_NULLPTR, 0 }, // index 61 & separator

    // QBluetoothDevice::LANAccessDevice
    { Q_NULLPTR, 0 }, // index 62 & separator

    // QBluetoothDevice::ImagingDevice
    { Q_NULLPTR, 0 }, // index 63 & separator

    // QBluetoothDevice::UncategorizedDevice
    { Q_NULLPTR, 0 }, // index 64 & separator
};

/*! Advertising Data Type (AD type) for LE scan records, as defined in Bluetooth CSS v6. */
enum ADType {
    ADType16BitUuidIncomplete = 0x02,
    ADType16BitUuidComplete = 0x03,
    ADType32BitUuidIncomplete = 0x04,
    ADType32BitUuidComplete = 0x05,
    ADType128BitUuidIncomplete = 0x06,
    ADType128BitUuidComplete = 0x07,
    // .. more will be added when required
};

// Endianness conversion for quint128 doesn't (yet) exist in qtendian.h
template <>
inline quint128 qbswap<quint128>(const quint128 src)
{
    quint128 dst;
    for (int i = 0; i < 16; i++)
        dst.data[i] = src.data[15 - i];
    return dst;
}

QBluetoothDeviceInfo::CoreConfigurations qtBtTypeForJavaBtType(jint javaType)
{
    const JCachedBtTypes::iterator it = cachedBtTypes()->find(javaType);
    if (it == cachedBtTypes()->end()) {
        QAndroidJniEnvironment env;

        if (javaType == QAndroidJniObject::getStaticField<jint>(
                            javaBluetoothDeviceClassName, javaDeviceTypeClassic)) {
            cachedBtTypes()->insert(javaType,
                                    QBluetoothDeviceInfo::BaseRateCoreConfiguration);
            return QBluetoothDeviceInfo::BaseRateCoreConfiguration;
        } else if (javaType == QAndroidJniObject::getStaticField<jint>(
                        javaBluetoothDeviceClassName, javaDeviceTypeLE)) {
            cachedBtTypes()->insert(javaType,
                                    QBluetoothDeviceInfo::LowEnergyCoreConfiguration);
            return QBluetoothDeviceInfo::LowEnergyCoreConfiguration;
        } else if (javaType == QAndroidJniObject::getStaticField<jint>(
                            javaBluetoothDeviceClassName, javaDeviceTypeDual)) {
            cachedBtTypes()->insert(javaType,
                                    QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration);
            return QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration;
        } else if (javaType == QAndroidJniObject::getStaticField<jint>(
                                javaBluetoothDeviceClassName, javaDeviceTypeUnknown)) {
            cachedBtTypes()->insert(javaType,
                                QBluetoothDeviceInfo::UnknownCoreConfiguration);
        } else {
            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
            }
            qCWarning(QT_BT_ANDROID) << "Unknown Bluetooth device type value";
        }

        return QBluetoothDeviceInfo::UnknownCoreConfiguration;
    } else {
        return it.value();
    }
}

QBluetoothDeviceInfo::MajorDeviceClass resolveAndroidMajorClass(jint javaType)
{
    QAndroidJniEnvironment env;

    const JCachedMajorTypes::iterator it = cachedMajorTypes()->find(javaType);
    if (it == cachedMajorTypes()->end()) {
        QAndroidJniEnvironment env;
        // precache all major device class fields
        int i = 0;
        jint fieldValue;
        QBluetoothDeviceInfo::MajorDeviceClass result = QBluetoothDeviceInfo::UncategorizedDevice;
        while (majorMappings[i].javaFieldName != Q_NULLPTR) {
            fieldValue = QAndroidJniObject::getStaticField<jint>(
                                    javaBluetoothClassDeviceMajorClassName, majorMappings[i].javaFieldName);
            if (env->ExceptionCheck()) {
                qCWarning(QT_BT_ANDROID) << "Unknown BluetoothClass.Device.Major field" << javaType;
                env->ExceptionDescribe();
                env->ExceptionClear();

                // add fallback value because field not readable
                cachedMajorTypes()->insert(javaType, QBluetoothDeviceInfo::UncategorizedDevice);
            } else {
                cachedMajorTypes()->insert(fieldValue, majorMappings[i].qtMajor);
            }

            if (fieldValue == javaType)
                result = majorMappings[i].qtMajor;

            i++;
        }

        return result;
    } else {
        return it.value();
    }
}

/*
    The index for major into the MinorClassJavaToQtMapping and initializedCacheTracker
    is major+1 except for UncategorizedDevice which is at index 0.
*/
int mappingIndexForMajor(QBluetoothDeviceInfo::MajorDeviceClass major)
{
    int mappingIndex = (int) major;
    if (major == QBluetoothDeviceInfo::UncategorizedDevice)
        mappingIndex = 0;
    else
        mappingIndex++;

    Q_ASSERT(mappingIndex >=0
             && mappingIndex <= (QBluetoothDeviceInfo::HealthDevice + 1));

    return mappingIndex;
}

void triggerCachingOfMinorsForMajor(QBluetoothDeviceInfo::MajorDeviceClass major)
{
    //qCDebug(QT_BT_ANDROID) << "Caching minor values for major" << major;
    int mappingIndex = mappingIndexForMajor(major);
    int sizeIndex = minorIndexSizes[mappingIndex];
    QAndroidJniEnvironment env;

    while (minorMappings[sizeIndex].javaFieldName != Q_NULLPTR) {
        jint fieldValue = QAndroidJniObject::getStaticField<jint>(
                    javaBluetoothClassDeviceClassName, minorMappings[sizeIndex].javaFieldName);
        if (env->ExceptionCheck()) { // field lookup failed? skip it
            env->ExceptionDescribe();
            env->ExceptionClear();
        }

        Q_ASSERT(fieldValue >= 0);
        cachedMinorTypes()->insert(fieldValue, minorMappings[sizeIndex].qtMinor);
        sizeIndex++;
    }

    initializedCacheTracker()->setBit(mappingIndex);
}

quint8 resolveAndroidMinorClass(QBluetoothDeviceInfo::MajorDeviceClass major, jint javaMinor)
{
    // there are no minor device classes in java with value 0
    //qCDebug(QT_BT_ANDROID) << "received minor class device:" << javaMinor;
    if (javaMinor == 0)
        return 0;

    int mappingIndex = mappingIndexForMajor(major);

    // whenever we encounter a not yet seen major device class
    // we populate the cache with all its related minor values
    if (!initializedCacheTracker()->at(mappingIndex))
        triggerCachingOfMinorsForMajor(major);

    const JCachedMinorTypes::iterator it = cachedMinorTypes()->find(javaMinor);
    if (it == cachedMinorTypes()->end())
        return 0;
    else
        return it.value();
}


DeviceDiscoveryBroadcastReceiver::DeviceDiscoveryBroadcastReceiver(QObject* parent): AndroidBroadcastReceiver(parent)
{
    addAction(valueForStaticField(JavaNames::BluetoothDevice, JavaNames::ActionFound));
    addAction(valueForStaticField(JavaNames::BluetoothAdapter, JavaNames::ActionDiscoveryStarted));
    addAction(valueForStaticField(JavaNames::BluetoothAdapter, JavaNames::ActionDiscoveryFinished));
}

// Runs in Java thread
void DeviceDiscoveryBroadcastReceiver::onReceive(JNIEnv *env, jobject context, jobject intent)
{
    Q_UNUSED(context);
    Q_UNUSED(env);

    QAndroidJniObject intentObject(intent);
    const QString action = intentObject.callObjectMethod("getAction", "()Ljava/lang/String;").toString();

    qCDebug(QT_BT_ANDROID) << "DeviceDiscoveryBroadcastReceiver::onReceive() - event:" << action;

    if (action == valueForStaticField(JavaNames::BluetoothAdapter,
                                      JavaNames::ActionDiscoveryFinished).toString()) {
        emit finished();
    } else if (action == valueForStaticField(JavaNames::BluetoothAdapter,
                                             JavaNames::ActionDiscoveryStarted).toString()) {

    } else if (action == valueForStaticField(JavaNames::BluetoothDevice,
                                             JavaNames::ActionFound).toString()) {
        //get BluetoothDevice
        QAndroidJniObject keyExtra = valueForStaticField(JavaNames::BluetoothDevice,
                                                         JavaNames::ExtraDevice);
        const QAndroidJniObject bluetoothDevice =
                intentObject.callObjectMethod("getParcelableExtra",
                                              "(Ljava/lang/String;)Landroid/os/Parcelable;",
                                              keyExtra.object<jstring>());

        if (!bluetoothDevice.isValid())
            return;

        keyExtra = valueForStaticField(JavaNames::BluetoothDevice,
                                       JavaNames::ExtraRssi);
        int rssi = intentObject.callMethod<jshort>("getShortExtra",
                                                "(Ljava/lang/String;S)S",
                                                keyExtra.object<jstring>(),
                                                0);

        const QBluetoothDeviceInfo info = retrieveDeviceInfo(env, bluetoothDevice, rssi);
        if (info.isValid())
            emit deviceDiscovered(info, false);
    }
}

// Runs in Java thread
void DeviceDiscoveryBroadcastReceiver::onReceiveLeScan(
        JNIEnv *env, jobject jBluetoothDevice, jint rssi, jbyteArray scanRecord)
{
    const QAndroidJniObject bluetoothDevice(jBluetoothDevice);
    if (!bluetoothDevice.isValid())
        return;

    const QBluetoothDeviceInfo info = retrieveDeviceInfo(env, bluetoothDevice, rssi, scanRecord);
    if (info.isValid())
        emit deviceDiscovered(info, true);
}

QBluetoothDeviceInfo DeviceDiscoveryBroadcastReceiver::retrieveDeviceInfo(JNIEnv *env, const QAndroidJniObject &bluetoothDevice, int rssi, jbyteArray scanRecord)
{
    const QString deviceName = bluetoothDevice.callObjectMethod<jstring>("getName").toString();
    const QBluetoothAddress deviceAddress(bluetoothDevice.callObjectMethod<jstring>("getAddress").toString());

    const QAndroidJniObject bluetoothClass = bluetoothDevice.callObjectMethod("getBluetoothClass",
                                                                        "()Landroid/bluetooth/BluetoothClass;");
    if (!bluetoothClass.isValid())
        return QBluetoothDeviceInfo();

    QBluetoothDeviceInfo::MajorDeviceClass majorClass = resolveAndroidMajorClass(
                                bluetoothClass.callMethod<jint>("getMajorDeviceClass"));
    // major device class is 5 bits from index 8 - 12
    quint32 classType = ((quint32(majorClass) & 0x1f) << 8);

    jint javaMinor = bluetoothClass.callMethod<jint>("getDeviceClass");
    quint8 minorDeviceType = resolveAndroidMinorClass(majorClass, javaMinor);

    // minor device class is 6 bits from index 2 - 7
    classType |= ((quint32(minorDeviceType) & 0x3f) << 2);

    static QList<quint32> services;
    if (services.count() == 0)
        services << QBluetoothDeviceInfo::PositioningService
                 << QBluetoothDeviceInfo::NetworkingService
                 << QBluetoothDeviceInfo::RenderingService
                 << QBluetoothDeviceInfo::CapturingService
                 << QBluetoothDeviceInfo::ObjectTransferService
                 << QBluetoothDeviceInfo::AudioService
                 << QBluetoothDeviceInfo::TelephonyService
                 << QBluetoothDeviceInfo::InformationService;

    // Matching BluetoothClass.Service values
    quint32 serviceResult = 0;
    quint32 current = 0;
    for (int i = 0; i < services.count(); i++) {
        current = services.at(i);
        int androidId = (current << 16); // Android values shift by 2 bytes compared to Qt enums
        if (bluetoothClass.callMethod<jboolean>("hasService", "(I)Z", androidId))
            serviceResult |= current;
    }

    // service class info is 11 bits from index 13 - 23
    classType |= (serviceResult << 13);

    QBluetoothDeviceInfo info(deviceAddress, deviceName, classType);
    info.setRssi(rssi);

    if (scanRecord != nullptr) {
        // Parse scan record
        jboolean isCopy;
        const char *scanRecordBuffer = reinterpret_cast<const char *>(env->GetByteArrayElements(scanRecord, &isCopy));
        const int scanRecordLength = env->GetArrayLength(scanRecord);

        QList<QBluetoothUuid> serviceUuids;
        int i = 0;

        // Spec 4.2, Vol 3, Part C, Chapter 11
        while (i < scanRecordLength) {
            // sizeof(EIR Data) = sizeof(Length) + sizeof(EIR data Type) + sizeof(EIR Data)
            // Length = sizeof(EIR data Type) + sizeof(EIR Data)

            const int nBytes = scanRecordBuffer[i];
            if (nBytes == 0)
                break;

            if ((i + nBytes) >= scanRecordLength)
                break;

            const int adType = scanRecordBuffer[i+1];
            const char *dataPtr = &scanRecordBuffer[i+2];
            QBluetoothUuid foundService;

            switch (adType) {
            case ADType16BitUuidIncomplete:
            case ADType16BitUuidComplete:
                foundService = QBluetoothUuid(qFromLittleEndian<quint16>(dataPtr));
                break;
            case ADType32BitUuidIncomplete:
            case ADType32BitUuidComplete:
                foundService = QBluetoothUuid(qFromLittleEndian<quint32>(dataPtr));
                break;
            case ADType128BitUuidIncomplete:
            case ADType128BitUuidComplete:
                foundService =
                    QBluetoothUuid(qToBigEndian<quint128>(qFromLittleEndian<quint128>(dataPtr)));
                break;
            default:
                // no other types supported yet and therefore skipped
                // https://www.bluetooth.org/en-us/specification/assigned-numbers/generic-access-profile
                break;
            }

            i += nBytes + 1;

            if (!foundService.isNull() && !serviceUuids.contains(foundService))
                serviceUuids.append(foundService);
        }

        info.setServiceUuids(serviceUuids, QBluetoothDeviceInfo::DataIncomplete);
    }

    if (QtAndroidPrivate::androidSdkVersion() >= 18) {
        jint javaBtType = bluetoothDevice.callMethod<jint>("getType");

        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        } else {
            info.setCoreConfigurations(qtBtTypeForJavaBtType(javaBtType));
        }
    }

    return info;
}

QT_END_NAMESPACE

