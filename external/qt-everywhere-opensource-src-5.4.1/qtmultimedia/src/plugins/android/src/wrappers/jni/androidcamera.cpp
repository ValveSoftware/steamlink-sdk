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

#include "androidcamera.h"
#include "androidsurfacetexture.h"
#include "qandroidmultimediautils.h"

#include <qstringlist.h>
#include <qdebug.h>
#include <qmutex.h>
#include <QtCore/private/qjnihelpers_p.h>
#include <QtCore/qthread.h>

QT_BEGIN_NAMESPACE

static const char QtCameraListenerClassName[] = "org/qtproject/qt5/android/multimedia/QtCameraListener";
static QMutex g_cameraMapMutex;
typedef QMap<int, AndroidCamera *> CameraMap;
Q_GLOBAL_STATIC(CameraMap, g_cameraMap)

static inline bool exceptionCheckAndClear(JNIEnv *env)
{
    if (Q_UNLIKELY(env->ExceptionCheck())) {
#ifdef QT_DEBUG
        env->ExceptionDescribe();
#endif // QT_DEBUG
        env->ExceptionClear();
        return true;
    }

    return false;
}

static QRect areaToRect(jobject areaObj)
{
    QJNIObjectPrivate area(areaObj);
    QJNIObjectPrivate rect = area.getObjectField("rect", "Landroid/graphics/Rect;");

    return QRect(rect.getField<jint>("left"),
                 rect.getField<jint>("top"),
                 rect.callMethod<jint>("width"),
                 rect.callMethod<jint>("height"));
}

static QJNIObjectPrivate rectToArea(const QRect &rect)
{
    QJNIObjectPrivate jrect("android/graphics/Rect",
                     "(IIII)V",
                     rect.left(), rect.top(), rect.right(), rect.bottom());

    QJNIObjectPrivate area("android/hardware/Camera$Area",
                    "(Landroid/graphics/Rect;I)V",
                    jrect.object(), 500);

    return area;
}

// native method for QtCameraLisener.java
static void notifyAutoFocusComplete(JNIEnv* , jobject, int id, jboolean success)
{
    QMutexLocker locker(&g_cameraMapMutex);
    AndroidCamera *obj = g_cameraMap->value(id, 0);
    if (obj)
        Q_EMIT obj->autoFocusComplete(success);
}

static void notifyPictureExposed(JNIEnv* , jobject, int id)
{
    QMutexLocker locker(&g_cameraMapMutex);
    AndroidCamera *obj = g_cameraMap->value(id, 0);
    if (obj)
        Q_EMIT obj->pictureExposed();
}

static void notifyPictureCaptured(JNIEnv *env, jobject, int id, jbyteArray data)
{
    QMutexLocker locker(&g_cameraMapMutex);
    AndroidCamera *obj = g_cameraMap->value(id, 0);
    if (obj) {
        const int arrayLength = env->GetArrayLength(data);
        QByteArray bytes(arrayLength, Qt::Uninitialized);
        env->GetByteArrayRegion(data, 0, arrayLength, (jbyte*)bytes.data());
        Q_EMIT obj->pictureCaptured(bytes);
    }
}

static void notifyFrameFetched(JNIEnv *env, jobject, int id, jbyteArray data)
{
    QMutexLocker locker(&g_cameraMapMutex);
    AndroidCamera *obj = g_cameraMap->value(id, 0);
    if (obj) {
        const int arrayLength = env->GetArrayLength(data);
        QByteArray bytes(arrayLength, Qt::Uninitialized);
        env->GetByteArrayRegion(data, 0, arrayLength, (jbyte*)bytes.data());

        Q_EMIT obj->frameFetched(bytes);
    }
}

class AndroidCameraPrivate : public QObject
{
    Q_OBJECT
public:
    AndroidCameraPrivate();
    ~AndroidCameraPrivate();

    Q_INVOKABLE bool init(int cameraId);

    Q_INVOKABLE void release();
    Q_INVOKABLE bool lock();
    Q_INVOKABLE bool unlock();
    Q_INVOKABLE bool reconnect();

    Q_INVOKABLE AndroidCamera::CameraFacing getFacing();
    Q_INVOKABLE int getNativeOrientation();

    Q_INVOKABLE QSize getPreferredPreviewSizeForVideo();
    Q_INVOKABLE QList<QSize> getSupportedPreviewSizes();

    Q_INVOKABLE AndroidCamera::ImageFormat getPreviewFormat();
    Q_INVOKABLE void setPreviewFormat(AndroidCamera::ImageFormat fmt);

    Q_INVOKABLE QSize previewSize() const { return m_previewSize; }
    Q_INVOKABLE void updatePreviewSize();
    Q_INVOKABLE bool setPreviewTexture(void *surfaceTexture);

    Q_INVOKABLE bool isZoomSupported();
    Q_INVOKABLE int getMaxZoom();
    Q_INVOKABLE QList<int> getZoomRatios();
    Q_INVOKABLE int getZoom();
    Q_INVOKABLE void setZoom(int value);

    Q_INVOKABLE QString getFlashMode();
    Q_INVOKABLE void setFlashMode(const QString &value);

    Q_INVOKABLE QString getFocusMode();
    Q_INVOKABLE void setFocusMode(const QString &value);

    Q_INVOKABLE int getMaxNumFocusAreas();
    Q_INVOKABLE QList<QRect> getFocusAreas();
    Q_INVOKABLE void setFocusAreas(const QList<QRect> &areas);

    Q_INVOKABLE void autoFocus();
    Q_INVOKABLE void cancelAutoFocus();

    Q_INVOKABLE bool isAutoExposureLockSupported();
    Q_INVOKABLE bool getAutoExposureLock();
    Q_INVOKABLE void setAutoExposureLock(bool toggle);

    Q_INVOKABLE bool isAutoWhiteBalanceLockSupported();
    Q_INVOKABLE bool getAutoWhiteBalanceLock();
    Q_INVOKABLE void setAutoWhiteBalanceLock(bool toggle);

    Q_INVOKABLE int getExposureCompensation();
    Q_INVOKABLE void setExposureCompensation(int value);
    Q_INVOKABLE float getExposureCompensationStep();
    Q_INVOKABLE int getMinExposureCompensation();
    Q_INVOKABLE int getMaxExposureCompensation();

    Q_INVOKABLE QString getSceneMode();
    Q_INVOKABLE void setSceneMode(const QString &value);

    Q_INVOKABLE QString getWhiteBalance();
    Q_INVOKABLE void setWhiteBalance(const QString &value);

    Q_INVOKABLE void updateRotation();

    Q_INVOKABLE QList<QSize> getSupportedPictureSizes();
    Q_INVOKABLE void setPictureSize(const QSize &size);
    Q_INVOKABLE void setJpegQuality(int quality);

    Q_INVOKABLE void startPreview();
    Q_INVOKABLE void stopPreview();

    Q_INVOKABLE void takePicture();

    Q_INVOKABLE void fetchEachFrame(bool fetch);
    Q_INVOKABLE void fetchLastPreviewFrame();

    Q_INVOKABLE void applyParameters();

    Q_INVOKABLE QStringList callParametersStringListMethod(const QByteArray &methodName);

    int m_cameraId;
    QMutex m_parametersMutex;
    QSize m_previewSize;
    int m_rotation;
    QJNIObjectPrivate m_info;
    QJNIObjectPrivate m_parameters;
    QJNIObjectPrivate m_camera;
    QJNIObjectPrivate m_cameraListener;

Q_SIGNALS:
    void previewSizeChanged();
    void previewStarted();
    void previewStopped();

    void autoFocusStarted();

    void whiteBalanceChanged();

    void previewFetched(const QByteArray &preview);
};

AndroidCamera::AndroidCamera(AndroidCameraPrivate *d, QThread *worker)
    : QObject(),
      d_ptr(d),
      m_worker(worker)

{
    qRegisterMetaType<QList<int> >();
    qRegisterMetaType<QList<QSize> >();
    qRegisterMetaType<QList<QRect> >();

    connect(d, &AndroidCameraPrivate::previewSizeChanged, this, &AndroidCamera::previewSizeChanged);
    connect(d, &AndroidCameraPrivate::previewStarted, this, &AndroidCamera::previewStarted);
    connect(d, &AndroidCameraPrivate::previewStopped, this, &AndroidCamera::previewStopped);
    connect(d, &AndroidCameraPrivate::autoFocusStarted, this, &AndroidCamera::autoFocusStarted);
    connect(d, &AndroidCameraPrivate::whiteBalanceChanged, this, &AndroidCamera::whiteBalanceChanged);
    connect(d, &AndroidCameraPrivate::previewFetched, this, &AndroidCamera::previewFetched);
}

AndroidCamera::~AndroidCamera()
{
    Q_D(AndroidCamera);
    if (d->m_camera.isValid()) {
        g_cameraMapMutex.lock();
        g_cameraMap->remove(d->m_cameraId);
        g_cameraMapMutex.unlock();
    }

    release();
    m_worker->exit();
    m_worker->wait(5000);
}

AndroidCamera *AndroidCamera::open(int cameraId)
{
    AndroidCameraPrivate *d = new AndroidCameraPrivate();
    QThread *worker = new QThread;
    worker->start();
    d->moveToThread(worker);
    connect(worker, &QThread::finished, d, &AndroidCameraPrivate::deleteLater);
    bool ok = true;
    QMetaObject::invokeMethod(d, "init", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, ok), Q_ARG(int, cameraId));
    if (!ok) {
        worker->quit();
        worker->wait(5000);
        delete d;
        delete worker;
        return 0;
    }

    AndroidCamera *q = new AndroidCamera(d, worker);
    g_cameraMapMutex.lock();
    g_cameraMap->insert(cameraId, q);
    g_cameraMapMutex.unlock();
    return q;
}

int AndroidCamera::cameraId() const
{
    Q_D(const AndroidCamera);
    return d->m_cameraId;
}

bool AndroidCamera::lock()
{
    Q_D(AndroidCamera);
    bool ok = true;
    QMetaObject::invokeMethod(d, "lock", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, ok));
    return ok;
}

bool AndroidCamera::unlock()
{
    Q_D(AndroidCamera);
    bool ok = true;
    QMetaObject::invokeMethod(d, "unlock", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, ok));
    return ok;
}

bool AndroidCamera::reconnect()
{
    Q_D(AndroidCamera);
    bool ok = true;
    QMetaObject::invokeMethod(d, "reconnect", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, ok));
    return ok;
}

void AndroidCamera::release()
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "release", Qt::BlockingQueuedConnection);
}

AndroidCamera::CameraFacing AndroidCamera::getFacing()
{
    Q_D(AndroidCamera);
    return d->getFacing();
}

int AndroidCamera::getNativeOrientation()
{
    Q_D(AndroidCamera);
    return d->getNativeOrientation();
}

QSize AndroidCamera::getPreferredPreviewSizeForVideo()
{
    Q_D(AndroidCamera);
    return d->getPreferredPreviewSizeForVideo();
}

QList<QSize> AndroidCamera::getSupportedPreviewSizes()
{
    Q_D(AndroidCamera);
    return d->getSupportedPreviewSizes();
}

AndroidCamera::ImageFormat AndroidCamera::getPreviewFormat()
{
    Q_D(AndroidCamera);
    return d->getPreviewFormat();
}

void AndroidCamera::setPreviewFormat(ImageFormat fmt)
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "setPreviewFormat", Q_ARG(AndroidCamera::ImageFormat, fmt));
}

QSize AndroidCamera::previewSize() const
{
    Q_D(const AndroidCamera);
    return d->m_previewSize;
}

void AndroidCamera::setPreviewSize(const QSize &size)
{
    Q_D(AndroidCamera);
    d->m_parametersMutex.lock();
    bool areParametersValid = d->m_parameters.isValid();
    d->m_parametersMutex.unlock();
    if (!areParametersValid)
        return;

    d->m_previewSize = size;
    QMetaObject::invokeMethod(d, "updatePreviewSize");
}

bool AndroidCamera::setPreviewTexture(AndroidSurfaceTexture *surfaceTexture)
{
    Q_D(AndroidCamera);
    bool ok = true;
    QMetaObject::invokeMethod(d,
                              "setPreviewTexture",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, ok),
                              Q_ARG(void *, surfaceTexture ? surfaceTexture->surfaceTexture() : 0));
    return ok;
}

bool AndroidCamera::isZoomSupported()
{
    Q_D(AndroidCamera);
    return d->isZoomSupported();
}

int AndroidCamera::getMaxZoom()
{
    Q_D(AndroidCamera);
    return d->getMaxZoom();
}

QList<int> AndroidCamera::getZoomRatios()
{
    Q_D(AndroidCamera);
    return d->getZoomRatios();
}

int AndroidCamera::getZoom()
{
    Q_D(AndroidCamera);
    return d->getZoom();
}

void AndroidCamera::setZoom(int value)
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "setZoom", Q_ARG(int, value));
}

QStringList AndroidCamera::getSupportedFlashModes()
{
    Q_D(AndroidCamera);
    return d->callParametersStringListMethod("getSupportedFlashModes");
}

QString AndroidCamera::getFlashMode()
{
    Q_D(AndroidCamera);
    return d->getFlashMode();
}

void AndroidCamera::setFlashMode(const QString &value)
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "setFlashMode", Q_ARG(QString, value));
}

QStringList AndroidCamera::getSupportedFocusModes()
{
    Q_D(AndroidCamera);
    return d->callParametersStringListMethod("getSupportedFocusModes");
}

QString AndroidCamera::getFocusMode()
{
    Q_D(AndroidCamera);
    return d->getFocusMode();
}

void AndroidCamera::setFocusMode(const QString &value)
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "setFocusMode", Q_ARG(QString, value));
}

int AndroidCamera::getMaxNumFocusAreas()
{
    Q_D(AndroidCamera);
    return d->getMaxNumFocusAreas();
}

QList<QRect> AndroidCamera::getFocusAreas()
{
    Q_D(AndroidCamera);
    return d->getFocusAreas();
}

void AndroidCamera::setFocusAreas(const QList<QRect> &areas)
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "setFocusAreas", Q_ARG(QList<QRect>, areas));
}

void AndroidCamera::autoFocus()
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "autoFocus");
}

void AndroidCamera::cancelAutoFocus()
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "cancelAutoFocus", Qt::QueuedConnection);
}

bool AndroidCamera::isAutoExposureLockSupported()
{
    Q_D(AndroidCamera);
    return d->isAutoExposureLockSupported();
}

bool AndroidCamera::getAutoExposureLock()
{
    Q_D(AndroidCamera);
    return d->getAutoExposureLock();
}

void AndroidCamera::setAutoExposureLock(bool toggle)
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "setAutoExposureLock", Q_ARG(bool, toggle));
}

bool AndroidCamera::isAutoWhiteBalanceLockSupported()
{
    Q_D(AndroidCamera);
    return d->isAutoWhiteBalanceLockSupported();
}

bool AndroidCamera::getAutoWhiteBalanceLock()
{
    Q_D(AndroidCamera);
    return d->getAutoWhiteBalanceLock();
}

void AndroidCamera::setAutoWhiteBalanceLock(bool toggle)
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "setAutoWhiteBalanceLock", Q_ARG(bool, toggle));
}

int AndroidCamera::getExposureCompensation()
{
    Q_D(AndroidCamera);
    return d->getExposureCompensation();
}

void AndroidCamera::setExposureCompensation(int value)
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "setExposureCompensation", Q_ARG(int, value));
}

float AndroidCamera::getExposureCompensationStep()
{
    Q_D(AndroidCamera);
    return d->getExposureCompensationStep();
}

int AndroidCamera::getMinExposureCompensation()
{
    Q_D(AndroidCamera);
    return d->getMinExposureCompensation();
}

int AndroidCamera::getMaxExposureCompensation()
{
    Q_D(AndroidCamera);
    return d->getMaxExposureCompensation();
}

QStringList AndroidCamera::getSupportedSceneModes()
{
    Q_D(AndroidCamera);
    return d->callParametersStringListMethod("getSupportedSceneModes");
}

QString AndroidCamera::getSceneMode()
{
    Q_D(AndroidCamera);
    return d->getSceneMode();
}

void AndroidCamera::setSceneMode(const QString &value)
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "setSceneMode", Q_ARG(QString, value));
}

QStringList AndroidCamera::getSupportedWhiteBalance()
{
    Q_D(AndroidCamera);
    return d->callParametersStringListMethod("getSupportedWhiteBalance");
}

QString AndroidCamera::getWhiteBalance()
{
    Q_D(AndroidCamera);
    return d->getWhiteBalance();
}

void AndroidCamera::setWhiteBalance(const QString &value)
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "setWhiteBalance", Q_ARG(QString, value));
}

void AndroidCamera::setRotation(int rotation)
{
    Q_D(AndroidCamera);
    //We need to do it here and not in worker class because we cache rotation
    d->m_parametersMutex.lock();
    bool areParametersValid = d->m_parameters.isValid();
    d->m_parametersMutex.unlock();
    if (!areParametersValid)
        return;

    d->m_rotation = rotation;
    QMetaObject::invokeMethod(d, "updateRotation");
}

int AndroidCamera::getRotation() const
{
    Q_D(const AndroidCamera);
    return d->m_rotation;
}

QList<QSize> AndroidCamera::getSupportedPictureSizes()
{
    Q_D(AndroidCamera);
    return d->getSupportedPictureSizes();
}

void AndroidCamera::setPictureSize(const QSize &size)
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "setPictureSize", Q_ARG(QSize, size));
}

void AndroidCamera::setJpegQuality(int quality)
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "setJpegQuality", Q_ARG(int, quality));
}

void AndroidCamera::takePicture()
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "takePicture", Qt::BlockingQueuedConnection);
}

void AndroidCamera::fetchEachFrame(bool fetch)
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "fetchEachFrame", Q_ARG(bool, fetch));
}

void AndroidCamera::fetchLastPreviewFrame()
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "fetchLastPreviewFrame");
}

QJNIObjectPrivate AndroidCamera::getCameraObject()
{
    Q_D(AndroidCamera);
    return d->m_camera;
}

int AndroidCamera::getNumberOfCameras()
{
    return QJNIObjectPrivate::callStaticMethod<jint>("android/hardware/Camera",
                                                     "getNumberOfCameras");
}

void AndroidCamera::getCameraInfo(int id, AndroidCameraInfo *info)
{
    Q_ASSERT(info);

    QJNIObjectPrivate cameraInfo("android/hardware/Camera$CameraInfo");
    QJNIObjectPrivate::callStaticMethod<void>("android/hardware/Camera",
                                              "getCameraInfo",
                                              "(ILandroid/hardware/Camera$CameraInfo;)V",
                                              id, cameraInfo.object());

    AndroidCamera::CameraFacing facing = AndroidCamera::CameraFacing(cameraInfo.getField<jint>("facing"));
    // The orientation provided by Android is counter-clockwise, we need it clockwise
    info->orientation = (360 - cameraInfo.getField<jint>("orientation")) % 360;

    switch (facing) {
    case AndroidCamera::CameraFacingBack:
        info->name = QByteArray("back");
        info->description = QStringLiteral("Rear-facing camera");
        info->position = QCamera::BackFace;
        break;
    case AndroidCamera::CameraFacingFront:
        info->name = QByteArray("front");
        info->description = QStringLiteral("Front-facing camera");
        info->position = QCamera::FrontFace;
        break;
    default:
        break;
    }
}

void AndroidCamera::startPreview()
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "startPreview");
}

void AndroidCamera::stopPreview()
{
    Q_D(AndroidCamera);
    QMetaObject::invokeMethod(d, "stopPreview");
}

AndroidCameraPrivate::AndroidCameraPrivate()
    : QObject(),
      m_parametersMutex(QMutex::Recursive)
{
}

AndroidCameraPrivate::~AndroidCameraPrivate()
{
}

bool AndroidCameraPrivate::init(int cameraId)
{
    m_cameraId = cameraId;
    QJNIEnvironmentPrivate env;
    m_camera = QJNIObjectPrivate::callStaticObjectMethod("android/hardware/Camera",
                                                         "open",
                                                         "(I)Landroid/hardware/Camera;",
                                                         cameraId);
    if (exceptionCheckAndClear(env) || !m_camera.isValid())
        return false;

    m_cameraListener = QJNIObjectPrivate(QtCameraListenerClassName, "(I)V", m_cameraId);
    m_info = QJNIObjectPrivate("android/hardware/Camera$CameraInfo");
    m_camera.callStaticMethod<void>("android/hardware/Camera",
                                    "getCameraInfo",
                                    "(ILandroid/hardware/Camera$CameraInfo;)V",
                                    cameraId,
                                    m_info.object());

    QJNIObjectPrivate params = m_camera.callObjectMethod("getParameters",
                                                         "()Landroid/hardware/Camera$Parameters;");
    m_parameters = QJNIObjectPrivate(params);

    return true;
}

void AndroidCameraPrivate::release()
{
    m_previewSize = QSize();
    m_parametersMutex.lock();
    m_parameters = QJNIObjectPrivate();
    m_parametersMutex.unlock();
    if (m_camera.isValid())
        m_camera.callMethod<void>("release");
}

bool AndroidCameraPrivate::lock()
{
    QJNIEnvironmentPrivate env;
    m_camera.callMethod<void>("lock");
    return !exceptionCheckAndClear(env);
}

bool AndroidCameraPrivate::unlock()
{
    QJNIEnvironmentPrivate env;
    m_camera.callMethod<void>("unlock");
    return !exceptionCheckAndClear(env);
}

bool AndroidCameraPrivate::reconnect()
{
    QJNIEnvironmentPrivate env;
    m_camera.callMethod<void>("reconnect");
    return !exceptionCheckAndClear(env);
}

AndroidCamera::CameraFacing AndroidCameraPrivate::getFacing()
{
    return AndroidCamera::CameraFacing(m_info.getField<jint>("facing"));
}

int AndroidCameraPrivate::getNativeOrientation()
{
    return m_info.getField<jint>("orientation");
}

QSize AndroidCameraPrivate::getPreferredPreviewSizeForVideo()
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return QSize();

    QJNIObjectPrivate size = m_parameters.callObjectMethod("getPreferredPreviewSizeForVideo",
                                                           "()Landroid/hardware/Camera$Size;");

    return QSize(size.getField<jint>("width"), size.getField<jint>("height"));
}

QList<QSize> AndroidCameraPrivate::getSupportedPreviewSizes()
{
    QList<QSize> list;

    QMutexLocker parametersLocker(&m_parametersMutex);

    if (m_parameters.isValid()) {
        QJNIObjectPrivate sizeList = m_parameters.callObjectMethod("getSupportedPreviewSizes",
                                                                   "()Ljava/util/List;");
        int count = sizeList.callMethod<jint>("size");
        for (int i = 0; i < count; ++i) {
            QJNIObjectPrivate size = sizeList.callObjectMethod("get",
                                                               "(I)Ljava/lang/Object;",
                                                               i);
            list.append(QSize(size.getField<jint>("width"), size.getField<jint>("height")));
        }

        qSort(list.begin(), list.end(), qt_sizeLessThan);
    }

    return list;
}

AndroidCamera::ImageFormat AndroidCameraPrivate::getPreviewFormat()
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return AndroidCamera::Unknown;

    return AndroidCamera::ImageFormat(m_parameters.callMethod<jint>("getPreviewFormat"));
}

void AndroidCameraPrivate::setPreviewFormat(AndroidCamera::ImageFormat fmt)
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return;

    m_parameters.callMethod<void>("setPreviewFormat", "(I)V", jint(fmt));
    applyParameters();
}

void AndroidCameraPrivate::updatePreviewSize()
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (m_previewSize.isValid()) {
        m_parameters.callMethod<void>("setPreviewSize", "(II)V", m_previewSize.width(), m_previewSize.height());
        applyParameters();
    }

    emit previewSizeChanged();
}

bool AndroidCameraPrivate::setPreviewTexture(void *surfaceTexture)
{
    QJNIEnvironmentPrivate env;
    m_camera.callMethod<void>("setPreviewTexture",
                              "(Landroid/graphics/SurfaceTexture;)V",
                              static_cast<jobject>(surfaceTexture));
    return !exceptionCheckAndClear(env);
}

bool AndroidCameraPrivate::isZoomSupported()
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return false;

    return m_parameters.callMethod<jboolean>("isZoomSupported");
}

int AndroidCameraPrivate::getMaxZoom()
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return 0;

    return m_parameters.callMethod<jint>("getMaxZoom");
}

QList<int> AndroidCameraPrivate::getZoomRatios()
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    QList<int> ratios;

    if (m_parameters.isValid()) {
        QJNIObjectPrivate ratioList = m_parameters.callObjectMethod("getZoomRatios",
                                                                    "()Ljava/util/List;");
        int count = ratioList.callMethod<jint>("size");
        for (int i = 0; i < count; ++i) {
            QJNIObjectPrivate zoomRatio = ratioList.callObjectMethod("get",
                                                                     "(I)Ljava/lang/Object;",
                                                                     i);

            ratios.append(zoomRatio.callMethod<jint>("intValue"));
        }
    }

    return ratios;
}

int AndroidCameraPrivate::getZoom()
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return 0;

    return m_parameters.callMethod<jint>("getZoom");
}

void AndroidCameraPrivate::setZoom(int value)
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return;

    m_parameters.callMethod<void>("setZoom", "(I)V", value);
    applyParameters();
}

QString AndroidCameraPrivate::getFlashMode()
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    QString value;

    if (m_parameters.isValid()) {
        QJNIObjectPrivate flashMode = m_parameters.callObjectMethod("getFlashMode",
                                                                    "()Ljava/lang/String;");
        if (flashMode.isValid())
            value = flashMode.toString();
    }

    return value;
}

void AndroidCameraPrivate::setFlashMode(const QString &value)
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return;

    m_parameters.callMethod<void>("setFlashMode",
                                  "(Ljava/lang/String;)V",
                                  QJNIObjectPrivate::fromString(value).object());
    applyParameters();
}

QString AndroidCameraPrivate::getFocusMode()
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    QString value;

    if (m_parameters.isValid()) {
        QJNIObjectPrivate focusMode = m_parameters.callObjectMethod("getFocusMode",
                                                                    "()Ljava/lang/String;");
        if (focusMode.isValid())
            value = focusMode.toString();
    }

    return value;
}

void AndroidCameraPrivate::setFocusMode(const QString &value)
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return;

    m_parameters.callMethod<void>("setFocusMode",
                                  "(Ljava/lang/String;)V",
                                  QJNIObjectPrivate::fromString(value).object());
    applyParameters();
}

int AndroidCameraPrivate::getMaxNumFocusAreas()
{
    if (QtAndroidPrivate::androidSdkVersion() < 14)
        return 0;

    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return 0;

    return m_parameters.callMethod<jint>("getMaxNumFocusAreas");
}

QList<QRect> AndroidCameraPrivate::getFocusAreas()
{
    QList<QRect> areas;

    if (QtAndroidPrivate::androidSdkVersion() < 14)
        return areas;

    QMutexLocker parametersLocker(&m_parametersMutex);

    if (m_parameters.isValid()) {
        QJNIObjectPrivate list = m_parameters.callObjectMethod("getFocusAreas",
                                                               "()Ljava/util/List;");

        if (list.isValid()) {
            int count = list.callMethod<jint>("size");
            for (int i = 0; i < count; ++i) {
                QJNIObjectPrivate area = list.callObjectMethod("get",
                                                               "(I)Ljava/lang/Object;",
                                                               i);

                areas.append(areaToRect(area.object()));
            }
        }
    }

    return areas;
}

void AndroidCameraPrivate::setFocusAreas(const QList<QRect> &areas)
{
    if (QtAndroidPrivate::androidSdkVersion() < 14)
        return;

    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return;

    QJNIObjectPrivate list;

    if (!areas.isEmpty()) {
        QJNIEnvironmentPrivate env;
        QJNIObjectPrivate arrayList("java/util/ArrayList", "(I)V", areas.size());
        for (int i = 0; i < areas.size(); ++i) {
            arrayList.callMethod<jboolean>("add",
                                           "(Ljava/lang/Object;)Z",
                                           rectToArea(areas.at(i)).object());
            exceptionCheckAndClear(env);
        }
        list = arrayList;
    }

    m_parameters.callMethod<void>("setFocusAreas", "(Ljava/util/List;)V", list.object());

    applyParameters();
}

void AndroidCameraPrivate::autoFocus()
{
    m_camera.callMethod<void>("autoFocus",
                              "(Landroid/hardware/Camera$AutoFocusCallback;)V",
                              m_cameraListener.object());
    emit autoFocusStarted();
}

void AndroidCameraPrivate::cancelAutoFocus()
{
    m_camera.callMethod<void>("cancelAutoFocus");
}

bool AndroidCameraPrivate::isAutoExposureLockSupported()
{
    if (QtAndroidPrivate::androidSdkVersion() < 14)
        return false;

    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return false;

    return m_parameters.callMethod<jboolean>("isAutoExposureLockSupported");
}

bool AndroidCameraPrivate::getAutoExposureLock()
{
    if (QtAndroidPrivate::androidSdkVersion() < 14)
        return false;

    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return false;

    return m_parameters.callMethod<jboolean>("getAutoExposureLock");
}

void AndroidCameraPrivate::setAutoExposureLock(bool toggle)
{
    if (QtAndroidPrivate::androidSdkVersion() < 14)
        return;

    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return;

    m_parameters.callMethod<void>("setAutoExposureLock", "(Z)V", toggle);
    applyParameters();
}

bool AndroidCameraPrivate::isAutoWhiteBalanceLockSupported()
{
    if (QtAndroidPrivate::androidSdkVersion() < 14)
        return false;

    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return false;

    return m_parameters.callMethod<jboolean>("isAutoWhiteBalanceLockSupported");
}

bool AndroidCameraPrivate::getAutoWhiteBalanceLock()
{
    if (QtAndroidPrivate::androidSdkVersion() < 14)
        return false;

    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return false;

    return m_parameters.callMethod<jboolean>("getAutoWhiteBalanceLock");
}

void AndroidCameraPrivate::setAutoWhiteBalanceLock(bool toggle)
{
    if (QtAndroidPrivate::androidSdkVersion() < 14)
        return;

    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return;

    m_parameters.callMethod<void>("setAutoWhiteBalanceLock", "(Z)V", toggle);
    applyParameters();
}

int AndroidCameraPrivate::getExposureCompensation()
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return 0;

    return m_parameters.callMethod<jint>("getExposureCompensation");
}

void AndroidCameraPrivate::setExposureCompensation(int value)
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return;

    m_parameters.callMethod<void>("setExposureCompensation", "(I)V", value);
    applyParameters();
}

float AndroidCameraPrivate::getExposureCompensationStep()
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return 0;

    return m_parameters.callMethod<jfloat>("getExposureCompensationStep");
}

int AndroidCameraPrivate::getMinExposureCompensation()
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return 0;

    return m_parameters.callMethod<jint>("getMinExposureCompensation");
}

int AndroidCameraPrivate::getMaxExposureCompensation()
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return 0;

    return m_parameters.callMethod<jint>("getMaxExposureCompensation");
}

QString AndroidCameraPrivate::getSceneMode()
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    QString value;

    if (m_parameters.isValid()) {
        QJNIObjectPrivate sceneMode = m_parameters.callObjectMethod("getSceneMode",
                                                                    "()Ljava/lang/String;");
        if (sceneMode.isValid())
            value = sceneMode.toString();
    }

    return value;
}

void AndroidCameraPrivate::setSceneMode(const QString &value)
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return;

    m_parameters.callMethod<void>("setSceneMode",
                                  "(Ljava/lang/String;)V",
                                  QJNIObjectPrivate::fromString(value).object());
    applyParameters();
}

QString AndroidCameraPrivate::getWhiteBalance()
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    QString value;

    if (m_parameters.isValid()) {
        QJNIObjectPrivate wb = m_parameters.callObjectMethod("getWhiteBalance",
                                                             "()Ljava/lang/String;");
        if (wb.isValid())
            value = wb.toString();
    }

    return value;
}

void AndroidCameraPrivate::setWhiteBalance(const QString &value)
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return;

    m_parameters.callMethod<void>("setWhiteBalance",
                                  "(Ljava/lang/String;)V",
                                  QJNIObjectPrivate::fromString(value).object());
    applyParameters();

    emit whiteBalanceChanged();
}

void AndroidCameraPrivate::updateRotation()
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    m_parameters.callMethod<void>("setRotation", "(I)V", m_rotation);
    applyParameters();
}

QList<QSize> AndroidCameraPrivate::getSupportedPictureSizes()
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    QList<QSize> list;

    if (m_parameters.isValid()) {
        QJNIObjectPrivate sizeList = m_parameters.callObjectMethod("getSupportedPictureSizes",
                                                                   "()Ljava/util/List;");
        int count = sizeList.callMethod<jint>("size");
        for (int i = 0; i < count; ++i) {
            QJNIObjectPrivate size = sizeList.callObjectMethod("get",
                                                               "(I)Ljava/lang/Object;",
                                                               i);
            list.append(QSize(size.getField<jint>("width"), size.getField<jint>("height")));
        }

        qSort(list.begin(), list.end(), qt_sizeLessThan);
    }

    return list;
}

void AndroidCameraPrivate::setPictureSize(const QSize &size)
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return;

    m_parameters.callMethod<void>("setPictureSize", "(II)V", size.width(), size.height());
    applyParameters();
}

void AndroidCameraPrivate::setJpegQuality(int quality)
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    if (!m_parameters.isValid())
        return;

    m_parameters.callMethod<void>("setJpegQuality", "(I)V", quality);
    applyParameters();
}

void AndroidCameraPrivate::startPreview()
{
    //We need to clear preview buffers queue here, but there is no method to do it
    //Though just resetting preview callback do the trick
    m_camera.callMethod<void>("setPreviewCallbackWithBuffer",
                              "(Landroid/hardware/Camera$PreviewCallback;)V",
                              jobject(0));
    m_cameraListener.callMethod<void>("preparePreviewBuffer", "(Landroid/hardware/Camera;)V", m_camera.object());
    QJNIObjectPrivate buffer = m_cameraListener.callObjectMethod<jbyteArray>("callbackBuffer");
    m_camera.callMethod<void>("addCallbackBuffer", "([B)V", buffer.object());
    m_camera.callMethod<void>("setPreviewCallbackWithBuffer",
                              "(Landroid/hardware/Camera$PreviewCallback;)V",
                              m_cameraListener.object());
    m_camera.callMethod<void>("startPreview");
    emit previewStarted();
}

void AndroidCameraPrivate::stopPreview()
{
    m_camera.callMethod<void>("stopPreview");
    emit previewStopped();
}

void AndroidCameraPrivate::takePicture()
{
    m_camera.callMethod<void>("takePicture", "(Landroid/hardware/Camera$ShutterCallback;"
                                             "Landroid/hardware/Camera$PictureCallback;"
                                             "Landroid/hardware/Camera$PictureCallback;)V",
                                              m_cameraListener.object(),
                                              jobject(0),
                                              m_cameraListener.object());
}

void AndroidCameraPrivate::fetchEachFrame(bool fetch)
{
    m_cameraListener.callMethod<void>("fetchEachFrame", "(Z)V", fetch);
}

void AndroidCameraPrivate::fetchLastPreviewFrame()
{
    QJNIEnvironmentPrivate env;
    QJNIObjectPrivate data = m_cameraListener.callObjectMethod("lockAndFetchPreviewBuffer", "()[B");
    if (!data.isValid()) {
        m_cameraListener.callMethod<void>("unlockPreviewBuffer");
        return;
    }
    const int arrayLength = env->GetArrayLength(static_cast<jbyteArray>(data.object()));
    QByteArray bytes(arrayLength, Qt::Uninitialized);
    env->GetByteArrayRegion(static_cast<jbyteArray>(data.object()),
                            0,
                            arrayLength,
                            reinterpret_cast<jbyte *>(bytes.data()));
    m_cameraListener.callMethod<void>("unlockPreviewBuffer");

    emit previewFetched(bytes);
}

void AndroidCameraPrivate::applyParameters()
{
    QJNIEnvironmentPrivate env;
    m_camera.callMethod<void>("setParameters",
                              "(Landroid/hardware/Camera$Parameters;)V",
                              m_parameters.object());
    exceptionCheckAndClear(env);
}

QStringList AndroidCameraPrivate::callParametersStringListMethod(const QByteArray &methodName)
{
    QMutexLocker parametersLocker(&m_parametersMutex);

    QStringList stringList;

    if (m_parameters.isValid()) {
        QJNIObjectPrivate list = m_parameters.callObjectMethod(methodName.constData(),
                                                               "()Ljava/util/List;");

        if (list.isValid()) {
            int count = list.callMethod<jint>("size");
            for (int i = 0; i < count; ++i) {
                QJNIObjectPrivate string = list.callObjectMethod("get",
                                                                 "(I)Ljava/lang/Object;",
                                                                 i);
                stringList.append(string.toString());
            }
        }
    }

    return stringList;
}

bool AndroidCamera::initJNI(JNIEnv *env)
{
    jclass clazz = QJNIEnvironmentPrivate::findClass(QtCameraListenerClassName,
                                                     env);

    static const JNINativeMethod methods[] = {
        {"notifyAutoFocusComplete", "(IZ)V", (void *)notifyAutoFocusComplete},
        {"notifyPictureExposed", "(I)V", (void *)notifyPictureExposed},
        {"notifyPictureCaptured", "(I[B)V", (void *)notifyPictureCaptured},
        {"notifyFrameFetched", "(I[B)V", (void *)notifyFrameFetched}
    };

    if (clazz && env->RegisterNatives(clazz,
                                      methods,
                                      sizeof(methods) / sizeof(methods[0])) != JNI_OK) {
        return false;
    }

    return true;
}

QT_END_NAMESPACE

#include "androidcamera.moc"
