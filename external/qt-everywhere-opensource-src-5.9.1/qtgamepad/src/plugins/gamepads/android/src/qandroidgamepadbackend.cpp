/****************************************************************************
**
** Copyright (C) 2015 BogDan Vatra <bogdan@kde.org>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Gamepad module
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "qandroidgamepadbackend_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPair>
#include <QtCore/QThread>
#include <QtCore/QVector>

#include <functional>
#include <vector>

#include <QVariant>
#include <jni.h>

QT_BEGIN_NAMESPACE
namespace {
    const QLatin1String AXES_KEY("axes");
    const QLatin1String BUTTONS_KEY("buttons");

    class FunctionEvent : public QEvent
    {
        typedef std::function<void()> Function;
    public:
        explicit FunctionEvent(const Function &func)
            : QEvent(QEvent::User)
            , m_function(func)
        {}
        inline void call() { m_function(); }
        static inline void runOnQtThread(QObject *receiver, const Function &func) {
            if (qApp->thread() == QThread::currentThread())
                func();
            else
                qApp->postEvent(receiver, new FunctionEvent(func));
        }

    private:
        Function m_function;
    };

    const char keyEventClass[] = "android/view/KeyEvent";
    inline int keyField(const char *field)
    {
        return QJNIObjectPrivate::getStaticField<jint>(keyEventClass, field);
    }

    const char motionEventClass[] = "android/view/MotionEvent";
    inline int motionField(const char *field)
    {
        return QJNIObjectPrivate::getStaticField<jint>(motionEventClass, field);
    }

    const char inputDeviceClass[] = "android/view/InputDevice";
    inline int inputDeviceField(const char *field)
    {
        return QJNIObjectPrivate::getStaticField<jint>(inputDeviceClass, field);
    }


    struct DefaultMapping : public QAndroidGamepadBackend::Mapping {
        DefaultMapping()
        {
            buttonsMap[keyField("KEYCODE_BUTTON_A")] = QGamepadManager::ButtonA;
            buttonsMap[keyField("KEYCODE_BUTTON_B")] = QGamepadManager::ButtonB;
            buttonsMap[keyField("KEYCODE_BUTTON_X")] = QGamepadManager::ButtonX;
            buttonsMap[keyField("KEYCODE_BUTTON_Y")] = QGamepadManager::ButtonY;
            buttonsMap[keyField("KEYCODE_BUTTON_L1")] = QGamepadManager::ButtonL1;
            buttonsMap[keyField("KEYCODE_BUTTON_R1")] = QGamepadManager::ButtonR1;
            buttonsMap[keyField("KEYCODE_BUTTON_L2")] = QGamepadManager::ButtonL2;
            buttonsMap[keyField("KEYCODE_BUTTON_R2")] = QGamepadManager::ButtonR2;
            buttonsMap[keyField("KEYCODE_BUTTON_SELECT")] = QGamepadManager::ButtonSelect;
            buttonsMap[keyField("KEYCODE_BUTTON_START")] = QGamepadManager::ButtonStart;
            buttonsMap[keyField("KEYCODE_BUTTON_THUMBL")] = QGamepadManager::ButtonL3;
            buttonsMap[keyField("KEYCODE_BUTTON_THUMBR")] = QGamepadManager::ButtonR3;
            buttonsMap[keyField("KEYCODE_DPAD_UP")] = QGamepadManager::ButtonUp;
            buttonsMap[keyField("KEYCODE_DPAD_DOWN")] = QGamepadManager::ButtonDown;
            buttonsMap[keyField("KEYCODE_DPAD_RIGHT")] = QGamepadManager::ButtonRight;
            buttonsMap[keyField("KEYCODE_DPAD_LEFT")] = QGamepadManager::ButtonLeft;
            buttonsMap[keyField("KEYCODE_DPAD_CENTER")] = QGamepadManager::ButtonCenter;
            buttonsMap[keyField("KEYCODE_BUTTON_MODE")] = QGamepadManager::ButtonGuide;

            if (QtAndroidPrivate::androidSdkVersion() >= 12) {
                axisMap[motionField("AXIS_X")].gamepadAxis = QGamepadManager::AxisLeftX;
                axisMap[motionField("AXIS_Y")].gamepadAxis = QGamepadManager::AxisLeftY;
                axisMap[motionField("AXIS_HAT_X")].gamepadAxis = QGamepadManager::AxisLeftX;
                axisMap[motionField("AXIS_HAT_Y")].gamepadAxis = QGamepadManager::AxisLeftY;
                axisMap[motionField("AXIS_Z")].gamepadAxis = QGamepadManager::AxisRightX;
                axisMap[motionField("AXIS_RZ")].gamepadAxis = QGamepadManager::AxisRightY;
                {
                    auto &axis = axisMap[motionField("AXIS_LTRIGGER")];
                    axis.gamepadAxis = QGamepadManager::AxisInvalid;
                    axis.gamepadMinButton = QGamepadManager::ButtonL2;
                    axis.gamepadMaxButton = QGamepadManager::ButtonL2;
                }
                {
                    auto &axis = axisMap[motionField("AXIS_RTRIGGER")];
                    axis.gamepadAxis = QGamepadManager::AxisInvalid;
                    axis.gamepadMinButton = QGamepadManager::ButtonR2;
                    axis.gamepadMaxButton = QGamepadManager::ButtonR2;
                }

                allAndroidAxes.push_back(motionField("AXIS_X"));
                allAndroidAxes.push_back(motionField("AXIS_Y"));
                allAndroidAxes.push_back(motionField("AXIS_Z"));
                allAndroidAxes.push_back(motionField("AXIS_RZ"));
                allAndroidAxes.push_back(motionField("AXIS_BRAKE"));
                allAndroidAxes.push_back(motionField("AXIS_GAS"));

                for (int i = 1; i < 16; ++i)
                    allAndroidAxes.push_back(motionField(QByteArray("AXIS_GENERIC_").append(QByteArray::number(i)).constData()));

                allAndroidAxes.push_back(motionField("AXIS_HAT_X"));
                allAndroidAxes.push_back(motionField("AXIS_HAT_Y"));
                allAndroidAxes.push_back(motionField("AXIS_LTRIGGER"));
                allAndroidAxes.push_back(motionField("AXIS_RTRIGGER"));
                allAndroidAxes.push_back(motionField("AXIS_RUDDER"));
                allAndroidAxes.push_back(motionField("AXIS_THROTTLE"));
                allAndroidAxes.push_back(motionField("AXIS_WHEEL"));
            }

            if (QtAndroidPrivate::androidSdkVersion() >= 12) {
                acceptedSources.push_back(inputDeviceField("SOURCE_GAMEPAD"));
                acceptedSources.push_back(inputDeviceField("SOURCE_CLASS_JOYSTICK"));
                if (QtAndroidPrivate::androidSdkVersion() >= 21) {
                    acceptedSources.push_back(inputDeviceField("SOURCE_HDMI"));
                }
            } else {
                acceptedSources.push_back(inputDeviceField("SOURCE_DPAD"));
            }

            ACTION_DOWN = keyField("ACTION_DOWN");
            ACTION_UP = keyField("ACTION_UP");
            ACTION_MULTIPLE = keyField("ACTION_MULTIPLE");
            FLAG_LONG_PRESS = keyField("FLAG_LONG_PRESS");
        }
        std::vector<int> acceptedSources;
        std::vector<int> allAndroidAxes;
        int ACTION_DOWN, ACTION_MULTIPLE, ACTION_UP, FLAG_LONG_PRESS;
    };

    void onInputDeviceAdded(JNIEnv *, jclass, jlong qtNativePtr, int deviceId)
    {
        if (!qtNativePtr)
            return;
        reinterpret_cast<QAndroidGamepadBackend*>(qtNativePtr)->addDevice(deviceId);
    }
    void onInputDeviceRemoved(JNIEnv *, jclass, jlong qtNativePtr, int deviceId)
    {
        if (!qtNativePtr)
            return;
        reinterpret_cast<QAndroidGamepadBackend*>(qtNativePtr)->removeDevice(deviceId);
    }
    void onInputDeviceChanged(JNIEnv *, jclass, jlong qtNativePtr, int deviceId)
    {
        if (!qtNativePtr)
            return;
        reinterpret_cast<QAndroidGamepadBackend*>(qtNativePtr)->updateDevice(deviceId);
    }

    static JNINativeMethod methods[] = {
        {"onInputDeviceAdded", "(JI)V", (void *)onInputDeviceAdded},
        {"onInputDeviceRemoved", "(JI)V", (void *)onInputDeviceRemoved},
        {"onInputDeviceChanged", "(JI)V", (void *)onInputDeviceChanged}
    };

    const char qtGamePadClassName[] = "org/qtproject/qt5/android/gamepad/QtGamepad";

    inline void setAxisInfo(QJNIObjectPrivate &event, int axis, QAndroidGamepadBackend::Mapping::AndroidAxisInfo &info)
    {
        QJNIObjectPrivate device(event.callObjectMethod("getDevice", "()Landroid/view/InputDevice;"));
        if (device.isValid()) {
            const int source = event.callMethod<jint>("getSource", "()I");
            QJNIObjectPrivate motionRange = device.callObjectMethod("getMotionRange","(II)Landroid/view/InputDevice$MotionRange;", axis, source);
            if (motionRange.isValid()) {
                info.flatArea = motionRange.callMethod<jfloat>("getFlat", "()F");
                info.minValue = motionRange.callMethod<jfloat>("getMin", "()F");
                info.maxValue = motionRange.callMethod<jfloat>("getMax", "()F");
                info.fuzz = motionRange.callMethod<jfloat>("getFuzz", "()F");
                return;
            }
        }
        info.flatArea = 0;
    }

} // namespace

Q_GLOBAL_STATIC(DefaultMapping, g_defaultMapping)

void QAndroidGamepadBackend::Mapping::AndroidAxisInfo::restoreSavedData(const QVariantMap &value)
{
    gamepadAxis = QGamepadManager::GamepadAxis(value[QLatin1Literal("axis")].toInt());
    gamepadMinButton = QGamepadManager::GamepadButton(value[QLatin1Literal("minButton")].toInt());
    gamepadMaxButton = QGamepadManager::GamepadButton(value[QLatin1Literal("maxButton")].toInt());
}

QVariantMap QAndroidGamepadBackend::Mapping::AndroidAxisInfo::dataToSave() const
{
    QVariantMap data;
    data[QLatin1Literal("axis")] = gamepadAxis;
    data[QLatin1Literal("minButton")] = gamepadMinButton;
    data[QLatin1Literal("maxButton")] = gamepadMaxButton;
    return data;
}

QAndroidGamepadBackend::QAndroidGamepadBackend(QObject *parent)
    : QGamepadBackend(parent)
{
    QtAndroidPrivate::registerGenericMotionEventListener(this);
    QtAndroidPrivate::registerKeyEventListener(this);
}

QAndroidGamepadBackend::~QAndroidGamepadBackend()
{
    QtAndroidPrivate::unregisterGenericMotionEventListener(this);
    QtAndroidPrivate::unregisterKeyEventListener(this);
}

void QAndroidGamepadBackend::addDevice(int deviceId)
{
    if (deviceId == -1)
        return;

    QMutexLocker lock(&m_mutex);
    QJNIObjectPrivate inputDevice = QJNIObjectPrivate::callStaticObjectMethod(inputDeviceClass, "getDevice", "(I)Landroid/view/InputDevice;", deviceId);
    int sources = inputDevice.callMethod<jint>("getSources", "()I");
    bool acceptable = false;
    for (int source : g_defaultMapping()->acceptedSources) {
        if ( (source & sources) == source) {
            acceptable = true;
            break;
        }
    }

    if (acceptable) {
        m_devices.insert(deviceId, *g_defaultMapping());
        int productId = qHash(inputDevice.callObjectMethod("getDescriptor", "()Ljava/lang/String;").toString());
        m_devices[deviceId].productId = productId;
        if (productId) {
            QVariant settings = readSettings(productId);
            if (!settings.isNull()) {
                auto &deviceInfo = m_devices[deviceId];
                deviceInfo.needsConfigure = false;

                QVariantMap data = settings.toMap()[AXES_KEY].toMap();
                for (QVariantMap::const_iterator it = data.begin(); it != data.end(); ++it)
                    deviceInfo.axisMap[it.key().toInt()].restoreSavedData(it.value().toMap());

                data = settings.toMap()[BUTTONS_KEY].toMap();
                for (QVariantMap::const_iterator it = data.begin(); it != data.end(); ++it)
                    deviceInfo.buttonsMap[it.key().toInt()] = QGamepadManager::GamepadButton(it.value().toInt());
            }
        }
        FunctionEvent::runOnQtThread(this, [this, deviceId]{
            emit gamepadAdded(deviceId);
        });
    }
}

void QAndroidGamepadBackend::updateDevice(int deviceId)
{
    Q_UNUSED(deviceId)
    //QMutexLocker lock(&m_mutex);
}

void QAndroidGamepadBackend::removeDevice(int deviceId)
{
    QMutexLocker lock(&m_mutex);
    if (m_devices.remove(deviceId)) {
        FunctionEvent::runOnQtThread(this, [this, deviceId]{
            emit gamepadRemoved(deviceId);
        });
    }
}

bool QAndroidGamepadBackend::event(QEvent *ev)
{
    if (ev->type() == QEvent::User) {
        static_cast<FunctionEvent*>(ev)->call();
        return true;
    }
    return QGamepadBackend::event(ev);
}

bool QAndroidGamepadBackend::isConfigurationNeeded(int deviceId)
{
    QMutexLocker lock(&m_mutex);
    auto it = m_devices.find(deviceId);
    if (it == m_devices.end())
        return false;
    return it->needsConfigure;
}

bool QAndroidGamepadBackend::configureButton(int deviceId, QGamepadManager::GamepadButton button)
{
    QMutexLocker lock(&m_mutex);
    auto it = m_devices.find(deviceId);
    if (it == m_devices.end())
        return false;

    it.value().calibrateButton = button;
    return true;
}

bool QAndroidGamepadBackend::configureAxis(int deviceId, QGamepadManager::GamepadAxis axis)
{
    QMutexLocker lock(&m_mutex);
    auto it = m_devices.find(deviceId);
    if (it == m_devices.end())
        return false;

    it.value().calibrateAxis = axis;
    return true;
}

bool QAndroidGamepadBackend::setCancelConfigureButton(int deviceId, QGamepadManager::GamepadButton button)
{
    QMutexLocker lock(&m_mutex);
    auto it = m_devices.find(deviceId);
    if (it == m_devices.end())
        return false;

    it.value().cancelConfigurationButton = button;
    return true;
}

void QAndroidGamepadBackend::resetConfiguration(int deviceId)
{
    QMutexLocker lock(&m_mutex);
    auto it = m_devices.find(deviceId);
    if (it == m_devices.end())
        return;

    it.value().axisMap.clear();
    it.value().buttonsMap.clear();
    it.value().calibrateButton = QGamepadManager::ButtonInvalid;
    it.value().calibrateAxis = QGamepadManager::AxisInvalid;
    it.value().cancelConfigurationButton = QGamepadManager::ButtonInvalid;
    it.value().needsConfigure = false;
}

bool QAndroidGamepadBackend::handleKeyEvent(jobject event)
{
    QJNIObjectPrivate ev(event);
    QMutexLocker lock(&m_mutex);
    const int deviceId = ev.callMethod<jint>("getDeviceId", "()I");
    const auto deviceIt = m_devices.find(deviceId);
    if (deviceIt == m_devices.end())
        return false;

    const int action = ev.callMethod<jint>("getAction", "()I");
    if (action != g_defaultMapping()->ACTION_DOWN &&
            action != g_defaultMapping()->ACTION_UP) {
        return false;
    }
    const int flags = ev.callMethod<jint>("getFlags", "()I");
    if ((flags & g_defaultMapping()->FLAG_LONG_PRESS) == g_defaultMapping()->FLAG_LONG_PRESS)
        return false;

    const int keyCode = ev.callMethod<jint>("getKeyCode", "()I");
    auto &deviceMap = deviceIt.value();

    if (deviceMap.cancelConfigurationButton != QGamepadManager::ButtonInvalid &&
            (deviceMap.calibrateButton != QGamepadManager::ButtonInvalid ||
             deviceMap.calibrateAxis != QGamepadManager::AxisInvalid)) {

        const auto buttonsMapIt = deviceMap.buttonsMap.find(keyCode);
        if (buttonsMapIt != deviceMap.buttonsMap.end() &&
                deviceMap.cancelConfigurationButton == buttonsMapIt.value()) {
            deviceMap.calibrateButton = QGamepadManager::ButtonInvalid;
            deviceMap.calibrateAxis = QGamepadManager::AxisInvalid;
            FunctionEvent::runOnQtThread(this, [this, deviceId]{
                emit configurationCanceled(deviceId);
            });
            return true;
        }
    }

    if (deviceMap.calibrateButton != QGamepadManager::ButtonInvalid) {
        deviceMap.buttonsMap[keyCode] = deviceMap.calibrateButton;
        auto but = deviceMap.calibrateButton;
        deviceMap.calibrateButton = QGamepadManager::ButtonInvalid;
        saveData(deviceMap);
        FunctionEvent::runOnQtThread(this, [this, deviceId, but]{
            emit buttonConfigured(deviceId, but);
        });
    }

    const auto buttonsMapIt = deviceMap.buttonsMap.find(keyCode);
    if (buttonsMapIt == deviceMap.buttonsMap.end())
        return false;

    const auto button = buttonsMapIt.value();
    if (action == g_defaultMapping()->ACTION_DOWN) {
        FunctionEvent::runOnQtThread(this, [this, deviceId, button]{
            emit gamepadButtonPressed(deviceId, button, 1.0);
        });
    } else {
        FunctionEvent::runOnQtThread(this, [this, deviceId, button]{
           emit gamepadButtonReleased(deviceId, button);
        });
    }
    return true;
}

bool QAndroidGamepadBackend::handleGenericMotionEvent(jobject event)
{
    // GenericMotionEvent was introduced in API-12
    Q_ASSERT(QtAndroidPrivate::androidSdkVersion() >= 12);

    QJNIObjectPrivate ev(event);
    QMutexLocker lock(&m_mutex);
    const int deviceId = ev.callMethod<jint>("getDeviceId", "()I");
    const auto deviceIt = m_devices.find(deviceId);
    if (deviceIt == m_devices.end())
        return false;

    auto &deviceMap = deviceIt.value();
    if (deviceMap.calibrateAxis != QGamepadManager::AxisInvalid ||
            deviceMap.calibrateButton != QGamepadManager::ButtonInvalid) {
        double lastValue = 0;
        int lastAxis = -1;
        for (int axis : g_defaultMapping()->allAndroidAxes) {
            double value = ev.callMethod<jfloat>("getAxisValue", "(I)F", axis);
            if (fabs(value) > fabs(lastValue)) {
                lastValue = value;
                lastAxis = axis;
            }
        }

        if (!lastValue || lastAxis == -1)
            return false;

        if (deviceMap.calibrateAxis != QGamepadManager::AxisInvalid) {
            deviceMap.axisMap[lastAxis].gamepadAxis = deviceMap.calibrateAxis;
            auto axis = deviceMap.calibrateAxis;
            deviceMap.calibrateAxis = QGamepadManager::AxisInvalid;
            saveData(deviceMap);
            FunctionEvent::runOnQtThread(this, [this, deviceId, axis]{
                emit axisConfigured(deviceId, axis);
            });
        } else if (deviceMap.calibrateButton != QGamepadManager::ButtonInvalid &&
                   deviceMap.calibrateButton != QGamepadManager::ButtonUp &&
                   deviceMap.calibrateButton != QGamepadManager::ButtonDown &&
                   deviceMap.calibrateButton != QGamepadManager::ButtonLeft &&
                   deviceMap.calibrateButton != QGamepadManager::ButtonRight) {
            auto &axis = deviceMap.axisMap[lastAxis];
            axis.gamepadAxis = QGamepadManager::AxisInvalid;
            setAxisInfo(ev, lastAxis, axis);
            bool save = false;
            if (lastValue == axis.minValue) {
                axis.gamepadMinButton = deviceMap.calibrateButton;
                if (axis.gamepadMaxButton == QGamepadManager::ButtonInvalid)
                    axis.gamepadMaxButton = deviceMap.calibrateButton;
                save = true;
            } else if (lastValue == axis.maxValue) {
                axis.gamepadMaxButton = deviceMap.calibrateButton;
                if (axis.gamepadMinButton == QGamepadManager::ButtonInvalid)
                    axis.gamepadMinButton = deviceMap.calibrateButton;
                save = true;
            }

            if (save) {
                auto but = deviceMap.calibrateButton;
                deviceMap.calibrateButton = QGamepadManager::ButtonInvalid;
                saveData(deviceMap);
                FunctionEvent::runOnQtThread(this, [this, deviceId, but]{
                    emit buttonConfigured(deviceId, but);
                });
            }
        }
    }

    typedef QPair<QGamepadManager::GamepadAxis, double> GamepadAxisValue;
    QVector<GamepadAxisValue> axisValues;
    typedef QPair<QGamepadManager::GamepadButton, double> GamepadButtonValue;
    QVector<GamepadButtonValue> buttonValues;
    auto setValue = [&axisValues, &buttonValues](Mapping::AndroidAxisInfo &axisInfo, double value) {
        if (axisInfo.setValue(value)) {
            if (axisInfo.gamepadAxis != QGamepadManager::AxisInvalid) {
                axisValues.push_back(GamepadAxisValue(axisInfo.gamepadAxis, axisInfo.lastValue));
            } else {
                if (axisInfo.lastValue < 0) {
                    buttonValues.push_back(GamepadButtonValue(axisInfo.gamepadMinButton, axisInfo.lastValue));
                    axisInfo.gamepadLastButton = axisInfo.gamepadMinButton;
                } else if (axisInfo.lastValue > 0) {
                    buttonValues.push_back(GamepadButtonValue(axisInfo.gamepadMaxButton, axisInfo.lastValue));
                    axisInfo.gamepadLastButton = axisInfo.gamepadMaxButton;
                } else {
                    buttonValues.push_back(GamepadButtonValue(axisInfo.gamepadLastButton, 0.0));
                    axisInfo.gamepadLastButton = QGamepadManager::ButtonInvalid;
                }
            }
        }
    };
    for (auto it = deviceMap.axisMap.begin(); it != deviceMap.axisMap.end(); ++it) {
        auto &axisInfo = it.value();
        if (axisInfo.flatArea == -1)
            setAxisInfo(ev, it.key(), axisInfo);
        const int historicalValues = ev.callMethod<jint>("getHistorySize", "()I");
        for (int i = 0; i < historicalValues; ++i) {
            double value = ev.callMethod<jfloat>("getHistoricalAxisValue", "(II)F", it.key(), i);
            setValue(axisInfo, value);
        }
        double value = ev.callMethod<jfloat>("getAxisValue", "(I)F", it.key());
        setValue(axisInfo, value);
    }

    if (!axisValues.isEmpty()) {
        FunctionEvent::runOnQtThread(this, [this, deviceId, axisValues]{
            for (const auto &axisValue : axisValues)
                emit gamepadAxisMoved(deviceId, axisValue.first, axisValue.second);
        });
    }

    if (!buttonValues.isEmpty()) {
        FunctionEvent::runOnQtThread(this, [this, deviceId, buttonValues]{
            for (const auto &buttonValue : buttonValues)
                if (buttonValue.second)
                    emit gamepadButtonPressed(deviceId, buttonValue.first, fabs(buttonValue.second));
                else
                    emit gamepadButtonReleased(deviceId, buttonValue.first);
        });
    }

    return false;
}

bool QAndroidGamepadBackend::start()
{
    {
        QMutexLocker lock(&m_mutex);
        if (QtAndroidPrivate::androidSdkVersion() >= 16) {
            if (!m_qtGamepad.isValid())
                m_qtGamepad = QJNIObjectPrivate(qtGamePadClassName, "(Landroid/app/Activity;)V", QtAndroidPrivate::activity());
            m_qtGamepad.callMethod<void>("register", "(J)V", jlong(this));
        }
    }

    QJNIObjectPrivate ids = QJNIObjectPrivate::callStaticObjectMethod(inputDeviceClass, "getDeviceIds", "()[I");
    jintArray jarr = jintArray(ids.object());
    QJNIEnvironmentPrivate env;
    size_t sz = env->GetArrayLength(jarr);
    jint *buff = env->GetIntArrayElements(jarr, nullptr);
    for (size_t i = 0; i < sz; ++i)
        addDevice(buff[i]);
    env->ReleaseIntArrayElements(jarr, buff, 0);
    return true;
}

void QAndroidGamepadBackend::stop()
{
    QMutexLocker lock(&m_mutex);
    if (QtAndroidPrivate::androidSdkVersion() >= 16 && m_qtGamepad.isValid())
        m_qtGamepad.callMethod<void>("unregister", "()V");
}

void QAndroidGamepadBackend::saveData(const QAndroidGamepadBackend::Mapping &deviceInfo)
{
    if (!deviceInfo.productId)
        return ;

    QVariantMap settings, data;
    for (auto it = deviceInfo.axisMap.begin(); it != deviceInfo.axisMap.end(); ++it)
        data[QString::number(it.key())] = it.value().dataToSave();
    settings[AXES_KEY] = data;

    data.clear();
    for (auto it = deviceInfo.buttonsMap.begin(); it != deviceInfo.buttonsMap.end(); ++it)
        data[QString::number(it.key())] = it.value();
    settings[BUTTONS_KEY] = data;

    saveSettings(deviceInfo.productId, settings);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void */*reserved*/)
{
    static bool initialized = false;
    if (initialized)
        return JNI_VERSION_1_6;
    initialized = true;

    JNIEnv* env;
    // get the JNIEnv pointer.
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
        return JNI_ERR;

    // search for Java class which declares the native methods
    jclass javaClass = env->FindClass("org/qtproject/qt5/android/gamepad/QtGamepad");
    if (!javaClass)
        return JNI_ERR;

    // register our native methods
    if (env->RegisterNatives(javaClass, methods,
                             sizeof(methods) / sizeof(methods[0])) < 0) {
        return JNI_ERR;
    }
    return JNI_VERSION_1_6;
}

QT_END_NAMESPACE
