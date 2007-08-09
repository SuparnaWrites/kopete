/*
	oscarclientstream.h - Kopete Oscar Protocol
	
	Copyright (c) 2004 Matt Rogers <mattr@kde.org>
	Copyright (c) 2007 Roman Jarosz <kedgedev@centrum.cz>
	
	Based on code Copyright (c) 2004 SuSE Linux AG <http://www.suse.com>
	Based on Iris, Copyright (C) 2003  Justin Karneges
	
	Kopete (c) 2002-2007 by the Kopete developers <kopete-devel@kde.org>
	
	*************************************************************************
	*                                                                       *
	* This library is free software; you can redistribute it and/or         *
	* modify it under the terms of the GNU Lesser General Public            *
	* License as published by the Free Software Foundation; either          *
	* version 2 of the License, or (at your option) any later version.      *
	*                                                                       *
	*************************************************************************
*/

#ifndef OSCARCLIENTSTREAM_H
#define OSCARCLIENTSTREAM_H

#include "stream.h"
#include "liboscar_export.h"

#include <QtNetwork/QAbstractSocket>

class Connection;

class LIBOSCAR_EXPORT ClientStream : public Stream
{
	Q_OBJECT
public:
	explicit ClientStream( QAbstractSocket *socket, QObject *parent = 0 );
	~ClientStream();

	void connectToServer( const QString& host, quint16 port );

	bool isOpen() const;

	void close();

	/** Connection related stuff */
	void setConnection( Connection* c );
	Connection* connection() const;

	/**
	 * Are there any messages waiting to be read
	 */
	bool transfersAvailable() const;

	/**
	 * Read a message received from the server
	 */
	Transfer * read();

	/**
	 * Send a message to the server
	 */
	void write( Transfer* request );

	int error() const;
	QString errorString() const;

	void setNoopTime( int mills );

Q_SIGNALS:
	void connected();

private Q_SLOTS:
	/**
	 * collects wire ready outgoing data from the core protocol and sends
	 */ 
	void cp_outgoingData( const QByteArray& );
	/**
	 * collects parsed incoming data as a transfer from the core protocol and queues
	 */
	void cp_incomingData();

	void socketConnected();
	void socketDisconnected();
	void socketError( QAbstractSocket::SocketError );
	void socketReadyRead();
	void socketBytesWritten( qint64 );

	void doNoop();
	void doReadyRead();
	
private:
	class Private;
	Private *d;

	void processNext();
};

#endif