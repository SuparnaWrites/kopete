/***************************************************************************
                          imchatservice.cpp  -  description
                             -------------------
    begin                : Tue Nov 27 2001
    copyright            : (C) 2001 by Olaf Lueg
    email                : olueg@olsd.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include "msnswitchboardsocket.h"
#include "msnprotocol.h"
#include "msncontact.h"
#include "msnfiletransfersocket.h"
#include "kopete.h"
#include "kopetetransfermanager.h"
#include <time.h>
// qt
#include <qdatetime.h>
#include <qfont.h>
#include <qcolor.h>
// kde
#include <klocale.h>
#include <kglobal.h>
#include <kdebug.h>
#include <kmessagebox.h>



MSNSwitchBoardSocket::MSNSwitchBoardSocket(int _id)
{
	mId=_id;
}

MSNSwitchBoardSocket::~MSNSwitchBoardSocket()
{
}

void MSNSwitchBoardSocket::connectToSwitchBoard(QString ID, QString address, QString auth)
{
	QString server = address.left( address.find( ":" ) );
	uint port = address.right( address.length() - address.findRev( ":" ) - 1 ).toUInt();

	QObject::connect( this, SIGNAL( blockRead( const QString & ) ),
		this, SLOT(slotReadMessage( const QString & ) ) );

	QObject::connect( this, SIGNAL( onlineStatusChanged( MSNSocket::OnlineStatus ) ),
		this, SLOT( slotOnlineStatusChanged( MSNSocket::OnlineStatus ) ) );

	QObject::connect( this, SIGNAL( socketClosed( int ) ),
		this, SLOT( slotSocketClosed( int ) ) );

	connect( server, port );

	// we need these for the handshake later on (when we're connected)
	m_ID = ID;
	m_auth = auth;

}

void MSNSwitchBoardSocket::handleError( uint code, uint id )
{
	switch( code )
	{
		case 208:
		{
			QString msg = i18n( "Invalid user! \n"
				"This MSN user does not exist. Please check the MSN ID" );
			KMessageBox::error( 0, msg, i18n( "MSN Plugin - Kopete" ) );
			break;
		}
		case 217:
		{
			// TODO: we need to know the nickname instead of the handle.
			QString msg = i18n( "The user %1 is currently not signed in.\n"
				"Messages will not be delivered." ).arg( m_msgHandle );
			KMessageBox::error( 0, msg, i18n( "MSN Plugin - Kopete" ) );
			slotSocketClosed( 0 ); // emit signal to get ourselves removed...
			break;
		}
		default:
			MSNSocket::handleError( code, id );
			break;
	}
}

void MSNSwitchBoardSocket::parseCommand( const QString &cmd, uint /* id */,
	const QString &data )
{
	kdDebug() << "MSNSwitchBoardSocket::parseCommand" << endl;

	if( cmd == "NAK" )
	{
		emit msgAcknowledgement(false);    // msg was not accepted
	}
	else if( cmd == "ACK" )
	{
		emit msgAcknowledgement(true);   // msg has received
	}
	else if( cmd == "JOI" )
	{
		// new user joins the chat, update user in chat list
		emit switchBoardIsActive(true);   
		QString handle = data.section( ' ', 0, 0 );
		QString screenname = data.section( ' ', 1, 1 );
    emit updateChatMember( handle, screenname, true, this );
    
		if( !m_chatMembers.contains( handle ) )
			m_chatMembers.append( handle );

    if(!m_messagesQueue.empty()) sendMessageQueue();
	}
	else if( cmd == "IRO" )
	{
		// we have joined a multi chat session- this are the users in this chat
    emit switchBoardIsActive(true);
		QString handle = data.section( ' ', 2, 2 );  
		if( !m_chatMembers.contains( handle ) )
			m_chatMembers.append( handle );

      
    QString screenname = data.section( ' ', 3, 3);
		emit updateChatMember( handle,  screenname, true, this);
	}
	else if( cmd == "USR" )
	{
		callUser();
	}
	else if( cmd == "BYE" )
	{
		// some has disconnect from chat, update user in chat list
		QString handle = data.section( ' ', 0, 0 ).replace(
			QRegExp( "\r\n" ), "" );

		userLeftChat(handle);

	}
	else if( cmd == "MSG" )
	{
		kdDebug() << "MSNSwitchBoardSocket::parseCommand: Received MSG: " << endl;
		QString len = data.section( ' ', 2, 2 );

		// we need to know who's sending is the block...
		m_msgHandle = data.section( ' ', 0, 0 );

		readBlock(len.toUInt());
	}
}
void MSNSwitchBoardSocket::slotReadMessage( const QString &msg )
{
	kdDebug() << "MSNSwitchBoardSocket::slotReadMessage" << endl;

	// incoming message for File-transfer
	if( msg.contains("Content-Type: text/x-msmsgsinvite; charset=UTF-8") )
	{
		// filetransfer  
		if( msg.contains("Invitation-Command: ACCEPT") )
		{
			QString ip_adress = msg.right( msg.length() - msg.find( "IP-Address:" ) - 12 );
			ip_adress.truncate( ip_adress.find("\r\n") );
			QString authcook = msg.right( msg.length() - msg.find(  "AuthCookie:" ) - 12 );
			authcook.truncate( authcook.find("\r\n") );
			QString port = msg.right( msg.length() - msg.find(  "Port:" ) - 6 );
			port.truncate( port.find("\r\n") );

			kdDebug() << "MSNSwitchBoardSocket::slotReadMessage : filetransfer: - ip:" <<ip_adress <<" : " <<port <<" -authcook: " <<authcook<<  endl;

			MSNFileTransferSocket *MFTS=new  MSNFileTransferSocket(m_myHandle,authcook,m_filetransferName);
			MFTS->setKopeteTransfer(kopeteapp->transferManager()->addTransfer(MSNProtocol::protocol()->contacts()[ m_msgHandle ]->metaContact(),m_filetransferName,0,i18n("Kopete")));
			MFTS->connect(ip_adress, port.toUInt());

			m_lastId++;  //FIXME:  there is no ACK for prev command ; without m_lastId++, future messages are queued  (MSNSocket::m_lastId should be private)
  
		}
		else  if( msg.contains("Application-File:") )  //not "Application-Name: File Transfer" because the File Transfer label is sometimes translate 
		{ 
			QString cookie = msg.right( msg.length() - msg.find( "Invitation-Cookie:" ) - 19 );
			cookie.truncate( cookie.find("\r\n") );
			QString filename = msg.right( msg.length() - msg.find( "Application-File:" ) - 18 );
			filename.truncate( filename.find("\r\n") );
			QString filesize = msg.right( msg.length() - msg.find( "Application-FileSize:" ) - 22 );
			filesize.truncate( filesize.find("\r\n") );

			kdDebug() << "MSNSwitchBoardService::slotReadMessage: " <<
				"invitation cookie: " << cookie << endl;

			QString contact = MSNProtocol::protocol()->contacts()[ m_msgHandle ]->displayName();
			QString txt = i18n("%1 tried to send you a file.\n"
				"Name: %2 \nSize: %3 bytes\n"
				"Would you like to accept?\n").arg( contact).arg( filename).arg( filesize );

			int r=KMessageBox::questionYesNo (0l, txt, i18n( "MSN Plugin - Kopete" ), i18n( "Accept" ), i18n( "Refuse" ));
       
			if(r== KMessageBox::Yes)
			{
				QCString message=QString(
					"MIME-Version: 1.0\r\n"
					"Content-Type: text/x-msmsgsinvite; charset=UTF-8\r\n"
					"\r\n"
					"Invitation-Command: ACCEPT\r\n"
					"Invitation-Cookie: " + cookie + "\r\n"
					"Launch-Application: FALSE\r\n"
					"Request-Data: IP-Address:\r\n"//192.168.0.2\r\n" // hardcoded IP?!
					"Port: 6891").utf8();
				QCString command=QString("MSG").utf8();
				QCString args = QString( "N" ).utf8();
				sendCommand( command , args, true, message );
				m_filetransferName=filename;
                               
			}
			else
			{
				QCString message=QString(
					"MIME-Version: 1.0\r\n"
					"Content-Type: text/x-msmsgsinvite; charset=UTF-8\r\n"
					"\r\n"
					"Invitation-Command: CANCEL\r\n"
					"Invitation-Cookie: " + cookie + "\r\n"
					"Cancel-Code: REJECT").utf8();
				QCString command=QString("MSG").utf8();
				QCString args = QString( "N" ).utf8();
				sendCommand( command , args, true, message );
			}
		}
	}
	else if(msg.contains("MIME-Version: 1.0\r\nContent-Type: text/x-msmsgscontrol\r\nTypingUser:"))
	{
		QString message;
		message = msg.right(msg.length() - msg.findRev(" ")-1);
		message = message.replace(QRegExp("\r\n"),"");
		emit userTypingMsg(message);    // changed 20.10.2001
	}
	else// if(msg.contains("Content-Type: text/plain;"))
	{
		// Some MSN Clients (like CCMSN) don't like to stick to the rules.
		// In case of CCMSN, it doesn't send what the content type is when
		// sending a text message. So if it's not supplied, we'll just
		// assume its that.

		QColor fontColor;
		QFont font;

		if ( msg.contains( "X-MMS-IM-Format" ) )
		{
			QString fontName;
			QString fontInfo;
			QString color;
			int pos1 = msg.find( "X-MMS-IM-Format" ) + 15;

			fontInfo = msg.mid(pos1, msg.find("\r\n\r\n") - pos1 );
			color = parseFontAttr(fontInfo, "CO");

			// FIXME: this is so BAAAAAAAAAAAAD :(
			if (!color.isEmpty())
			{
				if ( color.length() == 2) // only #RR (red color) given
					fontColor.setRgb(
						color.mid(0,2).toInt(0,16),
						0,
						0);
				else if ( color.length() == 4) // #GGRR (green, red) given.
				{
					fontColor.setRgb(
						color.mid(2,2).toInt(0,16),
						color.mid(0,2).toInt(0,16),
						0);
				}
				else if ( color.length() == 6) // full #BBGGRR given
				{
					fontColor.setRgb(
						color.mid(4,2).toInt(0, 16),
						color.mid(2,2).toInt(0,16),
						color.mid(0,2).toInt(0,16));
				}
			}

			// FIXME: The below regexps do work, but are quite ugly.
			// Reason is that a \1 inside the replacement string is
			// not possible.
			// When importing kopete into kdenetwork, convert this to
			// KRegExp3 from libkdenetwork, which does exactly this.
			fontName = fontInfo.replace(
				QRegExp( ".*FN=" ), "" ).replace(
				QRegExp( ";.*" ), "" ).replace( QRegExp( "%20" ), " " );

			if( !fontName.isEmpty() )
			{
				kdDebug() << "MSNSwitchBoardService::slotReadMessage: Font: '" <<
					fontName << "'" << endl;

				font = QFont( fontName,
					parseFontAttr( fontInfo, "PF" ).toInt(), // font size
					parseFontAttr( fontInfo, "EF" ).contains( 'B' ) ? QFont::Bold : QFont::Normal,
					parseFontAttr( fontInfo, "EF" ).contains( 'I' ) ? true : false );
			}
		}

		kdDebug() << "MSNSwitchBoardService::slotReadMessage: Message: " <<
			endl << msg.right( msg.length() - msg.find("\r\n\r\n") - 4) <<
			endl;

		kdDebug() << "MSNSwitchBoardService::slotReadMessage: User handle: "
			<< m_msgHandle << endl;

		KopeteContactPtrList others;
    others.append( MSNProtocol::protocol()->myself() );
    QStringList::iterator it;
    for ( it = m_chatMembers.begin(); it != m_chatMembers.end(); ++it )
       if(*it != m_msgHandle) others.append( MSNProtocol::protocol()->contacts()[*it] );

			KopeteMessage kmsg(
			MSNProtocol::protocol()->contacts()[ m_msgHandle ] , others,
			msg.right( msg.length() - msg.find("\r\n\r\n") - 4 ),
			KopeteMessage::Inbound );

		kmsg.setFg( fontColor );
		kmsg.setFont( font );

		emit msgReceived( kmsg );
	}
}

// this sends the user is typing msg
void MSNSwitchBoardSocket::slotTypingMsg()
{
	QCString message = QString("MIME-Version: 1.0\r\n"
		"Content-Type: text/x-msmsgscontrol\r\n"
		"TypingUser: " + m_myHandle + "\r\n"
		"\r\n").utf8();

	// Length is appended by sendCommand()
	QString args = "U";
	sendCommand( "MSG", args, true, message );
}

// this Invites an Contact
void MSNSwitchBoardSocket::slotInviteContact(QString handle)
{
	sendCommand( "CAL", handle );
}

// this sends a short message to the server
void MSNSwitchBoardSocket::slotSendMsg( const KopeteMessage &msg )
{
	if ( onlineStatus() != Connected || m_chatMembers.empty())
  {
      m_messagesQueue.append(msg);
		return;
  }


	kdDebug() << "MSNSwitchBoardSocket::slotSendMsg" << endl;

	QString head =
		"MIME-Version: 1.0\r\n"
		"Content-Type: text/plain; charset=UTF-8\r\n"
		"X-MMS-IM-Format: FN=MS%20Serif; EF=; ";

	// Color support
	if (msg.fg().isValid()) {
		QString colorCode = QColor(msg.fg().blue(),msg.fg().green(),msg.fg().red()).name().remove(0,1);  //colours aren't sent in RGB but in BGR (O.G.)
		head += "CO=" + colorCode;
	} else {
		head += "CO=0";
	}

	head += "; CS=0; PF=0\r\n"
		"\r\n";

	head += msg.body().replace( QRegExp( "\n" ), "\r\n" );
	QString args = "A";
	sendCommand( "MSG", args, true, head );

	// TODO: send our fonts as well
	KopeteContactPtrList others;
	others.append( MSNProtocol::protocol()->contacts()[ m_myHandle ] );
	emit msgReceived( msg );    // send the own msg to chat window
}

void MSNSwitchBoardSocket::slotSocketClosed( int /*state */)
{
	// we have lost the connection, send a message to chatwindow (this will not displayed)
	emit switchBoardIsActive(false);
	emit switchBoardClosed( this );

}

void MSNSwitchBoardSocket::slotCloseSession()
{
	sendCommand( "OUT", QString::null, false );
}

void MSNSwitchBoardSocket::callUser()
{
	sendCommand( "CAL", m_msgHandle );
}

// Check if we are connected. If so, then send the handshake.
void MSNSwitchBoardSocket::slotOnlineStatusChanged( MSNSocket::OnlineStatus status )
{
	if ( status != Connected )
		return;

	QCString command;
	QString args;

	if( !m_ID ) // we're inviting
	{
		command = "USR";
		args = m_myHandle + " " + m_auth;
	}
	else // we're invited
	{
		command = "ANS";
		args = m_myHandle + " " + m_auth + " " + m_ID;
	}
	
	sendCommand( command, args );
	
	// send active message
	emit switchBoardIsActive(true);

}


void MSNSwitchBoardSocket::sendMessageQueue() //O.G.
{
	if ( onlineStatus() != Connected || m_chatMembers.empty())
		return;

	for ( QValueList<KopeteMessage>::iterator it = m_messagesQueue.begin(); it!=m_messagesQueue.end(); it = m_messagesQueue.begin() )
	{
		slotSendMsg( *it)  ;
		m_messagesQueue.remove(it);
	}
}

void MSNSwitchBoardSocket::userLeftChat( QString handle ) //O.G.
{
		emit updateChatMember( handle, QString::null, false, this );
		if( m_chatMembers.contains( handle ) )
			m_chatMembers.remove( handle );

		kdDebug() << "MSNSwitchBoardSocket::parseCommand: " <<
			handle << " left the chat." << endl;

		if(m_chatMembers.empty()) disconnect();
}


// FIXME: This is nasty... replace with a regexp or so.
QString MSNSwitchBoardSocket::parseFontAttr(QString str, QString attr)
{
	QString tmp;
	int pos1=0, pos2=0;

	pos1 = str.find(attr + "=");

	if (pos1 == -1)
		return "";

	pos2 = str.find(";", pos1+3);

	if (pos2 == -1)
		tmp = str.mid(pos1+3, str.length() - pos1 - 3);
	else
		tmp = str.mid(pos1+3, pos2 - pos1 - 3);

	return tmp;
}

#include "msnswitchboardsocket.moc"



/*
 * Local variables:
 * c-indentation-style: k&r
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
// vim: set noet ts=4 sts=4 sw=4:

