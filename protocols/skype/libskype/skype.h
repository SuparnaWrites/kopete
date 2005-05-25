/*  This file is part of the KDE project
    Copyright (C) 2005 Michal Vaner <vorner@seznam.cz>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

*/
#ifndef SKYPE_H
#define SKYPE_H

#include <qobject.h>

class SkypePrivate;
class SkypeAccount;

/**
 * This class is internal backend for skype. It provides slots for such things like "send a IM" and so
 * @author Kopete Developers
 */
class Skype : public QObject
{
	Q_OBJECT
	private:
		///The d pointer for private things
		SkypePrivate *d;
		/**
		 * Will try to hitchhike a message. It will hitchhike it, show it in the proper chat session and so on, but just when it is enabled by hitchhike mode and will maker it as read if enabled
		 * @param messageId ID of the message to hitchHike
		 */
		void hitchHike(const QString &messageId);
	private slots:
		/**
		 * Adds new message do be sent to skype (normaly is sent imediatelly).
		 * If there is no connection to skype, it is created before it is sent.
		 * @param message Message to send to skype
		 * @param deleteQueue If this is true, all message waiting to be sent are deleted and only this one stays in the queue
		 */
		void queueSkypeMessage(const QString &message, bool deleteQueue);
		/**
		 * Listens for closed skype connection
		 */
		void closed(int reason);
		/**
		 * Listens for finishing the connecting atempt and sending the queue if it was successfull
		 * @param error - Did it work or was there some error?
		 * @param protocolVer - Version of protocol used by this connection
		 */
		void connectionDone(int error, int protocolVer);
		/**
		 * This one showes an error message
		 * @param message What to write on the dialog box
		 */
		void error(const QString &message);
		/**
		 * This one scans messages from Skype API and acts acordingly to them (changing online status, showing messages....
		 * @param message What the skype said
		 */
		void skypeMessage(const QString &message);
		/**
		 * This one resets the online status showed on the icon in kopete depending on last values from skype.
		 * Used when the status changes
		 */
		void resetStatus();
		/**
		 * Makes the Skype search for something and saves what it was to decide later, what to do with it
		 * @param what What are we searching for
		 */
		void search(const QString &what);
	public:
		/**
		 * Constructor
		 * @param account The account that this connection belongs to
		 */
		Skype(SkypeAccount &account);
		/**
		 * Destructor
		 */
		~Skype();
		///Can we comunicate with the skype program right now?
		bool canComunicate();
		/**
		 * Enables or disables hitchhake mode of incoming messages
		 * @see SkypeAccount::setHitchHike
		 */
		void setHitchMode(bool value);
		/**
		 * Enables or disables mark read messages mode
		 * @see SkypeAccount::setMarkRead
		 */
		void setMarkMode(bool value);
		/**
		 * Enables/disables scanning for unread messages after login
		 * @see SkypeAccount::setScanForUnread
		 */
		void setScanForUnread(bool value);
		/**
		 * Is that call incoming call?
		 * @param callId What call you mean?
		 * @return true if the call is incoming
		 */
		bool isCallIncoming(const QString &callId);
	public slots:
		/**
		 * Tell the skype to go online
		 */
		void setOnline();
		/**
		 * Tell the skype to go offline
		 */
		void setOffline();
		/**
		 * Tell the skype to go offline
		 */
		void setAway();
		/**
		 * Tell the skype to go not available
		 */
		void setNotAvailable();
		/**
		 * Tell the skype to go to Do not disturb
		 */
		void setDND();
		/**
		 * Tell the skype to go to Skype me mode
		 */
		void setSkypeMe();
		/**
		 * Tell the skype to go invisible
		 */
		void setInvisible();
		/**
		 * This sets the values of the account.
		 * @see SkypeAccount
		 */
		void setValues(int launchType, const QString &appName);
		/**
		 * Retrieve info of that contact
		 * @param contact What contact wants it
		 */
		void getContactInfo(const QString &contact);
		/**
		 * Asks skype for buddy status of some contact. Buddystatus is some property that ondicates, weather it is in contact list, awaiting authorization, just been mentioned or what exactly happened with it..
		 * After skype responses, you will get the response by emiting the received signal
		 * @param contact It is the contact id of the user you want to check.
		 */
		void getContactBuddy(const QString &contact);
		/**
		 * Sends a message trough skype
		 * @param user To who it should be sent
		 * @param body What to send
		 * @see sentMessage
		 */
		void send(const QString &user, const QString &body);
		/**
		 * Begins new call.
		 * @param userId ID of user to call
		 * @see acceptCall
		 * @see hangUp
		 * @see holdCall
		 * @see callStatus
		 * @see callError
		 */
		void makeCall(const QString &userId);
		/**
		 * Accept an incoming call
		 * @param callId ID of call to accept.
		 * @see makeCall
		 * @see hangUp
		 * @see holdCall
		 * @see callStatus
		 * @see callError
		 * @see newCall
		 */
		void acceptCall(const QString &callId);
		/**
		 * Hang up (finish) call in progress or deny an incoming call
		 * @param callId Which one
		 * @see makeCall
		 * @see acceptCall
		 * @see holdCall
		 * @see callStatus
		 * @see callError
		 * @see newCall
		 */
		void hangUp(const QString &callId);
		/**
		 * Hold call in progress or resume holded call. That call will not finish, you just leave it for later.
		 * @param callId Which call
		 * @see makeCall
		 * @see acceptCall
		 * @see hangUp
		 * @see callStatus
		 * @see callError
		 * @see newCall
		 */
		void togleHoldCall(const QString &callId);
		/**
		 * Get the skoype out balance
		 */
		void getSkypeOut();
	signals:
		/**
		 * Emited when the skype changes to online (or says it goes online)
		 */
		void wentOnline();
		/**
		 * Emited when the skype goes offline
		 */
		void wentOffline();
		/**
		 * Emited when the skype goes away
		 */
		void wentAway();
		/**
		 * Emited when the skype goes to Not awailable
		 */
		void wentNotAvailable();
		/**
		 * Emited when the skype goes to DND mode
		 */
		void wentDND();
		/**
		 * Emited when skype changes to skype me mode
		 */
		void wentSkypeMe();
		/**
		 * Emited when skype becomes invisible
		 */
		void wentInvisible();
		/**
		 * Emited when atempt to connect started
		 */
		void statusConnecting();
		/**
		 * Emited when new user should be added to the list
		 * @param name The skype name of the user
		 */
		void newUser(const QString &name);
		/**
		 * All contacts should be asked to request update of their information. This is emited after the connection to skype is made.
		 */
		void updateAllContacts();
		/**
		 * This is emited whenever some contact should be notified of info change
		 * @param contact What contact is it
		 * @param change The change. The syntax is [property (displayname, onlinestatus..)] [value]
		 */
		void contactInfo(const QString &contact, const QString &change);
		/**
		 * This is emited when a new message is received
		 * @param user Contact ID of user that sent it. It is NOT guaranteed that the user is in list!
		 * @param body The message body that was received
		 */
		void receivedIM(const QString &user, const QString &body);
		/**
		 * This is emited when an Id of the last outgoing message is known
		 * @param id The ID of that message
		 */
		void gotMessageId(const QString &id);
		/**
		 * Emited when some message has been successfully sent
		 * @param id ID of the sent message
		 */
		void sentMessage(const QString &id);
		/**
		 * This slot notifies about call status (onhold, in progress, routing, finished..)
		 * @param callId WHat call is it?
		 * @param status New status of the call.
		 * @see makeCall
		 * @see acceptCall
		 * @see hangUp
		 * @see holdCall
		 * @see callError
		 * @see newCall
		 */
		void callStatus(const QString &callId, const QString &status);
		/**
		 * This slot informs of error that happened to the call. It is translated error and can be directly showed to user.
		 * @param callId ID of the call that has an error.
		 * @param message The error text
		 * @see makeCall
		 * @see acceptCall
		 * @see hangUp
		 * @see holdCall
		 * @see callStatus
		 * @see newCall
		 */
		void callError(const QString &callId, const QString &message);
		/**
		 * Indicates a new call is established (is being established, incoming or so). In short, there is some new call.
		 * @param callId ID of the new call
		 * @param userId ID of the other user, or list of users (if more than one) divided by spaces
		 * @see makeCall
		 * @see acceptCall
		 * @see hangUp
		 * @see holdCall]
		 * @see callStatus
		 * @see callError
		 */
		void newCall(const QString &callId, const QString &userId);
		/**
		 * Skype out balance info
		 * @param balance How much does the user have
		 * @param currency And what is it that he has
		 */
		void skypeOutInfo(int balance, const QString &currency);
};

#endif
