/*
    kirccontext.cpp - IRC Context

    Copyright (c) 2005-2007 by Michel Hermier <michel.hermier@gmail.com>

    Kopete    (c) 2005-2007 by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include "kirccontext.moc"

#include "kircentity.h"

using namespace KIrc;

class KIrc::ContextPrivate
{
public:
	ContextPrivate()
		: defaultCodec(0)
	{ }

	QList<Entity *> entities;

	QTextCodec *defaultCodec;
};

Context::Context(QObject *parent)
	: Handler(parent)
	, d_ptr(new ContextPrivate)
{
}

Context::~Context()
{
	delete d_ptr;
}
/*
QList<KIrc::Entity *> Context::anonymous() const
{

}
*/
QList<KIrc::Entity *> Context::entities() const
{
	return findChildren<Entity *>();
}

KIrc::Entity *Context::entityFromName(const QByteArray &name)
{
	Q_D(Context);
	Entity *entity = 0;

	if (name.isEmpty())
		return entity;

	//TODO: optimize this using Hash or something
	foreach( Entity* e, d->entities )
	{
		if ( e->name()==name )
		{
			entity=e;
			break;
		}
	}

	if (!entity)
	{
		entity = new Entity(this);
		entity->setName(name);
		add( entity );
	}

	return entity;
}

QList<KIrc::Entity *> Context::entitiesFromNames(const QList<QByteArray> &names)
{
	QList<Entity *> entities;
	Entity *entity;

	// This is slow and can easily be optimised with sorted lists
	foreach (const QByteArray &name, names)
	{
		entity = entityFromName(name);
		if (entity)
			entities.append(entity);
//		else
//			warn the user
	}

	return entities;
}

QList<KIrc::Entity *> Context::entitiesFromNames(const QByteArray &names, char sep)
{
	return entitiesFromNames(names.split(sep));
}

QTextCodec *Context::defaultCodec() const
{
	Q_D(const Context);

	return d->defaultCodec;
}

void Context::setDefaultCodec(QTextCodec *defaultCodec)
{
	Q_D(Context);

	d->defaultCodec = defaultCodec;
}

void Context::postEvent(QEvent *event)
{
//	delete event;
	emit ircEvent( event );
}

void Context::add(Entity *entity)
{
	Q_D(Context);
	if (!d->entities.contains(entity))
	{
		d->entities.append(entity);
		connect(entity, SIGNAL(destroyed(KIrc::Entity *)),
			this, SLOT(remove(KIrc::Entity *)));
	}
}

void Context::remove(Entity *entity)
{
	Q_D(Context);
	d->entities.removeAll(entity);
//	disconnect(entity);
}


#if 0
Status Context::SET()
{
}
#endif
