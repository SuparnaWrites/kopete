/*
    kopeteviewmanager.cpp - View Manager

    Copyright (c) 2003      by Jason Keirstead       <jason@keirstead.org>
    Kopete    (c) 2002-2005 by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include <QList>
#include <QTextDocument>
#include <QtAlgorithms>

#include <kconfig.h>
#include <kdebug.h>
#include <klocale.h>
#include <kplugininfo.h>
#include <knotification.h>
#include <kglobal.h>
#include <kwindowsystem.h>

#include "kopetebehaviorsettings.h"
#include "kopeteaccount.h"
#include "kopetepluginmanager.h"
#include "kopeteviewplugin.h"
#include "kopetechatsessionmanager.h"
#include "kopetemetacontact.h"
#include "kopetecontact.h"
#include "kopetemessageevent.h"
#include "kopeteview.h"
#include "kopetechatsession.h"
#include "kopetegroup.h"
#include "kopetepicture.h"

#include "kopeteviewmanager.h"

/**
 * Used to exrtract the message that will be shown in the notification popup.
 * 
 * Was in KopeteSystemTray::squashMessage in KDE3
 */

static QString squashMessage( const Kopete::Message& msg )
{
	QString msgText = msg.parsedBody();

	QRegExp rx( "(<a.*>((http://)?(.+))</a>)" );
	rx.setMinimal( true );
	if ( rx.indexIn( msgText ) == -1 )
	{
		// no URLs in text, just pick the first 30 chars of
		// the parsed text if necessary. We used parsed text
		// so that things like "<knuff>" show correctly
		//  Escape it after snipping it to not snip entities
		msgText =msg.plainBody() ;
		if( msgText.length() > 30 )
			msgText = msgText.left( 30 ) + QString::fromLatin1( " ..." );
		msgText=Kopete::Message::escape(msgText);
	}
	else
	{
		QString plainText = msg.plainBody();
		if ( plainText.length() > 30 )
		{
			QString fullUrl = rx.cap( 2 );
			QString shorterUrl;
			if ( fullUrl.length() > 30 )
			{
				QString urlWithoutProtocol = rx.cap( 4 );
				shorterUrl = urlWithoutProtocol.left( 27 )
						+ QString::fromLatin1( "... " );
			}
			else
			{
				shorterUrl = fullUrl.left( 27 )
						+ QString::fromLatin1( "... " );
			}
			// remove message text
			msgText = QString::fromLatin1( "... " ) +
					rx.cap( 1 ) +
					QString::fromLatin1( " ..." );
			// find last occurrence of URL (the one inside the <a> tag)
			int revUrlOffset = msgText.lastIndexOf( fullUrl );
			msgText.replace( revUrlOffset,
						fullUrl.length(), shorterUrl );
		}
	}
 	kDebug(14000) << k_funcinfo << msgText;	
	return msgText;
}



typedef QMap<Kopete::ChatSession*,KopeteView*> ManagerMap;
typedef QList<Kopete::MessageEvent*> EventList;

struct KopeteViewManagerPrivate
{
    ~KopeteViewManagerPrivate()
    {
        qDeleteAll(managerMap);
        managerMap.clear();
        qDeleteAll(eventList);
        eventList.clear();
    }

    ManagerMap managerMap;
    EventList eventList;
    KopeteView *activeView;

    bool useQueueOrStack;
    bool raiseWindow;
    bool queueUnreadMessages;
    bool queueOnlyHighlightedMessagesInGroupChats;
    bool queueOnlyMessagesOnAnotherDesktop;
    bool balloonNotifyIgnoreClosesChatView;
    bool foreignMessage;
};

KopeteViewManager *KopeteViewManager::s_viewManager = 0L;

KopeteViewManager *KopeteViewManager::viewManager()
{
    if( !s_viewManager )
            s_viewManager = new KopeteViewManager();
    return s_viewManager;
}

KopeteViewManager::KopeteViewManager()
{
    s_viewManager=this;
    d = new KopeteViewManagerPrivate;
    d->activeView = 0L;
    d->foreignMessage=false;

    connect( Kopete::BehaviorSettings::self(), SIGNAL( configChanged() ), this, SLOT( slotPrefsChanged() ) );

    connect( Kopete::ChatSessionManager::self() , SIGNAL( display( Kopete::Message &, Kopete::ChatSession *) ),
            this, SLOT ( messageAppended( Kopete::Message &, Kopete::ChatSession *) ) );

    connect( Kopete::ChatSessionManager::self() , SIGNAL( readMessage() ),
            this, SLOT ( nextEvent() ) );

    slotPrefsChanged();
}

KopeteViewManager::~KopeteViewManager()
{
// 	kDebug(14000) << k_funcinfo;

    //delete all open chatwindow.
    ManagerMap::Iterator it;
    for ( it = d->managerMap.begin(); it != d->managerMap.end(); ++it )
    {
        it.value()->closeView( true ); //this does not clean the map, but we don't care
    }

    delete d;
}

void KopeteViewManager::slotPrefsChanged()
{
	d->useQueueOrStack = Kopete::BehaviorSettings::self()->useMessageQueue() || Kopete::BehaviorSettings::self()->useMessageStack();
    d->raiseWindow = Kopete::BehaviorSettings::self()->raiseMessageWindow();
    d->queueUnreadMessages = Kopete::BehaviorSettings::self()->queueUnreadMessages();
    d->queueOnlyHighlightedMessagesInGroupChats = Kopete::BehaviorSettings::self()->queueOnlyHighlightedMessagesInGroupChats();
    d->queueOnlyMessagesOnAnotherDesktop = Kopete::BehaviorSettings::self()->queueOnlyMessagesOnAnotherDesktop();
    d->balloonNotifyIgnoreClosesChatView = Kopete::BehaviorSettings::self()->balloonNotifyIgnoreClosesChatView();
}

KopeteView *KopeteViewManager::view( Kopete::ChatSession* session, const QString &requestedPlugin )
{
    // kDebug(14000) << k_funcinfo;

    if( d->managerMap.contains( session ) && d->managerMap[ session ] )
    {
        return d->managerMap[ session ];
    }
    else
    {
        Kopete::PluginManager *pluginManager = Kopete::PluginManager::self();
        Kopete::ViewPlugin *viewPlugin = 0L;

        QString pluginName = requestedPlugin.isEmpty() ? Kopete::BehaviorSettings::self()->viewPlugin() : requestedPlugin;
        if( !pluginName.isEmpty() )
        {
            viewPlugin = (Kopete::ViewPlugin*)pluginManager->loadPlugin( pluginName );

            if( !viewPlugin )
            {
                kWarning(14000) << "Requested view plugin, " << pluginName
                                 << ", was not found. Falling back to chat window plugin" << endl;
            }
        }

        if( !viewPlugin )
        {
            viewPlugin = (Kopete::ViewPlugin*)pluginManager->loadPlugin( QLatin1String("kopete_chatwindow") );
        }

        if( viewPlugin )
        {
            KopeteView *newView = viewPlugin->createView(session);

            d->foreignMessage = false;
            d->managerMap.insert( session, newView );

            connect( session, SIGNAL( closing(Kopete::ChatSession *) ),
                            this, SLOT(slotChatSessionDestroyed(Kopete::ChatSession*)) );

            return newView;
        }
        else
        {
            kError(14000) << "Could not create a view, no plugins available!" << endl;
            return 0L;
        }
    }
}


void KopeteViewManager::messageAppended( Kopete::Message &msg, Kopete::ChatSession *manager)
{
    // kDebug(14000) << k_funcinfo;

    bool outgoingMessage = ( msg.direction() == Kopete::Message::Outbound );

    if( !outgoingMessage || d->managerMap.contains( manager ) )
    {
        d->foreignMessage=!outgoingMessage; //let know for the view we are about to create
        manager->view(true,msg.requestedPlugin())->appendMessage( msg );
        d->foreignMessage=false; //the view is created, reset the flag

		bool appendMessageEvent = d->useQueueOrStack;

		QWidget *w;
		if( d->queueUnreadMessages && ( w = dynamic_cast<QWidget*>(view( manager )) ) )
		{
			// append msg event to queue if chat window is active but not the chat view in it...
			appendMessageEvent = appendMessageEvent && !(w->isActiveWindow() && manager->view() == d->activeView);
			// ...and chat window is on another desktop
			appendMessageEvent = appendMessageEvent && (!d->queueOnlyMessagesOnAnotherDesktop 
#ifdef Q_WS_X11
					||!KWindowSystem::windowInfo( w->topLevelWidget()->winId(), NET::WMDesktop ).isOnCurrentDesktop());
#else
					);
#endif					
		}
		else
		{
			// append if no chat window exists already
			appendMessageEvent = appendMessageEvent && !view( manager )->isVisible();
		}

		// in group chats always append highlighted messages to queue
		appendMessageEvent = appendMessageEvent && (!d->queueOnlyHighlightedMessagesInGroupChats || manager->members().count() == 1 || msg.importance() == Kopete::Message::Highlight);

		
		Kopete::MessageEvent *event=0L;
		if ( !outgoingMessage )
		{
			event=new Kopete::MessageEvent(msg,manager);
			d->eventList.append( event );
			connect(event, SIGNAL(done(Kopete::MessageEvent *)), this, SLOT(slotEventDeleted(Kopete::MessageEvent *)));
		}
		
		if ( event && ( !manager->account()->isAway() || Kopete::BehaviorSettings::self()->enableEventsWhileAway() )
					&& msg.direction() != Kopete::Message::Internal )
		{
			QWidget *w=dynamic_cast<QWidget*>(manager->view(false));
			if( (!manager->view(false) || !w || manager->view() != d->activeView ||
						   Kopete::BehaviorSettings::self()->showEventsIfActive() || !w->isActiveWindow())
						   && msg.from())
			{
				QString msgFrom;
				msgFrom.clear();
				if( msg.from()->metaContact() )
					msgFrom = msg.from()->metaContact()->displayName();
				else
					msgFrom = msg.from()->contactId();

				QString eventId;
				KLocalizedString body = ki18n( "<qt>Incoming message from %1<br />\"%2\"</qt>" );
				switch( msg.importance() )
				{
					case Kopete::Message::Low:
						eventId = QLatin1String( "kopete_contact_lowpriority" );
						break;
					case Kopete::Message::Highlight:
						eventId = QLatin1String( "kopete_contact_highlight" );
						body = ki18n( "<qt>A highlighted message arrived from %1<br />\"%2\"</qt>" );
						break;
					default:
						eventId = QLatin1String( "kopete_contact_incoming" );
				}
				KNotification *notify=new KNotification(eventId, w, KNotification::Persistant);
				notify->setText(body.subs( Qt::escape(msgFrom) ).subs( squashMessage( msg )  ).toString());
				notify->setPixmap( QPixmap::fromImage(msg.from()->metaContact()->picture().image()) );
                notify->setActions(( QStringList() <<  i18n( "View" )  <<   i18n( "Ignore" )) );
				
				foreach(QString cl , msg.classes())
					notify->addContext( qMakePair( QString::fromLatin1("class") , cl ) );

				Kopete::MetaContact *mc= msg.from()->metaContact();
				if(mc)
				{
					notify->addContext( qMakePair( QString::fromLatin1("metacontact") , mc->metaContactId()) );
					foreach( Kopete::Group *g , mc->groups() )
					{
						notify->addContext( qMakePair( QString::fromLatin1("group") , QString::number(g->groupId())) );
					}
				}
				connect(notify,SIGNAL(activated()), manager , SLOT(raiseView()) );
				connect(notify,SIGNAL(action1Activated()), manager , SLOT(raiseView()) );
                connect(notify,SIGNAL(action2Activated()), event , SLOT(discard()) );
				connect(event, SIGNAL(done(Kopete::MessageEvent*)) , notify , SLOT(close() ));
				notify->sendEvent();
			}
		}

		if( /* appendMessageEvent && */ event  )
		{
			Kopete::ChatSessionManager::self()->postNewEvent(event);
		}
		else if( d->eventList.isEmpty() )
		{
			readMessages( manager, outgoingMessage );
		}

	}
}

void KopeteViewManager::readMessages( Kopete::ChatSession *manager, bool outgoingMessage, bool activate )
{
    // kDebug( 14000 ) << k_funcinfo;
    d->foreignMessage=!outgoingMessage; //let know for the view we are about to create
    KopeteView *thisView = manager->view( true );
    d->foreignMessage=false; //the view is created, reset the flag
    if( ( outgoingMessage && !thisView->isVisible() ) || d->raiseWindow || activate )
            thisView->raise( activate );
    else if( !thisView->isVisible() )
            thisView->makeVisible();

    foreach (Kopete::MessageEvent *event, d->eventList)
    {
        if ( event->message().manager() == manager )
        {
            event->apply();
            d->eventList.removeAll(event);
        }
    }
}

void KopeteViewManager::slotEventDeleted( Kopete::MessageEvent *event )
{
    // kDebug(14000) << k_funcinfo;
    Kopete::ChatSession *kmm=event->message().manager();
    if(!kmm)
            return;

    // d->eventList.remove( event );
    d->eventList.removeAll(event);

    if ( event->state() == Kopete::MessageEvent::Applied )
    {
        readMessages( kmm, false, true );
    }
    else if ( event->state() == Kopete::MessageEvent::Ignored && d->balloonNotifyIgnoreClosesChatView )
    {
        bool bAnotherWithThisManager = false;
        foreach (Kopete::MessageEvent *event, d->eventList)
        {
            if ( event->message().manager() == kmm )
            {
                bAnotherWithThisManager = true;
            }
        }
        if ( !bAnotherWithThisManager && kmm->view( false ) )
            kmm->view()->closeView( true );
    }
}

void KopeteViewManager::nextEvent()
{
    // kDebug( 14000 ) << k_funcinfo;

    if( d->eventList.isEmpty() )
        return;

    Kopete::MessageEvent* event = d->eventList.first();

    if ( event )
        event->apply();
}

void KopeteViewManager::slotViewActivated( KopeteView *view )
{
    // kDebug( 14000 ) << k_funcinfo;
    d->activeView = view;

    foreach (Kopete::MessageEvent *event, d->eventList)
    {
        if ( event->message().manager() == view->msgManager() )
        {
            event->deleteLater();
        }
    }
}

void KopeteViewManager::slotViewDestroyed( KopeteView *closingView )
{
    // kDebug( 14000 ) << k_funcinfo;

    if( d->managerMap.contains( closingView->msgManager() ) )
    {
        d->managerMap.remove( closingView->msgManager() );
        // closingView->msgManager()->setCanBeDeleted( true );
    }

    if( closingView == d->activeView )
        d->activeView = 0L;
}

void KopeteViewManager::slotChatSessionDestroyed( Kopete::ChatSession *manager )
{
    // kDebug( 14000 ) << k_funcinfo;

	if( d->managerMap.contains( manager ) )
	{
		KopeteView *v=d->managerMap[ manager ];
		v->closeView( true );
		delete v;   //closeView call deleteLater,  but in this case this is not enough, because some signal are called that case crash
		d->managerMap.remove( manager );
	}
}

KopeteView* KopeteViewManager::activeView() const
{
    return d->activeView;
}


#include "kopeteviewmanager.moc"

// vim: set noet ts=4 sts=4 sw=4:
