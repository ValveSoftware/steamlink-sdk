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

#ifndef AVFVIDEOWIDGET_H
#define AVFVIDEOWIDGET_H

#include <QtWidgets/QWidget>

@class AVPlayerLayer;
#if defined(Q_OS_OSX)
@class NSView;
#else
@class UIView;
#endif

QT_BEGIN_NAMESPACE

class AVFVideoWidget : public QWidget
{
public:
    AVFVideoWidget(QWidget *parent);
    virtual ~AVFVideoWidget();

    QSize sizeHint() const;
    Qt::AspectRatioMode aspectRatioMode() const;
    void setAspectRatioMode(Qt::AspectRatioMode mode);
    void setPlayerLayer(AVPlayerLayer *layer);

protected:
    void resizeEvent(QResizeEvent *);
    void paintEvent(QPaintEvent *);

private:
    void updateAspectRatio();
    void updatePlayerLayerBounds(const QSize &size);

    QSize m_nativeSize;
    Qt::AspectRatioMode m_aspectRatioMode;
    AVPlayerLayer *m_playerLayer;
#if defined(Q_OS_OSX)
    NSView *m_nativeView;
#else
    UIView *m_nativeView;
#endif
};

QT_END_NAMESPACE

#endif // AVFVIDEOWIDGET_H
