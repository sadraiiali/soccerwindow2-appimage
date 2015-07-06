// -*-c++-*-

/*!
  \file action_sequence_selector.h
  \brief action sequence data selector widget Source File.
*/

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA, Hidehisa AKIYAMA

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <QtGui>

#include "action_sequence_selector.h"

#include "action_sequence_tree_view.h"

#include "agent_id.h"
#include "main_data.h"
#include "action_sequence_description.h"
#include "action_sequence_log_parser.h"
#include "debug_log_data.h"
#include "options.h"

#include <rcsc/common/logger.h>
#include <rcsc/game_time.h>

#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>

#include <map>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdio>

namespace {

const int ID_COLUMN = ActionSequenceTreeView::ID_COLUMN;
const int VALUE_COLUMN = ActionSequenceTreeView::VALUE_COLUMN;
const int LENGTH_COLUMN = ActionSequenceTreeView::LENGTH_COLUMN;
const int SEQ_COLUMN = ActionSequenceTreeView::SEQ_COLUMN;
const int DESC_COLUMN = ActionSequenceTreeView::DESC_COLUMN;

}

class DescriptionDialog
    : public QDialog {
public:
    DescriptionDialog( QWidget * parent,
                       const ActionSequenceDescription & seq )
        : QDialog( parent )
      {
          this->setWindowTitle( tr( "Evaluation Description ID=%1" ).arg( seq.id() ) );

          QTextEdit * view = new QTextEdit();
          view->setReadOnly( true );
          view->setLineWrapMode( QTextEdit::NoWrap );

          view->append( tr( "Evaluation Description ID=%1 value=%2" )
                        .arg( seq.id() )
                        .arg( seq.value() ) );
          view->append( tr( "-----------------------------------" ) );
          for ( std::vector< std::string >::const_iterator it = seq.evaluationDescription().begin(),
                    end = seq.evaluationDescription().end();
                it != end;
                ++it )
          {
              view->append( QString::fromStdString( *it ) );
          }

          // if ( ! seq.rankingData().empty() )
          // {
          //     view->append( tr( "-----------------------------------" ) );
          //     view->append( QString::fromStdString( seq.rankingData() ) );
          // }

          QVBoxLayout * layout = new QVBoxLayout();
          layout->setContentsMargins( 2, 2, 2, 2 );
          layout->addWidget( view );

          this->setLayout( layout );
          this->setModal( false );
          this->resize( 500, 300 );
      }
    ~DescriptionDialog()
      {
          // std::cerr << "delete DescriptionDialog ID=" << M_id << std::endl;
      }

protected:

    void closeEvent( QCloseEvent * e )
      {
          QDialog::closeEvent( e );
          this->deleteLater();
      }

};


/*-------------------------------------------------------------------*/
/*!

*/
ActionSequenceSelector::ActionSequenceSelector( QWidget * parent,
                                                MainData & main_data )
    : QDialog( parent ),
      M_main_data( main_data ),
      M_modified( false )
{
    this->setWindowTitle( tr( "Action Sequence Selector" ) );

    QVBoxLayout * top_layout = new QVBoxLayout();
    top_layout->setContentsMargins( 2, 2, 2, 2 );
    this->setLayout( top_layout );

    //
    // control panel
    //
    QLayout * ctrl_layout = createControlPanel();
    top_layout->addLayout( ctrl_layout );

    //
    // tree view
    //

    M_tree_view = createTreeView();
    top_layout->addWidget( M_tree_view );

    //
    // bottom buttons
    //
    {
        QHBoxLayout * bottom_layout = new QHBoxLayout();


        // save training data
        QPushButton * save_btn = new QPushButton( tr( "Save Rank" ) );
        save_btn->setAutoDefault( false );
        connect( save_btn, SIGNAL( clicked() ), this, SLOT( saveCurrentRank() ) );
        bottom_layout->addWidget( save_btn );

        // close window
        QPushButton * close_btn = new QPushButton( tr( "Close" ) );
        close_btn->setAutoDefault( false );
        connect( close_btn, SIGNAL( clicked() ), this, SLOT( close() ) );
        bottom_layout->addWidget( close_btn );

        //
        top_layout->addLayout( bottom_layout );
    }

    this->resize( 1000, 600 );
}

/*-------------------------------------------------------------------*/
/*!

*/
ActionSequenceSelector::~ActionSequenceSelector()
{
    // std::cerr << "delete ActionSequenceSelector" << std::endl;
}

/*-------------------------------------------------------------------*/
/*!

*/
QLayout *
ActionSequenceSelector::createControlPanel()
{
    QHBoxLayout * layout = new QHBoxLayout();

    M_info_label = new QLabel();
    layout->addWidget( M_info_label );

    M_hits_label = new QLabel();
    layout->addWidget( M_hits_label );

    layout->addStretch();

    layout->addWidget( new QLabel( tr( "Filter " ) ) );
    //
    M_filter_id = new QLineEdit;
    M_filter_id->setValidator( new QRegExpValidator( QRegExp( "\\d+(\\s\\d+)*" ) ) );
    layout->addWidget( new QLabel( tr( "ID(s):" ) ) );
    layout->addWidget( M_filter_id );

    connect( M_filter_id, SIGNAL( textEdited( const QString & ) ),
             this, SLOT( setFilter( const QString & ) ) );
    //
    M_filter_length = new QLineEdit;
    M_filter_length->setValidator( new QIntValidator( 1, 100 ) );
    layout->addWidget( new QLabel( tr( "Length:" ) ) );
    layout->addWidget( M_filter_length );

    connect( M_filter_length, SIGNAL( textEdited( const QString & ) ),
             this, SLOT( setFilter( const QString & ) ) );
    //
    M_filter_string = new QLineEdit();
    layout->addWidget( new QLabel( tr( "String(s):" ) ) );
    layout->addWidget( M_filter_string );

    connect( M_filter_string, SIGNAL( textEdited( const QString & ) ),
             this, SLOT( setFilter( const QString & ) ) );

    return layout;
}

/*-------------------------------------------------------------------*/
/*!

*/
QTreeWidget *
ActionSequenceSelector::createTreeView()
{
    QTreeWidget * tree_view = new ActionSequenceTreeView();

    connect( tree_view, SIGNAL( itemSelectionChanged() ),
             this, SLOT( slotItemSelectionChanged() ) );
    connect( tree_view, SIGNAL( itemDoubleClicked( QTreeWidgetItem *, int ) ),
             this, SLOT( slotItemDoubleClicked( QTreeWidgetItem *, int ) ) );
    // connect( tree_view, SIGNAL( customContextMenuRequested( const QPoint & ) ),
    //          this, SLOT( slotContextMenuRequested( const QPoint & ) ) );

    return tree_view;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
ActionSequenceSelector::showEvent( QShowEvent * event )
{
    QDialog::showEvent( event );

    updateListView();
    //updateTreeView();
}

/*-------------------------------------------------------------------*/
/*!

*/
void
ActionSequenceSelector::closeEvent( QCloseEvent * event )
{
    QDialog::closeEvent( event );

    if ( M_modified )
    {
        saveCurrentRank();
    }
    clearSelection();

    emit selected( -1 );
    emit windowClosed();
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
ActionSequenceSelector::updateSequenceData()
{
    //std::cerr << "(ActionSequenceSelector::updateDataListView)" << std::endl;

    const AgentID pl = Options::instance().selectedAgent();
    if ( pl.isNull() )
    {
        QMessageBox::critical( this,
                               tr( "Error" ),
                               tr( "player not selected" ),
                               QMessageBox::Ok, QMessageBox::NoButton );
        return false;
    }

    const boost::shared_ptr< const DebugLogData > data = M_main_data.debugLogHolder().getData( pl.unum() );

    if ( ! data )
    {
        std::cerr << __FILE__ << ": (updateSequenceData) "
                  << "no debug log data. unum = "
                  << pl.unum() << std::endl;
        return false;
    }

    std::stringstream buf;

    for ( DebugLogData::TextCont::const_iterator it = data->textCont().begin(),
              end = data->textCont().end();
          it != end;
          ++ it )
    {
        if ( it->level_ & rcsc::Logger::PLAN )
        {
            buf << it->msg_;
        }
    }

    ActionSequenceHolder::Ptr seqs = ActionSequenceLogParser::parse( buf );

    if ( ! seqs )
    {
        std::cerr << __FILE__ << ": (updateSequenceData) "
                  << "ERROR on parsing action sequence log!" << std::endl;
        QMessageBox::critical( this,
                               tr( "Error" ),
                               tr( "Error on parsing action sequence log!" ),
                               QMessageBox::Ok, QMessageBox::NoButton );
        return false;
    }

    seqs->setFirstPlayer( pl );
    M_main_data.setActionSequenceHolder( M_main_data.debugLogHolder().currentTime(), seqs );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActionSequenceSelector::updateListView()
{
    //std::cerr << "(ActionSequenceSelector::updateDataListView)" << std::endl;

    if ( ! updateSequenceData() )
    {
        return;
    }

    const ActionSequenceHolder::ConstPtr & seqs = M_main_data.actionSequenceHolder();
    if ( ! seqs )
    {
        return;
    }

    M_filter_id->clear();
    M_tree_view->clear();

    //
    // print sequence data
    //

    int hits = 0;

    for ( ActionSequenceHolder::Cont::const_iterator it = seqs->data().begin(),
              end = seqs->data().end();
          it != end;
          ++it )
    {
        const ActionSequenceDescription & seq = *(it->second);

        ++hits;

        std::ostringstream buf;
        QString seq_str;

        if ( seq.actions().empty() )
        {
            buf << "\nempty sequence";
        }
        else
        {
            for ( std::vector< ActionDescription >::const_iterator a = seq.actions().begin(),
                      a_end = seq.actions().end();
                  a != a_end;
                  ++a )
            {
                seq_str += QString::number( a->id() );
                seq_str += '\n';
                buf << "\n[" << a->description() << "]";
            }

            seq_str.chop( 1 );
        }

        // if ( ! seq.evaluationDescription().empty() )
        // {
        //     buf << '\n';
        //     for ( std::vector< std::string >::const_iterator e = seq.evaluationDescription().begin(),
        //               e_end = seq.evaluationDescription().end();
        //           e != e_end;
        //           ++e )
        //     {
        //         buf << "                    "
        //             << "(eval) " << *e << '\n';
        //     }
        // }

        buf << '\n';

        QTreeWidgetItem * item = new QTreeWidgetItem();
        item->setData( ID_COLUMN, Qt::DisplayRole, seq.id() );
        item->setData( VALUE_COLUMN, Qt::DisplayRole, seq.value() );
        item->setData( LENGTH_COLUMN, Qt::DisplayRole, static_cast< int >( seq.actions().size() ) );
        item->setText( SEQ_COLUMN, seq_str );
        item->setText( DESC_COLUMN, QString::fromStdString( buf.str() ) );

        //item->setFlags( item->flags() | Qt::ItemIsEditable | Qt::ItemIsDropEnabled );
        item->setFlags( item->flags() | Qt::ItemIsEditable );
        M_tree_view->addTopLevelItem( item );
    }

    const rcsc::GameTime & current = M_main_data.debugLogHolder().currentTime();

    M_info_label->setText( tr( "Unum=%1 Time=[%2, %3]" )
                           .arg( seqs->firstPlayerID().unum() )
                           .arg( current.cycle() )
                           .arg( current.stopped() ) );
    M_hits_label->setText( tr( " %1 hits" ).arg( hits ) );

    M_tree_view->resizeColumnToContents( VALUE_COLUMN );
    M_tree_view->sortItems( VALUE_COLUMN, Qt::DescendingOrder );

    //
    setFilter( QString() );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
ActionSequenceSelector::updateTreeView()
{
    if ( ! updateSequenceData() )
    {
        return;
    }

    const ActionSequenceHolder::ConstPtr & seqs = M_main_data.actionSequenceHolder();
    if ( ! seqs )
    {
        return;
    }

    M_filter_id->clear();
    M_tree_view->clear();

    std::map< int, QTreeWidgetItem * > item_map;


    int hits = 0;

    for ( ActionSequenceHolder::Cont::const_iterator it = seqs->data().begin(),
              end = seqs->data().end();
          it != end;
          ++it )
    {
        const ActionSequenceDescription & seq = *(it->second);
        ++hits;

        const std::vector< ActionDescription >::const_iterator a_end = seq.actions().end();
        std::vector< ActionDescription >::const_iterator parent_a = a_end;
        QTreeWidgetItem * parent = static_cast< QTreeWidgetItem * >( 0 );

        for ( std::vector< ActionDescription >::const_iterator a = seq.actions().begin();
              a != a_end;
              ++a )
        {
            std::map< int, QTreeWidgetItem * >::iterator m = item_map.find( a->id() );
            if ( m == item_map.end() )
            {
                break;
            }
            parent_a = a;
            parent = m->second;
        }

        std::vector< ActionDescription >::const_iterator new_a;
        if ( parent_a == a_end )
        {
            new_a = seq.actions().begin();
        }
        else
        {
            new_a = parent_a + 1;
        }

        for ( ; new_a != a_end; ++new_a )
        {
            QTreeWidgetItem * item = new QTreeWidgetItem();
            item->setData( ID_COLUMN, Qt::DisplayRole, new_a->id() );
            item->setData( VALUE_COLUMN, Qt::DisplayRole, seq.value() );
            item->setData( LENGTH_COLUMN, Qt::DisplayRole,
                           static_cast< int >( std::distance( seq.actions().begin(), new_a ) + 1 ) );

            QString text = tr( "[" );
            text += QString::fromStdString( new_a->description() );
            text += tr( "]" );
            item->setText( DESC_COLUMN, text );

            if ( parent )
            {
                parent->addChild( item );
            }
            else
            {
                M_tree_view->addTopLevelItem( item );
            }

            item_map.insert( std::pair< int, QTreeWidgetItem * >( new_a->id(), item ) );
            parent = item;
        }
    }

    M_tree_view->expandAll();
}

/*-------------------------------------------------------------------*/
/*!

*/
void
ActionSequenceSelector::clearSelection()
{
    if ( ! M_tree_view->selectedItems().isEmpty() )
    {
        M_tree_view->clearSelection();
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
ActionSequenceSelector::slotItemSelectionChanged()
{
    QTreeWidgetItem * item = M_tree_view->currentItem();
    if ( ! item )
    {
        std::cerr << "(ActionSequenceSelector) NULL" << std::endl;
        return;
    }

    bool ok = false;
    int id = item->data( ID_COLUMN, Qt::DisplayRole ).toInt( &ok );
    if ( ok )
    {
        //M_tree_view->scrollToItem( item, QAbstractItemView::PositionAtCenter );
        M_tree_view->scrollToItem( item, QAbstractItemView::EnsureVisible );
        emit selected( id );
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
ActionSequenceSelector::slotItemDoubleClicked( QTreeWidgetItem * item,
                                               int column )
{
    if ( ! item )
    {
        return;
    }

    if ( column == DESC_COLUMN )
    {
        showDescriptionDialog( item );
    }
    else if ( column == VALUE_COLUMN )
    {
        M_tree_view->editItem( item, VALUE_COLUMN );
        M_modified = true;
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
ActionSequenceSelector::showAllItems()
{
    for ( int i = 0; i < M_tree_view->topLevelItemCount(); ++i )
    {
        QTreeWidgetItem * item = M_tree_view->topLevelItem( i );
        if ( item )
        {
            item->setHidden( false );
        }
    }

    M_hits_label->setText( tr( " %1 hits" ).arg( M_tree_view->topLevelItemCount() ) );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
ActionSequenceSelector::showDescriptionDialog( QTreeWidgetItem * item )
{
    if ( ! item )
    {
        return;
    }

    bool ok = false;
    int id = item->data( ID_COLUMN, Qt::DisplayRole ).toInt( &ok );
    if ( ! ok )
    {
        return;
    }

    ActionSequenceHolder::ConstPtr holder =  M_main_data.actionSequenceHolder();

    if ( ! holder )
    {
        return;
    }

    ActionSequenceDescription::ConstPtr ptr = holder->getSequence( id );
    if ( ptr
         && ! ptr->evaluationDescription().empty() )
    {
        DescriptionDialog * dialog = new DescriptionDialog( this, *ptr );
        connect( this, SIGNAL( windowClosed() ), dialog, SLOT( close() ) );
        dialog->show();
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
ActionSequenceSelector::setFilter( const QString & )
{
    QString id_text = M_filter_id->text();
    QString length_text = M_filter_length->text();
    QString string_text = M_filter_string->text();
    if ( id_text.isEmpty()
         && length_text.isEmpty()
         && string_text.isEmpty() )
    {
        showAllItems();
        return;
    }

    QStringList ids = id_text.split( QChar( ' ' ) );
    QStringList strs = string_text.split( QChar( ' ' ) );

    int count = 0;
    for ( int i = 0; i < M_tree_view->topLevelItemCount(); ++i )
    {
        QTreeWidgetItem * item = M_tree_view->topLevelItem( i );
        if ( item )
        {
            if ( ! length_text.isEmpty() )
            {
                if ( item->text( LENGTH_COLUMN ) != length_text )
                {
                    item->setHidden( true );
                    continue;
                }
            }

            bool id_found = true;
            if ( ! id_text.isEmpty() )
            {
                id_found = false;
                QString text = item->text( SEQ_COLUMN );
                Q_FOREACH( QString s, text.split( QChar( '\n' ) ) )
                {
                    if ( ids.contains( s ) )
                    {
                        id_found = true;
                        break;
                    }
                }
            }

            bool str_found = true;
            if ( ! string_text.isEmpty() )
            {
                QString text = item->text( DESC_COLUMN );
                Q_FOREACH( QString s, strs )
                {
                    if ( ! s.isEmpty()
                         && ! text.contains( s, Qt::CaseInsensitive ) )
                    {
                        str_found = false;
                        break;
                    }
                }
            }

            if ( id_found && str_found )
            {
                ++count;
                item->setHidden( false );
            }
            else
            {
                item->setHidden( true );
            }
        }
    }

    M_hits_label->setText( tr( " %1 hits" ).arg( count ) );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
ActionSequenceSelector::saveCurrentRank()
{
    ActionSequenceHolder::ConstPtr holder =  M_main_data.actionSequenceHolder();
    if ( ! holder
         || holder->data().empty() )
    {
        return;
    }

    std::string outfile;
    {
        ActionSequenceHolder::Cont::const_iterator it = holder->data().begin();
        const char * buf = it->second->rankingData().c_str();
        double value;
        char qid[16];
        if ( std::sscanf( buf, " %lf qid:%[^ ] ", &value, qid ) != 2 )
        {
            return;
        }

        outfile = Options::instance().debugLogDir();
        outfile += '/';
        outfile += qid;
        outfile += ".rank";
        //std::cerr << "(saveCurrentRank) outfile=[" << outfile << "]" << std::endl;
    }

    std::ofstream fout( outfile.c_str() );
    if ( ! fout.is_open() )
    {
        std::cerr << "(saveCurrentRank) could not open the output file. ["
                  << outfile << "]" << std::endl;
        return;
    }


    M_tree_view->sortItems( VALUE_COLUMN, Qt::DescendingOrder );

    const int count = M_tree_view->topLevelItemCount();
    for ( int i = 0; i < count; ++i )
    {
        QTreeWidgetItem * item = M_tree_view->topLevelItem( i );
        if ( ! item ) continue;

        bool ok = false;
        int id = item->data( ID_COLUMN, Qt::DisplayRole ).toInt( &ok );
        if ( ! ok ) continue;

        QString value_str = item->data( VALUE_COLUMN, Qt::DisplayRole ).toString();

        ActionSequenceDescription::ConstPtr seq = holder->getSequence( id );
        if ( ! seq ) continue;
        if ( seq->rankingData().empty() ) continue;

        const char * buf = seq->rankingData().c_str();
        while ( *buf == ' ' ) ++buf;
        while ( *buf != ' ' && *buf != '\0' ) ++buf;
        while ( *buf == ' ' ) ++buf;

        fout << value_str.toStdString() << ' ' << buf << '\n';
    }

    M_modified = false;
}
