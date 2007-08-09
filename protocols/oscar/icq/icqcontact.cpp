/*
  icqontact.cpp  -  Oscar Protocol Plugin

  Copyright (c) 2003      by Stefan Gehn  <metz@gehn.net>
  Copyright (c) 2003      by Olivier Goffart <oggoffart@kde.org>
  Copyright (c) 2006,2007 by Roman Jarosz <kedgedev@centrum.cz>

  Kopete    (c) 2003-2007 by the Kopete developers  <kopete-devel@kde.org>

  *************************************************************************
  *                                                                       *
  * This program is free software; you can redistribute it and/or modify  *
  * it under the terms of the GNU General Public License as published by  *
  * the Free Software Foundation; either version 2 of the License, or     *
  * (at your option) any later version.                                   *
  *                                                                       *
  *************************************************************************
*/

#include "icqcontact.h"

#include <qtimer.h>
#include <klocale.h>
#include <knotification.h>
#include <kinputdialog.h>
#include <krandom.h>
#include <ktoggleaction.h>
#include <kicon.h>

#include "kopeteuiglobal.h"
#include "kopetemetacontact.h"

#include "icqprotocol.h"
#include "icqaccount.h"
#include "icquserinfowidget.h"
#include "icqauthreplydialog.h"

#include "oscarutils.h"
#include "contactmanager.h"
#include "oscarstatusmanager.h"


ICQContact::ICQContact( Kopete::Account* account, const QString &name, Kopete::MetaContact *parent,
						const QString& icon, const OContact& ssiItem )
: ICQContactBase( account, name, parent, icon, ssiItem )
{
	mProtocol = static_cast<ICQProtocol *>(protocol());
	m_infoWidget = 0L;

	if ( ssiItem.waitingAuth() )
		setOnlineStatus( mProtocol->statusManager()->waitingForAuth() );
	else
		setPresenceTarget( Oscar::Presence( Oscar::Presence::Offline ) );

	QObject::connect( mAccount->engine(), SIGNAL( loggedIn() ), this, SLOT( loggedIn() ) );
	//QObject::connect( mAccount->engine(), SIGNAL( userIsOnline( const QString& ) ), this, SLOT( userOnline( const QString&, UserDetails ) ) );
	QObject::connect( mAccount->engine(), SIGNAL( userIsOffline( const QString& ) ), this, SLOT( userOffline( const QString& ) ) );
	QObject::connect( mAccount->engine(), SIGNAL( authRequestReceived( const QString&, const QString& ) ),
	                  this, SLOT( slotGotAuthRequest( const QString&, const QString& ) ) );
	QObject::connect( mAccount->engine(), SIGNAL( authReplyReceived( const QString&, const QString&, bool ) ),
	                  this, SLOT( slotGotAuthReply(const QString&, const QString&, bool ) ) );
	QObject::connect( mAccount->engine(), SIGNAL( receivedIcqLongInfo( const QString& ) ),
	                  this, SLOT( receivedLongInfo( const QString& ) ) );
	QObject::connect( mAccount->engine(), SIGNAL( receivedUserInfo( const QString&, const UserDetails& ) ),
	                  this, SLOT( userInfoUpdated( const QString&, const UserDetails& ) ) );
	QObject::connect( mAccount->engine(), SIGNAL(receivedIcqTlvInfo(const QString&)),
	                  this, SLOT(receivedTlvInfo(const QString&)) );
}

ICQContact::~ICQContact()
{
	delete m_infoWidget;
}

void ICQContact::setSSIItem( const OContact& ssiItem )
{
	if ( ssiItem.waitingAuth() )
		setOnlineStatus( mProtocol->statusManager()->waitingForAuth() );

	if ( ssiItem.type() != 0xFFFF && ssiItem.waitingAuth() == false &&
	     onlineStatus() == Kopete::OnlineStatus::Unknown )
	{
		//make sure they're offline
		setPresenceTarget( Oscar::Presence( Oscar::Presence::Offline ) );
	}

	if ( mAccount->isConnected() && m_ssiItem.metaInfoId() != ssiItem.metaInfoId() )
	{
		// User info has changed, check nickname or status description if needed
		if ( m_details.onlineStatusMsgSupport() )
			mAccount->engine()->requestMediumTlvInfo( contactId(), ssiItem.metaInfoId() );
		else if ( ssiItem.alias().isEmpty() )
			requestShortInfo();
	}

	ICQContactBase::setSSIItem( ssiItem );
}

void ICQContact::userInfoUpdated( const QString& contact, const UserDetails& details )
{
	if ( Oscar::normalize( contact  ) != Oscar::normalize( contactId() ) )
		return;

	// invalidate old away message if user was offline
	if ( !isOnline() )
		removeProperty( mProtocol->awayMessage );

	kDebug( OSCAR_ICQ_DEBUG ) << k_funcinfo << "extendedStatus is " << details.extendedStatus();
	Oscar::Presence presence = mProtocol->statusManager()->presenceOf( details.extendedStatus(), details.userClass() );

	refreshStatus( details, presence );

	if ( details.dcOutsideSpecified() )
	{
		if ( details.dcExternalIp().isUnspecified() )
			removeProperty( mProtocol->ipAddress );
		else
			setProperty( mProtocol->ipAddress, details.dcExternalIp().toString() );
	}

	if ( details.capabilitiesSpecified() )
	{
		if ( details.clientName().isEmpty() )
			removeProperty( mProtocol->clientFeatures );
		else
			setProperty( mProtocol->clientFeatures, details.clientName() );
	}

	OscarContact::userInfoUpdated( contact, details );
}

void ICQContact::refreshStatus( const UserDetails& details, Oscar::Presence presence )
{
	// Filter our XStatus and ExtStatus
	presence.setFlags( presence.flags() & ~Oscar::Presence::StatusTypeMask );

	// XStatus don't support offline status so don't show it (xtrazStatusSpecified can be true if contact was online)
	if ( details.xtrazStatusSpecified() && presence.type() != Oscar::Presence::Offline )
	{
		presence.setFlags( presence.flags() | Oscar::Presence::XStatus );
		presence.setXtrazStatus( details.xtrazStatus() );
	}
	else if ( !m_statusDescription.isEmpty() )
	{
		presence.setFlags( presence.flags() | Oscar::Presence::ExtStatus );
		presence.setDescription( m_statusDescription );
	}

	setPresenceTarget( presence );

	Oscar::Presence selfPres( mProtocol->statusManager()->presenceOf( account()->myself()->onlineStatus() ) );
	bool selfVisible = !(selfPres.flags() & Oscar::Presence::Invisible);

	if ( selfVisible && isReachable() && presence.type() != Oscar::Presence::Offline )
	{
		Client::ICQStatus contactStatus = Client::ICQOnline;
		if ( details.xtrazStatusSpecified() )
		{
			contactStatus = Client::ICQXStatus;
		}
		else
		{
			switch ( presence.type() )
			{
			case Oscar::Presence::Online:
				contactStatus = Client::ICQOnline;
				break;
			case Oscar::Presence::Away:
				contactStatus = Client::ICQAway;
				break;
			case Oscar::Presence::NotAvailable:
				contactStatus = Client::ICQNotAvailable;
				break;
			case Oscar::Presence::Occupied:
				contactStatus = Client::ICQOccupied;
				break;
			case Oscar::Presence::DoNotDisturb:
				contactStatus = Client::ICQDoNotDisturb;
				break;
			case Oscar::Presence::FreeForChat:
				contactStatus = Client::ICQFreeForChat;
				break;
			default:
				break;
			}
		}

		// FIXME: How can we check if client supports status plugin messages?
		if ( details.onlineStatusMsgSupport() )
			contactStatus |= Client::ICQPluginStatus;

		// If contact is online and doesn't support status plugin messages than
		// this contact can't have online status message.
		if ( contactStatus == Client::ICQOnline && !details.onlineStatusMsgSupport() )
		{
			mAccount->engine()->removeICQAwayMessageRequest( contactId() );
			removeProperty( mProtocol->awayMessage );
		}
		else
		{
			mAccount->engine()->addICQAwayMessageRequest( contactId(), contactStatus );
		}
	}
	else
	{
		mAccount->engine()->removeICQAwayMessageRequest( contactId() );
	}
}

void ICQContact::userOnline( const QString& userId )
{
	if ( Oscar::normalize( userId ) != Oscar::normalize( contactId() ) )
		return;

	kDebug(OSCAR_ICQ_DEBUG) << "Setting " << userId << " online";
	setPresenceTarget( Oscar::Presence( Oscar::Presence::Online ) );
}

void ICQContact::userOffline( const QString& userId )
{
	if ( Oscar::normalize( userId ) != Oscar::normalize( contactId() ) )
		return;

	kDebug(OSCAR_ICQ_DEBUG) << "Setting " << userId << " offline";
	if ( m_ssiItem.waitingAuth() )
		setOnlineStatus( mProtocol->statusManager()->waitingForAuth() );
	else
		refreshStatus( m_details, Oscar::Presence( Oscar::Presence::Offline ) );
	
	removeProperty( mProtocol->awayMessage );
}

void ICQContact::loggedIn()
{
	if ( metaContact()->isTemporary() )
		return;

	if ( m_ssiItem.waitingAuth() )
		setOnlineStatus( mProtocol->statusManager()->waitingForAuth() );

	if ( !m_ssiItem.metaInfoId().isEmpty() )
	{
		m_requestingNickname = true;
		int time = ( KRandom::random() % 20 ) * 1000;
		kDebug(OSCAR_ICQ_DEBUG) << k_funcinfo << "updating nickname and status description in " << time/1000 << " seconds";
		QTimer::singleShot( time, this, SLOT( requestMediumTlvInfo() ) );
	}
	else if ( ( ( hasProperty( Kopete::Global::Properties::self()->nickName().key() ) && nickName() == contactId() )
	            || !hasProperty( Kopete::Global::Properties::self()->nickName().key() ) )
	          && !m_requestingNickname && m_ssiItem.alias().isEmpty() )
	{
		m_requestingNickname = true;
		int time = ( KRandom::random() % 20 ) * 1000;
		kDebug(OSCAR_ICQ_DEBUG) << k_funcinfo << "updating nickname in " << time/1000 << " seconds";
		QTimer::singleShot( time, this, SLOT( requestShortInfo() ) );
	}

}

void ICQContact::slotRequestAuth()
{
	QString reason = KInputDialog::getText( i18n("Request Authorization"),
	                                        i18n("Reason for requesting authorization:") );
	if ( !reason.isNull() )
		mAccount->engine()->requestAuth( contactId(), reason );
}

void ICQContact::slotSendAuth()
{
	kDebug(OSCAR_ICQ_DEBUG) << k_funcinfo << "Sending auth reply";
	ICQAuthReplyDialog replyDialog( 0, false );

	replyDialog.setUser( property( Kopete::Global::Properties::self()->nickName() ).value().toString() );
	if ( replyDialog.exec() )
		mAccount->engine()->sendAuth( contactId(), replyDialog.reason(), replyDialog.grantAuth() );
}

void ICQContact::slotGotAuthReply( const QString& contact, const QString& reason, bool granted )
{
	if ( Oscar::normalize( contact ) != Oscar::normalize( contactId() ) )
		return;

	kDebug(OSCAR_ICQ_DEBUG) << k_funcinfo;
	QString message;
	if( granted )
	{
		message = i18n( "User %1 has granted your authorization request.\nReason: %2" ,
			  property( Kopete::Global::Properties::self()->nickName() ).value().toString() ,
			  reason );

		// remove the unknown status
		setPresenceTarget( Oscar::Presence( Oscar::Presence::Offline ) );
	}
	else
	{
		message = i18n( "User %1 has rejected the authorization request.\nReason: %2" ,
			  property( Kopete::Global::Properties::self()->nickName() ).value().toString() ,
			  reason );
	}
	KNotification::event( QString::fromLatin1("icq_authorization"), message );
}

void ICQContact::slotGotAuthRequest( const QString& contact, const QString& reason )
{
	if ( Oscar::normalize( contact ) != Oscar::normalize( contactId() ) )
		return;

	ICQAuthReplyDialog *replyDialog = new ICQAuthReplyDialog();

	connect( replyDialog, SIGNAL( okClicked() ), this, SLOT( slotAuthReplyDialogOkClicked() ) );
	replyDialog->setUser( property( Kopete::Global::Properties::self()->nickName() ).value().toString() );
	replyDialog->setRequestReason( reason );
	replyDialog->setModal( true );
	replyDialog->show();
}

void ICQContact::slotAuthReplyDialogOkClicked()
{
    // Do not need to delete will delete itself automatically
    ICQAuthReplyDialog *replyDialog = (ICQAuthReplyDialog*)sender();

    if (replyDialog)
	mAccount->engine()->sendAuth( contactId(), replyDialog->reason(), replyDialog->grantAuth() );
}

void ICQContact::receivedLongInfo( const QString& contact )
{
	if ( Oscar::normalize( contact ) != Oscar::normalize( contactId() ) )
	{
		if ( m_infoWidget )
			m_infoWidget->delayedDestruct();
		return;
	}

	QTextCodec* codec = contactCodec();

	kDebug(OSCAR_ICQ_DEBUG) << k_funcinfo << "received long info from engine";

	ICQGeneralUserInfo genInfo = mAccount->engine()->getGeneralInfo( contact );
	if ( m_ssiItem.alias().isEmpty() && !genInfo.nickName.get().isEmpty() )
		setNickName( codec->toUnicode( genInfo.nickName.get() ) );
	emit haveBasicInfo( genInfo );

	ICQWorkUserInfo workInfo = mAccount->engine()->getWorkInfo( contact );
	emit haveWorkInfo( workInfo );

	ICQEmailInfo emailInfo = mAccount->engine()->getEmailInfo( contact );
	emit haveEmailInfo( emailInfo );

	ICQNotesInfo notesInfo = mAccount->engine()->getNotesInfo( contact );
	emit haveNotesInfo( notesInfo );

	ICQMoreUserInfo moreInfo = mAccount->engine()->getMoreInfo( contact );
	emit haveMoreInfo( moreInfo );

	ICQInterestInfo interestInfo = mAccount->engine()->getInterestInfo( contact );
	emit haveInterestInfo( interestInfo );

	ICQOrgAffInfo orgAffInfo = mAccount->engine()->getOrgAffInfo( contact );
	emit haveOrgAffInfo( orgAffInfo );
}

void ICQContact::requestMediumTlvInfo()
{
	if ( mAccount->isConnected() && !m_ssiItem.metaInfoId().isEmpty() )
		mAccount->engine()->requestMediumTlvInfo( contactId(), m_ssiItem.metaInfoId() );
}

void ICQContact::receivedTlvInfo( const QString& contact )
{
	if ( Oscar::normalize( contact ) != Oscar::normalize( contactId() ) )
		return;

	ICQFullInfo info = mAccount->engine()->getFullInfo( contact );

	m_requestingNickname = false; //done requesting nickname
	if ( m_ssiItem.alias().isEmpty() && !info.nickName.get().isEmpty() )
		setNickName( QString::fromUtf8( info.nickName.get() ) );

	m_statusDescription = QString::fromUtf8( info.statusDescription.get() );

	Oscar::Presence presence = mProtocol->statusManager()->presenceOf( onlineStatus() );
	
	refreshStatus( m_details, presence );
}

#if 0
void ICQContact::slotContactChanged(const UserInfo &u)
{
	if (u.sn != contactName())
		return;

	// update mInfo and general stuff from OscarContact
	slotParseUserInfo(u);

	/*kDebug(14190) << k_funcinfo << "Called for '"
		<< displayName() << "', contactName()=" << contactName() << endl;*/
	QStringList capList;
	// Append client name and version in case we found one
	if (!mInfo.clientName.isEmpty())
	{
		if (!mInfo.clientVersion.isEmpty())
		{
			capList << i18nc("Translators: client-name client-version",
				"%1 %2", mInfo.clientName, mInfo.clientVersion);
		}
		else
		{
			capList << mInfo.clientName;
		}
	}
	// and now for some general informative capabilities
	if (hasCap(CAP_UTF8))
		capList << i18n("UTF-8");
	if (hasCap(CAP_RTFMSGS))
		capList << i18n("RTF-Messages");
	if (hasCap(CAP_IMIMAGE))
		capList << i18n("DirectIM/IMImage");
	if (hasCap(CAP_CHAT))
		capList << i18n("Groupchat");

	if (capList.count() > 0)
		setProperty(mProtocol->clientFeatures, capList.join(", "));
	else
		removeProperty(mProtocol->clientFeatures);

	unsigned int newStatus = 0;
	mInvisible = (mInfo.icqextstatus & ICQ_STATUS_IS_INVIS);

	if (mInfo.icqextstatus & ICQ_STATUS_IS_FFC)
		newStatus = OSCAR_FFC;
	else if (mInfo.icqextstatus & ICQ_STATUS_IS_DND)
		newStatus = OSCAR_DND;
	else if (mInfo.icqextstatus & ICQ_STATUS_IS_OCC)
		newStatus = OSCAR_OCC;
	else if (mInfo.icqextstatus & ICQ_STATUS_IS_NA)
		newStatus = OSCAR_NA;
	else if (mInfo.icqextstatus & ICQ_STATUS_IS_AWAY)
		newStatus = OSCAR_AWAY;
	else
		newStatus = OSCAR_ONLINE;

	if (this != account()->myself())
	{
		if(newStatus != onlineStatus().internalStatus())
		{
			if(newStatus != OSCAR_ONLINE) // if user changed to some state other than online
			{
				mAccount->engine()->requestAwayMessage(this);
			}
			else // user changed to "Online" status and has no away message anymore
			{
				removeProperty(mProtocol->awayMessage);
			}
		}
	}

	setStatus(newStatus);
}

void ICQContact::slotOffgoingBuddy(QString sender)
{
	if(sender != contactName())
		return;

	removeProperty(mProtocol->clientFeatures);
	removeProperty(mProtocol->awayMessage);
	setOnlineStatus(mProtocol->statusOffline);
}

void ICQContact::gotIM(OscarSocket::OscarMessageType /*type*/, const QString &message)
{
	// Build a Kopete::Message and set the body as Rich Text
	Kopete::ContactPtrList tmpList;
	tmpList.append(account()->myself());
	Kopete::Message msg(this, tmpList, message, Kopete::Message::Inbound,
		Kopete::Message::RichText);
	manager(true)->appendMessage(msg);
}


void ICQContact::slotSendMsg(Kopete::Message& message, Kopete::ChatSession *)
{
	if (message.plainBody().isEmpty()) // no text, do nothing
		return;

	// Check to see if we're even online
	if(!account()->isConnected())
	{
		KMessageBox::sorry(Kopete::UI::Global::mainWidget(),
			i18n("<qt>You must be logged on to ICQ before you can "
				"send a message to a user.</qt>"),
			i18n("Not Signed On"));
		return;
	}

	// FIXME: We don't do HTML in ICQ
	// we might be able to do that in AIM and we might also convert
	// HTML to RTF for ICQ type-2 messages  [mETz]
	static_cast<OscarAccount*>(account())->engine()->sendIM(
		message.plainBody(), this, false);

	// Show the message we just sent in the chat window
	manager(Kopete::Contact::CanCreate)->appendMessage(message);
	manager(Kopete::Contact::CanCreate)->messageSucceeded();
}

#endif

bool ICQContact::isReachable()
{
	return account()->isConnected();
}

QList<KAction*> *ICQContact::customContextMenuActions()
{
	QList<KAction*> *actionCollection = new QList<KAction*>();

	actionRequestAuth = new KAction( i18n("&Request Authorization"), this );
        //, "actionRequestAuth");
	actionRequestAuth->setIcon( KIcon( "mail-reply-sender" ) );
	QObject::connect( actionRequestAuth, SIGNAL(triggered(bool)), this, SLOT(slotRequestAuth()) );

	actionSendAuth = new KAction( i18n("&Grant Authorization"), this );
        //, "actionSendAuth");
	actionSendAuth->setIcon( KIcon( "mail-forward" ) );
	QObject::connect( actionSendAuth, SIGNAL(triggered(bool)), this, SLOT(slotSendAuth()) );

	m_actionIgnore = new KToggleAction(i18n("&Ignore"), this );
        //, "actionIgnore");
	QObject::connect( m_actionIgnore, SIGNAL(triggered(bool)), this, SLOT(slotIgnore()) );

	m_actionVisibleTo = new KToggleAction(i18n("Always &Visible To"), this );
        //, "actionVisibleTo");
	QObject::connect( m_actionVisibleTo, SIGNAL(triggered(bool)), this, SLOT(slotVisibleTo()) );

	m_actionInvisibleTo = new KToggleAction(i18n("Always &Invisible To"), this );
        //, "actionInvisibleTo");
	QObject::connect( m_actionInvisibleTo, SIGNAL(triggered(bool)), this, SLOT(slotInvisibleTo()) );

	m_selectEncoding = new KAction( i18n( "Select Encoding..." ), this );
        //, "changeEncoding" );
	m_selectEncoding->setIcon( KIcon( "character-set" ) );
	QObject::connect( m_selectEncoding, SIGNAL(triggered(bool)), this, SLOT(changeContactEncoding()) );

	bool on = account()->isConnected();
	if ( m_ssiItem.waitingAuth() )
		actionRequestAuth->setEnabled(on);
	else
		actionRequestAuth->setEnabled(false);

	actionSendAuth->setEnabled(on);
	m_actionIgnore->setEnabled(on);
	m_actionVisibleTo->setEnabled(on);
	m_actionInvisibleTo->setEnabled(on);

	ContactManager* ssi = account()->engine()->ssiManager();
	m_actionIgnore->setChecked( ssi->findItem( m_ssiItem.name(), ROSTER_IGNORE ));
	m_actionVisibleTo->setChecked( ssi->findItem( m_ssiItem.name(), ROSTER_VISIBLE ));
	m_actionInvisibleTo->setChecked( ssi->findItem( m_ssiItem.name(), ROSTER_INVISIBLE ));

	actionCollection->append(actionRequestAuth);
	actionCollection->append(actionSendAuth);
    actionCollection->append( m_selectEncoding );

	actionCollection->append(m_actionIgnore);
	actionCollection->append(m_actionVisibleTo);
	actionCollection->append(m_actionInvisibleTo);

	return actionCollection;
}


void ICQContact::slotUserInfo()
{
	m_infoWidget = new ICQUserInfoWidget( Kopete::UI::Global::mainWidget() );
	QObject::connect( m_infoWidget, SIGNAL( finished() ), this, SLOT( closeUserInfoDialog() ) );
	m_infoWidget->setContact( this );
	m_infoWidget->show();
	if ( account()->isConnected() )
		mAccount->engine()->requestFullInfo( contactId() );
}

void ICQContact::closeUserInfoDialog()
{
	QObject::disconnect( this, 0, m_infoWidget, 0 );
	m_infoWidget->delayedDestruct();
	m_infoWidget = 0L;
}

void ICQContact::slotIgnore()
{
	account()->engine()->setIgnore( contactId(), m_actionIgnore->isChecked() );
}

void ICQContact::slotVisibleTo()
{
	account()->engine()->setVisibleTo( contactId(), m_actionVisibleTo->isChecked() );
}

void ICQContact::slotInvisibleTo()
{
	account()->engine()->setInvisibleTo( contactId(), m_actionInvisibleTo->isChecked() );
}

#if 0

void ICQContact::slotReadAwayMessage()
{
	kDebug(14153) << k_funcinfo << "account='" << account()->accountId() <<
		"', contact='" << displayName() << "'" << endl;

	if (!awayMessageDialog)
	{
		awayMessageDialog = new ICQReadAway(this, 0L, "awayMessageDialog");
		if(!awayMessageDialog)
			return;
		QObject::connect(awayMessageDialog, SIGNAL(closing()), this, SLOT(slotCloseAwayMessageDialog()));
		awayMessageDialog->show();
	}
	else
	{
		awayMessageDialog->raise();
	}
}


void ICQContact::slotCloseAwayMessageDialog()
{
	awayMessageDialog->delayedDestruct();
	awayMessageDialog = 0L;
}


const QString ICQContact::awayMessage()
{
	kDebug(14150) << k_funcinfo <<  property(mProtocol->awayMessage).value().toString();
	return property(mProtocol->awayMessage).value().toString();
}


void ICQContact::setAwayMessage(const QString &message)
{
	/*kDebug(14150) << k_funcinfo <<
		"Called for '" << displayName() << "', away msg='" << message << "'" << endl;*/
	setProperty(mProtocol->awayMessage, message);
	emit awayMessageChanged();
}


void ICQContact::slotUpdGeneralInfo(const int seq, const ICQGeneralUserInfo &inf)
{
	// compare reply's sequence with the one we sent with our last request
	if(seq != userinfoRequestSequence)
		return;
	generalInfo = inf;

	if(!generalInfo.firstName.isEmpty())
		setProperty(mProtocol->firstName, generalInfo.firstName);
	else
		removeProperty(mProtocol->firstName);

	if(!generalInfo.lastName.isEmpty())
		setProperty(mProtocol->lastName, generalInfo.lastName);
	else
		removeProperty(mProtocol->lastName);

	if(!generalInfo.eMail.isEmpty())
		setProperty(mProtocol->emailAddress, generalInfo.eMail);
	else
		removeProperty(mProtocol->emailAddress);
	/*
	if(!generalInfo.phoneNumber.isEmpty())
		setProperty("privPhoneNum", generalInfo.phoneNumber);
	else
		removeProperty("privPhoneNum");

	if(!generalInfo.faxNumber.isEmpty())
		setProperty("privFaxNum", generalInfo.faxNumber);
	else
		removeProperty("privFaxNum");

	if(!generalInfo.cellularNumber.isEmpty())
		setProperty("privMobileNum", generalInfo.cellularNumber);
	else
		removeProperty("privMobileNum");
	*/

	if(contactName() == displayName() && !generalInfo.nickName.isEmpty())
	{
		kDebug(14153) << k_funcinfo << "setting new displayname for former UIN-only Contact";
		setDisplayName(generalInfo.nickName);
	}

	incUserInfoCounter();
}


void ICQContact::slotSnacFailed(WORD snacID)
{
	if (userinfoRequestSequence != 0)
		kDebug(14153) << k_funcinfo << "snacID = " << snacID << " seq = " << userinfoRequestSequence;

	//TODO: ugly interaction between snacID and request sequence, see OscarSocket::sendCLI_TOICQSRV
	if (snacID == (0x0000 << 16) | userinfoRequestSequence)
	{
		userinfoRequestSequence = 0;
		emit userInfoRequestFailed();
	}
}

void ICQContact::slotIgnore()
{
	kDebug(14150) << k_funcinfo <<
		"Called; ignore = " << actionIgnore->isChecked() << endl;
	setIgnore(actionIgnore->isChecked(), true);
}

void ICQContact::slotVisibleTo()
{
	kDebug(14150) << k_funcinfo <<
		"Called; visible = " << actionVisibleTo->isChecked() << endl;
	setVisibleTo(actionVisibleTo->isChecked(), true);
}
#endif
#include "icqcontact.moc"
//kate: indent-mode csands; tab-width 4; replace-tabs off; space-indent off;