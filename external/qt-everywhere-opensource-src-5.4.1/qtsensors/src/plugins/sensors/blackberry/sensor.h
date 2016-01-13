/****************************************************************************
**
** Copyright (C) 2012 Research In Motion
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

//
// This file is a copy of the "sensor.h" header for the BlackBerry Playbook OS.
// It is only inclulded here, because it is not available in the Playbook NDK.
//
#if !defined(Q_OS_BLACKBERRY_TABLET)
#error "This file is supposed to be used only for BlackBerry Playbook OS."
#endif

#ifndef SENSOR_H_
#define SENSOR_H_

#include <stddef.h>
#include <stdint.h>

#include <devctl.h>
#include <errno.h>

typedef enum {
    //Raw
    SENSOR_TYPE_ACCELEROMETER = 0,
    SENSOR_TYPE_MAGNETOMETER = 1,
    SENSOR_TYPE_GYROSCOPE = 2,
    SENSOR_TYPE_ALTIMETER = 3,
    SENSOR_TYPE_TEMPERATURE = 4,
    SENSOR_TYPE_PROXIMITY = 5,
    SENSOR_TYPE_LIGHT = 6,
    SENSOR_TYPE_GRAVITY = 7,
    SENSOR_TYPE_LINEAR_ACCEL = 8,
    SENSOR_TYPE_ROTATION_VECTOR = 9,
    SENSOR_TYPE_ORIENTATION = 10,
    SENSOR_TYPE_ACCEL_LEGACY = 11,
    SENSOR_TYPE_ROTATION_MATRIX = 12,
    SENSOR_TYPE_ROTATION_MATRIX_MAG = 13,
    SENSOR_TYPE_AZIMUTH_PITCH_ROLL = 14,
    SENSOR_TYPE_FACE_DETECT = 15,
    SENSOR_TYPE_PRESSURE = 16,
} sensor_type_e;

typedef enum {
    SENSOR_ACCURACY_UNRELIABLE,
    SENSOR_ACCURACY_LOW,
    SENSOR_ACCURACY_MEDIUM,
    SENSOR_ACCURACY_HIGH,
} sensor_accuracy_e;

typedef struct {
    float resolution;
    float range_min;
    float range_max;
    uint32_t delay_min;
    uint32_t delay_max;
    uint32_t delay_default;
    float power;
} sensor_info_t;

typedef struct {
    size_t size;                    // The size of this structure, can be used for version
    sensor_type_e type;             // The sensor type, used to index into appropriate payload
    uint32_t flags;                 // Flags
    sensor_accuracy_e accuracy;     // The accuracy associated with this sample
    uint64_t timestamp;             // Time stamp of data acquisition, value in nano-seconds
    union {
        struct {
            struct {
                /* Accelerometer, Linear Acceleration, Gravity  -> m/s/s (meters/second/second)
                 * Magnetometer                                 -> uT (micro Tesla)
                 * Gyroscope                                    -> r/s (radian's/second)
                 */
                float x, y, z;      // data of sensor for x,y and z axis
            } dsp, raw;             // dsp values are signal processed/calibrated, raw is not
            union {
                struct {
                    float temperature;// temperature of gyro sensor in degrees Celcius
                } gyro;
            };
        } motion;                   // Used by motion sensors like Accel, Mag, Gyro, Linear Accel and Gravity
        float raw_data[18];         // Misc bucket for data payload
        float rotation_matrix[3*3]; // Rotation Matrix
        struct {
            int screen;             // Screen Rotation in degrees - 0, 90, 180 or 270
            char face[64];          // String based representation of device face
        } orientation;
        struct {
            float azimuth;          // 0 to 359 degrees
            float pitch;            // -180 t0 180 degrees
            float roll;             // -90 to 90 degrees
        } apr;
        struct {
            float distance;         // range_min -> range_max, discrete steps of distance or actual value in cm
            float normalized;       // 0.0 -> 1.0 (close -> far), normalized unit-less signal from raw sensor
        } proximity_s;
        struct {
            float pressure;         // Pressure in Pascal's
            float temperature;      // Temperature in degrees Celcius
        } pressure_s;
        struct {
            float altitude;         // Altitude in meters relative to mean sea level
        } altitude_s;
        struct {
            float illuminance;      // Illuminance in lux
        } light_s;
        struct {
            int face_detect;        // 0 -> 1, bool value indicating if an object is close to or touching the screen
            float probability;      // 0 -> 1, probability indicating if an object is close to or touching the screen
        } face_detect_s;
        struct {
            float temperature;      // Temperature in degree Celcius
        } temperature_s;

        // Deprecated
        float proximity;            // see proximity_s.distance
        float pressure;             // see pressure_s.pressure
        float altitude;             // see altitude_s.altitude
        float illuminance;          // see light_s.illuminance
        int face_detect;            // see face_detect_s.face_detect
        float temperature;          // see temperature_s.temperature
        struct {
            float x,y,z;
            struct {
                float x,y,z;
            } raw;
        } axis_s;                   // see motion
    };
} sensor_event_t;


/* ****************************************************************************
 * devctl() common to all sensors
 *
 * Paths:
 * dev/sensor/accel
 * dev/sensor/mag
 * dev/sensor/gyro
 * dev/sensor/alt
 * dev/sensor/temp
 * dev/sensor/prox
 * dev/sensor/light
 * dev/sensor/gravity
 * dev/sensor/linAccel
 * dev/sensor/rotVect
 * dev/sensor/orientation
 * dev/sensor/rotMatrix
 * dev/sensor/apr
 * dev/sensor/faceDetect
 * dev/sensor/pressure
 *
 * Example usage:
 *      fd = open( "/dev/sensor/???", O_RDONLY);  // ??? = sensor file name
 *
 */

typedef struct {  // re-used for all the "enable" type controls
    unsigned int enable;
} sensor_devctl_enable_tx_t;

/* DCMD_SENSOR_RATE
 * Set Sensor Update Period Rate.
 *
 * Example usage:
 *      sensor_devctl_rate_u rateSet;
 *      rateSet.tx.rate = myValue;
 *      result = devctl(fd, DCMD_SENSOR_RATE, &rateSet, sizeof(rateSet), NULL);
 *  where
 *      rateSet.tx.rate = Update Event Period in micro-seconds
 *
 *      rateSet.rx.rate = The period in microseconds the system granted.
 *      result = EOK - success, or see errno return code descriptions
 *             = EINVAL - invalid rate parameter, sensor will use default rate
 */
typedef union {
    struct {
        unsigned int rate;
    } tx, rx;
} sensor_devctl_rate_u;


/* DCMD_SENSOR_ENABLE
 * Enable/Disable Sensor.
 *
 * Example usage:
 *      sensor_devctl_enable_u enSensor;
 *      enSensor.tx.enable = 1;  // 1 = enable, 0 = disable
 *      result = devctl(fd, DCMD_SENSOR_ENABLE, &enSensor, sizeof(enSensor), NULL);
 *  where
 *      result = EOK - success, or see errno return code descriptions
 */
typedef union {
    sensor_devctl_enable_tx_t tx;
} sensor_devctl_enable_u;


/* DCMD_SENSOR_NAME
 * Get Sensor name.
 *
 * Example usage:
 *      sensor_devctl_name_u sensorName;
 *      result = devctl(fd, DCMD_SENSOR_NAME, &sensorName, sizeof(sensorName), NULL);
 *      printf("My name is %s", sensorName.rx.name);
 *  where
 *      result = EOK - success, or see errno return code descriptions
 */
#define SENSOR_MAX_NAME_SIZE    20
typedef union {
    struct {
        char name[SENSOR_MAX_NAME_SIZE];
    } rx;
} sensor_devctl_name_u;


/* DCMD_SENSOR_CALIBRATE
 * Request Sensor Calibrate.
 *
 * Example usage:
 *      sensor_devctl_calibrate_u sensorCal;
 *      sensorCal.tx.enable = 1;  // 1 = start cal, 0 = stop cal
 *      result = devctl(fd, DCMD_SENSOR_CALIBRATE, &sensorCal, sizeof(sensorCal), NULL);
 *  where
 *      result = EOK - success, or see errno return code descriptions
 */
typedef union {
    sensor_devctl_enable_tx_t tx;
} sensor_devctl_calibrate_u;


/* DCMD_SENSOR_QUEUE
 * Enable/Disable Sensor Event Queuing.  Sensor Services by default queues only
 * one event.  If a new event comes in before the client reads the last event, the
 * previous event is overwritten. When queue is enabled, up to X events will be queued
 * by the system.  Client can set their read buffers up to X * sizeof(sensor_data_t)
 * to be able to read all events queued.
 *
 * Example usage:
 *      sensor_devctl_queue_u sensorQue;
 *      sensorQue.tx.enable = 1;  // 1 = enable, 0 = disable
 *      result = devctl(fd, DCMD_SENSOR_QUEUE, &sensorQue, sizeof(sensorQue), NULL);
 *  where
 *      sensorQue.rx.size - number of events that will be queued
 *      result = EOK - success, or see errno return code descriptions
 */
typedef union {
    sensor_devctl_enable_tx_t tx;
    struct {
        unsigned int size;
    } rx;
} sensor_devctl_queue_u;


/* DCMD_SENSOR_INFO
 * Get Sensor Information.
 *
 * Example usage:
 *      sensor_devctl_info_u sensorInfo;
 *      result = devctl(fd, DCMD_SENSOR_INFO, &sensorInfo, sizeof(sensorInfo), NULL);
 *  where
 *      result = EOK - success, or see errno return code descriptions
 *      sensorInfo.rx.info = sensor info, see sensor_info_t
 */
typedef union {
    struct {
        sensor_info_t info;
    } rx;
} sensor_devctl_info_u;


/* DCMD_SENSOR_SKIPDUPEVENT
 * Enable/Disable Sensor Event duplicate event filtering.  When enabled, exactly
 * duplicate events from the sensor will be filtered.  Some sensor hardware supports reduced
 * reporting which will filter events that are the same within a certain threshold.
 *
 * Example usage:
 *      sensor_devctl_skipdupevent_u sensorSkipDup;
 *      sensorSkipDup.tx.enable = 1;  // 1 = enable, 0 = disable
 *      result = devctl(fd, DCMD_SENSOR_SKIPDUPEVENT, &sensorSkipDup, sizeof(sensorSkipDup), NULL);
 *  where
 *      result = EOK - success, or see errno return code descriptions
 */
typedef union {
    sensor_devctl_enable_tx_t tx;
} sensor_devctl_skipdupevent_u;


/* DCMD_SENSOR_BKGRND
 * Request Sensor work when system is in user standby mode.  By default, when the system
 * is put in standby, all sensors are turned off and no events are sent to clients.
 * By enabling background mode, the sensor will stay active when the system is in standby.
 * This will reduce battery life.
 *
 * Example usage:
 *      sensor_devctl_bkgrnd_u sensorBkgrnd;
 *      sensorBkgrnd.tx.enable = 1;  // 1 = enable, 0 = disable
 *      result = devctl(fd, DCMD_SENSOR_BKGRND, &sensorBkgrnd, sizeof(sensorBkgrnd), NULL);
 *  where
 *      result = EOK - success, or see errno return code descriptions
 */
typedef union {
    sensor_devctl_enable_tx_t tx;
} sensor_devctl_bkgrnd_u;

/* DCMD_SENSOR_UNBLOCK
 * UNBLOCK a blocked read
 *
 * Example usage:
 *      sensor_devctl_unblock_u sensorUnblock;
 *      sensorUnblock.tx.option = 0;         // unblock client read with EINTR, zero bytes returned
 *      sensorUnblock.tx.option = reserved;  // for future use
 *      result = devctl(fd, DCMD_SENSOR_UNBLOCK, &sensorUnblock, sizeof(sensorUnblock), NULL);
 *  where
 *      result = EOK - success, or see errno return code descriptions
 */
typedef union {
    struct {
        int option;
    } tx;
} sensor_devctl_unblock_u;


#define DCMD_SENSOR_ENABLE           __DIOT(_DCMD_INPUT,  1, sensor_devctl_enable_u )
#define DCMD_SENSOR_RATE             __DIOTF(_DCMD_INPUT, 2, sensor_devctl_rate_u )
#define DCMD_SENSOR_INFO             __DIOF(_DCMD_INPUT,  3, sensor_devctl_info_u )
#define DCMD_SENSOR_SKIPDUPEVENT     __DIOT(_DCMD_INPUT,  4, sensor_devctl_skipdupevent_u )
#define DCMD_SENSOR_BKGRND           __DIOT(_DCMD_INPUT,  5, sensor_devctl_bkgrnd_u )
#define DCMD_SENSOR_QUEUE            __DIOTF(_DCMD_INPUT, 6, sensor_devctl_queue_u )
#define DCMD_SENSOR_CALIBRATE        __DIOT(_DCMD_INPUT,  7, sensor_devctl_calibrate_u )
#define DCMD_SENSOR_NAME             __DIOF(_DCMD_INPUT,  9, sensor_devctl_name_u )
#define DCMD_SENSOR_UNBLOCK          __DIOT(_DCMD_INPUT, 10, sensor_devctl_unblock_u )

#endif
