/*
    kopeteaccount.cpp - Kopete Account

    Copyright (c) 2003-2005 by Olivier Goffart       <ogoffart@kde.org>
    Copyright (c) 2003-2004 by Martijn Klingens      <klingens@kde.org>
    Copyright (c) 2004      by Richard Smith         <kde@metafoo.co.uk>
    Kopete    (c) 2002-2005 by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This library is free software; you can redistribute it and/or         *
    * modify it under the terms of the GNU Lesser General Public            *
    * License as published by the Free Software Foundation; either          *
    * version 2 of the License, or (at your option) any later version.      *
    *                                                                       *
    *************************************************************************
*/

#include <qapplication.h>
#include <QTimer>
#include <QPixmap>
#include <QIcon>

#include <kconfig.h>
#include <kdebug.h>
#include <kdialog.h>
#include <kdeversion.h>
#include <klocale.h>
#include <kiconloader.h>
#include <kiconeffect.h>
#include <kicon.h>
#include <kaction.h>
#include <kmenu.h>
#include <kmessagebox.h>
#include <kcomponentdata.h>
#include <kmenu.h>
#include <kactionmenu.h>
#include <kconfiggroup.h>

#include "kopeteaccount.h"
#include "kopeteidentity.h"
#include "kopeteidentitymanager.h"
#include "kabcpersistence.h"
#include "kopetecontactlist.h"
#include "kopeteaccountmanager.h"
#include "kopetecontact.h"
#include "kopetemetacontact.h"
#include "kopeteprotocol.h"
#include "kopetepluginmanager.h"
#include "kopetegroup.h"
#include "kopetebehaviorsettings.h"
#include "kopeteutils.h"
#include "kopeteuiglobal.h"
#include "kopeteblacklister.h"
#include "kopeteonlinestatusmanager.h"
#include "editaccountwidget.h"

namespace Kopete
{


class Account::Private
{
public:
	Private( Protocol *protocol, const QString &accountId )
	 : protocol( protocol ), id( accountId )
	 , excludeconnect( true ), priority( 0 )
	 , connectionTry(0), identity( 0 ), myself( 0 )
	 , suppressStatusTimer( 0 ), suppressStatusNotification( false )
	 , blackList( new Kopete::BlackLister( protocol->pluginId(), accountId ) )
	{ }


	~Private() { delete blackList; }

	Protocol *protocol;
	QString id;
	QString accountLabel;
	bool excludeconnect;
	uint priority;
	QHash<QString, Contact*> contacts;
	QColor color;
	uint connectionTry;
	Identity *identity;
	Contact *myself;
	QTimer suppressStatusTimer;
	bool suppressStatusNotification;
	Kopete::BlackLister *blackList;
	KConfigGroup *configGroup;
	QString customIcon;
	Kopete::OnlineStatus restoreStatus;
	QString restoreMessage;
};

Account::Account( Protocol *parent, const QString &accountId )
 : QObject( parent ), d( new Private( parent, accountId ) )
{
	d->configGroup=new KConfigGroup(KGlobal::config(), QString::fromLatin1( "Account_%1_%2" ).arg( d->protocol->pluginId(), d->id ));

	d->excludeconnect = d->configGroup->readEntry( "ExcludeConnect", false );
	d->color = d->configGroup->readEntry( "Color" , QColor() );
	d->customIcon = d->configGroup->readEntry( "Icon", QString() );
	d->priority = d->configGroup->readEntry( "Priority", 0 );

	Identity *identity = Kopete::IdentityManager::self()->findIdentity( d->configGroup->readEntry("Identity", QString()) );

	// if the identity was not found, use the default one which will for sure exist
	// FIXME: maybe it could show a passive dialog telling that to the user
	if (!identity)
		identity = Kopete::IdentityManager::self()->defaultIdentity();

	setIdentity( identity );

	d->restoreStatus = Kopete::OnlineStatus::Online;
	d->restoreMessage = "";

	QObject::connect( &d->suppressStatusTimer, SIGNAL( timeout() ),
		this, SLOT( slotStopSuppression() ) );
}

Account::~Account()
{
	d->contacts.remove( d->myself->contactId() );
	// Delete all registered child contacts first
	foreach (Contact* c, d->contacts) QObject::disconnect(c, SIGNAL( contactDestroyed( Kopete::Contact * ) ), this, 0);
	qDeleteAll(d->contacts);
	d->contacts.clear();

	kDebug( 14010 ) << k_funcinfo << " account '" << d->id << "' about to emit accountDestroyed ";
	emit accountDestroyed(this);

	delete d->myself;
	delete d->configGroup;
	delete d;
}

void Account::reconnect()
{
	kDebug( 14010 ) << k_funcinfo << "account " << d->id << " restoreStatus " << d->restoreStatus.status() << " restoreMessage " << d->restoreMessage;
	setOnlineStatus( d->restoreStatus, d->restoreMessage );
}

void Account::disconnected( DisconnectReason reason )
{
	kDebug( 14010 ) << k_funcinfo << reason;
	//reconnect if needed
	if(reason == BadPassword )
	{
		QTimer::singleShot(0, this, SLOT(reconnect()));
	}
	else if ( Kopete::BehaviorSettings::self()->reconnectOnDisconnect() == true && reason > Manual )
	{
		d->connectionTry++;
		//use a timer to allow the plugins to clean up after return
		if(d->connectionTry < 3)
			QTimer::singleShot(10000, this, SLOT(reconnect())); // wait 10 seconds before reconnect
	}
	if(reason== OtherClient)
	{
		Kopete::Utils::notifyConnectionLost(this, i18n("You have been disconnected"), i18n( "You have connected from another client or computer to the account '%1'" , d->id), i18n("Most proprietary Instant Messaging services do not allow you to connect from more than one location. Check that nobody is using your account without your permission. If you need a service that supports connection from various locations at the same time, use the Jabber protocol."));
	}
}

Protocol *Account::protocol() const
{
	return d->protocol;
}

QString Account::accountId() const
{
	return d->id;
}

const QColor Account::color() const
{
	return d->color;
}

void Account::setColor( const QColor &color )
{
	d->color = color;
	if ( d->color.isValid() )
		d->configGroup->writeEntry( "Color", d->color );
	else
		d->configGroup->deleteEntry( "Color" );
	emit colorChanged( color );
}

void Account::setPriority( uint priority )
{
 	d->priority = priority;
	d->configGroup->writeEntry( "Priority", d->priority );
}

uint Account::priority() const
{
	return d->priority;
}


QPixmap Account::accountIcon(const int size) const
{
	QString icon= d->customIcon.isEmpty() ? d->protocol->pluginIcon() : d->customIcon;

	// FIXME: this code is duplicated with OnlineStatus, can we merge it somehow?
	QPixmap base = KIconLoader::global()->loadIcon(
		icon, K3Icon::Small, size );

	if ( d->color.isValid() )
	{
		KIconEffect effect;
		base = effect.apply( base, KIconEffect::Colorize, 1, d->color, 0);
	}

	if ( size > 0 && base.width() != size )
	{
		base.scaled( size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
	}

	return base;
}

KConfigGroup* Kopete::Account::configGroup() const
{
	return d->configGroup;
}

void Account::setAccountLabel( const QString &label )
{
	d->accountLabel = label;
}

QString Account::accountLabel() const
{
	if( d->accountLabel.isNull() )
		return d->id;
	return d->accountLabel;
}

void Account::setExcludeConnect( bool b )
{
	d->excludeconnect = b;
	d->configGroup->writeEntry( "ExcludeConnect", d->excludeconnect );
}

bool Account::excludeConnect() const
{
	return d->excludeconnect;
}

void Account::registerContact( Contact *c )
{
	d->contacts.insert( c->contactId(), c );
	QObject::connect( c, SIGNAL( contactDestroyed( Kopete::Contact * ) ),
		SLOT( contactDestroyed( Kopete::Contact * ) ) );
}

void Account::contactDestroyed( Contact *c )
{
	d->contacts.remove( c->contactId() );
}


const QHash<QString, Contact*>& Account::contacts()
{
	return d->contacts;
}


Kopete::MetaContact* Account::addContact( const QString &contactId, const QString &displayName , Group *group, AddMode mode  )
{

	if ( contactId == d->myself->contactId() )
	{
		KMessageBox::queuedMessageBox( Kopete::UI::Global::mainWidget(), KMessageBox::Error,
			i18n("You are not allowed to add yourself to the contact list. The addition of \"%1\" to account \"%2\" will not take place.", contactId, accountId()), i18n("Error Creating Contact")
		);
		return false;
	}

	bool isTemporary = mode == Temporary;

	Contact *c = d->contacts[ contactId ];

	if(!group)
		group=Group::topLevel();

	if ( c && c->metaContact() )
	{
		if ( c->metaContact()->isTemporary() && !isTemporary )
		{
			kDebug( 14010 ) << k_funcinfo <<  " You are trying to add an existing temporary contact. Just add it on the list";

			c->metaContact()->setTemporary(false, group );
			ContactList::self()->addMetaContact(c->metaContact());
		}
		else
		{
			// should we here add the contact to the parentContact if any?
			kDebug( 14010 ) << k_funcinfo << "Contact already exists";
		}
		return c->metaContact();
	}

	MetaContact *parentContact = new MetaContact();
	if(!displayName.isEmpty())
		parentContact->setDisplayName( displayName );

	//Set it as a temporary contact if requested
	if ( isTemporary )
		parentContact->setTemporary( true );
	else
		parentContact->addToGroup( group );

	if ( c )
	{
		c->setMetaContact( parentContact );
		if ( mode == ChangeKABC )
		{
			kDebug( 14010 ) << k_funcinfo << " changing KABC";
			KABCPersistence::self()->write( parentContact );
		}
	}
	else
	{
		if ( !createContact( contactId, parentContact ) )
		{
			delete parentContact;
			return 0L;
		}
	}

	ContactList::self()->addMetaContact( parentContact );
	return parentContact;
}

bool Account::addContact(const QString &contactId , MetaContact *parent, AddMode mode )
{
	if ( contactId == myself()->contactId() )
	{
	    	KMessageBox::error( Kopete::UI::Global::mainWidget(),
			i18n("You are not allowed to add yourself to the contact list. The addition of \"%1\" to account \"%2\" will not take place.", contactId, accountId()), i18n("Error Creating Contact")
		);
		return 0L;
	}

	bool isTemporary= parent->isTemporary();
	Contact *c = d->contacts[ contactId ];
	if ( c && c->metaContact() )
	{
		if ( c->metaContact()->isTemporary() && !isTemporary )
		{
			kDebug( 14010 ) <<
				"Account::addContact: You are trying to add an existing temporary contact. Just add it on the list" << endl;

				//setMetaContact ill take care about the deletion of the old contact
			c->setMetaContact(parent);
			return true;
		}
		else
		{
			// should we here add the contact to the parentContact if any?
			kDebug( 14010 ) << "Account::addContact: Contact already exists";
		}
		return false; //(the contact is not in the correct metacontact, so false)
	}

	bool success = createContact(contactId, parent);

	if ( success && mode == ChangeKABC )
	{
		kDebug( 14010 ) << k_funcinfo << " changing KABC";
		KABCPersistence::self()->write( parent );
	}

	return success;
}

KActionMenu * Account::actionMenu()
{
	//default implementation
// 	KActionMenu *menu = new KActionMenu( QIcon(myself()->onlineStatus().iconFor( this )), accountId(), 0, 0);
	KActionMenu *menu = new KActionMenu( accountId(), this );
#ifdef __GNUC__
#warning No icon shown, we should go away from QPixmap genered icons with overlays.
#endif
	QString nick;
       	if (identity()->hasProperty( Kopete::Global::Properties::self()->nickName().key() ))
		nick = identity()->property( Kopete::Global::Properties::self()->nickName() ).value().toString();
	else
		nick = myself()->nickName();

	menu->menu()->addTitle( myself()->onlineStatus().iconFor( myself() ),
		nick.isNull() ? accountLabel() : i18n( "%2 <%1>", accountLabel(), nick )
	);

	OnlineStatusManager::self()->createAccountStatusActions(this, menu);
	menu->menu()->addSeparator();

	KAction *propertiesAction = new KAction( i18n("Properties"), menu );
	QObject::connect( propertiesAction, SIGNAL(triggered(bool)), this, SLOT( editAccount() ) );
	menu->addAction( propertiesAction );

	return menu;
}


bool Account::isConnected() const
{
	return myself() && myself()->isOnline();
}

bool Account::isAway() const
{
	return d->myself && ( d->myself->onlineStatus().status() == Kopete::OnlineStatus::Away );
}
Identity * Account::identity() const
{
	return d->identity;
}

void Account::setIdentity( Identity *ident )
{
	if (d->identity)
	{
		d->identity->removeAccount( this );
	}

	ident->addAccount( this );
	d->identity = ident;
	d->configGroup->writeEntry("Identity", ident->identityId());
}

Contact * Account::myself() const
{
	return d->myself;
}

void Account::setMyself( Contact *myself )
{
	//FIXME  does it make sens to change the myself contact to another ?   - Olivier 2005-11-21

	bool wasConnected = isConnected();

	if ( d->myself )
	{
		QObject::disconnect( d->myself, SIGNAL( onlineStatusChanged( Kopete::Contact *, const Kopete::OnlineStatus &, const Kopete::OnlineStatus & ) ),
			this, SLOT( slotOnlineStatusChanged( Kopete::Contact *, const Kopete::OnlineStatus &, const Kopete::OnlineStatus & ) ) );
		QObject::disconnect( d->myself, SIGNAL( propertyChanged( Kopete::Contact *, const QString &, const QVariant &, const QVariant & ) ),
			this, SLOT( slotContactPropertyChanged( Kopete::Contact *, const QString &, const QVariant &, const QVariant & ) ) );
	}

	d->myself = myself;

//	d->contacts.remove( myself->contactId() );

	QObject::connect( d->myself, SIGNAL( onlineStatusChanged( Kopete::Contact *, const Kopete::OnlineStatus &, const Kopete::OnlineStatus & ) ),
		this, SLOT( slotOnlineStatusChanged( Kopete::Contact *, const Kopete::OnlineStatus &, const Kopete::OnlineStatus & ) ) );
	QObject::connect( d->myself, SIGNAL( propertyChanged( Kopete::Contact *, const QString &, const QVariant &, const QVariant & ) ),
		this, SLOT( slotContactPropertyChanged( Kopete::Contact *, const QString &, const QVariant &, const QVariant & ) ) );

	QObject::connect( d->myself, SIGNAL( onlineStatusChanged( Kopete::Contact *, const Kopete::OnlineStatus &, const Kopete::OnlineStatus & ) ),
		identity(), SLOT( updateOnlineStatus()));

	if ( isConnected() != wasConnected )
		emit isConnectedChanged();
}

void Account::slotOnlineStatusChanged( Contact * /* contact */,
	const OnlineStatus &newStatus, const OnlineStatus &oldStatus )
{
	bool wasOffline = !oldStatus.isDefinitelyOnline();
	bool isOffline  = !newStatus.isDefinitelyOnline();

	if ( wasOffline || newStatus.status() == OnlineStatus::Offline )
	{
		// Wait for five seconds until we treat status notifications for contacts
		// as unrelated to our own status change.
		// Five seconds may seem like a long time, but just after your own
		// connection it's basically neglectible, and depending on your own
		// contact list's size, the protocol you are using, your internet
		// connection's speed and your computer's speed you *will* need it.
		d->suppressStatusNotification = true;
		d->suppressStatusTimer.setSingleShot( true );
		d->suppressStatusTimer.start( 5000 );
		//the timer is also used to reset the d->connectionTry
	}

	if ( !isOffline )
	{
		d->restoreStatus = newStatus;
		d->restoreMessage = identity()->property( Kopete::Global::Properties::self()->statusMessage() ).value().toString();
//		kDebug( 14010 ) << k_funcinfo << "account " << d->id << " restoreStatus " << d->restoreStatus.status() << " restoreMessage " << d->restoreMessage;
	}

/*	kDebug(14010) << k_funcinfo << "account " << d->id << " changed status. was "
	               << Kopete::OnlineStatus::statusTypeToString(oldStatus.status()) << ", is "
	               << Kopete::OnlineStatus::statusTypeToString(newStatus.status()) << endl;*/
	if ( wasOffline != isOffline )
		emit isConnectedChanged();
}

void Account::setAllContactsStatus( const Kopete::OnlineStatus &status )
{
	d->suppressStatusNotification = true;
	d->suppressStatusTimer.setSingleShot( true );
	d->suppressStatusTimer.start( 5000 );

	QHashIterator<QString, Contact*> it(d->contacts);
	for (  ; it.hasNext(); ) {
		it.next();
		if ( it.value() != d->myself )
			it.value()->setOnlineStatus( status );
	}
}

void Account::slotContactPropertyChanged( Contact * /* contact */,
	const QString &key, const QVariant &old, const QVariant &newVal )
{
	if ( key == Kopete::Global::Properties::self()->statusMessage().key() && old != newVal && isConnected() )
	{
		d->restoreMessage = newVal.toString();
//		kDebug( 14010 ) << k_funcinfo << "account " << d->id << " restoreMessage " << d->restoreMessage;
	}
}

void Account::slotStopSuppression()
{
	d->suppressStatusNotification = false;
	if(isConnected())
		d->connectionTry=0;
}

bool Account::suppressStatusNotification() const
{
	return d->suppressStatusNotification;
}

bool Account::removeAccount()
{
	//default implementation
	return true;
}


BlackLister* Account::blackLister()
{
	return d->blackList;
}

void Account::block( const QString &contactId )
{
	d->blackList->addContact( contactId );
}

void Account::unblock( const QString &contactId )
{
	d->blackList->removeContact( contactId );
}

bool Account::isBlocked( const QString &contactId )
{
	return d->blackList->isBlocked( contactId );
}

void Account::editAccount(QWidget *parent)
{
	KDialog *editDialog = new KDialog( parent );
	editDialog->setCaption( i18n( "Edit Account" ) );
	editDialog->setButtons( KDialog::Ok | KDialog::Apply | KDialog::Cancel );

	KopeteEditAccountWidget *m_accountWidget = protocol()->createEditAccountWidget( this, editDialog );
	if ( !m_accountWidget )
		return;

	// FIXME: Why the #### is EditAccountWidget not a QWidget?!? This sideways casting
	//        is braindead and error-prone. Looking at MSN the only reason I can see is
	//        because it allows direct subclassing of designer widgets. But what is
	//        wrong with embedding the designer widget in an empty QWidget instead?
	//        Also, if this REALLY has to be a pure class and not a widget, then the
	//        class should at least be renamed to EditAccountIface instead - Martijn
	QWidget *w = dynamic_cast<QWidget *>( m_accountWidget );
	if ( !w )
		return;

	editDialog->setMainWidget( w );
	if ( editDialog->exec() == QDialog::Accepted )
	{
		if( m_accountWidget->validateData() )
			m_accountWidget->apply();
	}

	editDialog->deleteLater();
}

void Account::setCustomIcon( const QString & i)
{
	d->customIcon = i;
	if(!i.isEmpty())
		d->configGroup->writeEntry( "Icon", i );
	else
		d->configGroup->deleteEntry( "Icon" );
	emit colorChanged( color() );
}

QString Account::customIcon()  const
{
	return d->customIcon;
}

} // END namespace Kopete

#include "kopeteaccount.moc"
