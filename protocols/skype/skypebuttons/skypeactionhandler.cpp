/*  Skype Action Handler for Kopete
    Copyright (C) 2009 Pali Rohár <pali.rohar@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <QCoreApplication>
#include <QObject>
#include <QtDBus>
#include <QProcess>
#include <QTimer>
#include <QEventLoop>

#define KOPETE_INTERFACE "org.kde.kopete", "/Kopete", "org.kde.Kopete"
#define SKYPE_INTERFACE "org.kde.kopete", "/SkypeActionHandler", "org.kde.kopete"

int main(int argc, char** argv) {
	QCoreApplication app(argc, argv);

	if ( app.arguments().count() < 2 )//Empty message
		return 1;

	QDBusInterface kopete(KOPETE_INTERFACE);

	if ( ! kopete.isValid() ) {//Check if Kopete is running
		QProcess process;
		process.start("kopete");//Start Kopete
		process.waitForStarted();
		if ( process.error() == QProcess::FailedToStart )//Cant start Kopete
			return 1;
		process.waitForFinished();
		QDBusInterface kopete(KOPETE_INTERFACE);
		if ( ! kopete.isValid() )//Cant connect to Kopete
			return 1;
	}

	if ( ! QDBusReply <QStringList> (kopete.call("accounts")).value().contains("Skype") ) {//Check if account Skype exist
		//TODO: Add Skype account to continue
		return 1;
	}

	if ( ! QDBusReply <bool> (kopete.call("isConnected", "Skype", "Skype")).value() ) {//Check if account Skype is connected
		kopete.call("connect", "Skype", "Skype");//try connect Skype

		int count = 60;
		QEventLoop * eventloop = new QEventLoop;
		QTimer * timer = new QTimer;

		QObject::connect( timer, SIGNAL(timeout()), eventloop, SLOT(quit()) );
		timer->start(500);

		bool ok = false;
		while ( count > 0 ) {//Check every 500ms if account Skype is connected, if no try connect
			eventloop->exec();
			if ( ! QDBusReply <bool> (kopete.call("isConnected", "Skype", "Skype")).value() ) {
				kopete.call("connect", "Skype", "Skype");
				--count;
			} else {
				ok = true;
				break;
			}
		}

		QObject::disconnect( timer, SIGNAL(timeout()), eventloop, SLOT(quit()) );
		timer->stop();
		delete timer;
		delete eventloop;

		if ( ! ok )//Cant login to Skype
			return 1;

		//Wait 1s while SkypeActionHandler register to dbus
		eventloop = new QEventLoop;
		QTimer::singleShot( 1000, eventloop, SLOT(quit()) );
		eventloop->exec();
		delete eventloop;
	}

	QDBusInterface skype(SKYPE_INTERFACE);

	if ( ! skype.isValid() )//Cant connect to Skype protocol
		return 1;

	skype.call("SkypeActionHandler", app.arguments().at(1));//Send message to Kopete

	return 0;
}