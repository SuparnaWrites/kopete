//
// C++ Interface: privacymanager
//
// Description: 
//
//
// Author: SUSE AG <>, (C) 2004
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef PRIVACYMANAGER_H
#define PRIVACYMANAGER_H

#include <qobject.h>
#include <qstringlist.h>

class Client;

/**
Keeps a record of the server side privacy allow and deny lists, default policy and whether the user is allowed to change privacy settings

@author SUSE AG
*/
class PrivacyManager : public QObject
{
Q_OBJECT
public:
	PrivacyManager( Client * client, const char *name = 0);
	~PrivacyManager();
	// accessors
	bool isBlocked( const QString & dn );
	QStringList allowList();
	QStringList denyList();
	bool isPrivacyLocked();
	bool defaultDeny();
	bool defaultAllow();
	// mutators
	void setDefaultAllow( bool allow );
	void setDefaultDeny( bool deny );
	void addAllow( const QString & dn );
	void addDeny( const QString & dn );
	void removeAllow( const QString & dn );
	void removeDeny( const QString & dn );
	// change everything at once
	void setPrivacy( bool defaultDeny, const QStringList & allowList, const QStringList & denyList );

public slots: 
	/** 
	 * Used to initialise the privacy manager using the server side privacy list
	 */
	void slotGotPrivacyDetails( bool locked, bool defaultDeny, const QStringList & allowList, const QStringList & denyList );
protected slots:
	// Receive the results of Tasks manipulating the privacy lists
	void slotDefaultPolicyChanged();
	void slotAllowAdded();
	void slotDenyAdded();
	void slotAllowRemoved();
	void slotDenyRemoved();	
private:
	Client * m_client;
	bool m_locked;
	bool m_defaultDeny;
	QStringList m_allowList;
	QStringList m_denyList;
};

#endif
