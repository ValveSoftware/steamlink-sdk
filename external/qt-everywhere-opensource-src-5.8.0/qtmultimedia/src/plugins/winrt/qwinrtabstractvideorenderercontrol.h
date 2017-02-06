/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

#ifndef QWINRTABSTRACTVIDEORENDERERCONTROL_H
#define QWINRTABSTRACTVIDEORENDERERCONTROL_H

#include <QtMultimedia/QVideoRendererControl>
#include <QtMultimedia/QVideoSurfaceFormat>

#include <qt_windows.h>

struct ID3D11Device;
struct ID3D11Texture2D;

QT_BEGIN_NAMESPACE

class QWinRTAbstractVideoRendererControlPrivate;
class QWinRTAbstractVideoRendererControl : public QVideoRendererControl
{
    Q_OBJECT
public:
    explicit QWinRTAbstractVideoRendererControl(const QSize &size, QObject *parent = 0);
    ~QWinRTAbstractVideoRendererControl();

    enum BlitMode {
        DirectVideo,
        MediaFoundation
    };

    QAbstractVideoSurface *surface() const Q_DECL_OVERRIDE;
    void setSurface(QAbstractVideoSurface *surface) Q_DECL_OVERRIDE;

    QSize size() const;
    void setSize(const QSize &size);

    void setScanLineDirection(QVideoSurfaceFormat::Direction direction);

    BlitMode blitMode() const;
    void setBlitMode(BlitMode mode);

    virtual bool render(ID3D11Texture2D *texture) = 0;
    virtual bool dequeueFrame(QVideoFrame *frame);

    static ID3D11Device *d3dDevice();

public slots:
    void setActive(bool active);

protected:
    void shutdown();

private slots:
    void syncAndRender();

private:
    void textureToFrame();
    Q_INVOKABLE void present();

    QScopedPointer<QWinRTAbstractVideoRendererControlPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QWinRTAbstractVideoRendererControl)
};

class CriticalSectionLocker
{
public:
    CriticalSectionLocker(CRITICAL_SECTION *section)
        : m_section(section)
    {
        EnterCriticalSection(m_section);
    }
    ~CriticalSectionLocker()
    {
        LeaveCriticalSection(m_section);
    }
private:
    CRITICAL_SECTION *m_section;
};

QT_END_NAMESPACE

#endif // QWINRTABSTRACTVIDEORENDERERCONTROL_H
