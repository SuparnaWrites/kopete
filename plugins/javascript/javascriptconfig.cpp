
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "javascriptconfig.h"

#include "javascriptfile.h"

#include "kopeteaccount.h"
#include "kopeteuiglobal.h"

#include <kstandarddirs.h>

#include <kdebug.h>
#include <ksimpleconfig.h>
#include <klocale.h>
#include <kzip.h>
#include <kurl.h>
#include <ktemporaryfile.h>
#include <kdesktopfile.h>
#include <kmessagebox.h>

#include <qregexp.h>

struct JavaScriptConfig::Private
{
	KConfig *config;
	QMap< QString, JavaScriptFile * > scripts;
};

JavaScriptConfig* JavaScriptConfig::m_config = 0L;

JavaScriptConfig *JavaScriptConfig::instance()
{
	if( !m_config )
		m_config = new JavaScriptConfig(0L);
	return m_config;
}

JavaScriptConfig::JavaScriptConfig(QObject *parent)
	: QObject(parent)
	, MimeTypeHandler(false)
	, d(new JavaScriptConfig::Private)
{
	kDebug() << k_funcinfo;

	d->config = new KConfig("javascriptplugin.rc");

	foreach( const QString group, d->config->groupList() )
	{
		if( group.endsWith( "_Script" ) )
		{
			d->config->setGroup( group );
			JavaScriptFile *s = new JavaScriptFile(this);
			s->id = d->config->readEntry("ID", QString::number( time( NULL ) ) );
			s->name = d->config->readEntry("Name", "" );
			s->description = d->config->readEntry("Description", "" );
			s->author = d->config->readEntry("Author", "Unknown" );
			s->version = d->config->readEntry("Version", "Unknown" );
			s->fileName = d->config->readEntry("FileName", "");
			s->accounts = d->config->readEntry("Accounts", QStringList());
			foreach( const QString &function, d->config->readEntry("Functions", QStringList()) )
				s->functions.insert( function, d->config->readEntry( function + "_Function", QString() ) );
			s->immutable = d->config->entryIsImmutable( group );

			d->scripts.insert( s->id, s );
		}
	}

	//Handler for script packages
	registerAsMimeHandler( QLatin1String("application/x-kopete-javascript") );
	registerAsMimeHandler( QLatin1String("application/zip") );
}

JavaScriptConfig::~JavaScriptConfig()
{
	apply();
	delete d->config;

	delete d;
}

QList<JavaScriptFile *> JavaScriptConfig::allScripts() const
{
	return d->scripts.values();
}

bool JavaScriptConfig::signalsEnabled() const
{
	d->config->setGroup("Global");
	return d->config->readEntry( QLatin1String("SignalsEnabled"), true );
}

void JavaScriptConfig::setSignalsEnabled( bool val )
{
	d->config->setGroup("Global");
	d->config->writeEntry( QLatin1String("signalsEnabled"), val );
}

bool JavaScriptConfig::writeEnabled() const
{
	d->config->setGroup("Global");
	return d->config->readEntry( QLatin1String("writeEnabled"), true );
}

void JavaScriptConfig::setWriteEnabled( bool val )
{
	d->config->setGroup("Global");
	d->config->writeEntry( QLatin1String("writeEnabled"), val );
}

bool JavaScriptConfig::treeEnabled() const
{
	d->config->setGroup("Global");
	return d->config->readEntry( QLatin1String("treeEnabled"), true );
}

void JavaScriptConfig::setTreeEnabled( bool val )
{
	d->config->setGroup("Global");
	d->config->writeEntry( QLatin1String("treeEnabled"), val );
}

bool JavaScriptConfig::factoryEnabled() const
{
	d->config->setGroup("Global");
	return d->config->readEntry( QLatin1String("factoryEnabled"), true );
}

void JavaScriptConfig::setFactoryEnabled( bool val )
{
	d->config->setGroup("Global");
	d->config->writeEntry( QLatin1String("factoryEnabled"), val );
}

void JavaScriptConfig::apply()
{
	foreach( JavaScriptFile *s, d->scripts )
	{
		d->config->setGroup( s->id + "_Script" );
		d->config->writeEntry("ID", s->id );
		d->config->writeEntry("Name", s->name );
		d->config->writeEntry("Description", s->description );
		d->config->writeEntry("Author", s->author );
		d->config->writeEntry("Version", s->version );
		d->config->writeEntry("FileName", s->fileName );
		d->config->writeEntry("Functions", s->functions.keys() );
		d->config->writeEntry("Accounts", s->accounts );
		for( QMap<QString,QString>::iterator it2 = s->functions.begin(); it2 != s->functions.end(); ++it2 )
			d->config->writeEntry( it2.key() + "_Function", it2.value() );
	}

	d->config->sync();

	emit changed();
}

JavaScriptFile *JavaScriptConfig::addScript( const QString &fileName, const QString &name, const QString &description,
	const QString &author, const QString &version, const QMap<QString,QString> &functions,
	const QString &id )
{
	JavaScriptFile *s = new JavaScriptFile(this);
	s->id = id;
	s->name = name;
	s->fileName = fileName;
	s->description = description;
	s->author = author;
	s->version = version;
	s->functions = functions;
	s->immutable = false;
	d->scripts.insert( s->id, s );

	return s;
}

JavaScriptFile* JavaScriptConfig::script( const QString &id )
{
	return d->scripts[id];
}

void JavaScriptConfig::removeScript( const QString &id )
{
	d->scripts.remove( id );
}

QList<JavaScriptFile *> JavaScriptConfig::scriptsFor( Kopete::Account *account )
{
	QString key;
	if( account )
		key = account->accountId();
	else
		key = "GLOBAL_SCRIPT";

	QList<JavaScriptFile *> retVal;
	foreach( JavaScriptFile *scriptFile, d->scripts )
	{
		kDebug() << scriptFile->accounts;
		if( scriptFile->accounts.contains( key ) || scriptFile->accounts.contains( "GLOBAL_SCRIPT" ) )
			retVal.append( scriptFile );
	}

	return retVal;
}

void JavaScriptConfig::setScriptEnabled( Kopete::Account *account, const QString &script, bool enabled )
{
	QString key;
	if( account )
		key = account->accountId();
	else
		key = "GLOBAL_SCRIPT";

	kDebug() << k_funcinfo << key << " " << script << " " << enabled;

	JavaScriptFile *scriptPtr = d->scripts[script];
	if( scriptPtr )
	{
		if( scriptPtr->accounts.contains( script ) )
		{
			if( !enabled )
				scriptPtr->accounts.removeAll( key );
		}
		else if( enabled )
		{
			scriptPtr->accounts.append( key );
		}
	}
	else
	{
		kError() << k_funcinfo << script << " is not a valid script!" << endl;
	}
}

void JavaScriptConfig::installPackage( const QString &archiveName, bool &retVal )
{
	retVal = false;
	QString localScriptsDir( KStandardDirs::locateLocal("data", QLatin1String("kopete/scripts")) );

	if(localScriptsDir.isEmpty())
	{
		KMessageBox::queuedMessageBox(
			Kopete::UI::Global::mainWidget(),
			KMessageBox::Error, i18n("Could not find suitable place " \
			"to install scripts into.")
		);
		return;
	}

	KZip archive( archiveName );
	if ( !archive.open(QIODevice::ReadOnly) )
	{
		KMessageBox::queuedMessageBox(
			Kopete::UI::Global::mainWidget(),
			KMessageBox::Error,
			i18n("Could not open \"%1\" for unpacking.", archiveName )
		);
		return;
	}

	const KArchiveDirectory* rootDir = archive.directory();
	QStringList desktopFiles = rootDir->entries().filter( QRegExp( QString::fromLatin1("^(.*)\\.desktop$") ) );

	if( desktopFiles.size() == 1 )
	{
		const KArchiveFile *manifestEntry = static_cast<const KArchiveFile*>( rootDir->entry( desktopFiles[0] ) );
		if( manifestEntry )
		{
			KTemporaryFile manifest;
			manifest.open();
			manifestEntry->copyTo( manifest.fileName() );
			KDesktopFile manifestFile( manifest.fileName() );

			if( manifestFile.readType() == QLatin1String("KopeteScript") )
			{
#ifdef __GNUC__
#warning find a better uinique id
#endif
				QString id = QString::number( time( NULL ) );
				QString dir = localScriptsDir + QChar('/') + id;
				rootDir->copyTo( dir );
				KSimpleConfig conf( dir + QChar('/') + manifestFile.readUrl() );

				QMap<QString,QString> functions;

				foreach( const QString &function, conf.readEntry("Functions", QStringList()) )
					functions.insert( function, conf.readEntry( function + "_Function", QString()));

				addScript( conf.readEntry("FileName"), conf.readEntry("Name"),
					conf.readEntry("Description"), conf.readEntry("Author"),
					conf.readEntry("Version"), functions, id );

				retVal = true;
				return;
			}
		}
	}

	KMessageBox::queuedMessageBox(
		Kopete::UI::Global::mainWidget(),
		KMessageBox::Error, i18n("The file \"%1\" is not a valid Kopete script package.",
		                         archiveName)
	);
}

void JavaScriptConfig::handleURL( const QString &, const KUrl &url ) const
{
	bool retVal = false;
	const_cast<JavaScriptConfig*>(this)->installPackage( url.path(), retVal );
}

#include "javascriptconfig.moc"
