 /*
  * jabberjinglesession.cpp - A Jingle session.
  *
  * Copyright (c) 2008 by Detlev Casanova <detlev.casanova@gmail.com>
  *
  * Kopete    (c) by the Kopete developers  <kopete-devel@kde.org>
  *
  * *************************************************************************
  * *                                                                       *
  * * This program is free software; you can redistribute it and/or modify  *
  * * it under the terms of the GNU General Public License as published by  *
  * * the Free Software Foundation; either version 2 of the License, or     *
  * * (at your option) any later version.                                   *
  * *                                                                       *
  * *************************************************************************
  */
#include "jabberjinglesession.h"
#include "jabberjinglecontent.h"
#include "jinglemediamanager.h"

#include "jinglesession.h"
#include "jinglecontent.h"

#include <KDebug>

//using namespace XMPP;

JabberJingleSession::JabberJingleSession(JingleCallsManager* parent)
: m_callsManager(parent)
{
	qDebug() << "Created a new JabberJingleSession";
}

JabberJingleSession::~JabberJingleSession()
{
	for (int i = 0; i < jabberJingleContents.count(); i++)
		delete jabberJingleContents[i];
	delete m_jingleSession;
}

void JabberJingleSession::setJingleSession(XMPP::JingleSession* sess)
{
	qDebug() << "Setting JingleSession in the JabberJingleSession :" << (unsigned int) sess;
	m_jingleSession = sess;
	connect(sess, SIGNAL(needData(XMPP::JingleContent*)), this, SLOT(writeRtpData(XMPP::JingleContent*)));
	//connect(sess, SIGNAL(destroyed()), this, SIGNAL(sessionTerminated()));
	// Create Contents :
	for (int i = 0; i < sess->contents().count(); i++)
	{
		JabberJingleContent *jContent = new JabberJingleContent(this, sess->contents()[i]);
		jabberJingleContents << jContent;
	}
}

void JabberJingleSession::setMediaManager(JingleMediaManager* mm)
{
	kDebug(KDE_DEFAULT_DEBUG_AREA) << "Setting media manager. ( address =" << (int) mm << ")";
	m_mediaManager = mm;
	//FIXME:Could be accessed with m_callsManager.
}

void JabberJingleSession::writeRtpData(XMPP::JingleContent* content)
{
	qDebug() << "Called void JabberJingleSession::writeRtpData(XMPP::JingleContent* content)";
	JabberJingleContent *jContent = contentWithName(content->name());
	if (jContent == 0)
	{
		jContent = new JabberJingleContent(this, content);
		jabberJingleContents << jContent;
	}
	jContent->startWritingRtpData();
	//FIXME:need different m_rtpSession for each content.
}

JabberJingleContent *JabberJingleSession::contentWithName(const QString& name)
{
	for (int i = 0; i < jabberJingleContents.count(); i++)
	{
		if (jabberJingleContents[i]->contentName() == name)
			return jabberJingleContents[i];
	}
	return 0;
}

JingleMediaManager *JabberJingleSession::mediaManager() const
{
	kDebug(KDE_DEFAULT_DEBUG_AREA) << "m_mediaManager is" << (m_mediaManager == 0 ? "Null" : "Valid");
	return m_mediaManager;
}

QTime JabberJingleSession::upTime()
{
	//TODO:Implement me !
	return QTime(0, 0);
}

