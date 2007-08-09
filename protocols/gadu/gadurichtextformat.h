// -*- Mode: c++-mode; c-basic-offset: 2; indent-tabs-mode: t; tab-width: 2; -*-
//
// Copyright (C) 2004 Grzegorz Jaskiewicz <gj at pointblue.com.pl>
//
// gadurichtextformat.h
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301, USA.


#ifndef GADURTF_H
#define GADURTF_H

#include <libgadu.h>

#include <QtGui/QColor>
class QString;

namespace Kopete { class Message; }
class KGaduMessage;

class GaduRichTextFormat {
public:
	GaduRichTextFormat();
	~GaduRichTextFormat();
	QString convertToHtml( const QString&, unsigned int, void* );
	KGaduMessage* convertToGaduMessage( const Kopete::Message& );

private:
	QString formatOpeningTag( const QString& , const QString& = QString() );
	QString formatClosingTag( const QString& );
	bool insertRtf( uint );
	QString unescapeGaduMessage( QString& );
	void parseAttributes( const QString, const QString );
	QString escapeBody( QString& );
	QColor color;
	gg_msg_richtext_format	rtfs;
	gg_msg_richtext_color	rtcs;
	gg_msg_richtext*	header;
	QByteArray		rtf;

};
#endif