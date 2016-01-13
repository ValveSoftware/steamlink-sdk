/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the demonstration applications of the Qt Toolkit.
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


#include <qdeclarative.h>

class TileData : public QObject
{
    Q_OBJECT
public:
    TileData() : _hasFlag(false), _hasMine(false), _hint(-1), _flipped(false) {}

    Q_PROPERTY(bool hasFlag READ hasFlag WRITE setHasFlag NOTIFY hasFlagChanged)
    bool hasFlag() const { return _hasFlag; }

    Q_PROPERTY(bool hasMine READ hasMine NOTIFY hasMineChanged)
    bool hasMine() const { return _hasMine; }

    Q_PROPERTY(int hint READ hint NOTIFY hintChanged)
    int hint() const { return _hint; }

    Q_PROPERTY(bool flipped READ flipped NOTIFY flippedChanged())
    bool flipped() const { return _flipped; }

    void setHasFlag(bool flag) {if(flag==_hasFlag) return; _hasFlag = flag; emit hasFlagChanged();}
    void setHasMine(bool mine) {if(mine==_hasMine) return; _hasMine = mine; emit hasMineChanged();}
    void setHint(int hint) { if(hint == _hint) return; _hint = hint; emit hintChanged(); }
    void flip() { if (_flipped) return; _flipped = true; emit flippedChanged(); }
    void unflip() { if(!_flipped) return; _flipped = false; emit flippedChanged(); }

signals:
    void flippedChanged();
    void hasFlagChanged();
    void hintChanged();
    void hasMineChanged();

private:
    bool _hasFlag;
    bool _hasMine;
    int _hint;
    bool _flipped;
};

class MinehuntGame : public QObject
{
    Q_OBJECT
public:
    MinehuntGame();

    Q_PROPERTY(QDeclarativeListProperty<TileData> tiles READ tiles CONSTANT)
    QDeclarativeListProperty<TileData> tiles();

    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    bool isPlaying() {return playing;}

    Q_PROPERTY(bool hasWon READ hasWon NOTIFY hasWonChanged)
    bool hasWon() {return won;}

    Q_PROPERTY(int numMines READ numMines NOTIFY numMinesChanged)
    int numMines() const{return nMines;}

    Q_PROPERTY(int numFlags READ numFlags NOTIFY numFlagsChanged)
    int numFlags() const{return nFlags;}

public slots:
    Q_INVOKABLE bool flip(int row, int col);
    Q_INVOKABLE bool flag(int row, int col);
    void setBoard();
    void reset();

signals:
    void isPlayingChanged();
    void hasWonChanged();
    void numMinesChanged();
    void numFlagsChanged();

private:
    bool onBoard( int r, int c ) const { return r >= 0 && r < numRows && c >= 0 && c < numCols; }
    TileData *tile( int row, int col ) { return onBoard(row, col) ? _tiles[col+numRows*row] : 0; }
    int getHint(int row, int col);
    void setPlaying(bool b){if(b==playing) return; playing=b; emit isPlayingChanged();}

    QList<TileData *> _tiles;
    int numCols;
    int numRows;
    bool playing;
    bool won;
    int remaining;
    int nMines;
    int nFlags;
};
