/*
    kopeteviewplugin.cpp - View Manager

    Copyright (c) 2005      by Jason Keirstead       <jason@keirstead.org>

    *************************************************************************
    *                                                                       *
    * This library is free software; you can redistribute it and/or         *
    * modify it under the terms of the GNU Lesser General Public            *
    * License as published by the Free Software Foundation; either          *
    * version 2 of the License, or (at your option) any later version.      *
    *                                                                       *
    *************************************************************************
*/

#include "kopeteviewplugin.h"

Kopete::ViewPlugin::ViewPlugin( KInstance *instance, QObject *parent, const char *name ) :
	Kopete::Plugin( instance, parent, name )
{

}

void Kopete::ViewPlugin::aboutToUnload()
{
	emit readyForUnload();
}
