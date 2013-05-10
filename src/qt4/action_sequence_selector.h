// -*-c++-*-

/*!
  \file action_sequence_selector.h
  \brief action sequence data selector widget Header File.
*/

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA

 This code is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this code; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifndef SOCCERWINDOW2_ACTION_SEQUENCE_SELECTOR_H
#define SOCCERWINDOW2_ACTION_SEQUENCE_SELECTOR_H

#include <QFrame>

class MainData;

class QDialog;
class QTextEdit;
class QLineEdit;
class QPushButton;
class QListWidget;
class QShowEvent;


/*!
  \class ActionSequenceSelector
  \brief action sequence data selector widget
*/
class ActionSequenceSelector
    : public QFrame {

    Q_OBJECT

private:
    const MainData & M_main_data;

private:
    QListWidget * M_list_view;

public:

    ActionSequenceSelector( QWidget * parent,
                            const MainData & main_data );
    ~ActionSequenceSelector();

protected:

    void showEvent( QShowEvent * event );

public slots:

    void updateChain();

signals:
    void configured();
};

#endif