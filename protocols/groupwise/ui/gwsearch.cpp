/*
    Kopete Groupwise Protocol
    gwsearch.cpp - logic for server side search widget

    Copyright (c) 2006      Novell, Inc	 	 	 http://www.opensuse.org
    Copyright (c) 2004      SUSE Linux AG	 	 http://www.suse.com
    
    Kopete (c) 2002-2004 by the Kopete developers <kopete-devel@kde.org>
 
    *************************************************************************
    *                                                                       *
    * This library is free software; you can redistribute it and/or         *
    * modify it under the terms of the GNU General Public                   *
    * License as published by the Free Software Foundation; either          *
    * version 2 of the License, or (at your option) any later version.      *
    *                                                                       *
    *************************************************************************
*/

#include <qcombobox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <q3listview.h>
#include <qpushbutton.h>
//Added by qt3to4:
#include <QPixmap>
#include <Q3ValueList>
//#include <qvaluelist.h>

#include <kdebug.h>
#include <klocale.h>

#include <kopetemetacontact.h>

#include "client.h"
#include "gwaccount.h"
#include "gwcontact.h"
#include "gwcontactproperties.h"
#include "gwprotocol.h"
#include "tasks/searchusertask.h"

#include "gwsearch.h"

class GWSearchResultsLVI : public Q3ListViewItem
{
public:
	GWSearchResultsLVI( Q3ListView * parent, GroupWise::ContactDetails details, int status, const QPixmap & statusPM/*, const QString & userId */)
	: Q3ListViewItem( parent, QString(), details.givenName, details.surname, GroupWiseProtocol::protocol()->dnToDotted( details.dn ) ), m_details( details ), m_status( status )
	{
		setPixmap( 0, statusPM );
	}
	QString key( int column, bool ascending ) const
	{
		if ( column == 0 )
			return QString::number( 99 - m_status );
		else
			return Q3ListViewItem::key( column, ascending );
	}
	GroupWise::ContactDetails m_details;
	int m_status;
};

GroupWiseContactSearch::GroupWiseContactSearch( GroupWiseAccount * account, Q3ListView::SelectionMode mode, bool onlineOnly,  QWidget *parent )
 : QWidget( parent ), m_account( account ), m_onlineOnly( onlineOnly )
{
	m_results->setSelectionMode( mode );
	m_results->setAllColumnsShowFocus( true );
	connect( m_details, SIGNAL( clicked() ), SLOT( slotShowDetails() ) );
	connect( m_results, SIGNAL( selectionChanged() ), SLOT( slotValidateSelection() ) );
	connect( m_search, SIGNAL( clicked() ), SLOT( slotDoSearch() ) );
	connect( m_clear, SIGNAL( clicked() ), SLOT( slotClear() ) );
}


GroupWiseContactSearch::~GroupWiseContactSearch()
{
}

void GroupWiseContactSearch::slotClear()
{
	m_firstName->clear();
	m_lastName->clear();
	m_userId->clear();
	m_title->clear();
	m_dept->clear();
}

void GroupWiseContactSearch::slotDoSearch()
{
	// build a query
	Q3ValueList< GroupWise::UserSearchQueryTerm > searchTerms;
	if ( !m_firstName->text().isEmpty() )
	{
		GroupWise::UserSearchQueryTerm arg;
		arg.argument = m_firstName->text();
		arg.field = "Given Name";
		arg.operation = searchOperation( m_firstNameOperation->currentIndex() );
		searchTerms.append( arg );
	}
	if ( !m_lastName->text().isEmpty() )
	{
		GroupWise::UserSearchQueryTerm arg;
		arg.argument = m_lastName->text();
		arg.field = "Surname";
		arg.operation = searchOperation( m_lastNameOperation->currentIndex() );
		searchTerms.append( arg );
	}
	if ( !m_userId->text().isEmpty() )
	{
		GroupWise::UserSearchQueryTerm arg;
		arg.argument = m_userId->text();
		arg.field = NM_A_SZ_USERID;
		arg.operation = searchOperation( m_userIdOperation->currentIndex() );
		searchTerms.append( arg );
	}
	if ( !m_title->text().isEmpty() )
	{
		GroupWise::UserSearchQueryTerm arg;
		arg.argument = m_title->text();
		arg.field = NM_A_SZ_TITLE;
		arg.operation = searchOperation( m_titleOperation->currentIndex() );
		searchTerms.append( arg );
	}
	if ( !m_dept->text().isEmpty() )
	{
		GroupWise::UserSearchQueryTerm arg;
		arg.argument = m_dept->text();
		arg.field = NM_A_SZ_DEPARTMENT;
		arg.operation = searchOperation( m_deptOperation->currentIndex() );
		searchTerms.append( arg );
	}
	if ( !searchTerms.isEmpty() )
	{
		// start a search task
		SearchUserTask * st = new SearchUserTask( m_account->client()->rootTask() );
		st->search( searchTerms );
		connect( st, SIGNAL( finished() ), SLOT( slotGotSearchResults() ) );
		st->go( true );
		m_matchCount->setText( i18n( "Searching" ) );
	}
	else
	{
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << "no query to perform!";
	}
	
}

void GroupWiseContactSearch::slotShowDetails()
{
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo;
	// get the first selected result
	Q3ValueList< ContactDetails > selected = selectedResults();
	if ( selected.count() )
	{
		// if they are already in our contact list, show that version
		ContactDetails dt = selected.first();
		GroupWiseContact * c = m_account->contactForDN( dt.dn );
		GroupWiseContactProperties * p;
		if ( c )
			p = new GroupWiseContactProperties( c, this );
		else
			p = new GroupWiseContactProperties( dt, this );
		p->setObjectName( "gwcontactproperties" );
	}
}

void GroupWiseContactSearch::slotGotSearchResults()
{
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo;
	SearchUserTask * st = ( SearchUserTask * ) sender();
	m_searchResults = st->results();
	
	m_matchCount->setText( i18np( "1 matching user found", "%1 matching users found", m_searchResults.count() ) );

	m_results->clear();
	Q3ValueList< GroupWise::ContactDetails >::Iterator it = m_searchResults.begin();
	Q3ValueList< GroupWise::ContactDetails >::Iterator end = m_searchResults.end();
	for ( ; it != end; ++it )
	{
		// it's necessary to change the status used for the LVIs,
		// because the status returned by the server does not go linearly from Unknown to Available
		// which is no use for us to sort on, and converting it to a Kopete::OnlineStatus is overkill here
		int statusOrdered;
		switch ( (*it).status )
		{
			case 0: //unknown
				statusOrdered = 0;
				break;
			case 1: //offline
				statusOrdered = 1;
				break;
			case 2: //online
				statusOrdered = 5;
				break;
			case 3: //busy
				statusOrdered = 2;
				break;
			case 4: // away
				statusOrdered = 3;
				break;
			case 5: //idle
				statusOrdered = 4;
				break;
			default:
				statusOrdered = 0;
				break;
		}

		new GWSearchResultsLVI( m_results, *it, statusOrdered,
				m_account->protocol()->gwStatusToKOS( (*it).status ).iconFor( m_account ) );
	}
	// if there was only one hit, select it
	if ( m_results->childCount() == 1 )
		m_results->firstChild()->setSelected( true );
	
	slotValidateSelection();
}

Q3ValueList< GroupWise::ContactDetails > GroupWiseContactSearch::selectedResults()
{
    Q3ValueList< GroupWise::ContactDetails > lst;
    Q3ListViewItemIterator it( m_results );
    while ( it.current() ) {
        if ( it.current()->isSelected() )
            lst.append( static_cast< GWSearchResultsLVI * >( it.current() )->m_details );
        ++it;
    }
	return lst;
}
// 	GWSearchResultsLVI * selection = static_cast< GWSearchResultsLVI * >( m_results->selectedItem() );
// 	contactId = selection->m_dn;
// 	if ( displayName.isEmpty() )
// 		displayName = selection->text( 1 ) + ' ' + selection->text( 3 );


unsigned char GroupWiseContactSearch::searchOperation( int comboIndex )
{
	switch ( comboIndex )
	{
		case 0:
			return NMFIELD_METHOD_SEARCH;
		case 1:
			return NMFIELD_METHOD_MATCHBEGIN;
		case 2:
			return NMFIELD_METHOD_EQUAL;
	}
	return NMFIELD_METHOD_IGNORE;
}

void GroupWiseContactSearch::slotValidateSelection()
{
	bool ok = false;
	// if we only allow online contacts to be selected
	if ( m_onlineOnly )
	{
		// check that one of the selected items is online
		Q3ListViewItemIterator it( m_results );
		while ( it.current() )
		{
			if ( it.current()->isSelected() && 
					!( static_cast< GWSearchResultsLVI * >( it.current() )->m_status == 1 ) )
			{
				ok = true;
				break;
			}
			++it;
		}
	}
	else
	{
		// check that at least one item is selected
		Q3ListViewItemIterator it( m_results );
		while ( it.current() )
		{
			if ( it.current()->isSelected() )
			{
				ok = true;
				break;
			}
			++it;
		}
    }

	emit selectionValidates( ok );
}

#include "gwsearch.moc"