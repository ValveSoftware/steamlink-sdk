/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#ifndef QQUICKEVENTS_P_P_H
#define QQUICKEVENTS_P_P_H

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

#include <private/qtquickglobal_p.h>
#include <qqml.h>

#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtGui/qvector2d.h>
#include <QtGui/qevent.h>
#include <QtGui/qkeysequence.h>
#include <QtQuick/qquickitem.h>

QT_BEGIN_NAMESPACE

class QQuickPointerDevice;
class QQuickPointerEvent;
class QQuickPointerMouseEvent;
class QQuickPointerTabletEvent;
class QQuickPointerTouchEvent;

class QQuickKeyEvent : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int key READ key)
    Q_PROPERTY(QString text READ text)
    Q_PROPERTY(int modifiers READ modifiers)
    Q_PROPERTY(bool isAutoRepeat READ isAutoRepeat)
    Q_PROPERTY(int count READ count)
    Q_PROPERTY(quint32 nativeScanCode READ nativeScanCode)
    Q_PROPERTY(bool accepted READ isAccepted WRITE setAccepted)

public:
    QQuickKeyEvent()
        : event(QEvent::None, 0, 0)
    {}

    void reset(QEvent::Type type, int key, Qt::KeyboardModifiers modifiers,
               const QString &text = QString(), bool autorep = false, ushort count = 1)
    {
        event = QKeyEvent(type, key, modifiers, text, autorep, count);
        event.setAccepted(false);
    }

    void reset(const QKeyEvent &ke)
    {
        event = ke;
        event.setAccepted(false);
    }

    int key() const { return event.key(); }
    QString text() const { return event.text(); }
    int modifiers() const { return event.modifiers(); }
    bool isAutoRepeat() const { return event.isAutoRepeat(); }
    int count() const { return event.count(); }
    quint32 nativeScanCode() const { return event.nativeScanCode(); }

    bool isAccepted() { return event.isAccepted(); }
    void setAccepted(bool accepted) { event.setAccepted(accepted); }

    Q_REVISION(2) Q_INVOKABLE bool matches(QKeySequence::StandardKey key) const { return event.matches(key); }

private:
    QKeyEvent event;
};

// used in Qt Location
class Q_QUICK_PRIVATE_EXPORT QQuickMouseEvent : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal x READ x)
    Q_PROPERTY(qreal y READ y)
    Q_PROPERTY(int button READ button)
    Q_PROPERTY(int buttons READ buttons)
    Q_PROPERTY(int modifiers READ modifiers)
    Q_PROPERTY(int source READ source REVISION 7)
    Q_PROPERTY(bool wasHeld READ wasHeld)
    Q_PROPERTY(bool isClick READ isClick)
    Q_PROPERTY(bool accepted READ isAccepted WRITE setAccepted)

public:
    QQuickMouseEvent()
      : _x(0), _y(0), _button(Qt::NoButton), _buttons(Qt::NoButton), _modifiers(Qt::NoModifier)
      , _source(Qt::MouseEventNotSynthesized), _wasHeld(false), _isClick(false), _accepted(false)
    {}

    void reset(qreal x, qreal y, Qt::MouseButton button, Qt::MouseButtons buttons,
               Qt::KeyboardModifiers modifiers, bool isClick = false, bool wasHeld = false)
    {
        _x = x;
        _y = y;
        _button = button;
        _buttons = buttons;
        _modifiers = modifiers;
        _source = Qt::MouseEventNotSynthesized;
        _wasHeld = wasHeld;
        _isClick = isClick;
        _accepted = true;
    }

    qreal x() const { return _x; }
    qreal y() const { return _y; }
    int button() const { return _button; }
    int buttons() const { return _buttons; }
    int modifiers() const { return _modifiers; }
    int source() const { return _source; }
    bool wasHeld() const { return _wasHeld; }
    bool isClick() const { return _isClick; }

    // only for internal usage
    void setX(qreal x) { _x = x; }
    void setY(qreal y) { _y = y; }
    void setPosition(const QPointF &point) { _x = point.x(); _y = point.y(); }
    void setSource(Qt::MouseEventSource s) { _source = s; }

    bool isAccepted() { return _accepted; }
    void setAccepted(bool accepted) { _accepted = accepted; }

private:
    qreal _x;
    qreal _y;
    Qt::MouseButton _button;
    Qt::MouseButtons _buttons;
    Qt::KeyboardModifiers _modifiers;
    Qt::MouseEventSource _source;
    bool _wasHeld : 1;
    bool _isClick : 1;
    bool _accepted : 1;
};

class QQuickWheelEvent : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal x READ x)
    Q_PROPERTY(qreal y READ y)
    Q_PROPERTY(QPoint angleDelta READ angleDelta)
    Q_PROPERTY(QPoint pixelDelta READ pixelDelta)
    Q_PROPERTY(int buttons READ buttons)
    Q_PROPERTY(int modifiers READ modifiers)
    Q_PROPERTY(bool inverted READ inverted)
    Q_PROPERTY(bool accepted READ isAccepted WRITE setAccepted)

public:
    QQuickWheelEvent()
      : _x(0), _y(0), _buttons(Qt::NoButton), _modifiers(Qt::NoModifier)
      , _inverted(false), _accepted(false)
    {}

    void reset(qreal x, qreal y, const QPoint &angleDelta, const QPoint &pixelDelta,
                     Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers, bool inverted)
    {
        _x = x;
        _y = y;
        _angleDelta = angleDelta;
        _pixelDelta = pixelDelta;
        _buttons = buttons;
        _modifiers = modifiers;
        _accepted = true;
        _inverted = inverted;
    }

    qreal x() const { return _x; }
    qreal y() const { return _y; }
    QPoint angleDelta() const { return _angleDelta; }
    QPoint pixelDelta() const { return _pixelDelta; }
    int buttons() const { return _buttons; }
    int modifiers() const { return _modifiers; }
    bool inverted() const { return _inverted; }
    bool isAccepted() { return _accepted; }
    void setAccepted(bool accepted) { _accepted = accepted; }

private:
    qreal _x;
    qreal _y;
    QPoint _angleDelta;
    QPoint _pixelDelta;
    Qt::MouseButtons _buttons;
    Qt::KeyboardModifiers _modifiers;
    bool _inverted;
    bool _accepted;
};

class Q_QUICK_PRIVATE_EXPORT QQuickCloseEvent : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool accepted READ isAccepted WRITE setAccepted)

public:
    QQuickCloseEvent()
        : _accepted(true) {}

    bool isAccepted() { return _accepted; }
    void setAccepted(bool accepted) { _accepted = accepted; }

private:
    bool _accepted;
};

class Q_QUICK_PRIVATE_EXPORT QQuickEventPoint : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QPointF scenePos READ scenePos)
    Q_PROPERTY(State state READ state)
    Q_PROPERTY(quint64 pointId READ pointId)
    Q_PROPERTY(qreal timeHeld READ timeHeld)
    Q_PROPERTY(bool accepted READ isAccepted WRITE setAccepted)
    Q_PROPERTY(QQuickItem *grabber READ grabber WRITE setGrabber)

public:
    enum State {
        Pressed     = Qt::TouchPointPressed,
        Updated     = Qt::TouchPointMoved,
        Stationary  = Qt::TouchPointStationary,
        Released    = Qt::TouchPointReleased
        // Canceled    = Qt::TouchPointReleased << 1 // 0x10 // TODO maybe
    };
    Q_ENUM(State)

    QQuickEventPoint(QQuickPointerEvent *parent);

    void reset(Qt::TouchPointState state, QPointF scenePos, quint64 pointId, ulong timestamp);

    void invalidate() { m_valid = false; }

    QQuickPointerEvent *pointerEvent() const;
    QPointF scenePos() const { return m_scenePos; }
    State state() const { return m_state; }
    quint64 pointId() const { return m_pointId; }
    bool isValid() const { return m_valid; }
    qreal timeHeld() const { return (m_timestamp - m_pressTimestamp) / 1000.0; }
    bool isAccepted() const { return m_accept; }
    void setAccepted(bool accepted = true);
    QQuickItem *grabber() const;
    void setGrabber(QQuickItem *grabber);

private:
    QPointF m_scenePos;
    quint64 m_pointId;
    QPointer<QQuickItem> m_grabber;
    ulong m_timestamp;
    ulong m_pressTimestamp;
    State m_state;
    bool m_valid : 1;
    bool m_accept : 1;
    int m_reserved : 30;

    Q_DISABLE_COPY(QQuickEventPoint)
};

class Q_QUICK_PRIVATE_EXPORT QQuickEventTouchPoint : public QQuickEventPoint
{
    Q_OBJECT
    Q_PROPERTY(qreal rotation READ rotation)
    Q_PROPERTY(qreal pressure READ pressure)
    Q_PROPERTY(QPointingDeviceUniqueId uniqueId READ uniqueId)

public:
    QQuickEventTouchPoint(QQuickPointerTouchEvent *parent);

    void reset(const QTouchEvent::TouchPoint &tp, ulong timestamp);

    qreal rotation() const { return m_rotation; }
    qreal pressure() const { return m_pressure; }
    QPointingDeviceUniqueId uniqueId() const { return m_uniqueId; }

private:
    qreal m_rotation;
    qreal m_pressure;
    QPointingDeviceUniqueId m_uniqueId;

    Q_DISABLE_COPY(QQuickEventTouchPoint)
};

class Q_QUICK_PRIVATE_EXPORT QQuickPointerEvent : public QObject
{
    Q_OBJECT
    Q_PROPERTY(const QQuickPointerDevice *device READ device)
    Q_PROPERTY(Qt::KeyboardModifiers modifiers READ modifiers)
    Q_PROPERTY(Qt::MouseButtons button READ button)
    Q_PROPERTY(Qt::MouseButtons buttons READ buttons)

public:
    QQuickPointerEvent(QObject *parent = nullptr)
      : QObject(parent)
      , m_device(nullptr)
      , m_event(nullptr)
      , m_button(Qt::NoButton)
      , m_pressedButtons(Qt::NoButton)
    { }

    virtual ~QQuickPointerEvent();

public: // property accessors
    QQuickPointerDevice *device() const { return m_device; }
    Qt::KeyboardModifiers modifiers() const { return m_event ? m_event->modifiers() : Qt::NoModifier; }
    Qt::MouseButton button() const { return m_button; }
    Qt::MouseButtons buttons() const { return m_pressedButtons; }

public: // helpers for C++ only (during event delivery)
    virtual QQuickPointerEvent *reset(QEvent *ev) = 0;

    virtual bool isPressEvent() const = 0;
    virtual QQuickPointerMouseEvent *asPointerMouseEvent() { return nullptr; }
    virtual QQuickPointerTouchEvent *asPointerTouchEvent() { return nullptr; }
    virtual QQuickPointerTabletEvent *asPointerTabletEvent() { return nullptr; }
    virtual const QQuickPointerMouseEvent *asPointerMouseEvent() const { return nullptr; }
    virtual const QQuickPointerTouchEvent *asPointerTouchEvent() const { return nullptr; }
    virtual const QQuickPointerTabletEvent *asPointerTabletEvent() const { return nullptr; }
    bool isValid() const { return m_event != nullptr; }
    virtual bool allPointsAccepted() const = 0;
    bool isAccepted() { return m_event->isAccepted(); }
    void setAccepted(bool accepted) { m_event->setAccepted(accepted); }
    QVector<QPointF> unacceptedPressedPointScenePositions() const;

    virtual int pointCount() const = 0;
    virtual QQuickEventPoint *point(int i) const = 0;
    virtual QQuickEventPoint *pointById(quint64 pointId) const = 0;
    virtual QVector<QQuickItem *> grabbers() const = 0;
    virtual void clearGrabbers() const = 0;

    ulong timestamp() const { return m_event->timestamp(); }

protected:
    QQuickPointerDevice *m_device;
    QInputEvent *m_event; // original event as received by QQuickWindow
    Qt::MouseButton m_button;
    Qt::MouseButtons m_pressedButtons;

    Q_DISABLE_COPY(QQuickPointerEvent)
};

class Q_QUICK_PRIVATE_EXPORT QQuickPointerMouseEvent : public QQuickPointerEvent
{
    Q_OBJECT
public:
    QQuickPointerMouseEvent(QObject *parent = nullptr)
        : QQuickPointerEvent(parent), m_mousePoint(new QQuickEventPoint(this)) { }

    QQuickPointerEvent *reset(QEvent *) override;
    bool isPressEvent() const override;
    QQuickPointerMouseEvent *asPointerMouseEvent() override { return this; }
    const QQuickPointerMouseEvent *asPointerMouseEvent() const override { return this; }
    int pointCount() const override { return 1; }
    QQuickEventPoint *point(int i) const override;
    QQuickEventPoint *pointById(quint64 pointId) const override;
    bool allPointsAccepted() const override;
    QVector<QQuickItem *> grabbers() const override;
    void clearGrabbers() const override;

    QMouseEvent *asMouseEvent(const QPointF& localPos) const;

private:
    QQuickEventPoint *m_mousePoint;

    Q_DISABLE_COPY(QQuickPointerMouseEvent)
};

class Q_QUICK_PRIVATE_EXPORT QQuickPointerTouchEvent : public QQuickPointerEvent
{
    Q_OBJECT
public:
    QQuickPointerTouchEvent(QObject *parent = nullptr)
        : QQuickPointerEvent(parent)
        , m_pointCount(0)
        , m_synthMouseEvent(QEvent::MouseMove, QPointF(), Qt::NoButton, Qt::NoButton, Qt::NoModifier)
    { }

    QQuickPointerEvent *reset(QEvent *) override;
    bool isPressEvent() const override;
    QQuickPointerTouchEvent *asPointerTouchEvent() override { return this; }
    const QQuickPointerTouchEvent *asPointerTouchEvent() const override { return this; }
    int pointCount() const override { return m_pointCount; }
    QQuickEventPoint *point(int i) const override;
    QQuickEventPoint *pointById(quint64 pointId) const override;
    const QTouchEvent::TouchPoint *touchPointById(int pointId) const;
    bool allPointsAccepted() const override;
    QVector<QQuickItem *> grabbers() const override;
    void clearGrabbers() const override;

    QMouseEvent *syntheticMouseEvent(int pointID, QQuickItem *relativeTo) const;
    QTouchEvent *touchEventForItem(QQuickItem *item, bool isFiltering = false) const;

    QTouchEvent *asTouchEvent() const;

private:
    int m_pointCount;
    QVector<QQuickEventTouchPoint *> m_touchPoints;
    mutable QMouseEvent m_synthMouseEvent;

    Q_DISABLE_COPY(QQuickPointerTouchEvent)
};

// ### Qt 6: move this to qtbase, replace QTouchDevice and the enums in QTabletEvent
class Q_QUICK_PRIVATE_EXPORT QQuickPointerDevice : public QObject
{
    Q_OBJECT
    Q_PROPERTY(DeviceType type READ type CONSTANT)
    Q_PROPERTY(PointerType pointerType READ pointerType CONSTANT)
    Q_PROPERTY(Capabilities capabilities READ capabilities CONSTANT)
    Q_PROPERTY(int maximumTouchPoints READ maximumTouchPoints CONSTANT)
    Q_PROPERTY(int buttonCount READ buttonCount CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QPointingDeviceUniqueId uniqueId READ uniqueId CONSTANT)

public:
    enum DeviceType {
        UnknownDevice = 0x0000,
        Mouse = 0x0001,
        TouchScreen = 0x0002,
        TouchPad = 0x0004,
        Puck = 0x0008,
        Stylus = 0x0010,
        Airbrush = 0x0020,
        AllDevices = 0x003F
    };
    Q_DECLARE_FLAGS(DeviceTypes, DeviceType)
    Q_ENUM(DeviceType)
    Q_FLAG(DeviceTypes)

    enum PointerType {
        GenericPointer = 0x0001,
        Finger = 0x0002,
        Pen = 0x0004,
        Eraser = 0x0008,
        Cursor = 0x0010,
        AllPointerTypes = 0x001F
    };
    Q_DECLARE_FLAGS(PointerTypes, PointerType)
    Q_ENUM(PointerType)
    Q_FLAG(PointerTypes)

    enum CapabilityFlag {
        Position    = QTouchDevice::Position,
        Area        = QTouchDevice::Area,
        Pressure    = QTouchDevice::Pressure,
        Velocity    = QTouchDevice::Velocity,
        // some bits reserved in case we need more of QTouchDevice::Capabilities
        Scroll      = 0x0100, // mouse has a wheel, or there is OS-level scroll gesture recognition (dubious?)
        Hover       = 0x0200,
        Rotation    = 0x0400,
        XTilt       = 0x0800,
        YTilt       = 0x1000
    };
    Q_DECLARE_FLAGS(Capabilities, CapabilityFlag)
    Q_ENUM(CapabilityFlag)
    Q_FLAG(Capabilities)

    QQuickPointerDevice(DeviceType devType, PointerType pType, Capabilities caps, int maxPoints, int buttonCount, const QString &name, qint64 uniqueId = 0)
      : m_deviceType(devType), m_pointerType(pType), m_capabilities(caps)
      , m_maximumTouchPoints(maxPoints), m_buttonCount(buttonCount), m_name(name)
      , m_uniqueId(QPointingDeviceUniqueId::fromNumericId(uniqueId)), m_event(nullptr)
    {
        if (m_deviceType == Mouse) {
            m_event = new QQuickPointerMouseEvent;
        } else if (m_deviceType == TouchScreen || m_deviceType == TouchPad) {
            m_event = new QQuickPointerTouchEvent;
        } else {
            Q_ASSERT(false);
        }
    }

    ~QQuickPointerDevice() { delete m_event; }
    DeviceType type() const { return m_deviceType; }
    PointerType pointerType() const { return m_pointerType; }
    Capabilities capabilities() const { return m_capabilities; }
    bool hasCapability(CapabilityFlag cap) { return m_capabilities & cap; }
    int maximumTouchPoints() const { return m_maximumTouchPoints; }
    int buttonCount() const { return m_buttonCount; }
    QString name() const { return m_name; }
    QPointingDeviceUniqueId uniqueId() const { return m_uniqueId; }
    QQuickPointerEvent *pointerEvent() const { return m_event; }

    static QQuickPointerDevice *touchDevice(QTouchDevice *d);
    static QList<QQuickPointerDevice *> touchDevices();
    static QQuickPointerDevice *genericMouseDevice();
    static QQuickPointerDevice *tabletDevice(qint64);

private:
    DeviceType m_deviceType;
    PointerType m_pointerType;
    Capabilities m_capabilities;
    int m_maximumTouchPoints;
    int m_buttonCount;
    QString m_name;
    QPointingDeviceUniqueId m_uniqueId;
    // the device-specific event instance which is reused during event delivery
    QQuickPointerEvent *m_event;

    Q_DISABLE_COPY(QQuickPointerDevice)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QQuickPointerDevice::DeviceTypes)
Q_DECLARE_OPERATORS_FOR_FLAGS(QQuickPointerDevice::PointerTypes)
Q_DECLARE_OPERATORS_FOR_FLAGS(QQuickPointerDevice::Capabilities)

Q_QUICK_PRIVATE_EXPORT QDebug operator<<(QDebug, const QQuickPointerDevice *);
Q_QUICK_PRIVATE_EXPORT QDebug operator<<(QDebug, const QQuickPointerEvent *);
Q_QUICK_PRIVATE_EXPORT QDebug operator<<(QDebug, const QQuickEventPoint *);
//Q_QUICK_PRIVATE_EXPORT QDebug operator<<(QDebug, const QQuickEventTouchPoint *); TODO maybe

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickKeyEvent)
QML_DECLARE_TYPE(QQuickMouseEvent)
QML_DECLARE_TYPE(QQuickWheelEvent)
QML_DECLARE_TYPE(QQuickCloseEvent)
QML_DECLARE_TYPE(QQuickPointerDevice)
QML_DECLARE_TYPE(QPointingDeviceUniqueId)
QML_DECLARE_TYPE(QQuickPointerEvent)

#endif // QQUICKEVENTS_P_P_H
