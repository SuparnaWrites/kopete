 /*

    Copyright (c) 2006      by Olivier Goffart  <ogoffart at kde.org>

    Kopete    (c) 2006 by the Kopete developers <kopete-devel@kde.org>


    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
 */

#include "jabbertransport.h"
#include "jabbercontact.h"
#include "jabberaccount.h"
#include "jabberprotocol.h"
#include "jabbercontactpool.h"
#include "jabberchatsession.h"

#include <kopeteaccountmanager.h>
#include <kopetecontact.h>
#include <kopetecontactlist.h>

#include <kopeteversion.h>


#include <qpixmap.h>
#include <qtimer.h>
#include <kaction.h>
#include <kdebug.h>
#include <klocale.h>
#include <kconfig.h>

#include "xmpp_tasks.h"

JabberTransport::JabberTransport (JabberAccount * parentAccount, const QString & myselfContactId, const QString& gateway_type)
	:Kopete::Account ( parentAccount->protocol(), myselfContactId+"/"+parentAccount->accountId())
{
	m_status=Creating;
	m_account = parentAccount;
	m_account->addTransport( this, myselfContactId );
	
	JabberContact *myContact = m_account->contactPool()->addContact ( XMPP::RosterItem ( myselfContactId ), Kopete::ContactList::self()->myself(), false );
	setMyself( myContact );
	
	//we have to know if the account get loaded from the config, or newly created
	bool exist=configGroup()->readBoolEntry("exist",false);
	
	if(!exist)
	{
		setColor( account()->color() );
#if KOPETE_IS_VERSION(0,11,51)
		QString cIcon;
		if(gateway_type=="msn")
			cIcon="jabber_gateway_msn";
		else if(gateway_type=="icq")
			cIcon="jabber_gateway_icq";
		else if(gateway_type=="aim")
			cIcon="jabber_gateway_aim";
		else if(gateway_type=="yahoo")
			cIcon="jabber_gateway_yahoo";
		else if(gateway_type=="sms")
			cIcon="jabber_gateway_sms";
		else if(gateway_type=="gadu-gadu")
			cIcon="jabber_gateway_gadu";
		else if(gateway_type=="smtp")
			cIcon="jabber_gateway_smtp";
		else if(gateway_type=="http-ws") 
			cIcon="jabber_gateway_http-ws";
		else if(gateway_type=="qq")
			cIcon="jabber_gateway_qq";
		else if(gateway_type=="tlen")
			cIcon="jabber_gateway_tlen";
		else if(gateway_type=="irc")  //NOTE: this is not official 
			cIcon="irc_protocol";

		if( !cIcon.isEmpty() )
			setCustomIcon( cIcon );
#endif
		configGroup()->writeEntry("exist",true);
		QTimer::singleShot(0, this, SLOT(eatContacts()));
	}
	
	m_status=Normal;
}

JabberTransport::~JabberTransport ()
{
	m_account->removeTransport( myself()->contactId() );
}

KActionMenu *JabberTransport::actionMenu ()
{
	KActionMenu *menu = new KActionMenu( accountId(), myself()->onlineStatus().iconFor( this ),  this );
	QString nick = myself()->property( Kopete::Global::Properties::self()->nickName()).value().toString();

	menu->popupMenu()->insertTitle( myself()->onlineStatus().iconFor( myself() ),
	nick.isNull() ? accountLabel() : i18n( "%2 <%1>" ).arg( accountLabel(), nick )
								  );
	
	QPtrList<KAction> *customActions = myself()->customContextMenuActions(  );
	if( customActions && !customActions->isEmpty() )
	{
		menu->popupMenu()->insertSeparator();

		for( KAction *a = customActions->first(); a; a = customActions->next() )
			a->plug( menu->popupMenu() );
	}
	delete customActions;

	return menu;

/*	KActionMenu *m_actionMenu = Kopete::Account::actionMenu();

	m_actionMenu->popupMenu()->insertSeparator();

	m_actionMenu->insert(new KAction (i18n ("Join Groupchat..."), "jabber_group", 0,
		this, SLOT (slotJoinNewChat ()), this, "actionJoinChat"));

	m_actionMenu->popupMenu()->insertSeparator();

	m_actionMenu->insert ( new KAction ( i18n ("Services..."), "jabber_serv_on", 0,
										 this, SLOT ( slotGetServices () ), this, "actionJabberServices") );

	m_actionMenu->insert ( new KAction ( i18n ("Send Raw Packet to Server..."), "mail_new", 0,
										 this, SLOT ( slotSendRaw () ), this, "actionJabberSendRaw") );

	m_actionMenu->insert ( new KAction ( i18n ("Edit User Info..."), "identity", 0,
										 this, SLOT ( slotEditVCard () ), this, "actionEditVCard") );

	return m_actionMenu;*/
}


bool JabberTransport::createContact (const QString & contactId,  Kopete::MetaContact * metaContact)
{
#if 0 //TODO
	// collect all group names
	QStringList groupNames;
	Kopete::GroupList groupList = metaContact->groups();
	for(Kopete::Group *group = groupList.first(); group; group = groupList.next())
		groupNames += group->displayName();

	XMPP::Jid jid ( contactId );
	XMPP::RosterItem item ( jid );
	item.setName ( metaContact->displayName () );
	item.setGroups ( groupNames );

	// this contact will be created with the "dirty" flag set
	// (it will get reset if the contact appears in the roster during connect)
	JabberContact *contact = contactPool()->addContact ( item, metaContact, true );

	return ( contact != 0 );
#endif
	return false;
}


void JabberTransport::setOnlineStatus( const Kopete::OnlineStatus& status  , const QString &reason)
{
#if 0
	if( status.status() == Kopete::OnlineStatus::Offline )
	{
		disconnect( Kopete::Account::Manual );
		return;
	}

	if( isConnecting () )
	{
		errorConnectionInProgress ();
		return;
	}

	XMPP::Status xmppStatus ( "", reason );

	switch ( status.internalStatus () )
	{
		case JabberProtocol::JabberFreeForChat:
			xmppStatus.setShow ( "chat" );
			break;

		case JabberProtocol::JabberOnline:
			xmppStatus.setShow ( "" );
			break;

		case JabberProtocol::JabberAway:
			xmppStatus.setShow ( "away" );
			break;

		case JabberProtocol::JabberXA:
			xmppStatus.setShow ( "xa" );
			break;

		case JabberProtocol::JabberDND:
			xmppStatus.setShow ( "dnd" );
			break;

		case JabberProtocol::JabberInvisible:
			xmppStatus.setIsInvisible ( true );
			break;
	}

	if ( !isConnected () )
	{
		// we are not connected yet, so connect now
		m_initialPresence = xmppStatus;
		connect ();
	}
	else
	{
		setPresence ( xmppStatus );
	}
#endif
}

JabberProtocol * JabberTransport::protocol( ) const
{
	return m_account->protocol(); 
}

bool JabberTransport::removeAccount( )
{
	if(m_status == Removing  ||  m_status == AccountRemoved)
		return true; //so it can be deleted
	
	if (!account()->isConnected())
	{
		account()->errorConnectFirst ();
		return false;
	}
	
	m_status = Removing;
	XMPP::JT_Register *task = new XMPP::JT_Register ( m_account->client()->rootTask () );
	QObject::connect ( task, SIGNAL ( finished () ), this, SLOT ( removeAllContacts() ) );

	//JabberContact *my=static_cast<JabberContact*>(myself());
	task->unreg ( myself()->contactId() );
	task->go ( true );
	return false; //delay the removal
}

void JabberTransport::removeAllContacts( )
{
//	XMPP::JT_Register * task = (XMPP::JT_Register *) sender ();

/*	if ( ! task->success ())
	KMessageBox::queuedMessageBox ( 0L, KMessageBox::Error,
									i18n ("An error occured when trying to remove the transport:\n%1").arg(task->statusString()),
									i18n ("Jabber Service Unregistration"));
	*/ //we don't really care, we remove everithing anyway.

	kdDebug(JABBER_DEBUG_GLOBAL) << k_funcinfo << "delete all contacts of the transport"<< endl;
	QDictIterator<Kopete::Contact> it( contacts() ); 
	for( ; it.current(); ++it )
	{
		XMPP::JT_Roster * rosterTask = new XMPP::JT_Roster ( account()->client()->rootTask () );
		rosterTask->remove ( static_cast<JabberBaseContact*>(it.current())->rosterItem().jid() );
		rosterTask->go ( true );
	}
	m_status = Removing; //in theory that's already our status
	Kopete::AccountManager::self()->removeAccount( this ); //this will delete this
}

QString JabberTransport::legacyId( const XMPP::Jid & jid )
{
	if(jid.node().isEmpty())
		return QString();
	QString node = jid.node();
	return node.replace("%","@");
}

void JabberTransport::jabberAccountRemoved( )
{
	m_status = AccountRemoved;
	Kopete::AccountManager::self()->removeAccount( this ); //this will delete this	
}

void JabberTransport::eatContacts( )
{
	/*
	* "Gateway Contact Eating" (c)(r)(tm)(g)(o)(f)
	* this comes directly from my mind into the kopete code.
	* principle: - the transport is hungry
	*            - it will eat contacts which belong to him
	*            - the contact will die
	*            - a new contact will born, with the same characteristics, but owned by the transport
	* - Olivier 2006-01-17 -
	*/
	QDict<Kopete::Contact> cts=account()->contacts();
	QDictIterator<Kopete::Contact> it( cts ); 
	for( ; it.current(); ++it )
	{
		JabberContact *contact=dynamic_cast<JabberContact*>(it.current());
		if( contact && !contact->transport() && contact->rosterItem().jid().domain() == myself()->contactId() && contact != account()->myself())
		{
			XMPP::RosterItem item=contact->rosterItem();
			Kopete::MetaContact *mc=contact->metaContact();
			Kopete::OnlineStatus status = contact->onlineStatus();
			delete contact;
			Kopete::Contact *c2=account()->contactPool()->addContact( item , mc , false ); //not sure this is false;
			if(c2)
				c2->setOnlineStatus( status ); //put back the old status
		}
	}
}



#include "jabbertransport.moc"

// vim: set noet ts=4 sts=4 sw=4:
