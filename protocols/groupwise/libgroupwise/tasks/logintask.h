//
// C++ Interface: logintask
//
// Description: 
//
//
// Author: SUSE AG (C) 2004
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef LOGINTASK_H
#define LOGINTASK_H

#include "requesttask.h"

using namespace GroupWise;

/**
@author Kopete Developers
*/
class LoginTask : public RequestTask
{
Q_OBJECT
public:
	LoginTask( Task * parent );
	~LoginTask();
	/**
	 * Get the login fields ready to go
	 */
	void initialise();
	/**
	 * Only accepts the contactlist that comes back from the server, 
	 * processes it and notifies the client of the contactlist
	 */
	bool take( Transfer * transfer );
protected:
	void extractFolder( Field::MultiField * folderContainer );
	void extractContact( Field::MultiField * contactContainer );
	ContactDetails extractUserDetails( Field::FieldList & fields );
	void extractPrivacy( Field::FieldList & fields );
	QStringList readPrivacyItems( const QCString & tag, Field::FieldList & fields );

signals:
	void gotMyself( const ContactDetails & );
	void gotFolder( const FolderItem & );
	void gotContact( const ContactItem & );
	void gotContactUserDetails( const ContactDetails & );
	void gotPrivacySettings( bool locked, bool defaultDeny, const QStringList & allowList, const QStringList & denyList );
};

#endif
