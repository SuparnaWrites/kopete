/*
    typingnotifytask.h  - Send/Recieve typing notifications

    Copyright (c) 2004 by Matt Rogers <mattr@kde.org>
    Kopete    (c) 2002-2004 by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include "typingnotifytask.h"

#include <qstring.h>
#include <kdebug.h>
#include "transfer.h"
#include "buffer.h"
#include "connection.h"




TypingNotifyTask::TypingNotifyTask( Task* parent )
: Task( parent )
{
	m_notificationType = 0x0000;
}

TypingNotifyTask::~TypingNotifyTask()
{
}

bool TypingNotifyTask::forMe( const Transfer* transfer ) const
{
	const SnacTransfer* st = dynamic_cast<const SnacTransfer*>( transfer );
	if ( !st )
		return false;
	
	if ( st->snacService() == 0x0004  && st->snacSubtype() == 0x0014 )
		return true;
	else
		return false;
}

bool TypingNotifyTask::take( Transfer* transfer )
{
	if ( forMe( transfer ) )
	{
		setTransfer( transfer );
		handleNotification();
		return true;
	}
	
	return false;
}

void TypingNotifyTask::onGo()
{
	FLAP f = { 0x02, client()->flapSequence(), 0 };
	SNAC s = { 0x0004, 0x0014, 0x0000, client()->snacSequence() };
	Buffer* b = new Buffer();
	
	//notification id cookie. it's a quad-word 
	b->addDWord( 0x00000000 );
	b->addDWord( 0x00000000 );
	
	b->addWord( 0x0001 ); //mtn messages are always sent as type 1 messages
	
	b->addBUIN( client()->userId().latin1() );
	
	b->addWord( m_notificationType );
	
	Transfer* t = createTransfer( f, s, b );
	send( t );
	
	setSuccess( 0, QString::null );
}

void TypingNotifyTask::handleNotification()
{
	Buffer* b = transfer()->buffer();
	
	//I don't care about the QWORD or the channel
	b->skipBytes( 10 );
	
	QString contact( b->getBUIN() );
	
	switch ( b->getWord() )
	{
	case 0x0000:
		kdDebug(OSCAR_RAW_DEBUG) << k_funcinfo << contact << " has finished typing" << endl;
		emit typingFinished( contact );
		break;
	case 0x0002:
		kdDebug(OSCAR_RAW_DEBUG) << k_funcinfo << contact << " has started typing" << endl;
		emit typingStarted( contact );
	}
}

#include "typingnotifytask.moc"

// kate: indent-mode csands; space-indent off; replace-tabs off;

