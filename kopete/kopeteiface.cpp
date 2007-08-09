/*
    kopeteiface.cpp - Kopete DCOP Interface

    Copyright (c) 2002 by Hendrik vom Lehn       <hennevl@hennevl.de>

    Kopete    (c) 2002-2003      by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include <kmessagebox.h>
#include <klocale.h>
#include <kconfig.h>
#include <kglobal.h>

#include "kopeteiface.h"
#include "kopetemetacontact.h"
#include "kopetecontactlist.h"
#include "kopeteaccount.h"
#include "kopeteaccountmanager.h"
#include "kopetepluginmanager.h"
#include "kopeteprotocol.h"
#include "kopeteuiglobal.h"
#include "kopeteaway.h"
#include "kopetegroup.h"
#include "kopetecontact.h"
#include "kopetegeneralsettings.h"
#include "kopetebehaviorsettings.h"

KopeteIface::KopeteIface() : DCOPObject( "KopeteIface" )
{
	if (Kopete::BehaviorSettings::self()->useAutoAway())
	{
		connectDCOPSignal("kdesktop", "KScreensaverIface",
			"KDE_start_screensaver()", "setAutoAway()", false);
	}
	else
	{
		disconnectDCOPSignal("kdesktop", "KScreensaverIface",
			"KDE_start_screensaver()", "setAutoAway()");
	}
	// FIXME: AFAICT, this never seems to fire.
	connectDCOPSignal("kdesktop", "KScreensaverIface",
		"KDE_stop_screensaver()", "setActive()", false);
}

QStringList KopeteIface::contacts()
{
	return Kopete::ContactList::self()->contacts();
}

QStringList KopeteIface::reachableContacts()
{
	return Kopete::ContactList::self()->reachableContacts();
}

QStringList KopeteIface::onlineContacts()
{
	QStringList result;
	QListIterator<Kopete::Contact*> it(Kopete::ContactList::self()->onlineContacts());
	while ( it.hasNext() )
		result.append( it.next()->contactId() );

	return result;
}

QStringList KopeteIface::contactsStatus()
{
	return Kopete::ContactList::self()->contactStatuses();
}

QStringList KopeteIface::fileTransferContacts()
{
	return Kopete::ContactList::self()->fileTransferContacts();
}

QStringList KopeteIface::contactFileProtocols(const QString &displayName)
{
	return Kopete::ContactList::self()->contactFileProtocols(displayName);
}

QString KopeteIface::messageContact( const QString &contactId, const QString &messageText )
{
	Kopete::MetaContact *mc = Kopete::ContactList::self()->findMetaContactByContactId( contactId );
	if ( !mc )
	{
		return "No such contact.";
	}

	if ( mc->isReachable() )
		Kopete::ContactList::self()->messageContact( contactId, messageText );
	else
		return "The contact is not reachable";
	
	//Default return value
	return QString();
}
/*
void KopeteIface::sendFile(const QString &displayName, const KUrl &sourceURL,
	const QString &altFileName, uint fileSize)
{
	return Kopete::ContactList::self()->sendFile(displayName, sourceURL, altFileName, fileSize);
}

*/

QString KopeteIface::onlineStatus( const QString &metaContactId )
{
	Kopete::MetaContact *m = Kopete::ContactList::self()->metaContact( metaContactId );
	if( m )
	{
		Kopete::OnlineStatus status = m->status();
		return status.description();
	}

	return "Unknown Contact";
}

void KopeteIface::messageContactById( const QString &metaContactId )
{
	Kopete::MetaContact *m = Kopete::ContactList::self()->metaContact( metaContactId );
	if( m )
	{
		m->execute();
	}
}

bool KopeteIface::addContact( const QString &protocolName, const QString &accountId, const QString &contactId,
	const QString &displayName, const QString &groupName )
{
		//Get the protocol instance
	Kopete::Account *myAccount = Kopete::AccountManager::self()->findAccount( protocolName, accountId );

	if( myAccount )
	{
		QString contactName;
		Kopete::Group *realGroup=0L;
		//If the nickName isn't specified we need to display the userId in the prompt
		if( displayName.isEmpty() || displayName.isNull() )
			contactName = contactId;
		else
			contactName = displayName;

		if ( !groupName.isEmpty() )
			realGroup=Kopete::ContactList::self()->findGroup( groupName );

		// Confirm with the user before we add the contact
		// FIXME: This is completely bogus since the user may not
		// even be at the computer. We just need to add the contact --Matt
		if( KMessageBox::questionYesNo( Kopete::UI::Global::mainWidget(), i18n( "An external application is attempting to add the "
			" '%1' contact '%2' to your contact list. Do you want to allow this?" ,
			  protocolName, contactName ), i18n( "Allow Contact?" ), i18n("Allow"), i18n("Reject") ) == 3 ) // Yes == 3
		{
			//User said Yes
			myAccount->addContact( contactId, contactName, realGroup, Kopete::Account::DontChangeKABC);
			return true;
		} else {
			//User said No
			return false;
		}

	} else {
		//This protocol is not loaded
		KMessageBox::queuedMessageBox( Kopete::UI::Global::mainWidget(), KMessageBox::Error,
				 i18n("An external application has attempted to add a contact using "
				      " the %1 protocol, which either does not exist or is not loaded.", protocolName ),
				i18n("Missing Protocol"));

		return false;
	}
}

QStringList KopeteIface::accounts()
{
	QStringList list;
	QListIterator<Kopete::Account *> it( Kopete::AccountManager::self()->accounts() );
	Kopete::Account *account;
	while ( it.hasNext() )
	{
		account = it.next();
		list += ( account->protocol()->pluginId() +"||" + account->accountId() );
	}
	return list;
}

void KopeteIface::connect(const QString &protocolId, const QString &accountId )
{
	QListIterator<Kopete::Account *> it( Kopete::AccountManager::self()->accounts() );
	Kopete::Account *account;
	while ( it.hasNext() )
	{
		account = it.next();
		if( ( account->accountId() == accountId) )
		{
			if( protocolId.isEmpty() || account->protocol()->pluginId() == protocolId )
			{
				account->connect();
				break;
			}
		}
	}
}

void KopeteIface::disconnect(const QString &protocolId, const QString &accountId )
{
	QListIterator<Kopete::Account *> it( Kopete::AccountManager::self()->accounts() );
	Kopete::Account *account;
	while ( it.hasNext() )
	{
		account = it.next();
		if( ( account->accountId() == accountId) )
		{
			if( protocolId.isEmpty() || account->protocol()->pluginId() == protocolId )
			{
				account->disconnect();
				break;
			}
		}
	}
}

void KopeteIface::connectAll()
{
	Kopete::AccountManager::self()->connectAll();
}

void KopeteIface::disconnectAll()
{
	Kopete::AccountManager::self()->disconnectAll();
}

bool KopeteIface::loadPlugin( const QString &name )
{
	if ( Kopete::PluginManager::self()->setPluginEnabled( name ) )
	{
		QString argument = name;
		if ( !argument.startsWith( "kopete_" ) )
			argument.prepend( "kopete_" );
		return Kopete::PluginManager::self()->loadPlugin( argument );
	}
	else
	{
		return false;
	}
}

bool KopeteIface::unloadPlugin( const QString &name )
{
	if ( Kopete::PluginManager::self()->setPluginEnabled( name, false ) )
	{
		QString argument = name;
		if ( !argument.startsWith( "kopete_" ) )
			argument.prepend( "kopete_" );
		return Kopete::PluginManager::self()->unloadPlugin( argument );
	}
	else
	{
		return false;
	}
}

void KopeteIface::setAway()
{
	Kopete::AccountManager::self()->setAwayAll();
}

void KopeteIface::setAway(const QString &msg, bool away)
{
	Kopete::AccountManager::self()->setAwayAll(msg, away);
}

void KopeteIface::setAvailable()
{
	Kopete::AccountManager::self()->setAvailableAll();
}

void KopeteIface::setAutoAway()
{
	Kopete::Away::getInstance()->setAutoAway();
}

// vim: set noet ts=4 sts=4 sw=4:
