/***************************************************************************
 *   Copyright 2005-2008 Last.fm Ltd.                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301, USA.          *
 ***************************************************************************/

#include "FriendsPicker.h"
#include "widgets/UnicornLineEdit.h"
#include "widgets/UnicornWidget.h"
#include "lib/types/User.h"
#include <QDialogButtonBox>
#include <QListWidget>
#include <QVBoxLayout>


FriendsPicker::FriendsPicker( const User& user )
{    
    QVBoxLayout* v = new QVBoxLayout( this );
    v->addWidget( new Unicorn::LineEdit( tr("Search") ) );
    v->addWidget( new QListWidget );
    v->addWidget( ui.buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel ) );
 
    UnicornWidget::paintItBlack( this );    
    
    setWindowTitle( tr("Browse Friends") );
    
    WsReply* r = user.getFriends();
    connect( r, SIGNAL(finished( WsReply* )), SLOT(onGetFriendsReturn( WsReply* )) );
    
    connect( ui.buttons, SIGNAL(accepted()), SLOT(accept()) );
    connect( ui.buttons, SIGNAL(rejected()), SLOT(reject()) );
}


void
FriendsPicker::onGetFriendsReturn( WsReply* r )
{
    foreach (User u, User::getFriends( r ))
    {
        qDebug() << u;
    }
}


QList<User>
FriendsPicker::selection() const
{
    return QList<User>();
}