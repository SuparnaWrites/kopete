// -*- Mode: c++-mode; c-basic-offset: 2; indent-tabs-mode: t; tab-width: 2; -*-
//
// Copyright (C) 2003 Grzegorz Jaskiewicz 	<gj at pointblue.com.pl>
//
// gadueditaccount.cpp
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
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.

#include "gadueditaccount.h"
#include "gaduaccount.h"
#include "gaduprotocol.h"

#include <qradiobutton.h>
#include <qcombobox.h>
#include <qlabel.h>
#include <qstring.h>
#include <qcheckbox.h>
#include <qlineedit.h>
#include <qbutton.h>
#include <qregexp.h>
#include <qpushbutton.h>

#include <klineedit.h>
#include <kmessagebox.h>
#include <kdebug.h>
#include <klocale.h>

GaduEditAccount::GaduEditAccount( GaduProtocol* proto, KopeteAccount* ident, QWidget* parent, const char* name )
: GaduAccountEditUI( parent, name ), KopeteEditAccountWidget( ident ), protocol_( proto ), rcmd( 0 )
{

#ifdef __GG_LIBGADU_HAVE_OPENSSL
	isSsl = true;
#else
	isSsl = false;
#endif

	useTls_->setDisabled( false );

	if ( account() == NULL ) {
		useTls_->setCurrentItem( isSsl ? 0 : 2 );
		registerNew->setEnabled( true );
	}
	else {
		registerNew->setDisabled( true );
		loginEdit_->setDisabled( true );
		loginEdit_->setText( account()->accountId() );

		if ( account()->rememberPassword() ) {
			passwordEdit_->setText( account()->password()  );
		}
		else {
			passwordEdit_->setText( "" );
		}

		nickName->setText( account()->myself()->displayName() );

		rememberCheck_->setChecked( account()->rememberPassword() );
		autoLoginCheck_->setChecked( account()->autoLogin() );
		useTls_->setCurrentItem( isSsl ?  ( static_cast<GaduAccount*> (account()) ->useTls() ) : 2 );
	}
	
	QObject::connect( registerNew, SIGNAL( clicked( ) ), SLOT( registerNewAccount( ) ) );
}

void GaduEditAccount::registerNewAccount()
{
	registerNew->setDisabled( true );
	regDialog = new GaduRegisterAccount( NULL , "Register account dialog" );
	connect( regDialog, SIGNAL( registeredNumber( unsigned int, QString  ) ), SLOT( newUin( unsigned int, QString  ) ) );
	if ( regDialog->exec() != QDialog::Accepted ){
		registerNew->setDisabled( false );
		loginEdit_->setText( "" );
		rememberCheck_->setChecked( true );
		passwordEdit_->setText( "" );
		return;
	}
}

void GaduEditAccount::registrationFailed()
{
	KMessageBox::sorry( this, i18n( "<b>Registration FAILED.</b>" ), i18n( "Gadu-Gadu" ) );
}

void GaduEditAccount::newUin( unsigned int uni, QString password ) 
{
	loginEdit_->setText( QString::number( uni ) );
	passwordEdit_->setText( password );
}

bool GaduEditAccount::validateData()
{

	if ( loginEdit_->text().isEmpty() ) {
		KMessageBox::sorry( this, i18n( "<b>Enter UIN please.</b>" ), i18n( "Gadu-Gadu" ) );
		return false;
	}

	if ( loginEdit_->text().toInt() < 0 || loginEdit_->text().toInt() == 0 ) {
		KMessageBox::sorry( this, i18n( "<b>UIN should be a positive number.</b>" ), i18n( "Gadu-Gadu" ) );
		return false;
	}

	if ( passwordEdit_->text().isEmpty() && rememberCheck_->isChecked() ) {
		KMessageBox::sorry( this, i18n( "<b>Enter password please.</b>" ), i18n( "Gadu-Gadu" ) );
		return false;
	}

	return true;
}

KopeteAccount* GaduEditAccount::apply()
{
	if ( account() == NULL ) {
		setAccount( new GaduAccount( protocol_, loginEdit_->text() ) );
		account()->setAccountId( loginEdit_->text() );
		
	}

	account()->setAutoLogin( autoLoginCheck_->isChecked() );

	if( rememberCheck_->isChecked() && passwordEdit_->text().length() ) {
		account()->setPassword( passwordEdit_->text() );
	}
	else {
		account()->setPassword();
	}

	account()->myself()->rename( nickName->text() );

	// this is changed only here, so i won't add any proper handling now
	account()->setPluginData( account()->protocol(),  QString::fromAscii( "nickName" ), nickName->text() );

	account()->setAutoLogin( autoLoginCheck_->isChecked() );
	( static_cast<GaduAccount*> (account()) )->setUseTls( (GaduAccount::tlsConnection) useTls_->currentItem() );

	return account();
}

#include "gadueditaccount.moc"

