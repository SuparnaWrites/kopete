/*
    kopeteviewmanager.cpp - View Manager

    Copyright (c) 2003      by Jason Keirstead       <jason@keirstead.org>
    Kopete    (c) 2002-2003 by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/


#include <kapplication.h>
#include <kdebug.h>
#include <klocale.h>
#include <kgenericfactory.h>
#include <qptrlist.h>
#include "kopetenotifyclient.h"

#include "kopeteprefs.h"
#include "kopeteaccount.h"
#include "kopetemessagemanagerfactory.h"
#include "kopetemetacontact.h"
#include "chatview.h"
#include "kopeteemailwindow.h"
#include "kopeteevent.h"
//#include "systemtray.h"

#include "kopeteviewmanager.h"

typedef KGenericFactory<KopeteViewManager> ViewManagerFactory;
K_EXPORT_COMPONENT_FACTORY( kopete_chatwindow, ViewManagerFactory( "kopete_chatwindow" )  )



typedef QMap<Kopete::MessageManager*,KopeteView*> ManagerMap;
typedef QPtrList<KopeteEvent> EventList;

struct KopeteViewManagerPrivate
{
	ManagerMap managerMap;
	EventList eventList;
	KopeteView *activeView;

	bool useQueue;
	bool raiseWindow;
	bool foreignMessage;
};

KopeteViewManager *KopeteViewManager::s_viewManager = 0L;

KopeteViewManager *KopeteViewManager::viewManager()
{
	return s_viewManager;
}

KopeteViewManager::KopeteViewManager (QObject *parent, const char *name, const QStringList &/*args*/ )
	: Kopete::Plugin( ViewManagerFactory::instance(), parent, name )

{
	s_viewManager=this;
	d = new KopeteViewManagerPrivate;
	d->activeView = 0L;
	d->foreignMessage=false;
	connect( KopetePrefs::prefs(), SIGNAL( saved() ), this, SLOT( slotPrefsChanged() ) );
	connect( Kopete::MessageManagerFactory::factory() , SIGNAL ( requestView(KopeteView*& , Kopete::MessageManager * , Kopete::Message::MessageType  ) ) ,
		this, SLOT (slotRequestView(KopeteView*& , Kopete::MessageManager * , Kopete::Message::MessageType  )));
	connect( Kopete::MessageManagerFactory::factory() , SIGNAL( display( Kopete::Message &, Kopete::MessageManager *) ),
		this, SLOT ( messageAppended( Kopete::Message &, Kopete::MessageManager *) ) );

	connect( Kopete::MessageManagerFactory::factory() , SIGNAL ( getActiveView(KopeteView*&  ) ) ,
		this, SLOT (slotGetActiveView(KopeteView*&)));

	connect( Kopete::MessageManagerFactory::factory() , SIGNAL( readMessage() ),
		this, SLOT ( nextEvent() ) );


	slotPrefsChanged();
}

KopeteViewManager::~KopeteViewManager()
{
	kdDebug(14000) << k_funcinfo << endl;

	//delete all open chatwindow.
	ManagerMap::Iterator it;
	for ( it = d->managerMap.begin(); it != d->managerMap.end(); ++it )
		it.data()->closeView( true ); //this does not clean the map, but we don't care

	delete d;
}

void KopeteViewManager::slotPrefsChanged()
{
	d->useQueue = KopetePrefs::prefs()->useQueue();
	d->raiseWindow = KopetePrefs::prefs()->raiseMsgWindow();
}

KopeteView *KopeteViewManager::view( Kopete::MessageManager* manager, bool /*foreignMessage*/, Kopete::Message::MessageType type )
{
	/*if( d->eventMap.contains( manager ) )
	{
		d->eventMap[ manager ]->deleteLater();
		d->eventMap.remove( manager );
	}*/
	kdDebug(14000) << k_funcinfo << endl;
	if( d->managerMap.contains( manager ) && d->managerMap[ manager ] )
	{
		return d->managerMap[ manager ];
	}
	else
	{
		KopeteView *newView;
		QWidget *newViewWidget;

		if( type == Kopete::Message::Undefined )
		{
			int t = KopetePrefs::prefs()->interfacePreference();
			type = static_cast<Kopete::Message::MessageType>( t );    
		}

		if( type == Kopete::Message::Chat )
		{
			newView = new ChatView( manager );
			newViewWidget = newView->mainWidget();

			connect (newViewWidget, SIGNAL( typing(bool) ), manager, SLOT( typing(bool) ) );
			connect (manager, SIGNAL( remoteTyping( const Kopete::Contact *, bool) ), newViewWidget, SLOT( remoteTyping(const Kopete::Contact *, bool) ) );
		}
		else
		{
			newView = new KopeteEmailWindow( manager, d->foreignMessage );
			newViewWidget = newView->mainWidget();
		}
		d->foreignMessage=false;

		d->managerMap.insert( manager, newView );

		connect( newViewWidget, SIGNAL( closing( KopeteView * ) ), this, SLOT( slotViewDestroyed( KopeteView * ) ) );
		connect( newViewWidget, SIGNAL( messageSent(Kopete::Message &) ), manager, SLOT( sendMessage(Kopete::Message &) ) );
		connect( newViewWidget, SIGNAL( activated( KopeteView * ) ), this, SLOT( slotViewActivated( KopeteView * ) ) );
		connect( manager, SIGNAL( messageSuccess() ), newViewWidget, SLOT( messageSentSuccessfully() ));
		connect( manager, SIGNAL( closing(Kopete::MessageManager *) ), this, SLOT(slotMessageManagerDestroyed(Kopete::MessageManager*)) );

		return newView;
	}
}


void KopeteViewManager::messageAppended( Kopete::Message &msg, Kopete::MessageManager *manager)
{
	kdDebug(14000) << k_funcinfo << endl;

	bool outgoingMessage = ( msg.direction() == Kopete::Message::Outbound );

	if( !outgoingMessage || d->managerMap.contains( manager ) )
	{
		d->foreignMessage=!outgoingMessage; //let know for the view we are about to create
		manager->view(true)->appendMessage( msg );
		d->foreignMessage=false; //the view is created, reset the flag

		if ( d->useQueue && !view( manager, outgoingMessage )->isVisible()  )
		{
			if ( !outgoingMessage )
			{
			
				KopeteEvent *event=new KopeteEvent(msg,manager);
				d->eventList.append( event );
				connect(event, SIGNAL(done(KopeteEvent *)), this, SLOT(slotEventDeleted(KopeteEvent *)));
				Kopete::MessageManagerFactory::factory()->postNewEvent(event);
			}
		}
		else
		{
			readMessages( manager, outgoingMessage );
		}

		if ( !outgoingMessage && ( !manager->account()->isAway() || KopetePrefs::prefs()->soundIfAway() ) )
		{
			QString msgFrom = QString::null;
			if( msg.from()->metaContact() )
				msgFrom = msg.from()->metaContact()->displayName();
			else
				msgFrom = msg.from()->contactId();

			QString msgText = msg.plainBody();
			if( msgText.length() > 90 )
				msgText = msgText.left(88) + QString::fromLatin1("...");

			int winId = 0;  //KopeteSystemTray::systemTray() ? KopeteSystemTray::systemTray()->winId() : 0;
			QWidget *w=dynamic_cast<QWidget*>(manager->view(false));
			if(w) winId=w->topLevelWidget()->winId();
			
			
			QString event;
			QString body =i18n( "<qt>Incoming message from %1<br>\"%2\"</qt>" );;

			switch( msg.importance() )
			{
				case Kopete::Message::Low:
					event = QString::fromLatin1( "kopete_contact_lowpriority" );
					break;
				case Kopete::Message::Highlight:
					event = QString::fromLatin1( "kopete_contact_highlight" );
					body = i18n( "<qt>A highlighted message arrived from %1<br>\"%2\"</qt>" );
					break;
				default:
					event = QString::fromLatin1( "kopete_contact_incoming" );
			}
			KNotifyClient::event(winId,  event, body.arg( msgFrom, msgText ), msg.from()->metaContact(),
				i18n("View") , const_cast<Kopete::Contact*>(msg.from()) , SLOT(execute()) );

		}
	}
}

void KopeteViewManager::readMessages( Kopete::MessageManager *manager, bool outgoingMessage )
{
	kdDebug( 14000 ) << k_funcinfo << endl;
	d->foreignMessage=!outgoingMessage; //let know for the view we are about to create
	KopeteView *thisView = manager->view( true );
	d->foreignMessage=false; //the view is created, reset the flag
 	if( ( outgoingMessage && !thisView->isVisible() ) || d->raiseWindow )
		thisView->raise();

	else if( !thisView->isVisible() )
		thisView->makeVisible();

	QPtrListIterator<KopeteEvent> it( d->eventList );
	KopeteEvent* event;
	while ( ( event = it.current() ) != 0 )
	{
		++it;
		if ( event->message().manager() == manager )
		{
			event->apply();
			d->eventList.remove( event );
		}
	}
}

void KopeteViewManager::slotEventDeleted( KopeteEvent *event )
{
	kdDebug(14000) << k_funcinfo << endl;
	Kopete::MessageManager *kmm=event->message().manager();
	if(!kmm)
		return;
	
	if ( event->state() == KopeteEvent::Applied )
	{
		readMessages( kmm, false );
	}
	else if ( event->state() == KopeteEvent::Ignored ) 
	{
		d->eventList.remove( event );
		bool bAnotherWithThisManager = false;
		for( QPtrListIterator<KopeteEvent> it( d->eventList ); it; ++it )
		{
			KopeteEvent *event = it.current();
			if ( event->message().manager() == kmm )
				bAnotherWithThisManager = true;
		}
		if ( !bAnotherWithThisManager && kmm->view( false ) )
			kmm->view()->closeView( true );
	}
}

void KopeteViewManager::nextEvent()
{
	kdDebug( 14000 ) << k_funcinfo << endl;

	if( d->eventList.isEmpty() )
		return;

	KopeteEvent* event = d->eventList.first();

	if ( event )
		event->apply();
}

void KopeteViewManager::slotViewActivated( KopeteView *view )
{
	kdDebug( 14000 ) << k_funcinfo << endl;
	d->activeView = view;

	QPtrListIterator<KopeteEvent> it ( d->eventList );
	KopeteEvent* event;
	while ( ( event = it.current() ) != 0 )
	{
		++it;
		if ( event->message().manager() == view->msgManager() )
			event->deleteLater();
	}
	
}

void KopeteViewManager::slotViewDestroyed( KopeteView *closingView )
{
	kdDebug( 14000 ) << k_funcinfo << endl;

	if( d->managerMap.contains( closingView->msgManager() ) )
	{
		d->managerMap.remove( closingView->msgManager() );
//		closingView->msgManager()->setCanBeDeleted( true );
	}

	if( closingView == d->activeView )
		d->activeView = 0L;
}

void KopeteViewManager::slotMessageManagerDestroyed( Kopete::MessageManager *manager )
{
	kdDebug( 14000 ) << k_funcinfo << endl;

	if( d->managerMap.contains( manager ) )
	{
		d->managerMap[ manager ]->closeView( true );
	}
}

KopeteView* KopeteViewManager::activeView() const
{
	return d->activeView;
}

void KopeteViewManager::slotRequestView(KopeteView*& v, Kopete::MessageManager *kmm , Kopete::Message::MessageType type )
{
	v=view(kmm, false , type);
}

void KopeteViewManager::slotGetActiveView(KopeteView*&v)
{
	v=activeView();
}


#include "kopeteviewmanager.moc"

// vim: set noet ts=4 sts=4 sw=4:

