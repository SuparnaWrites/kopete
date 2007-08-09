/*
    nowlisteningplugin.cpp

    Kopete Now Listening To plugin

    Copyright (c) 2002,2003,2004,2005,2006,2007 by Will Stephenson <wstephenson@kde.org>
    Copyright (c) 2005-2006           by Michaël Larouche <larouche@kde.org>

    Kopete    (c) 2002,2003,2004,2005,2006,2007      by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include <QTimer>
#include <QStringList>
#include <QList>
#include <QRegExp>

#include <kdebug.h>
#include <kgenericfactory.h>
#include <kaction.h>

#include "config-kopete.h"
#include "kopetechatsessionmanager.h"
#include "kopetemetacontact.h"
#include "kopetecontact.h"
#include "kopetecommandhandler.h"
#include "kopeteaccount.h"
#include "kopeteprotocol.h"
#include "kopeteaccountmanager.h"
#include "kopetestatusmessage.h"

#include "nowlisteningconfig.h"
#include "nowlisteningplugin.h"
#include "nlmediaplayer.h"
#include "nlkscd.h"
#include "nlnoatun.h"
#include "nljuk.h"
#include "nlamarok.h"
#include "nlkaffeine.h"
#include "nlquodlibet.h"

#include "nowlisteningguiclient.h"

#if defined Q_WS_X11 && !defined K_WS_QTONLY && defined HAVE_XMMS
#include "nlxmms.h"
#endif

class NowListeningPlugin::Private
{
public:
	Private() : m_currentMediaPlayer(0L), m_currentChatSession(0L), m_currentMetaContact(0L),
				advertTimer(0L)
	{}
	~Private()
	{
		qDeleteAll(m_mediaPlayerList);
	}

	// abstracted media player interfaces
	QList<NLMediaPlayer*> m_mediaPlayerList;
	NLMediaPlayer *m_currentMediaPlayer;

	Kopete::ChatSession *m_currentChatSession;
	Kopete::MetaContact *m_currentMetaContact;

	// Used when using automatic advertising to know who has already gotten
	// the music information
	QStringList m_musicSentTo;

	// Used when advertising to status message.
	QTimer *advertTimer;
};

typedef KGenericFactory<NowListeningPlugin> NowListeningPluginFactory;
K_EXPORT_COMPONENT_FACTORY( kopete_nowlistening, NowListeningPluginFactory( "kopete_nowlistening" )  )

NowListeningPlugin::NowListeningPlugin( QObject *parent, const QStringList& /*args*/ )
: Kopete::Plugin( NowListeningPluginFactory::componentData(), parent )
{
	if ( pluginStatic_ )
		kDebug( 14307 )<<"####"<<"Now Listening already initialized";
	else
		pluginStatic_ = this;

	d = new Private;

	kDebug(14307) << k_funcinfo;

	// Connection for the "/media" command (always needed)
	connect( Kopete::ChatSessionManager::self(), SIGNAL(
			chatSessionCreated( Kopete::ChatSession * )) , SLOT( slotNewKMM(
			Kopete::ChatSession * ) ) );

	// If autoadvertising is on...
	connect(Kopete::ChatSessionManager::self(),
			SIGNAL(aboutToSend(Kopete::Message&)),
			this,
			SLOT(slotOutgoingMessage(Kopete::Message&)));

	QList<Kopete::ChatSession*> sessions = Kopete::ChatSessionManager::self()->sessions();
	for (QList<Kopete::ChatSession*>::Iterator it= sessions.begin(); it!=sessions.end() ; ++it)
	  slotNewKMM( *it );

	// set up known media players
	d->m_mediaPlayerList.append( new NLKscd() );
	d->m_mediaPlayerList.append( new NLNoatun() );
	d->m_mediaPlayerList.append( new NLJuk() );
	d->m_mediaPlayerList.append( new NLamaroK() );
	d->m_mediaPlayerList.append( new NLKaffeine() );
	d->m_mediaPlayerList.append( new NLQuodLibet() );

#if defined Q_WS_X11 && !defined K_WS_QTONLY && HAVE_XMMS
	d->m_mediaPlayerList.append( new NLXmms() );
#endif

	// User has selected a specific mediaPlayer so update the currentMediaPlayer pointer.
	if( NowListeningConfig::self()->useSpecifiedMediaPlayer() )
	{
		updateCurrentMediaPlayer();
	}

	// watch for '/media' getting typed
	Kopete::CommandHandler::commandHandler()->registerCommand(
		this,
		"media",
		SLOT( slotMediaCommand( const QString &, Kopete::ChatSession * ) ),
		i18n("USAGE: /media - Displays information on current song"),
		0
	);

	connect ( this , SIGNAL( settingsChanged() ) , this , SLOT( slotSettingsChanged() ) );

	// Advert the accounts with the current listened track.
	d->advertTimer = new QTimer(this);
	connect(d->advertTimer, SIGNAL( timeout() ), this, SLOT( slotAdvertCurrentMusic() ) );
	d->advertTimer->start(5000); // Update every 5 seconds
}

NowListeningPlugin::~NowListeningPlugin()
{
	//kDebug( 14307 ) << k_funcinfo;

	delete d;

	pluginStatic_ = 0L;
}

void NowListeningPlugin::slotNewKMM(Kopete::ChatSession *KMM)
{
	new NowListeningGUIClient( KMM, this );
}

NowListeningPlugin* NowListeningPlugin::plugin()
{
	return pluginStatic_ ;
}

void NowListeningPlugin::slotMediaCommand( const QString &args, Kopete::ChatSession *theChat )
{
	QString advert = mediaPlayerAdvert();
	if ( advert.isEmpty() )
	{
		// Catch no players/no track playing message case:
		// Since we can't stop a message send in a plugin, add some message text to
		// prevent us sending an empty message
		advert = i18nc("Message from Kopete user to another user; used when sending media information even though there are no songs playing or no media players running", "Now Listening for Kopete - it would tell you what I am listening to, if I was listening to something on a supported media player.");
	}

	Kopete::Message msg( theChat->myself(),theChat->members() );
	msg.setPlainBody( advert + ' ' + args );
	msg.setDirection( Kopete::Message::Outbound );

	theChat->sendMessage( msg );
}

void NowListeningPlugin::slotOutgoingMessage(Kopete::Message& msg)
{
	// Only do stuff if autoadvertising is on
	if(!NowListeningConfig::self()->chatAdvertising())
		return;

	QString originalBody = msg.plainBody();

	// If it is a /media message, don't process it
	if(originalBody.startsWith(NowListeningConfig::self()->header()))
		return;

	// What will be sent
	QString newBody;

	// Getting the list of contacts the message will be sent to to determine if at least
	// one of them has never gotten the current music information.
	Kopete::ContactPtrList dest = msg.to();
	bool mustSendAnyway = false;

	foreach( Kopete::Contact *c, dest )
	{
		const QString& cId = c->contactId();
		if( 0 == d->m_musicSentTo.contains( cId ) )
		{
			mustSendAnyway = true;

			// The contact will get the music information so we put it in the list.
			d->m_musicSentTo.push_back( cId );
		}
	}

	bool newTrack = newTrackPlaying();

	// We must send the music information if someone has never gotten it or the track(s)
	// has changed since it was last sent.
	if ( mustSendAnyway || newTrack )
	{
		QString advert = mediaPlayerAdvert(false); // false since newTrackPlaying() did the update
		if( !advert.isEmpty() )
			newBody = originalBody + "<br>" + advert;

		// If we send because the information has changed since it was last sent, we must
		// rebuild the list of contacts the latest information was sent to.
		if( newTrack )
		{
			d->m_musicSentTo.clear();
			foreach( Kopete::Contact *c, dest )
			{
				d->m_musicSentTo.push_back( c->contactId() );
			}
		}
	}

	// If the body has been modified, change the message
	if( !newBody.isEmpty() )
	{
		msg.setHtmlBody( newBody );
 	}
}

void NowListeningPlugin::slotAdvertCurrentMusic()
{
	// Do anything when statusAdvertising is off.
	if( !NowListeningConfig::self()->statusAdvertising() && !NowListeningConfig::self()->appendStatusAdvertising() )
		return;

	// This slot is called every 5 seconds, so we check if we have a new track playing.
	if( newTrackPlaying() )
	{
		QString advert;

		QString track, artist, album;
		bool isPlaying=false;

		if( NowListeningConfig::self()->useSpecifiedMediaPlayer() && d->m_currentMediaPlayer )
		{
			if( d->m_currentMediaPlayer->playing() )
			{
				track = d->m_currentMediaPlayer->track();
				artist = d->m_currentMediaPlayer->artist();
				album = d->m_currentMediaPlayer->album();
				isPlaying = true;
			}
		}
		else
		{
			foreach( NLMediaPlayer *i, d->m_mediaPlayerList )
			{
				if( i->playing() )
				{
					track = i->track();
					artist = i->artist();
					album = i->album();
					isPlaying = true;
				}
			}
		}

		QList<Kopete::Account*> accountsList = Kopete::AccountManager::self()->accounts();
		foreach( Kopete::Account* a, accountsList )
		{
			Kopete::StatusMessage currentStatusMessage = a->myself()->statusMessage();

			if(isPlaying)
			{
				currentStatusMessage.addMetaData("title", track);
				currentStatusMessage.addMetaData("artist", artist);
				currentStatusMessage.addMetaData("album", album);
			}

			if( NowListeningConfig::self()->appendStatusAdvertising() )
			{
				// Check for the now listening message in parenthesis,
				// include the header to not override other messages in parenthesis.
				QRegExp statusSong( QString("\\((%1[^.]*)\\)").arg( NowListeningConfig::header()) );

				// HACK: Don't keep appending the now listened song. Replace it in the status message.
				advert = currentStatusMessage.message();
				// Remove the braces when they are no listened song.
				QString mediaAdvert = mediaPlayerAdvert(false);
				if(!mediaAdvert.isEmpty())
				{
					if(statusSong.indexIn(advert) != -1)
					{
						advert = advert.replace(statusSong, QString("(%1)").arg(mediaPlayerAdvert(false)) );
					}
					else
					{
						advert += QString("(%1)").arg( mediaPlayerAdvert(false) );
					}
				}
				else
				{
					advert = advert.replace(statusSong, "");
				}
			}
			else
			{
				advert = mediaPlayerAdvert(false); // newTrackPlaying has done the update.
			}
			currentStatusMessage.setMessage( advert );

			a->setStatusMessage(currentStatusMessage);
		}
	}
}

QString NowListeningPlugin::mediaPlayerAdvert(bool update)
{
	// generate message for all players
	QString message;

	if( NowListeningConfig::self()->useSpecifiedMediaPlayer() && d->m_currentMediaPlayer != 0L )
	{
		buildTrackMessage(message, d->m_currentMediaPlayer, update);
	}
	else
	{
		foreach( NLMediaPlayer* i, d->m_mediaPlayerList )
		{
			buildTrackMessage(message, i, update);
		}
	}

	kDebug( 14307 ) << k_funcinfo << message;

	return message;
}

void NowListeningPlugin::buildTrackMessage(QString &message, NLMediaPlayer *player, bool update)
{
	QString perTrack = NowListeningConfig::self()->perTrack();

	if(update)
		player->update();
	if ( player->playing() )
	{
		kDebug( 14307 ) << k_funcinfo << player->name() << " is playing";
		if ( message.isEmpty() )
			message = NowListeningConfig::self()->header();

		if ( message != NowListeningConfig::self()->header() ) // > 1 track playing!
			message = message + NowListeningConfig::self()->conjunction();
		message = message + substDepthFirst( player, perTrack, false );
	}
}

bool NowListeningPlugin::newTrackPlaying(void) const
{
	if( NowListeningConfig::self()->useSpecifiedMediaPlayer() && d->m_currentMediaPlayer != 0L )
	{
		d->m_currentMediaPlayer->update();
		if( d->m_currentMediaPlayer->newTrack() )
		return true;
	}
	else
	{
		foreach( NLMediaPlayer* i, d->m_mediaPlayerList )
		{
			i->update();
			if( i->newTrack() )
			return true;
		}
	}
	return false;
}

QString NowListeningPlugin::substDepthFirst( NLMediaPlayer *player,
		QString in, bool inBrackets ) const
{
	QString track = player->track();
	QString artist = player->artist();
	QString album = player->album();
	QString playerName = player->name();

	for ( int i = 0; i < in.length(); i++ )
	{
		QChar c = in.at( i );
		//kDebug(14307) << "Now working on:" << in << " char is: " << c;
		if ( c == '(' )
		{
			// find matching bracket
			int depth = 0;
			//kDebug(14307) << "Looking for ')'";
			for ( unsigned int j = i + 1; j < (uint)in.length(); j++ )
			{
				QChar d = in.at( j );
				//kDebug(14307) << "Got " << d;
				if ( d == '(' )
					depth++;
				if ( d == ')' )
				{
					// have we found the match?
					if ( depth == 0 )
					{
						// recursively replace contents of matching ()
						QString substitution = substDepthFirst( player,
								in.mid( i + 1, j - i - 1), true ) ;
						in.replace ( i, j - i + 1, substitution );
						// perform substitution and return the result
						i = i + substitution.length() - 1;
						break;
					}
					else
						depth--;
				}
			}
		}
	}
	// no () found, perform substitution!
	// get each string (to) to substitute for (from)
	bool done = false;
	if ( in.contains ( "%track" ) )
	{
		if ( track.isEmpty() )
			track = i18n("Unknown track");

		in.replace( "%track", track );
		done = true;
	}

	if ( in.contains ( "%artist" ) && !artist.isEmpty() )
	{
		if ( artist.isEmpty() )
			artist = i18n("Unknown artist");
		in.replace( "%artist", artist );
		done = true;
	}
	if ( in.contains ( "%album" ) && !album.isEmpty() )
	{
		if ( album.isEmpty() )
			album = i18n("Unknown album");
		in.replace( "%album", album );
		done = true;
	}
	if ( in.contains ( "%player" ) && !playerName.isEmpty() )
	{
		if ( playerName.isEmpty() )
			playerName = i18n("Unknown player");
		in.replace( "%player", playerName );
		done = true;
	}
	// make whether we return anything dependent on whether we
	// were in brackets and if we were, if a substitution was made.
	if ( inBrackets && !done )
		return "";

	return in;
}

void NowListeningPlugin::advertiseToChat( Kopete::ChatSession *theChat, QString message )
{
	Kopete::ContactPtrList pl = theChat->members();

	// get on with it
	kDebug(14307) << k_funcinfo <<
		( pl.isEmpty() ? "has no " : "has " ) << "interested recipients: " << endl;
/*	for ( pl.first(); pl.current(); pl.next() )
		kDebug(14307) << "NowListeningPlugin::advertiseNewTracks() " << pl.current()->displayName(); */
	// if no-one in this KMM wants to be advertised to, don't send
	// any message
	if ( pl.isEmpty() )
		return;
	Kopete::Message msg( theChat->myself(), pl );
	msg.setHtmlBody( message );
	msg.setDirection( Kopete::Message::Outbound );

	theChat->sendMessage( msg );
}

void NowListeningPlugin::updateCurrentMediaPlayer()
{
	kDebug(14307) << k_funcinfo << "Update current media player (single mode)";

	d->m_currentMediaPlayer = d->m_mediaPlayerList.at( NowListeningConfig::self()->selectedMediaPlayer() );
}

void NowListeningPlugin::slotSettingsChanged()
{
	// Force reading config
	NowListeningConfig::self()->readConfig();

	// Update the currentMediaPlayer, because config has changed.
	if( NowListeningConfig::useSpecifiedMediaPlayer() )
		updateCurrentMediaPlayer();

	disconnect(Kopete::ChatSessionManager::self(),
			   SIGNAL(aboutToSend(Kopete::Message&)),
			   this,
			   SLOT(slotOutgoingMessage(Kopete::Message&)));

	d->advertTimer->stop();
	disconnect(d->advertTimer, SIGNAL(timeout()), this, SLOT(slotAdvertCurrentMusic()));

	if( NowListeningConfig::self()->chatAdvertising() )
	{
		kDebug(14307) << k_funcinfo << "Now using chat window advertising.";

		connect(Kopete::ChatSessionManager::self(),
				SIGNAL(aboutToSend(Kopete::Message&)),
				this,
				SLOT(slotOutgoingMessage(Kopete::Message&)));
	}
	else if( NowListeningConfig::self()->statusAdvertising() || NowListeningConfig::self()->appendStatusAdvertising() )
	{
		kDebug(14307) << k_funcinfo << "Now using status message advertising.";

		connect(d->advertTimer, SIGNAL(timeout()), this, SLOT(slotAdvertCurrentMusic()));
		d->advertTimer->start(5000);
	}
}

NowListeningPlugin* NowListeningPlugin::pluginStatic_ = 0L;

#include "nowlisteningplugin.moc"

// vim: set noet ts=4 sts=4 sw=4: