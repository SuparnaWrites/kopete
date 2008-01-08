/*
    Kopete Contactlist Model

    Copyright (c) 2007      by Matt Rogers            <mattr@kde.org>

    Kopete    (c) 2002-2007 by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#ifndef KOPETE_UI_CONTACTLISTPROXYMODEL_H
#define KOPETE_UI_CONTACTLISTPROXYMODEL_H

#include <QSortFilterProxyModel>

namespace Kopete {

class Group;
class Contactlist;
class MetaContact;

namespace UI {

/**
@author Aleix Pol <aleixpol@gmail.com>
*/
class ContactListProxyModel : public QSortFilterProxyModel
{
Q_OBJECT
	public:
		ContactListProxyModel(QObject* parent = 0);
		~ContactListProxyModel();
		
	public slots:
		void slotConfigChanged();
	protected:
		bool filterAcceptsRow ( int source_row, const QModelIndex & source_parent ) const;
		bool showOffline;
		bool showEmptyFolders;
};

}
}

#endif