/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include <QScrollArea>

QT_BEGIN_NAMESPACE
class QScroller;
class QGridLayout;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QPlainTextEdit;
QT_END_NAMESPACE

class MetricItemUpdater;
class SnapOverlay;

class SettingsWidget : public QScrollArea
{
    Q_OBJECT

public:
    SettingsWidget(bool smallscreen = false);

    void setScroller(QWidget *widget);

protected:
    bool eventFilter(QObject *, QEvent *);

private slots:
    void scrollTo();
    void snapModeChanged(int);
    void snapPositionsChanged();

private:
    enum SnapMode {
        NoSnap,
        SnapToInterval,
        SnapToList
    };

    void addToGrid(QGridLayout *grid, QWidget *label, int widgetCount, ...);
    QList<qreal> toPositionList(QPlainTextEdit *list, int vmin, int vmax);
    void updateScrollRanges();

    QWidget *m_widget;
    QSpinBox *m_scrollx, *m_scrolly, *m_scrolltime;
    QList<MetricItemUpdater *> m_metrics;

    SnapMode m_snapxmode;
    QComboBox *m_snapx;
    QWidget *m_snapxinterval;
    QPlainTextEdit *m_snapxlist;
    QSpinBox *m_snapxfirst;
    QSpinBox *m_snapxstep;

    SnapMode m_snapymode;
    QComboBox *m_snapy;
    QWidget *m_snapyinterval;
    QPlainTextEdit *m_snapylist;
    QSpinBox *m_snapyfirst;
    QSpinBox *m_snapystep;
    SnapOverlay *m_snapoverlay;

    bool m_smallscreen;
};

#endif
