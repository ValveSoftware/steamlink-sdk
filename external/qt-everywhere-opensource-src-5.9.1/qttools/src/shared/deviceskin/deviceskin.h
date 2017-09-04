/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#ifndef SKIN_H
#define SKIN_H

#include <QtWidgets/QWidget>
#include <QtGui/QPolygon>
#include <QtGui/QRegion>
#include <QtGui/QPixmap>
#include <QtCore/QVector>

QT_BEGIN_NAMESPACE

namespace qvfb_internal {
    class CursorWindow;
}

class QTextStream;

// ------- Button Area
struct DeviceSkinButtonArea {
    QString name;
    int keyCode{0};
    QPolygon area;
    QString text;
    bool activeWhenClosed{false};
    bool toggleArea{false};
    bool toggleActiveArea{false};
};

// -------- Parameters
struct DeviceSkinParameters {
    enum ReadMode { ReadAll, ReadSizeOnly };
    bool read(const QString &skinDirectory,  ReadMode rm,  QString *errorMessage);
    bool read(QTextStream &ts, ReadMode rm, QString *errorMessage);

    QSize screenSize() const { return screenRect.size(); }
    QSize secondaryScreenSize() const;
    bool hasSecondaryScreen() const;

    QString skinImageUpFileName;
    QString skinImageDownFileName;
    QString skinImageClosedFileName;
    QString skinCursorFileName;

    QImage skinImageUp;
    QImage skinImageDown;
    QImage skinImageClosed;
    QImage skinCursor;

    QRect screenRect;
    QRect backScreenRect;
    QRect closedScreenRect;
    int screenDepth;
    QPoint cursorHot;
    QVector<DeviceSkinButtonArea> buttonAreas;
    QList<int> toggleAreaList;

    int joystick;
    QString prefix;
    bool hasMouseHover;
};

// --------- Skin Widget
class DeviceSkin : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceSkin(const DeviceSkinParameters &parameters,  QWidget *p );
    ~DeviceSkin( );

    QWidget *view() const { return m_view; }
    void setView( QWidget *v );

    QWidget *secondaryView() const { return m_secondaryView; }
    void setSecondaryView( QWidget *v );

    void setZoom( double );
    void setTransform( const QMatrix& );

    bool hasCursor() const;

    QString prefix() const  {return m_parameters.prefix;}

signals:
    void popupMenu();
    void skinKeyPressEvent(int code, const QString& text, bool autorep);
    void skinKeyReleaseEvent(int code, const QString& text, bool autorep);

protected slots:
    void skinKeyRepeat();
    void moveParent();

protected:
    virtual void paintEvent( QPaintEvent * );
    virtual void mousePressEvent( QMouseEvent *e );
    virtual void mouseMoveEvent( QMouseEvent *e );
    virtual void mouseReleaseEvent( QMouseEvent * );

private:
    void calcRegions();
    void flip(bool open);
    void updateSecondaryScreen();
    void loadImages();
    void startPress(int);
    void endPress();

    const DeviceSkinParameters m_parameters;
    QVector<QRegion> buttonRegions;
    QPixmap skinImageUp;
    QPixmap skinImageDown;
    QPixmap skinImageClosed;
    QPixmap skinCursor;
    QWidget *parent;
    QWidget  *m_view;
    QWidget *m_secondaryView;
    QPoint parentpos;
    QPoint clickPos;
    bool buttonPressed;
    int buttonIndex;
    QMatrix transform;
    qvfb_internal::CursorWindow *cursorw;

    bool joydown;
    QTimer *t_skinkey;
    QTimer *t_parentmove;
    int onjoyrelease;

    bool flipped_open;
};

QT_END_NAMESPACE

#endif
