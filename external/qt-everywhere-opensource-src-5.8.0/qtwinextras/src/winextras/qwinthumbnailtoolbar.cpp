/****************************************************************************
 **
 ** Copyright (C) 2016 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2016 The Qt Company Ltd.
 ** Contact: https://www.qt.io/licensing/
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#include "qwinthumbnailtoolbar.h"
#include "qwinthumbnailtoolbar_p.h"
#include "qwinthumbnailtoolbutton.h"
#include "qwinthumbnailtoolbutton_p.h"
#include "windowsguidsdefs_p.h"
#include "qwinfunctions.h"

#include <QWindow>
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>

#include "qwinevent.h"
#include "qwinfunctions.h"
#include "qwinfunctions_p.h"
#include "qwineventfilter_p.h"

#ifndef THBN_CLICKED
#  define THBN_CLICKED 0x1800
#endif

#ifndef WM_DWMSENDICONICLIVEPREVIEWBITMAP
#  define WM_DWMSENDICONICLIVEPREVIEWBITMAP 0x0326
#endif

#ifndef WM_DWMSENDICONICTHUMBNAIL
#  define WM_DWMSENDICONICTHUMBNAIL 0x0323
#endif

QT_BEGIN_NAMESPACE

enum { dWM_SIT_DISPLAYFRAME = 1 , dWMWA_FORCE_ICONIC_REPRESENTATION = 7, dWMWA_HAS_ICONIC_BITMAP = 10 };

static const int windowsLimitedThumbbarSize = 7;

/*!
    \class QWinThumbnailToolBar
    \inmodule QtWinExtras
    \since 5.2
    \brief The QWinThumbnailToolBar class allows manipulating the thumbnail toolbar of a window.

    Applications can embed a toolbar in the thumbnail of a window, which is
    shown when hovering over its taskbar icon. A thumbnail toolbar may provide
    quick access to the commands of a window without requiring the user to restore
    or activate the window.

    \image thumbbar.png Media player thumbnail toolbar

    The following example code illustrates how to use the functions in the
    QWinThumbnailToolBar and QWinThumbnailToolButton class to implement a
    thumbnail toolbar:

    \snippet code/thumbbar.cpp thumbbar_cpp

    \sa QWinThumbnailToolButton
 */

/*!
    Constructs a QWinThumbnailToolBar with the specified \a parent.

    If \a parent is an instance of QWindow, it is automatically
    assigned as the thumbnail toolbar's \l window.
 */
QWinThumbnailToolBar::QWinThumbnailToolBar(QObject *parent) :
    QObject(parent), d_ptr(new QWinThumbnailToolBarPrivate)
{
    Q_D(QWinThumbnailToolBar);
    d->q_ptr = this;
    QWinEventFilter::setup();
    setWindow(qobject_cast<QWindow *>(parent));
}

/*!
    Destroys and clears the QWinThumbnailToolBar.
 */
QWinThumbnailToolBar::~QWinThumbnailToolBar()
{
}

/*!
    \property QWinThumbnailToolBar::window
    \brief the window whose thumbnail toolbar is manipulated
 */
void QWinThumbnailToolBar::setWindow(QWindow *window)
{
    Q_D(QWinThumbnailToolBar);
    if (d->window != window) {
        if (d->window) {
            d->window->removeEventFilter(d);
            if (d->window->handle()) {
                d->clearToolbar();
                setIconicPixmapNotificationsEnabled(false);
            }
        }
        d->window = window;
        if (d->window) {
            d->window->installEventFilter(d);
            if (d->window->isVisible()) {
                d->initToolbar();
                d->_q_scheduleUpdate();
            }
        }
    }
}

QWindow *QWinThumbnailToolBar::window() const
{
    Q_D(const QWinThumbnailToolBar);
    return d->window;
}

/*!
    Adds a \a button to the thumbnail toolbar.

    \note The number of buttons is limited to \c 7.
 */
void QWinThumbnailToolBar::addButton(QWinThumbnailToolButton *button)
{
    Q_D(QWinThumbnailToolBar);
    if (d->buttonList.size() >= windowsLimitedThumbbarSize) {
        qWarning() << "Cannot add " << button << " maximum number of buttons ("
                   << windowsLimitedThumbbarSize << ") reached.";
        return;
    }
    if (button && button->d_func()->toolbar != this) {
        if (button->d_func()->toolbar) {
            button->d_func()->toolbar->removeButton(button);
        }
        button->d_func()->toolbar = this;
        connect(button, &QWinThumbnailToolButton::changed,
                d, &QWinThumbnailToolBarPrivate::_q_scheduleUpdate);
        d->buttonList.append(button);
        d->_q_scheduleUpdate();
    }
}

/*!
    Removes the \a button from the thumbnail toolbar.
 */
void QWinThumbnailToolBar::removeButton(QWinThumbnailToolButton *button)
{
    Q_D(QWinThumbnailToolBar);
    if (button && d->buttonList.contains(button)) {
        button->d_func()->toolbar = 0;
        disconnect(button, &QWinThumbnailToolButton::changed,
                   d, &QWinThumbnailToolBarPrivate::_q_scheduleUpdate);

        d->buttonList.removeAll(button);
        d->_q_scheduleUpdate();
    }
}

/*!
    Sets the list of \a buttons in the thumbnail toolbar.

    \note Any existing buttons are replaced.
 */
void QWinThumbnailToolBar::setButtons(const QList<QWinThumbnailToolButton *> &buttons)
{
    Q_D(QWinThumbnailToolBar);
    d->buttonList.clear();
    for (QWinThumbnailToolButton *button : buttons)
        addButton(button);
    d->_q_updateToolbar();
}

/*!
    Returns the list of buttons in the thumbnail toolbar.
 */
QList<QWinThumbnailToolButton *> QWinThumbnailToolBar::buttons() const
{
    Q_D(const QWinThumbnailToolBar);
    return d->buttonList;
}

/*!
    \property QWinThumbnailToolBar::count
    \brief the number of buttons in the thumbnail toolbar

    \note The number of buttons is limited to \c 7.
 */
int QWinThumbnailToolBar::count() const
{
    Q_D(const QWinThumbnailToolBar);
    return d->buttonList.size();
}

void QWinThumbnailToolBarPrivate::updateIconicPixmapsEnabled(bool invalidate)
{
    Q_Q(QWinThumbnailToolBar);
    qtDwmApiDll.init();
    const HWND hwnd = handle();
    if (!hwnd) {
         qWarning() << Q_FUNC_INFO << "invoked with hwnd=0";
         return;
    }
    if (!qtDwmApiDll.dwmInvalidateIconicBitmaps)
        return;
    const bool enabled = iconicThumbnail || iconicLivePreview;
    q->setIconicPixmapNotificationsEnabled(enabled);
    if (enabled && invalidate) {
        const HRESULT hr = qtDwmApiDll.dwmInvalidateIconicBitmaps(hwnd);
        if (FAILED(hr))
            qWarning() << QWinThumbnailToolBarPrivate::msgComFailed("DwmInvalidateIconicBitmaps", hr);
    }
}

/*
    QWinThumbnailToolBarPrivate::IconicPixmapCache caches a HBITMAP of for one of
    the iconic thumbnail or live preview pixmaps. When the messages
    WM_DWMSENDICONICLIVEPREVIEWBITMAP or WM_DWMSENDICONICTHUMBNAIL are received
    (after setting the DWM window attributes accordingly), the bitmap matching the
    maximum size is constructed on demand.
 */

void QWinThumbnailToolBarPrivate::IconicPixmapCache::deleteBitmap()
{
    if (m_bitmap) {
        DeleteObject(m_bitmap);
        m_size = QSize();
        m_bitmap = 0;
    }
}

bool QWinThumbnailToolBarPrivate::IconicPixmapCache::setPixmap(const QPixmap &pixmap)
{
    if (pixmap.cacheKey() == m_pixmap.cacheKey())
        return false;
    deleteBitmap();
    m_pixmap = pixmap;
    return true;
}

HBITMAP QWinThumbnailToolBarPrivate::IconicPixmapCache::bitmap(const QSize &maxSize)
{
    if (m_pixmap.isNull())
        return 0;
    if (m_bitmap && m_size.width() <= maxSize.width() && m_size.height() <= maxSize.height())
        return m_bitmap;
    deleteBitmap();
    QPixmap pixmap = m_pixmap;
    if (pixmap.width() >= maxSize.width() || pixmap.height() >= maxSize.width())
        pixmap = pixmap.scaled(maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (const HBITMAP bitmap = QtWin::toHBITMAP(pixmap, QtWin::HBitmapAlpha)) {
        m_size = pixmap.size();
        m_bitmap = bitmap;
    }
    return m_bitmap;
}

/*!
    \fn  QWinThumbnailToolBar::iconicThumbnailPixmapRequested()

    This signal is emitted when the operating system requests a new iconic thumbnail pixmap,
    typically when the thumbnail is shown.

    \since 5.4
    \sa iconicThumbnailPixmap
*/

/*!
    \fn QWinThumbnailToolBar::iconicLivePreviewPixmapRequested()

    This signal is emitted when the operating system requests a new iconic live preview pixmap,
    typically when the user ALT-tabs to the application.

    \since 5.4
    \sa iconicLivePreviewPixmap
*/

/*!
    \property QWinThumbnailToolBar::iconicPixmapNotificationsEnabled
    \brief whether signals iconicThumbnailPixmapRequested() and iconicLivePreviewPixmapRequested()
     will be emitted

    \since 5.4
    \sa QWinThumbnailToolBar::iconicThumbnailPixmap, QWinThumbnailToolBar::iconicLivePreviewPixmap
 */

bool QWinThumbnailToolBar::iconicPixmapNotificationsEnabled() const
{
    Q_D(const QWinThumbnailToolBar);
    const HWND hwnd = d->handle();
    if (!hwnd)
        return false;
    return QtDwmApiDll::booleanWindowAttribute(hwnd, dWMWA_FORCE_ICONIC_REPRESENTATION);
}

void QWinThumbnailToolBar::setIconicPixmapNotificationsEnabled(bool enabled)
{
    Q_D(const QWinThumbnailToolBar);
    const HWND hwnd = d->handle();
    if (!hwnd) {
        qWarning() << Q_FUNC_INFO << "invoked with hwnd=0";
        return;
    }
    if (iconicPixmapNotificationsEnabled() == enabled)
        return;
    QtDwmApiDll::setBooleanWindowAttribute(hwnd, dWMWA_FORCE_ICONIC_REPRESENTATION, enabled);
    QtDwmApiDll::setBooleanWindowAttribute(hwnd, dWMWA_HAS_ICONIC_BITMAP, enabled);
}

/*!
    \property QWinThumbnailToolBar::iconicThumbnailPixmap
    \brief the pixmap for use as a thumbnail representation

    \since 5.4
    \sa QWinThumbnailToolBar::iconicPixmapNotificationsEnabled
 */

void QWinThumbnailToolBar::setIconicThumbnailPixmap(const QPixmap &pixmap)
{
    Q_D(QWinThumbnailToolBar);
    const bool changed = d->iconicThumbnail.setPixmap(pixmap);
    if (d->hasHandle()) // Potentially 0 when invoked from QML loading, see _q_updateToolbar()
        d->updateIconicPixmapsEnabled(changed && !d->withinIconicThumbnailRequest);
}

QPixmap QWinThumbnailToolBar::iconicThumbnailPixmap() const
{
    Q_D(const QWinThumbnailToolBar);
    return d->iconicThumbnail.pixmap();
}

/*!
    \property QWinThumbnailToolBar::iconicLivePreviewPixmap
    \brief the pixmap for use as a live (peek) preview when tabbing into the application

    \since 5.4
 */

void QWinThumbnailToolBar::setIconicLivePreviewPixmap(const QPixmap &pixmap)
{
    Q_D(QWinThumbnailToolBar);
    const bool changed = d->iconicLivePreview.setPixmap(pixmap);
    if (d->hasHandle()) // Potentially 0 when invoked from QML loading, see _q_updateToolbar()
        d->updateIconicPixmapsEnabled(changed && !d->withinIconicLivePreviewRequest);
}

QPixmap QWinThumbnailToolBar::iconicLivePreviewPixmap() const
{
    Q_D(const QWinThumbnailToolBar);
    return d->iconicLivePreview.pixmap();
}

inline void QWinThumbnailToolBarPrivate::updateIconicThumbnail(const MSG *message)
{
    qtDwmApiDll.init();
    if (!qtDwmApiDll.dwmSetIconicThumbnail || !iconicThumbnail)
        return;
    const QSize maxSize(HIWORD(message->lParam), LOWORD(message->lParam));
    if (const HBITMAP bitmap = iconicThumbnail.bitmap(maxSize)) {
        const HRESULT hr = qtDwmApiDll.dwmSetIconicThumbnail(message->hwnd, bitmap, dWM_SIT_DISPLAYFRAME);
        if (FAILED(hr))
            qWarning() << QWinThumbnailToolBarPrivate::msgComFailed("DwmSetIconicThumbnail", hr);
    }
}

inline void QWinThumbnailToolBarPrivate::updateIconicLivePreview(const MSG *message)
{
    qtDwmApiDll.init();
    if (!qtDwmApiDll.dwmSetIconicLivePreviewBitmap || !iconicLivePreview)
        return;
    RECT rect;
    GetClientRect(message->hwnd, &rect);
    const QSize maxSize(rect.right, rect.bottom);
    POINT offset = {0, 0};
    if (const HBITMAP bitmap = iconicLivePreview.bitmap(maxSize)) {
        const HRESULT hr = qtDwmApiDll.dwmSetIconicLivePreviewBitmap(message->hwnd, bitmap, &offset, dWM_SIT_DISPLAYFRAME);
        if (FAILED(hr))
            qWarning() << QWinThumbnailToolBarPrivate::msgComFailed("DwmSetIconicLivePreviewBitmap", hr);
    }
}

/*!
    Removes all buttons from the thumbnail toolbar.
 */
void QWinThumbnailToolBar::clear()
{
    setButtons(QList<QWinThumbnailToolButton *>());
}

static inline ITaskbarList4 *createTaskbarList()
{
    ITaskbarList4 *result = 0;
    HRESULT hresult = CoCreateInstance(CLSID_TaskbarList, 0, CLSCTX_INPROC_SERVER, qIID_ITaskbarList4, reinterpret_cast<void **>(&result));
    if (FAILED(hresult)) {
        const QString err = QtWin::errorStringFromHresult(hresult);
        qWarning("QWinThumbnailToolBar: qIID_ITaskbarList4 was not created: %#010x, %s.",
                 unsigned(hresult), qPrintable(err));
        return 0;
    }
    hresult = result->HrInit();
    if (FAILED(hresult)) {
        result->Release();
        const QString err = QtWin::errorStringFromHresult(hresult);
        qWarning("QWinThumbnailToolBar: qIID_ITaskbarList4 was not initialized: %#010x, %s.",
                 unsigned(hresult), qPrintable(err));
        return 0;
    }
    return result;
}

QWinThumbnailToolBarPrivate::QWinThumbnailToolBarPrivate() :
    QObject(0), updateScheduled(false), window(0), pTbList(createTaskbarList()), q_ptr(0),
    withinIconicThumbnailRequest(false), withinIconicLivePreviewRequest(false)
{
    buttonList.reserve(windowsLimitedThumbbarSize);
    QCoreApplication::instance()->installNativeEventFilter(this);
}

QWinThumbnailToolBarPrivate::~QWinThumbnailToolBarPrivate()
{
    if (pTbList)
        pTbList->Release();
    QCoreApplication::instance()->removeNativeEventFilter(this);
}

inline bool QWinThumbnailToolBarPrivate::hasHandle() const
{
    return window && window->handle();
}

inline HWND QWinThumbnailToolBarPrivate::handle() const
{
    return hasHandle() ? reinterpret_cast<HWND>(window->winId()) : HWND(0);
}

void QWinThumbnailToolBarPrivate::initToolbar()
{
#if !defined(_MSC_VER) || _MSC_VER >= 1600
    if (!pTbList || !window)
        return;
    THUMBBUTTON buttons[windowsLimitedThumbbarSize];
    initButtons(buttons);
    HRESULT hresult = pTbList->ThumbBarAddButtons(handle(), windowsLimitedThumbbarSize, buttons);
    if (FAILED(hresult))
        qWarning() << msgComFailed("ThumbBarAddButtons", hresult);
#else
    // ITaskbarList3::ThumbBarAddButtons() has a different signature in SDK 6.X
    Q_UNIMPLEMENTED();
#endif
}

void QWinThumbnailToolBarPrivate::clearToolbar()
{
    if (!pTbList || !window)
        return;
    THUMBBUTTON buttons[windowsLimitedThumbbarSize];
    initButtons(buttons);
    HRESULT hresult = pTbList->ThumbBarUpdateButtons(handle(), windowsLimitedThumbbarSize, buttons);
    if (FAILED(hresult))
        qWarning() << msgComFailed("ThumbBarUpdateButtons", hresult);
}

void QWinThumbnailToolBarPrivate::_q_updateToolbar()
{
    updateScheduled = false;
    if (!pTbList || !window)
        return;
    THUMBBUTTON buttons[windowsLimitedThumbbarSize];
    QList<HICON> createdIcons;
    initButtons(buttons);
    const int thumbbarSize = qMin(buttonList.size(), windowsLimitedThumbbarSize);
    // filling from the right fixes some strange bug which makes last button bg look like first btn bg
    for (int i = (windowsLimitedThumbbarSize - thumbbarSize); i < windowsLimitedThumbbarSize; i++) {
        QWinThumbnailToolButton *button = buttonList.at(i - (windowsLimitedThumbbarSize - thumbbarSize));
        buttons[i].dwFlags = static_cast<THUMBBUTTONFLAGS>(makeNativeButtonFlags(button));
        buttons[i].dwMask  = static_cast<THUMBBUTTONMASK>(makeButtonMask(button));
        if (!button->icon().isNull()) {;
            buttons[i].hIcon = QtWin::toHICON(button->icon().pixmap(GetSystemMetrics(SM_CXSMICON)));
            if (!buttons[i].hIcon)
                buttons[i].hIcon = static_cast<HICON>(LoadImage(0, IDI_APPLICATION, IMAGE_ICON, SM_CXSMICON, SM_CYSMICON, LR_SHARED));
            else
                createdIcons << buttons[i].hIcon;
        }
        if (!button->toolTip().isEmpty()) {
            buttons[i].szTip[button->toolTip().left(sizeof(buttons[i].szTip)/sizeof(buttons[i].szTip[0]) - 1).toWCharArray(buttons[i].szTip)] = 0;
        }
    }
    HRESULT hresult = pTbList->ThumbBarUpdateButtons(handle(), windowsLimitedThumbbarSize, buttons);
    if (FAILED(hresult))
        qWarning() << msgComFailed("ThumbBarUpdateButtons", hresult);
    updateIconicPixmapsEnabled(false);
    for (int i = 0; i < windowsLimitedThumbbarSize; i++) {
        if (buttons[i].hIcon) {
            if (createdIcons.contains(buttons[i].hIcon))
                DestroyIcon(buttons[i].hIcon);
            else
                DeleteObject(buttons[i].hIcon);
        }
    }
}

void QWinThumbnailToolBarPrivate::_q_scheduleUpdate()
{
    if (updateScheduled)
        return;
    updateScheduled = true;
    QTimer::singleShot(0, this, &QWinThumbnailToolBarPrivate::_q_updateToolbar);
}

bool QWinThumbnailToolBarPrivate::eventFilter(QObject *object, QEvent *event)
{
    if (object == window && event->type() == QWinEvent::TaskbarButtonCreated) {
        initToolbar();
        _q_scheduleUpdate();
    }
    return QObject::eventFilter(object, event);
}

bool QWinThumbnailToolBarPrivate::nativeEventFilter(const QByteArray &, void *message, long *result)
{
    const MSG *msg = static_cast<const MSG *>(message);
    if (handle() != msg->hwnd)
        return false;
    switch (msg->message) {
    case WM_COMMAND:
        if (HIWORD(msg->wParam) == THBN_CLICKED) {
            const int buttonId = LOWORD(msg->wParam) - (windowsLimitedThumbbarSize - qMin(windowsLimitedThumbbarSize, buttonList.size()));
            buttonList.at(buttonId)->click();
            if (result)
                *result = 0;
            return true;
        }
        break;
    case WM_DWMSENDICONICTHUMBNAIL:
        withinIconicThumbnailRequest = true;
        emit q_func()->iconicThumbnailPixmapRequested();
        withinIconicThumbnailRequest = false;
        updateIconicThumbnail(msg);
        return true;
    case WM_DWMSENDICONICLIVEPREVIEWBITMAP:
        withinIconicLivePreviewRequest = true;
        emit q_func()->iconicLivePreviewPixmapRequested();
        withinIconicLivePreviewRequest = false;
        updateIconicLivePreview(msg);
        return true;
    }
    return false;
}

void QWinThumbnailToolBarPrivate::initButtons(THUMBBUTTON *buttons)
{
    for (int i = 0; i < windowsLimitedThumbbarSize; i++) {
        memset(&buttons[i], 0, sizeof buttons[i]);
        buttons[i].iId = UINT(i);
        buttons[i].dwFlags = THBF_HIDDEN;
        buttons[i].dwMask  = THB_FLAGS;
    }
}

int QWinThumbnailToolBarPrivate::makeNativeButtonFlags(const QWinThumbnailToolButton *button)
{
    int nativeFlags = 0;
    if (button->isEnabled())
        nativeFlags |= THBF_ENABLED;
    else
        nativeFlags |= THBF_DISABLED;
    if (button->dismissOnClick())
        nativeFlags |= THBF_DISMISSONCLICK;
    if (button->isFlat())
        nativeFlags |= THBF_NOBACKGROUND;
    if (!button->isVisible())
        nativeFlags |= THBF_HIDDEN;
    if (!button->isInteractive())
        nativeFlags |= THBF_NONINTERACTIVE;
    return nativeFlags;
}

int QWinThumbnailToolBarPrivate::makeButtonMask(const QWinThumbnailToolButton *button)
{
    int mask = 0;
    mask |= THB_FLAGS;
    if (!button->icon().isNull())
        mask |= THB_ICON;
    if (!button->toolTip().isEmpty())
        mask |= THB_TOOLTIP;
    return mask;
}

QString QWinThumbnailToolBarPrivate::msgComFailed(const char *function, HRESULT hresult)
{
    return QString::fromLatin1("QWinThumbnailToolBar: %1() failed: #%2: %3")
            .arg(QLatin1String(function))
            .arg(unsigned(hresult), 10, 16, QLatin1Char('0'))
            .arg(QtWin::errorStringFromHresult(hresult));
}

QT_END_NAMESPACE

#include "moc_qwinthumbnailtoolbar.cpp"
