/*
    oscarcontact.h  -  Oscar Protocol Plugin

    Copyright (c) 2002 by Tom Linsky <twl6@po.cwru.edu>

    Kopete    (c) 2002 by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#ifndef OSCARCONTACT_H
#define OSCARCONTACT_H

#include <qwidget.h>
#include "kopetecontact.h"
#include "kopetemessage.h"

/**
  * Contact for oscar protocol
  * @author Tom Linsky <twl6@po.cwru.edu>
  */

struct UserInfo;
class QTimer;
class OscarProtocol;
class KopeteMessageManager;
class OscarProtocol;

class OscarContact : public KopeteContact
{
   Q_OBJECT
public:
	OscarContact(const QString name,
			OscarProtocol *protocol,
			KopeteMetaContact *parent);
	~OscarContact();
	/**
	 * Return the protocol specific serialized data
	 * that a plugin may want to store a contact list.
	 */
	virtual QString data(void) const;
	/** Returns the online status of the contact */
	virtual ContactStatus status(void) const;
	/** Returns the status icon of the contact */
	virtual QString statusIcon(void) const;
	/**
	 * Returns a set of custom menu items for
	 * the context menu
	 */
	virtual KActionCollection *customContextMenuActions(void);
	/* Return whether or not this contact is REACHABLE. */
	virtual bool isReachable(void);

public slots:
	/** Pops up a chat window */
	virtual void execute(void);
	/** Method to delete a contact from the contact list */
	virtual void slotDeleteContact(void);

public: // Public attributes
	/** The name of the contact */
	QString mName;
	/** The status of the contact */
	int mStatus;
	/** List of contacts.. I don't want this to be here */
	QPtrList<KopeteContact> theContacts;

private: // Private members
	KopeteMessageManager *msgManager();
	/** Initialzes the actions */
	void initActions(void);

	/**
	 * parses HTML AIM-Clients send to us and
	 * strips off most of it
	 */
	KopeteMessage parseAIMHTML ( QString m );
	/* used by above to strip off a tag*/
//	QStringList removeTag ( QString &message, QString tag );

private: // Private attributes
	KopeteMessageManager *mMsgManager;
	KAction* actionWarn;
	KAction* actionBlock;
	KActionCollection* actionCollection;

	OscarProtocol *mProtocol;
	
	/**
	 * The time of the last autoresponse,
	 * used to determine when to send an
	 * autoresponse again.
	 */
	long mLastAutoResponseTime;
		
	/** The contact's idle time */
	int mIdle;
	/** Timer for sending typing notifications */
	QTimer* mTypingTimer;

private slots: // Private slots
	/** Called when a buddy changes */
	void slotUpdateBuddy(int buddyNum);
	/** Called when a buddy has changed status */
	void slotBuddyChanged(UserInfo u);
	/** Called when we get a minityping notification */
	void slotGotMiniType(QString screenName, int type);
	/**
	 * Called when we are notified by the chat window
	 * that this person is being typed to...
	 */
	void slotTyping(bool typing);
	/**
	 * Called by a timer set up in slotTyping
	 * to do the "Buddy has entered text"
	 */
	void slotTextEntered();
	/** Called when a buddy is offgoing */
	void slotOffgoingBuddy(QString sn);
	/** Called when user info is requested */
	void slotUserInfo(void);
	/** Called when we want to send a message */
	void slotSendMsg(const KopeteMessage&, KopeteMessageManager *);
	/** Called when an IM is received */
	void slotIMReceived(QString sender, QString msg, bool isAuto);
	/** Called when nickname needs to be updated */
	void slotUpdateNickname(const QString);
	/** Warn the user */
	void slotWarn(void);
	/** Called when the status of the Kopete user(behind this computer)'s status has changed */
	void slotMainStatusChanged(int);
  /** No descriptions */
  void slotMoved(KopeteMetaContact *mc);
  /** Called when we want to block the contact */
  void slotBlock(void);
};

#endif
/*
 * Local variables:
 * c-indentation-style: k&r
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
// vim: set noet ts=4 sts=4 sw=4:

