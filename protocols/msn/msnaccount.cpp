/*
    msnaccount.h - Manages a single MSN account

    Copyright (c) 2003-2005 by Olivier Goffart       <ogoffart@kde.org>
    Copyright (c) 2003      by Martijn Klingens      <klingens@kde.org>
    Copyright (c) 2005      by Michaël Larouche     <larouche@kde.org>

    Kopete    (c) 2002-2007 by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include "msnaccount.h"

#include <config-kopete.h>

#include <kaction.h>
#include <kactionmenu.h>
#include <kconfig.h>
#include <kdebug.h>
#include <kinputdialog.h>
#include <kmessagebox.h>
#include <kmenu.h>
#include <kstandarddirs.h>
#include <kcodecs.h>
#include <klocale.h>
#include <kactionmenu.h>
#include <kicon.h>
#include <kconfiggroup.h>

#include <QFile>
#include <QRegExp>
#include <QValidator>
#include <QImage>
#include <QList>

#include "msncontact.h"
#include "msnnotifysocket.h"
#include "msnchatsession.h"
#include "kopetecontactlist.h"
#include "kopetegroup.h"
#include "kopetemetacontact.h"
#include "kopetepassword.h"
#include "kopeteuiglobal.h"
#include "kopeteglobal.h"
#include "kopetechatsessionmanager.h"
#include "contactaddednotifydialog.h"

#include "sha1.h"


#if !defined NDEBUG
#include "msndebugrawcmddlg.h"
#include <kglobal.h>
#endif

#ifdef MSN_WEBCAM
#include "avdevice/videodevicepool.h"
#endif
MSNAccount::MSNAccount( MSNProtocol *parent, const QString& AccountID )
	: Kopete::PasswordedAccount ( parent, AccountID.toLower() , false )
{
	m_notifySocket = 0L;
	m_connectstatus = MSNProtocol::protocol()->NLN;
	m_addWizard_metaContact = 0L;
	m_newContactList=false;

	// Init the myself contact
	setMyself( new MSNContact( this, accountId(), Kopete::ContactList::self()->myself() ) );
	//myself()->setOnlineStatus( MSNProtocol::protocol()->FLN );

	QObject::connect( Kopete::ContactList::self(), SIGNAL( groupRenamed( Kopete::Group *, const QString & ) ),
		SLOT( slotKopeteGroupRenamed( Kopete::Group * ) ) );
	QObject::connect( Kopete::ContactList::self(), SIGNAL( groupRemoved( Kopete::Group * ) ),
		SLOT( slotKopeteGroupRemoved( Kopete::Group * ) ) );

	m_openInboxAction = new KAction( KIcon("mail"), i18n( "Open Inbo&x..." ), this );
        //, "m_openInboxAction" );
	QObject::connect( m_openInboxAction, SIGNAL(triggered(bool)), this, SLOT(slotOpenInbox()) );

	m_changeDNAction = new KAction( i18n( "&Change Display Name..." ), this );
        //, "renameAction" );
	QObject::connect( m_changeDNAction, SIGNAL(triggered(bool)), this, SLOT(slotChangePublicName()) );

	m_startChatAction = new KAction( KIcon("mail"), i18n( "&Start Chat..." ), this );
        //, "startChatAction" );
	QObject::connect( m_startChatAction, SIGNAL(triggered(bool)), this, SLOT(slotStartChat()) );

	KConfigGroup *config=configGroup();

	m_blockList   = config->readEntry(  "blockList", QStringList() ) ;
	m_allowList   = config->readEntry(  "allowList", QStringList() ) ;
	m_reverseList = config->readEntry(  "reverseList", QStringList()  ) ;

	// Load the avatar
	m_pictureFilename = config->readEntry( "avatar", QString());
	resetPictureObject(true);

	static_cast<MSNContact *>( myself() )->setInfo( "PHH", config->readEntry("PHH") );
	static_cast<MSNContact *>( myself() )->setInfo( "PHM", config->readEntry("PHM") );
	static_cast<MSNContact *>( myself() )->setInfo( "PHW", config->readEntry("PHW") );
	//this is the display name
	static_cast<MSNContact *>( myself() )->setInfo( "MFN", config->readEntry("MFN") );

	//construct the group list
	//Before 2003-11-14 the MSN server allowed us to download the group list without downloading the whole contact list, but it's not possible anymore
	QList<Kopete::Group*> groupList = Kopete::ContactList::self()->groups();
	Kopete::Group *g;
	foreach(g, groupList)
	{
		QString groupGuid=g->pluginData( protocol(), accountId() + " id" );
		if ( !groupGuid.isEmpty() )
			m_groupList.insert( groupGuid , g );
	}

	// Set the client Id for the myself contact.  It sets what MSN feature we support.
	m_clientId = MSNProtocol::MSNC4 | MSNProtocol::InkFormatGIF | MSNProtocol::SupportMultiPacketMessaging;

#if 0
	Kopete::AV::VideoDevicePool::self()->scanDevices();
	if( Kopete::AV::VideoDevicePool::self()->hasDevices() )
	{
		m_clientId |= MSNProtocol::SupportWebcam;
	}
#endif
}


QString MSNAccount::serverName()
{
	return configGroup()->readEntry(  "serverName" , "messenger.hotmail.com" );
}

uint MSNAccount::serverPort()
{
	return configGroup()->readEntry(  "serverPort" , 1863 );
}

bool MSNAccount::useHttpMethod() const
{
	return configGroup()->readEntry(  "useHttpMethod" , false );
}

QString MSNAccount::myselfClientId() const
{
	return QString::number(m_clientId, 10);
}

void MSNAccount::connectWithPassword( const QString &passwd )
{
	m_newContactList=false;
	if ( isConnected() )
	{
		kDebug( 14140 ) << k_funcinfo <<"Ignoring Connect request "
			<< "(Already Connected)" << endl;
		return;
	}

	if ( m_notifySocket )
	{
		kDebug( 14140 ) << k_funcinfo <<"Ignoring Connect request (Already connecting)";
		return;
	}

	m_password = passwd;

	if ( m_password.isNull() )
	{
		kDebug( 14140 ) << k_funcinfo <<"Abort connection (null password)";
		return;
	}


	if ( contacts().count() <= 1 )
	{
		// Maybe the contact list.xml has been removed, and the serial number not updated
		// ( the 1 is for the myself contact )
		configGroup()->writeEntry( "serial", 0 );
	}

	m_openInboxAction->setEnabled( false );

	createNotificationServer(serverName(), serverPort());
}

void MSNAccount::createNotificationServer( const QString &host, uint port )
{
	if(m_notifySocket) //we are switching from one to another notifysocket.
	{
		//remove every slots to that socket, so we won't delete receive signals
		// from the old socket thinking they are from the new one
		QObject::disconnect( m_notifySocket , 0, this, 0 );
		m_notifySocket->deleteLater(); //be sure it will be deleted
		m_notifySocket=0L;
	}

	m_msgHandle.clear();

	myself()->setOnlineStatus( MSNProtocol::protocol()->CNT );


	m_notifySocket = new MSNNotifySocket( this, accountId() , m_password);
	m_notifySocket->setUseHttpMethod( useHttpMethod() );

	QObject::connect( m_notifySocket, SIGNAL( groupAdded( const QString&, const QString& ) ),
		SLOT( slotGroupAdded( const QString&, const QString& ) ) );
	QObject::connect( m_notifySocket, SIGNAL( groupRenamed( const QString&, const QString& ) ),
		SLOT( slotGroupRenamed( const QString&, const QString& ) ) );
	QObject::connect( m_notifySocket, SIGNAL( groupListed( const QString&, const QString& ) ),
		SLOT( slotGroupAdded( const QString&, const QString& ) ) );
	QObject::connect( m_notifySocket, SIGNAL( groupRemoved( const QString& ) ),
		SLOT( slotGroupRemoved( const QString& ) ) );
	QObject::connect( m_notifySocket, SIGNAL( contactList(const QString&, const QString&, const QString&, uint, const QString& ) ),
		SLOT( slotContactListed(const QString&, const QString&, const QString&, uint, const QString& ) ) );
	QObject::connect( m_notifySocket, SIGNAL(contactAdded(const QString&, const QString&, const QString&, const QString&, const QString& ) ),
		SLOT( slotContactAdded(const QString&, const QString&, const QString&, const QString&, const QString& ) ) );
	QObject::connect( m_notifySocket, SIGNAL( contactRemoved(const QString&, const QString&, const QString&, const QString& ) ),
		SLOT( slotContactRemoved(const QString&, const QString&, const QString&, const QString& ) ) );
	QObject::connect( m_notifySocket, SIGNAL( statusChanged( const Kopete::OnlineStatus & ) ),
		SLOT( slotStatusChanged( const Kopete::OnlineStatus & ) ) );
	QObject::connect( m_notifySocket, SIGNAL( invitedToChat( const QString&, const QString&, const QString&, const QString&, const QString& ) ),
		SLOT( slotCreateChat( const QString&, const QString&, const QString&, const QString&, const QString& ) ) );
	QObject::connect( m_notifySocket, SIGNAL( startChat( const QString&, const QString& ) ),
		SLOT( slotCreateChat( const QString&, const QString& ) ) );
	QObject::connect( m_notifySocket, SIGNAL( socketClosed() ),
		SLOT( slotNotifySocketClosed() ) );
	QObject::connect( m_notifySocket, SIGNAL( newContactList() ),
		SLOT( slotNewContactList() ) );
	QObject::connect( m_notifySocket, SIGNAL( receivedNotificationServer(const QString&, uint )  ),
		SLOT(createNotificationServer(const QString&, uint ) ) );
	QObject::connect( m_notifySocket, SIGNAL( hotmailSeted( bool ) ),
		m_openInboxAction, SLOT( setEnabled( bool ) ) );
	QObject::connect( m_notifySocket, SIGNAL( errorMessage(int, const QString& ) ),
		SLOT( slotErrorMessageReceived(int, const QString& ) ) );

	m_notifySocket->setStatus( m_connectstatus );
	m_notifySocket->connect(host, port);
}

void MSNAccount::disconnect()
{
	if ( m_notifySocket )
		m_notifySocket->disconnect();
}

KActionMenu * MSNAccount::actionMenu()
{
	KActionMenu *m_actionMenu=Kopete::Account::actionMenu();
	if ( isConnected() )
	{
		m_openInboxAction->setEnabled( true );
		m_startChatAction->setEnabled( true );
		m_changeDNAction->setEnabled( true );
	}
	else
	{
		m_openInboxAction->setEnabled( false );
		m_startChatAction->setEnabled( false );
		m_changeDNAction->setEnabled( false );
	}

	m_actionMenu->addSeparator();

	m_actionMenu->addAction( m_changeDNAction );
	m_actionMenu->addAction( m_startChatAction );

//	m_actionMenu->menu()->insertSeparator();

	m_actionMenu->addAction( m_openInboxAction );

#if !defined NDEBUG
	KActionMenu *debugMenu = new KActionMenu( "Debug", this );

	KAction *rawCmd = new KAction( i18n( "Send Raw C&ommand..." ), this );
        //, "m_debugRawCommand" );
	QObject::connect( rawCmd, SIGNAL(triggered()), this, SLOT(slotDebugRawCommand()) );
	debugMenu->addAction(rawCmd);

	m_actionMenu->addSeparator();
	m_actionMenu->addAction( debugMenu );
#endif

	return m_actionMenu;
}

MSNNotifySocket *MSNAccount::notifySocket()
{
	return m_notifySocket;
}


void MSNAccount::setOnlineStatus( const Kopete::OnlineStatus &status , const Kopete::StatusMessage &reason)
{
	kDebug( 14140 ) << k_funcinfo << status.description();

	// FIXME: When changing song, do don't anything while connected
	//if( reason.  contains("[Music]") && ( status == MSNProtocol::protocol()->UNK || status == MSNProtocol::protocol()->CNT ) )
	//	return;

	// Only send personal message when logged.
	if( m_notifySocket && m_notifySocket->isLogged() )
	{
		// Only update the personal/status message, don't change the online status
		// since it's the same.
		setStatusMessage( reason );
	}

	if(status.status()== Kopete::OnlineStatus::Offline)
		disconnect();
	else if ( m_notifySocket )
	{
		m_notifySocket->setStatus( status );
	}
	else
	{
		m_connectstatus = status;
		connect();
	}


}

void MSNAccount::setStatusMessage( const Kopete::StatusMessage &statusMessage )
{
	if ( m_notifySocket )
	{
		myself()->setStatusMessage( statusMessage );
		m_notifySocket->changePersonalMessage( statusMessage );
	}
	/*  Eh,  if we can't change the display name, don't let make the user think it has changed
	else if(type == MSNProtocol::PersonalMessageNormal) // Normal personalMessage, not a dynamic one that need formatting.
	{
		slotPersonalMessageChanged( personalMessage );
	}*/
}

void MSNAccount::slotStartChat()
{

	bool ok;
	QString handle = KInputDialog::getText( i18n( "Start Chat - MSN Plugin" ),
		i18n( "Please enter the email address of the person with whom you want to chat:" ), QString(), &ok ).toLower();
	if ( ok )
	{
		if ( MSNProtocol::validContactId( handle ) )
		{
			if ( !contacts()[ handle ] )
				addContact( handle, handle, 0L, Kopete::Account::Temporary );

			contacts()[ handle ]->execute();
		}
		else
		{
			KMessageBox::queuedMessageBox( Kopete::UI::Global::mainWidget(), KMessageBox::Sorry,
				i18n( "<qt>You must enter a valid email address.</qt>" ), i18n( "MSN Plugin" ) );
		}
	}
}

void MSNAccount::slotDebugRawCommand()
{
#if !defined NDEBUG
	if ( !isConnected() )
		return;

	MSNDebugRawCmdDlg *dlg = new MSNDebugRawCmdDlg( 0L );
	int result = dlg->exec();
	if ( result == QDialog::Accepted && m_notifySocket )
	{
		m_notifySocket->sendCommand( dlg->command(), dlg->params(),
					dlg->addId(), dlg->msg().replace( "\n", "\r\n" ).toUtf8() );
	}
	delete dlg;
#endif
}

void MSNAccount::slotChangePublicName()
{
	if ( !isConnected() )
	{
		return;
		//TODO:  change it anyway, and sync at the next connection
	}

	bool ok;
	QString name = KInputDialog::getText( i18n( "Change Display Name - MSN Plugin" ),
		i18n( "Enter the new display name by which you want to be visible to your friends on MSN:" ),
		myself()->property( Kopete::Global::Properties::self()->nickName()).value().toString(), &ok );

	if ( ok )
	{
		if ( name.length() > 387 )
		{
			KMessageBox::error( Kopete::UI::Global::mainWidget(),
				i18n( "<qt>The display name you entered is too long. Please use a shorter name.\n"
					"Your display name has <b>not</b> been changed.</qt>" ),
				i18n( "Change Display Name - MSN Plugin" ) );
			return;
		}

		setPublicName( name );
	}
}


void MSNAccount::slotOpenInbox()
{
	if ( m_notifySocket )
		m_notifySocket->slotOpenInbox();
}


void MSNAccount::slotNotifySocketClosed()
{
	kDebug( 14140 ) << k_funcinfo;

	Kopete::Account::DisconnectReason reason=(Kopete::Account::DisconnectReason)(m_notifySocket->disconnectReason());
	m_notifySocket->deleteLater();
	m_notifySocket = 0l;
	myself()->setOnlineStatus( MSNProtocol::protocol()->FLN );
	setAllContactsStatus( MSNProtocol::protocol()->FLN );
	disconnected(reason);


	if(reason == Kopete::Account::OtherClient)
	{ //close all chat sessions,   so new message will arive to the other client.

		QList<Kopete::ChatSession*> sessions = Kopete::ChatSessionManager::self()->sessions();
		QList<Kopete::ChatSession*>::Iterator it, itEnd = sessions.end();
		for (it=sessions.begin() ; it != itEnd ; it++ )
		{
			MSNChatSession *msnCS = dynamic_cast<MSNChatSession *>( *it );
			if ( msnCS && msnCS->account() == this )
			{
				msnCS->slotCloseSession();
			}
		}
	}

#if 0
	else if ( state == 0x10 ) // connection died unexpectedly
	{
		KMessageBox::queuedMessageBox( Kopete::UI::Global::mainWidget(), KMessageBox::Error , i18n( "The connection with the MSN server was lost unexpectedly.\n"
			"If you cannot reconnect now, the server might be down. In that case, please try again later." ),
			i18n( "Connection Lost - MSN Plugin" ), KMessageBox::Notify );
	}
#endif
	m_msgHandle.clear();
	// kDebug( 14140 ) << "MSNAccount::slotNotifySocketClosed - done";
}

void MSNAccount::slotStatusChanged( const Kopete::OnlineStatus &status )
{
//	kDebug( 14140 ) << k_funcinfo  << status.internalStatus();
	myself()->setOnlineStatus( status );

	if(m_newContactList)
	{
		m_newContactList=false;

		QHash<QString,Kopete::Contact*> contactList = contacts();
		QHash<QString,Kopete::Contact*>::Iterator it, itEnd = contactList.end();
		for ( it = contactList.begin(); it != itEnd; ++it )
		{
			MSNContact *c = static_cast<MSNContact *>( *it );
			if(c && c->isDeleted() && c->metaContact() && !c->metaContact()->isTemporary() && c!=myself())
			{
				if(c->serverGroups().isEmpty())
				{ //the contact is new, add it on the server
					c->setOnlineStatus( MSNProtocol::protocol()->FLN );
					addContactServerside( c->contactId() , c->metaContact()->groups() );
				}
				else //the contact had been deleted, give him the unknown status
				{
					c->clearServerGroups();
					c->setOnlineStatus( MSNProtocol::protocol()->UNK );
				}
			}
		}
	}
}

void MSNAccount::setPublicName( const QString &publicName )
{
	if ( m_notifySocket )
	{
		m_notifySocket->changePublicName( publicName, QString() );
	}
}

void MSNAccount::slotGroupAdded( const QString& groupName, const QString &groupGuid )
{
	if ( m_groupList.contains( groupGuid ) )
	{
		// Group can already be in the list since the idle timer does a 'List Groups'
		// command. Simply return, don't issue a warning.
		// kDebug( 14140 ) << k_funcinfo << "Group " << groupName << " already in list, skipped.";
		return;
	}

	//--------- Find the appropriate Kopete::Group, or create one ---------//
	QList<Kopete::Group*> groupList = Kopete::ContactList::self()->groups();
	Kopete::Group *fallBack = 0L;

	//check if we have one in the old group list. if yes, update the id translate map.
	QMap<QString, Kopete::Group*>::Iterator it, itEnd = m_oldGroupList.end();
	for( it=m_oldGroupList.begin() ; it != itEnd ; ++it )
	{
		Kopete::Group *g=it.value();
		if (g && g->pluginData( protocol(), accountId() + " displayName" ) == groupName  &&
				g->pluginData( protocol(), accountId() + " id" ).isEmpty() )
		{ //it has the same name! we got it.    (and it is not yet an msn group)
			fallBack=g;
			/*if ( g->displayName() != groupName )
			{
				// The displayName was changed in Kopete while we were offline
				// FIXME: update the server right now
			}*/
			break;
		}
	}

	if(!fallBack)
	{
		//it's certenly a new group !  search if one already exist with the same displayname.
		Kopete::Group *g;
		foreach( g, groupList )
		{
			/*   --This has been replaced by the loop right before.
			if ( !g->pluginData( protocol(), accountId() +  " id" ).isEmpty() )
			{
				if ( g->pluginData( protocol(), accountId() + " id" ).toUInt() == groupNumber )
				{
					m_groupList.insert( groupNumber, g );
					QString oldGroupName;
					if ( g->pluginData( protocol(), accountId() + " displayName" ) != groupName )
					{
						// The displayName of the group has been modified by another client
						slotGroupRenamed( groupName, groupNumber );
					}
					return;
				}
			}
			else {*/
			if ( g->displayName() == groupName && (groupGuid.isEmpty()|| g->type()==Kopete::Group::Normal)  &&
					g->pluginData( protocol(), accountId() + " id" ).isEmpty()  )
			{
				fallBack = g;
				kDebug( 14140 ) << k_funcinfo << "We didn't found the group " << groupName <<" in the old MSN group.  But kopete has already one with the same name.";
				break;
			}
		}
	}

	if ( !fallBack )
	{
		if( groupGuid.isEmpty()  )
		{	// The group #0 is an unremovable group. his default name is "~" ,
			// but the official client rename it i18n("others contact") at the first
			// connection.
			// In many case, the users don't use that group as a real group, or just as
			// a group to put all contact that are not sorted.
			fallBack = Kopete::Group::topLevel();
		}
		else
		{
			fallBack = new Kopete::Group( groupName );
			Kopete::ContactList::self()->addGroup( fallBack );
			kDebug( 14140 ) << k_funcinfo << "We didn't found the group " << groupName <<" So we're creating a new one.";

		}
	}

	fallBack->setPluginData( protocol(), accountId() + " id", groupGuid );
	fallBack->setPluginData( protocol(), accountId() + " displayName", groupName );
	m_groupList.insert( groupGuid, fallBack );

	// We have pending groups that we need add a contact to
	if ( tmp_addToNewGroup.contains(groupName) )
	{
		QStringList list=tmp_addToNewGroup[groupName];
		QStringList::Iterator it, itEnd = list.end();
		for ( it = list.begin(); it != itEnd; ++it )
		{
			QString contactId = *it;
			kDebug( 14140 ) << k_funcinfo << "Adding to new group: " << contactId;
			MSNContact *c = static_cast<MSNContact *>(contacts()[contactId]);
			if(c && c->hasProperty(MSNProtocol::protocol()->propGuid.key()) )
				notifySocket()->addContact( contactId, MSNProtocol::FL, QString(), c->guid(), groupGuid );
			else
			{
				// If we get to here, we're currently adding a new contact, add the groupGUID to the groupList
				// to add when contact will be added to contact list.
				if( tmp_addNewContactToGroup.contains( contactId ) )
					tmp_addNewContactToGroup[contactId].append(groupGuid);
				else
					tmp_addNewContactToGroup.insert(contactId, QStringList(groupGuid) );
			}
		}
		tmp_addToNewGroup.remove(groupName);
	}
}

void MSNAccount::slotGroupRenamed( const QString &groupGuid, const QString& groupName )
{
	if ( m_groupList.contains( groupGuid ) )
	{
		m_groupList[ groupGuid ]->setPluginData( protocol(), accountId() + " id", groupGuid );
		m_groupList[ groupGuid ]->setPluginData( protocol(), accountId() + " displayName", groupName );
		m_groupList[ groupGuid ]->setDisplayName( groupName );
	}
	else
	{
		slotGroupAdded( groupName, groupGuid );
	}
}

void MSNAccount::slotGroupRemoved( const QString& groupGuid )
{
	if ( m_groupList.contains( groupGuid ) )
	{
		m_groupList[ groupGuid ]->setPluginData( protocol(), QMap<QString,QString>() );
		m_groupList.remove( groupGuid );
	}
}

void MSNAccount::addGroup( const QString &groupName, const QString& contactToAdd )
{
	if ( !contactToAdd.isNull()  )
	{
		if( tmp_addToNewGroup.contains(groupName) )
		{
			tmp_addToNewGroup[groupName].append(contactToAdd);
			//A group with the same name is about to be added,
			// we don't need to add a second group with the same name
			kDebug( 14140 ) << k_funcinfo << "no need to re-add " << groupName << " for " << contactToAdd;
			return;
		}
		else
		{
			tmp_addToNewGroup.insert(groupName,QStringList(contactToAdd));
			kDebug( 14140 ) << k_funcinfo << "preparing to add " << groupName << " for " << contactToAdd;
		}
	}

	if ( m_notifySocket )
		m_notifySocket->addGroup( groupName );

}

void MSNAccount::slotKopeteGroupRenamed( Kopete::Group *g )
{
	if ( notifySocket() && g->type() == Kopete::Group::Normal )
	{
		if ( !g->pluginData( protocol(), accountId() + " id" ).isEmpty() &&
			g->displayName() != g->pluginData( protocol(), accountId() + " displayName" ) &&
			m_groupList.contains( g->pluginData( protocol(), accountId() + " id" ) ) )
		{
			notifySocket()->renameGroup( g->displayName(), g->pluginData( protocol(), accountId() + " id" ) );
		}
	}
}

void MSNAccount::slotKopeteGroupRemoved( Kopete::Group *g )
{
	//The old gorup list is only used whe syncing the contact list.
	//We can assume the contact list is already fully synced at this time.
	//The group g is maybe in the oldGroupList.  We remove everithing since
	//we don't need it anymore, no need to search it
	m_oldGroupList.clear();


	if ( !g->pluginData( protocol(), accountId() + " id" ).isEmpty() )
	{
		QString groupGuid = g->pluginData( protocol(), accountId() + " id" );
		if ( !m_groupList.contains( groupGuid ) )
		{
			// the group is maybe already removed in the server
			slotGroupRemoved( groupGuid );
			return;
		}

		//this is also done later, but he have to do it now!
		// (in slotGroupRemoved)
		m_groupList.remove(groupGuid);

		if ( groupGuid.isEmpty() )
		{
			// the group #0 can't be deleted
			// then we set it as the top-level group
			if ( g->type() == Kopete::Group::TopLevel )
				return;

			Kopete::Group::topLevel()->setPluginData( protocol(), accountId() + " id", "" );
			Kopete::Group::topLevel()->setPluginData( protocol(), accountId() + " displayName", g->pluginData( protocol(), accountId() + " displayName" ) );
			g->setPluginData( protocol(), accountId() + " id", QString() ); // the group should be soon deleted, but make sure

			return;
		}

		if ( m_notifySocket )
		{
			bool still_have_contact=false;
			// if contact are contains only in the group we are removing, abort the
			QHash<QString, Kopete::Contact*> contactList = contacts();
			QHash<QString, Kopete::Contact*>::Iterator it, itEnd = contactList.end();
			for ( it = contactList.begin(); it != itEnd; ++it )
			{
				MSNContact *c = static_cast<MSNContact *>( it.value() );
				if ( c && c->serverGroups().contains( groupGuid )  )
				{
					/** don't do that becasue theses may already have been sent
					m_notifySocket->removeContact( c->contactId(), groupNumber, MSNProtocol::FL );
					*/
					still_have_contact=true;
					break;
				}
			}
			if(!still_have_contact)
				m_notifySocket->removeGroup( groupGuid );
		}
	}
}

void MSNAccount::slotNewContactList()
{
		m_oldGroupList=m_groupList;
		QMap<QString, Kopete::Group*>::Iterator it, itEnd = m_oldGroupList.end();
		for( it=m_oldGroupList.begin() ; it != itEnd ; ++it )
		{	//they are about to be changed
			if(it.value())
				it.value()->setPluginData( protocol(), accountId() + " id", QString() );
		}

		m_allowList.clear();
		m_blockList.clear();
		m_reverseList.clear();
		m_groupList.clear();
		KConfigGroup *config=configGroup();
		config->writeEntry( "blockList" , QString() ) ;
		config->writeEntry( "allowList" , QString() );
		config->writeEntry( "reverseList" , QString() );

		// clear all date information which will be received.
		// if the information is not anymore on the server, it will not be received
		foreach ( Kopete::Contact *kc , contacts() )
		{
			MSNContact *c = static_cast<MSNContact *>( kc );
			c->setBlocked( false );
			c->setAllowed( false );
			c->setReversed( false );
			c->setDeleted( true );
			c->setInfo( "PHH", QString() );
			c->setInfo( "PHW", QString() );
			c->setInfo( "PHM", QString() );
			c->removeProperty( MSNProtocol::protocol()->propGuid );
		}
		m_newContactList=true;
}

void MSNAccount::slotContactListed( const QString& handle, const QString& publicName, const QString &contactGuid, uint lists, const QString& groups )
{
	// On empty lists handle might be empty, ignore that
	// ignore also the myself contact.
	if ( handle.isEmpty() || handle==accountId())
		return;

	MSNContact *c = static_cast<MSNContact *>( contacts()[ handle ] );

	if ( lists & 1 )	// FL
	{
		QStringList contactGroups = groups.split( ",", QString::SkipEmptyParts );
		if ( c )
		{
			if( !c->metaContact() )
			{
				kWarning( 14140 ) << k_funcinfo << "the contact " << c->contactId() << " has no meta contact";
				Kopete::MetaContact *metaContact = new Kopete::MetaContact();

				c->setMetaContact(metaContact);
				Kopete::ContactList::self()->addMetaContact( metaContact );
			}

			// Contact exists, update data.
			// Merging difference between server contact list and Kopete::Contact's contact list into MetaContact's contact-list
			c->setOnlineStatus( MSNProtocol::protocol()->FLN );
			if(!publicName.isEmpty() && publicName!=handle)
				c->setProperty( Kopete::Global::Properties::self()->nickName() , publicName );
			else
				c->removeProperty( Kopete::Global::Properties::self()->nickName() );
			c->setProperty( MSNProtocol::protocol()->propGuid, contactGuid);

			const QMap<QString, Kopete::Group *> oldServerGroups = c->serverGroups();
			c->clearServerGroups();
			for ( QStringList::ConstIterator it = contactGroups.begin(); it != contactGroups.end(); ++it )
			{
				QString newServerGroupID =  *it;
				if(m_groupList.contains(newServerGroupID))
				{
					Kopete::Group *newServerGroup=m_groupList[ newServerGroupID ] ;
					c->contactAddedToGroup( newServerGroupID, newServerGroup );
					if( !c->metaContact()->groups().contains(newServerGroup) )
					{
						// The contact has been added in a group by another client
						c->metaContact()->addToGroup( newServerGroup );
					}
				}
			}

			for ( QMap<QString, Kopete::Group *>::ConstIterator it = oldServerGroups.begin(); it != oldServerGroups.end(); ++it )
			{
				Kopete::Group *old_group=m_oldGroupList[it.key()];
				if(old_group)
				{
					QString oldnewID=old_group->pluginData(protocol() , accountId() +" id");
					if ( !oldnewID.isEmpty() && contactGroups.contains( oldnewID ) )
						continue; //ok, it's correctn no need to do anything.

					c->metaContact()->removeFromGroup( old_group );
				}
			}

			c->setDeleted(false);

			// Update server if the contact has been moved to another group while MSN was offline
			c->sync();
		}
		else
		{
			Kopete::MetaContact *metaContact = new Kopete::MetaContact();

			c = new MSNContact( this, handle, metaContact );
			c->setDeleted(true); //we don't want to sync
			c->setOnlineStatus( MSNProtocol::protocol()->FLN );
			if(!publicName.isEmpty() && publicName!=handle)
				c->setProperty( Kopete::Global::Properties::self()->nickName() , publicName );
			else
				c->removeProperty( Kopete::Global::Properties::self()->nickName() );
			c->setProperty( MSNProtocol::protocol()->propGuid, contactGuid );

			for ( QStringList::Iterator it = contactGroups.begin();
				it != contactGroups.end(); ++it )
			{
				QString groupGuid = *it;
				if(m_groupList.contains(groupGuid))
				{
					c->contactAddedToGroup( groupGuid, m_groupList[ groupGuid ] );
					metaContact->addToGroup( m_groupList[ groupGuid ] );
				}
			}
			Kopete::ContactList::self()->addMetaContact( metaContact );

			c->setDeleted(false);
		}
	}
	else //the contact is _not_ in the FL, it has been removed
	{
		if(c)
		{
			c->setOnlineStatus( static_cast<MSNProtocol*>(protocol())->UNK );
			c->clearServerGroups();
			//TODO: display a message and suggest to remove the contact.
			//  but i fear a simple messageBox  QuestionYesNo here gives a nice crash.
			   //delete ct;
		}
	}
	if ( lists & 2 )
		slotContactAdded( handle, "AL", publicName, QString(), QString() );
	else if(c)
		c->setAllowed(false);
	if ( lists & 4 )
		slotContactAdded( handle, "BL", publicName, QString(), QString() );
	else if(c)
		c->setBlocked(false);
	if ( lists & 8 )
		slotContactAdded( handle, "RL", publicName, QString(), QString() );
	else if(c)
		c->setReversed(false);
	if ( lists & 16 ) // This contact is on the pending list. Add to the reverse list and delete from the pending list
	{
		notifySocket()->addContact( handle, MSNProtocol::RL, QString(), QString(), QString() );
		notifySocket()->removeContact( handle, MSNProtocol::PL, QString(), QString() );
	}
}

void MSNAccount::slotContactAdded( const QString& handle, const QString& list, const QString& publicName, const QString& contactGuid, const QString &groupGuid )
{
	if ( list == "FL" )
	{
		bool new_contact = false;
		if ( !contacts()[ handle ] )
		{
			new_contact = true;

			Kopete::MetaContact *m= m_addWizard_metaContact ? m_addWizard_metaContact :  new Kopete::MetaContact();

			MSNContact *c = new MSNContact( this, handle, m );
			if(!publicName.isEmpty() && publicName!=handle)
				c->setProperty( Kopete::Global::Properties::self()->nickName() , publicName );
			else
				c->removeProperty( Kopete::Global::Properties::self()->nickName() );
			c->setProperty( MSNProtocol::protocol()->propGuid, contactGuid );
			// Add the new contact to the group he belongs.
			if ( tmp_addNewContactToGroup.contains(handle) )
			{
				QStringList list = tmp_addNewContactToGroup[handle];
				for ( QStringList::Iterator it = list.begin(); it != list.end(); ++it )
				{
					QString groupGuid = *it;

					// If the group didn't exist yet (yay for async operations), don't add the contact to the group
					// Let slotGroupAdded do it.
					if( m_groupList.contains(groupGuid) )
					{
						kDebug( 14140 ) << k_funcinfo << "Adding " << handle << " to group: " << groupGuid;
						notifySocket()->addContact( handle, MSNProtocol::FL, QString(), contactGuid, groupGuid );

						c->contactAddedToGroup( groupGuid, m_groupList[ groupGuid ] );

						m->addToGroup( m_groupList[ groupGuid ] );

					}
					if ( !m_addWizard_metaContact )
					{
						Kopete::ContactList::self()->addMetaContact( m );
					}
				}
				tmp_addNewContactToGroup.remove(handle);
			}

			c->setOnlineStatus( MSNProtocol::protocol()->FLN );

			m_addWizard_metaContact = 0L;
		}
		if ( !new_contact )
		{
			// Contact has been added to a group
			MSNContact *c = findContactByGuid(contactGuid);
			if(c != 0L)
			{
				// Make sure that the contact has always his contactGUID.
				if( !c->hasProperty(MSNProtocol::protocol()->propGuid.key()) )
					c->setProperty( MSNProtocol::protocol()->propGuid, contactGuid );

				if ( c->onlineStatus() == MSNProtocol::protocol()->UNK )
					c->setOnlineStatus( MSNProtocol::protocol()->FLN );

				if ( c->metaContact() && c->metaContact()->isTemporary() )
					c->metaContact()->setTemporary( false,  m_groupList.contains( groupGuid ) ?  m_groupList[ groupGuid ] : 0L );
				else
				{
					if(m_groupList.contains(groupGuid))
					{
						if( c->metaContact() )
							c->metaContact()->addToGroup( m_groupList[groupGuid] );
						c->contactAddedToGroup( groupGuid, m_groupList[ groupGuid ] );
					}
				}
			}
		}

		if ( !handle.isEmpty() && !m_allowList.contains( handle ) && !m_blockList.contains( handle ) )
		{
			kDebug(14140) << k_funcinfo << "Trying to add contact to AL. ";
			notifySocket()->addContact(handle, MSNProtocol::AL, QString(), QString(), QString() );
		}
	}
	else if ( list == "BL" )
	{
		if ( contacts()[ handle ] )
			static_cast<MSNContact *>( contacts()[ handle ] )->setBlocked( true );
		if ( !m_blockList.contains( handle ) )
		{
			m_blockList.append( handle );
			configGroup()->writeEntry( "blockList" , m_blockList ) ;
		}
	}
	else if ( list == "AL" )
	{
		if ( contacts()[ handle ] )
			static_cast<MSNContact *>( contacts()[ handle ] )->setAllowed( true );
		if ( !m_allowList.contains( handle ) )
		{
			m_allowList.append( handle );
			configGroup()->writeEntry( "allowList" , m_allowList ) ;
		}
	}
	else if ( list == "RL" )
	{
		// search for new Contacts
		Kopete::Contact *ct=contacts()[ handle ];
		if ( !ct || !ct->metaContact() || ct->metaContact()->isTemporary() )
		{
			// Users in the allow list or block list now never trigger the
			// 'new user' dialog, which makes it impossible to add those here.
			// Not necessarily bad, but the usability effects need more thought
			// before I declare it good :- )
			if ( !m_allowList.contains( handle ) && !m_blockList.contains( handle ) )
			{
				QString nick;			//in most case, the public name is not know
				if(publicName!=handle)  // so we don't whos it if it is not know
					nick=publicName;
				Kopete::UI::ContactAddedNotifyDialog *dialog=
						new Kopete::UI::ContactAddedNotifyDialog(  handle,nick,this,
								Kopete::UI::ContactAddedNotifyDialog::InfoButton );
				QObject::connect(dialog,SIGNAL(applyClicked(const QString&)),
								 this,SLOT(slotContactAddedNotifyDialogClosed(const QString& )));
				dialog->show();
			}
		}
		else
		{
			static_cast<MSNContact *>( ct )->setReversed( true );
		}
		m_reverseList.append( handle );
		configGroup()->writeEntry( "reverseList" , m_reverseList ) ;
	}
}

void MSNAccount::slotContactRemoved( const QString& handle, const QString& list, const QString& contactGuid, const QString& groupGuid )
{
	kDebug( 14140 ) << k_funcinfo << "handle: " << handle << " list: " << list << " contact-uid: " << contactGuid;
	MSNContact *c=static_cast<MSNContact *>( contacts()[ handle ] );
	if ( list == "BL" )
	{
		m_blockList.removeAll( handle );
		configGroup()->writeEntry( "blockList" , m_blockList ) ;
		if ( !m_allowList.contains( handle ) )
			notifySocket()->addContact( handle, MSNProtocol::AL, QString(), QString(), QString() );

		if(c)
			c->setBlocked( false );
	}
	else if ( list == "AL" )
	{
		m_allowList.removeAll( handle );
		configGroup()->writeEntry( "allowList" , m_allowList ) ;
		if ( !m_blockList.contains( handle ) )
			notifySocket()->addContact( handle, MSNProtocol::BL, QString(), QString(), QString() );

		if(c)
			c->setAllowed( false );
	}
	else if ( list == "RL" )
	{
		m_reverseList.removeAll( handle );
		configGroup()->writeEntry( "reverseList" , m_reverseList ) ;

		if ( c )
		{
			// Contact is removed from the reverse list
			// only MSN can do this, so this is currently not supported
			c->setReversed( false );
		/*
			InfoWidget *info = new InfoWidget( 0 );
			info->title->setText( "<b>" + i18n( "Contact removed" ) +"</b>" );
			QString dummy;
			dummy = "<center><b>" + imContact->getPublicName() + "( " +imContact->getHandle()  +" )</b></center><br>";
			dummy += i18n( "has removed you from his contact list" ) + "<br>";
			dummy += i18n( "This contact is now removed from your contact list" );
			info->infoText->setText( dummy );
			info->setCaption( "KMerlin - Info" );
			info->show();
		*/
		}
	}
	else if ( list == "FL" )
	{
		// The FL list only use the contact GUID, use the contact referenced by the GUID.
		MSNContact *contactRemoved = findContactByGuid(contactGuid);
		QStringList groupGuidList;
		bool deleteContact = groupGuid.isEmpty() ? true : false; // Delete the contact when the group GUID is empty.
		// Remove the contact from the contact list for all the group he is a member.
		if( groupGuid.isEmpty() )
		{
			if(contactRemoved)
			{
				QList<Kopete::Group*> groupList = contactRemoved->metaContact()->groups();
				QList<Kopete::Group*>::Iterator it, itEnd = groupList.end();
				for( it = groupList.begin(); it != itEnd; ++it )
				{
					Kopete::Group *group = *it;
					if ( !group->pluginData( protocol(), accountId() + " id" ).isEmpty() )
					{
						groupGuidList.append( group->pluginData( protocol(), accountId() + " id" ) );
					}
				}
			}
		}
		else
		{
			groupGuidList.append( groupGuid );
		}

		if( !groupGuidList.isEmpty() )
		{
			QStringList::const_iterator stringIt;
			for( stringIt = groupGuidList.begin(); stringIt != groupGuidList.end(); ++stringIt )
			{
				// Contact is removed from the FL list, remove it from the group
				if(contactRemoved != 0L)
						contactRemoved->contactRemovedFromGroup( *stringIt );

				//check if the group is now empty to remove it
				if ( m_notifySocket )
				{
					bool still_have_contact=false;
					// if contact are contains only in the group we are removing, abort the
					QHash<QString, Kopete::Contact*> contactList = contacts();
					QHash<QString, Kopete::Contact*>::Iterator it, itEnd = contactList.end();
					for ( it = contactList.begin(); it != itEnd; ++it )
					{
						MSNContact *c2 = static_cast<MSNContact *>( it.value() );
						if ( c2->serverGroups().contains( *stringIt ) )
						{
							still_have_contact=true;
							break;
						}
					}
					if(!still_have_contact)
						m_notifySocket->removeGroup( *stringIt );
				}
			}
		}
		if(deleteContact && contactRemoved)
		{
			kDebug(14140) << k_funcinfo << "Deleting the MSNContact " << contactRemoved->contactId();
			contactRemoved->deleteLater();
		}
	}
}

void MSNAccount::slotCreateChat( const QString& address, const QString& auth )
{
	slotCreateChat( 0L, address, auth, m_msgHandle.first(), m_msgHandle.first() );
}

void MSNAccount::slotCreateChat( const QString& ID, const QString& address, const QString& auth,
	const QString& handle_, const QString&  publicName )
{
	QString handle = handle_.toLower();

	if ( handle.isEmpty() )
	{
		// we have lost the handle?
		kDebug(14140) << k_funcinfo << "Impossible to open a chat session, I forgot the contact to invite";
		// forget it
		return;
	}

//	kDebug( 14140 ) << k_funcinfo <<"Creating chat for " << handle;

	if ( !contacts()[ handle ] )
		addContact( handle, publicName, 0L, Kopete::Account::Temporary );

	MSNContact *c = static_cast<MSNContact *>( contacts()[ handle ] );

	if ( c && myself() )
	{
		// we can't use simply c->manager(true) here to get the manager, because this will re-open
		// another chat session, and then, close this new one. We have to re-create the manager manualy.
		MSNChatSession *manager = dynamic_cast<MSNChatSession*>( c->manager( Kopete::Contact::CannotCreate ) );
		if(!manager)
		{
			Kopete::ContactPtrList chatmembers;
			chatmembers.append(c);
			manager = new MSNChatSession( protocol(), myself(), chatmembers  );
		}

		manager->createChat( handle, address, auth, ID );

		/**
		 *  This code should open a chat window when a socket is open
		 * It has been disabled because gaim open switchboeard too often
		 *
		 * the solution is to open the window only when the contact start typing
		 * see MSNChatSession::receivedTypingMsg
		 *

		KGlobal::config()->setGroup( "MSN" );
		bool notifyNewChat = KGlobal::config()->readEntry( "NotifyNewChat", false );
		if ( !ID.isEmpty() && notifyNewChat )
		{
			// this temporary message should open the window if they not exist
			QString body = i18n( "%1 has started a chat with you" ,c->metaContact()->displayName() );
			Kopete::Message tmpMsg = Kopete::Message( c, manager->members(), body, Kopete::Message::Internal, Kopete::Message::PlainText );
			manager->appendMessage( tmpMsg );
		}
		 */
	}

	if(!m_msgHandle.isEmpty())
		m_msgHandle.pop_front();
}

void MSNAccount::slotStartChatSession( const QString& handle )
{
	// First create a message manager, because we might get an existing
	// manager back, in which case we likely also have an active switchboard
	// connection to reuse...

	MSNContact *c = static_cast<MSNContact *>( contacts()[ handle ] );
	// if ( isConnected() && c && myself() && handle != m_msnId )
	if ( m_notifySocket && c && myself() && handle != accountId() )
	{
		if ( !c->manager(Kopete::Contact::CannotCreate) || !static_cast<MSNChatSession *>( c->manager( Kopete::Contact::CanCreate ) )->service() )
		{
			m_msgHandle.prepend(handle);
			m_notifySocket->createChatSession();
		}
	}
}

void MSNAccount::slotContactAddedNotifyDialogClosed(const QString& handle)
{
	const Kopete::UI::ContactAddedNotifyDialog *dialog =
			dynamic_cast<const Kopete::UI::ContactAddedNotifyDialog *>(sender());
	if(!dialog || !m_notifySocket)
		return;

	if(dialog->added())
	{
		Kopete::MetaContact *mc=dialog->addContact();
		if(mc)
		{ //if the contact has been added this way, it's because the other user added us.
		  // don't forgot to set the reversed flag  (Bug 114400)
			MSNContact *c=dynamic_cast<MSNContact*>(mc->contacts().first());
			if(c && c->contactId() == handle )
			{
				c->setReversed( true );
			}
		}
	}

	if ( !dialog->authorized() )
	{
		if ( m_allowList.contains( handle ) )
			m_notifySocket->removeContact( handle, MSNProtocol::AL, QString(), QString() );
		else if ( !m_blockList.contains( handle ) )
			m_notifySocket->addContact( handle, MSNProtocol::BL, QString(), QString(), QString() );
	}
	else
	{
		if ( m_blockList.contains( handle ) )
			m_notifySocket->removeContact( handle, MSNProtocol::BL, QString(), QString() );
		else if ( !m_allowList.contains( handle ) )
			m_notifySocket->addContact( handle, MSNProtocol::AL, QString(), QString(), QString() );
	}


}

void MSNAccount::slotErrorMessageReceived( int type, const QString &msg )
{
	KMessageBox::DialogType msgBoxType;
	QString caption = i18n( "MSN Plugin" );

	switch(type)
	{
		case MSNSocket::ErrorNormal:
		{
			msgBoxType = KMessageBox::Error;
			break;
		}
		case MSNSocket::ErrorSorry:
		{
			msgBoxType = KMessageBox::Sorry;
			break;
		}
		case MSNSocket::ErrorInformation:
		{
			msgBoxType = KMessageBox::Information;
			break;
		}
		case MSNSocket::ErrorInternal:
		{
			msgBoxType = KMessageBox::Information;
			caption = i18n( "MSN Internal Error" );
			break;
		}
		default:
		{
			msgBoxType = KMessageBox::Error;
			break;
		}
	}

	kDebug(14140) << k_funcinfo << msg;
	// Display the error
	KMessageBox::queuedMessageBox( Kopete::UI::Global::mainWidget(), msgBoxType, msg, caption );

}

bool MSNAccount::createContact( const QString &contactId, Kopete::MetaContact *metaContact )
{
	if ( !metaContact->isTemporary() && m_notifySocket)
	{
		m_addWizard_metaContact = metaContact;

		addContactServerside(contactId, metaContact->groups());

		// FIXME: Find out if this contact was really added or not!
		return true;
	}
	else
	{
		// This is a temporary contact. ( a person who messaged us but is not on our conntact list.
		// We don't want to create it on the server.Just create the local contact object and add it
		// Or we are diconnected, and in that case, the contact will be added when connecting
		MSNContact *newContact = new MSNContact( this, contactId, metaContact );
		newContact->setDeleted(true);
		return true;
	}

}

void MSNAccount::addContactServerside(const QString &contactId, QList<Kopete::Group*> groupList)
{
	// First of all, fill the temporary group list. The contact will be moved to his group(s).
	// When we receive back his contact GUID(required to move a contact between groups)
	Kopete::Group *group;
	foreach( group, groupList )
	{
		// TODO: It it time that libkopete generate a unique ID that contains protocols, account and contact id.
		QString groupId  = group->pluginData( protocol(), accountId() + " id" );
		// If the groupId is empty, that's mean the Kopete group is not on the MSN server.
		if( !groupId.isEmpty() )
		{
			// Something got corrupted on contact list.xml
			if( !m_groupList.contains(groupId) )
			{
				// Clear the group plugin data.
				group->setPluginData( protocol() , accountId() + " id" , QString());
				group->setPluginData( protocol() , accountId() + " displayName" , QString());
				kDebug( 14140 ) << k_funcinfo << " Group " << group->displayName() << " marked with id #" << groupId << " does not seems to be anymore on the server";

				// Add the group on MSN server, will fix the corruption.
				kDebug(14140) << k_funcinfo << "Fixing group corruption, re-adding " << group->displayName() << "to the server.";
				addGroup( group->displayName(), contactId);
			}
			else
			{
				// Add the group that the contact belong to add it when we will receive the contact GUID.
				if( tmp_addNewContactToGroup.contains( contactId ) )
					tmp_addNewContactToGroup[contactId].append(groupId);
				else
					tmp_addNewContactToGroup.insert(contactId, QStringList(groupId) );
			}
		}
		else
		{
			if( !group->displayName().isEmpty() && group->type() == Kopete::Group::Normal )
			{
				kDebug(14140) << k_funcinfo << "Group not on MSN server, add it";
				addGroup( group->displayName(), contactId );
			}
		}
	}

	// After add the contact to the top-level, it will be moved to required groups later.
	kDebug( 14140 ) << k_funcinfo << "Add the contact on the server ";
	m_notifySocket->addContact( contactId, MSNProtocol::FL, contactId, QString(), QString() );
}

MSNContact *MSNAccount::findContactByGuid(const QString &contactGuid)
{
	kDebug(14140) << k_funcinfo << "Looking for " << contactGuid;
	QHash<QString, Kopete::Contact*> contactList = contacts();
	QHash<QString, Kopete::Contact*>::Iterator it, itEnd = contactList.end();
	for ( it = contactList.begin(); it != itEnd; ++it )
	{
		MSNContact *c = dynamic_cast<MSNContact *>( it.value() );

		if(c && c->guid() == contactGuid )
		{
			kDebug(14140) << k_funcinfo << "OK found a contact. ";
			// Found the contact GUID
			return c;
		}
	}

	return 0L;
}

bool MSNAccount::isHotmail() const
{
	if ( !m_openInboxAction )
		return false;
	return m_openInboxAction->isEnabled();
}

QString MSNAccount::pictureUrl()
{
	return m_pictureFilename;
}

void MSNAccount::setPictureUrl(const QString &url)
{
	m_pictureFilename = url;
}

QString MSNAccount::pictureObject()
{
	if(m_pictureObj.isNull())
		resetPictureObject(true); //silent=true to keep infinite loop away
	return m_pictureObj;
}

void MSNAccount::resetPictureObject(bool silent)
{
	QString old=m_pictureObj;

	if(!configGroup()->readEntry("exportCustomPicture", false))
	{
		m_pictureObj="";
		myself()->removeProperty( Kopete::Global::Properties::self()->photo() );
	}
	else
	{
		QFile pictFile( m_pictureFilename );
		if (!pictFile.open(QIODevice::ReadOnly))
		{
			m_pictureObj="";
			myself()->removeProperty( Kopete::Global::Properties::self()->photo() );
		}
		else
		{
			QByteArray ar = pictFile.readAll();

			QString sha1d= QString((KCodecs::base64Encode(SHA1::hash(ar))));

			QString size=QString::number( pictFile.size() );
			QString all= "Creator"+accountId()+"Size"+size+"Type3Locationkopete.tmpFriendlyAAA=SHA1D"+ sha1d;
			m_pictureObj="<msnobj Creator=\"" + accountId() + "\" Size=\"" + size  + "\" Type=\"3\" Location=\"kopete.tmp\" Friendly=\"AAA=\" SHA1D=\""+sha1d+"\" SHA1C=\""+ QString(KCodecs::base64Encode(SHA1::hashString(all.toUtf8())))  +"\"/>";
			myself()->setProperty( Kopete::Global::Properties::self()->photo() , m_pictureFilename );
		}
	}

	if(old!=m_pictureObj && isConnected() && m_notifySocket && !silent)
	{
		//update the msn pict
		m_notifySocket->setStatus( myself()->onlineStatus() );
	}
}

#include "msnaccount.moc"

// vim: set noet ts=4 sts=4 sw=4:

