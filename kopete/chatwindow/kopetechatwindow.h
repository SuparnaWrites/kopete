/*
    kopetechatwindow.h - Chat Window

    Copyright (c) 2002      by Olivier Goffart        <ogoffart@tiscalinet.be>
    Copyright (c) 2004      by Martijn Klingens       <klingens@kde.org>

    Kopete    (c) 2002-2004 by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#ifndef KOPETECHATWINDOW_H
#define KOPETECHATWINDOW_H

#include <kparts/mainwindow.h>
#include <qmovie.h>
#include "kopetecontact.h"
#include "kdeversion.h"

class KAction;
class KToggleAction;
class KActionMenu;
class KTempFile;
class QPixmap;
class QTabWidget;
class KSqueezedTextLabel;
class KPushButton;
class QVBox;
class QVBoxLayout;
class QFrame;
class KTabWidget;
class QLabel;
class KopeteEmoticonAction;
class KopeteView;
class KSelectAction;
class ChatView;

namespace Kopete
{
class Message;
class MessageManager;
class Contact;
class Protocol;
typedef QPtrList<Contact>  ContactPtrList;
}

class KopeteChatWindow : public KParts::MainWindow
{
	Q_OBJECT

	enum {NEW_WINDOW, GROUP_BY_ACCOUNT, GROUP_ALL, GROUP_BY_GROUP, GROUP_BY_METACONTACT};

public:
	/**
	 * Find the appropriate window for a ChatView of the given protocol to
	 * dock into. If no such window exists, create one.
	 * @param protocol The protocol we are creating a view for
	 * @return A KopeteChatWindow suitable for docking a ChatView into. Guaranteed
	 *  to be a valid pointer.
	 */
	static KopeteChatWindow *window( Kopete::MessageManager *manager );
	~KopeteChatWindow();

	/**
	 * Attach an unattached chatview to this window
	 * @param chat The chat view to attach
	 */
	void attachChatView( ChatView *chat );

	/**
	 * Detach a chat view from this window
	 * @param chat The chat view to detach
	 */
	void detachChatView( ChatView *chat );

	/**
	 * Returns the number of chat views attached to this window
	 */
	const int chatViewCount() { return chatViewList.count(); }

	/**
	 * Returns the chatview in the currently active tab, or the only chat view
	 * if chatViewCount() == 1
	 */
	ChatView *activeView();

	void updateMembersActions();
	void setStatus( const QString & );

	KTempFile *backgroundFile;
	QPtrList<ChatView> chatViewList;

private:
	// All KopeteChatWindows are created by the findWindow function
	KopeteChatWindow( QWidget *parent = 0, const char* name = "KopeteChatWindow" );

	/**
	 * The window list has changed:
	 * For each chat window, update it's Move Tab to Window action
	 */
	static void windowListChanged();

	void initActions(void);
	void saveOptions(void);
	void readOptions(void);
	void checkDetachEnable();
	void createTabBar();
	void deleteTabBar();
	void addTab( ChatView* );
	void setPrimaryChatView( ChatView* );
	const QString fileContents( const QString &file ) const;

	ChatView *m_activeView;
	ChatView *m_popupView;
	bool m_alwaysShowTabs;
	bool updateBg;
	KTabWidget *m_tabBar;
	KPushButton *m_button_send;
	KSqueezedTextLabel *m_status_text;
	QVBoxLayout *mainLayout;
	QFrame *mainArea;
	QLabel *anim;
	QMovie animIcon;
	QPixmap normalIcon;

	KAction *chatSend;
	KAction *historyUp;
	KAction *historyDown;
	KAction *nickComplete;

#if !KDE_IS_VERSION( 3, 1, 90 )
	KToggleAction *mStatusbarAction;
#endif

	KAction *tabLeft;
	KAction *tabRight;
	KAction *tabDetach;
	KAction* tabClose;

	KToggleAction* membersLeft;
	KToggleAction* membersRight;
	KToggleAction* toggleMembers;

	KopeteEmoticonAction *actionSmileyMenu;
	KActionMenu *actionActionMenu;
	KActionMenu *actionContactMenu;
	KActionMenu *actionDetachMenu;
	KActionMenu *actionTabPlacementMenu;
	QString statusMsg;

signals:
	void closing( KopeteChatWindow* );

public slots:
	void slotSmileyActivated( const QString & );
	void setActiveView( QWidget *active );
	void updateBackground( const QPixmap &pm );

private slots:
//	void slotPrepareSmileyMenu();
	void slotPrepareContactMenu();
	void slotPrepareDetachMenu();
	void slotPreparePlacementMenu();
	void slotUpdateSendEnabled();

	void slotCut();
	void slotCopy();
	void slotPaste();

	void slotSetBgColor();
	void slotSetFgColor();
	void slotSetFont();

	void slotHistoryUp();
	void slotHistoryDown();

	void slotSendMessage();
	void slotChatSave();
	void slotChatPrint();

	void slotPreviousTab();
	void slotNextTab();
	void slotDetachChat( int newWindowIndex = -1 );
	void slotPlaceTabs( int tabPlacement );

	void slotViewMenuBar();
	void slotToggleStatusBar();

	void slotConfKeys();
	void slotConfToolbar();

	void slotViewMembersLeft();
	void slotViewMembersRight();
	void slotToggleViewMembers();
	void slotEnableUpdateBg() { updateBg = true; }

	void slotSetCaption( bool );
	void slotUpdateCaptionIcons( const ChatView * );
	void slotChatClosed();
	void slotTabContextMenu( QWidget*, const QPoint & );
	void slotStopAnimation( ChatView* );
	void slotNickComplete();
	void slotCloseChat( QWidget* );

protected:
	virtual void closeEvent( QCloseEvent *e );
	virtual void windowActivationChange( bool );
	virtual bool eventFilter( QObject *object, QEvent *event );
};

#endif

// vim: set noet ts=4 sts=4 sw=4:

