/****************************************************************************
**
** Copyright (C) 2013 Research In Motion
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
#ifndef MMRENDERERPLAYERVIDEORENDERERCONTROL_H
#define MMRENDERERPLAYERVIDEORENDERERCONTROL_H

#include <QPointer>
#include <qabstractvideosurface.h>
#include <qvideorenderercontrol.h>

typedef struct mmr_context mmr_context_t;

QT_BEGIN_NAMESPACE

class WindowGrabber;

class MmRendererPlayerVideoRendererControl : public QVideoRendererControl
{
    Q_OBJECT
public:
    explicit MmRendererPlayerVideoRendererControl(QObject *parent = 0);
    ~MmRendererPlayerVideoRendererControl();

    QAbstractVideoSurface *surface() const Q_DECL_OVERRIDE;
    void setSurface(QAbstractVideoSurface *surface) Q_DECL_OVERRIDE;

    // Called by media control
    void attachDisplay(mmr_context_t *context);
    void detachDisplay();
    void pause();
    void resume();

    void customEvent(QEvent *) Q_DECL_OVERRIDE;

private Q_SLOTS:
    void frameGrabbed(const QImage &frame, int);

private:
    QPointer<QAbstractVideoSurface> m_surface;

    WindowGrabber* m_windowGrabber;
    mmr_context_t *m_context;

    int m_videoId;
};

QT_END_NAMESPACE

#endif
