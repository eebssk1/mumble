/* Copyright (C) 2005-2007, Thorvald Natvig <thorvald@natvig.com>

   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   - Neither the name of the Mumble Developers nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "MainWindow.h"
#include "AudioWizard.h"
#include "AudioInput.h"
#include "ConnectDialog.h"
#include "Player.h"
#include "Channel.h"
#include "ACLEditor.h"
#include "BanEditor.h"
#include "Connection.h"
#include "ServerHandler.h"
#include "About.h"
#include "GlobalShortcut.h"
#include "VersionCheck.h"
#include "PlayerModel.h"
#include "AudioStats.h"
#include "Plugins.h"
#include "Log.h"
#include "Overlay.h"
#include "Global.h"
#include "Database.h"
#include "ViewCert.h"

MessageBoxEvent::MessageBoxEvent(QString m) : QEvent(static_cast<QEvent::Type>(MB_QEVENT)) {
	msg = m;
}

MainWindow::MainWindow(QWidget *p) : QMainWindow(p) {
	Channel::add(0, tr("Root"), NULL);

	aclEdit = NULL;
	banEdit = NULL;

	qtReconnect = new QTimer(this);
	qtReconnect->setInterval(10000);
	qtReconnect->setSingleShot(true);
	qtReconnect->setObjectName(QLatin1String("Reconnect"));

	createActions();
	setupUi(this);
	setupGui();

	connect(g.sh, SIGNAL(connected()), this, SLOT(serverConnected()));
	connect(g.sh, SIGNAL(disconnected(QString)), this, SLOT(serverDisconnected(QString)));
}

void MainWindow::createActions() {
	int idx = 1;
	gsPushTalk=new GlobalShortcut(this, idx++, tr("Push-to-Talk", "Global Shortcut"));
	gsPushTalk->setObjectName(QLatin1String("PushToTalk"));

	gsResetAudio=new GlobalShortcut(this, idx++, tr("Reset Audio Processor", "Global Shortcut"));
	gsResetAudio->setObjectName(QLatin1String("ResetAudio"));

	gsMuteSelf=new GlobalShortcut(this, idx++, tr("Toggle Mute Self", "Global Shortcut"));
	gsMuteSelf->setObjectName(QLatin1String("MuteSelf"));

	gsDeafSelf=new GlobalShortcut(this, idx++, tr("Toggle Deafen Self", "Global Shortcut"));
	gsDeafSelf->setObjectName(QLatin1String("DeafSelf"));

	gsUnlink=new GlobalShortcut(this, idx++, tr("Unlink Plugin", "Global Shortcut"));
	gsUnlink->setObjectName(QLatin1String("UnlinkPlugin"));

	gsCenterPos=new GlobalShortcut(this, idx++, tr("Force Center Position", "Global Shortcut"));
	gsCenterPos->setObjectName(QLatin1String("CenterPos"));

	GlobalShortcut *gs;

	gs = new GlobalShortcut(this, idx++, tr("Chan Parent", "Global Shortcut"));
	gs->setData(0);
	connect(gs, SIGNAL(triggered(bool)), this, SLOT(pushLink(bool)));

	for (int i = 1; i< 10;i++) {
		gs = new GlobalShortcut(this, idx++, tr("Chan Sub#%1", "Global Shortcut").arg(i));
		gs->setData(i);
		connect(gs, SIGNAL(triggered(bool)), this, SLOT(pushLink(bool)));
	}

	gs = new GlobalShortcut(this, idx++, tr("Chan All Subs", "Global Shortcut"));
	gs->setData(10);
	connect(gs, SIGNAL(triggered(bool)), this, SLOT(pushLink(bool)));

	gsPushMute=new GlobalShortcut(this, idx++, tr("Push-to-Mute", "Global Shortcut"));
	gsPushMute->setObjectName(QLatin1String("PushToMute"));

	gsMetaChannel=new GlobalShortcut(this, idx++, tr("Join Channel", "Global Shortcut"));
	gsMetaChannel->setObjectName(QLatin1String("MetaChannel"));

	gsToggleOverlay=new GlobalShortcut(this, idx++, tr("Toggle Overlay", "Global Shortcut"));
	gsToggleOverlay->setObjectName(QLatin1String("ToggleOverlay"));
	connect(gsToggleOverlay, SIGNAL(down()), g.o, SLOT(toggleShow()));

	gsAltTalk=new GlobalShortcut(this, idx++, tr("Alt Push-to-Talk", "Global Shortcut"));
	gsAltTalk->setObjectName(QLatin1String("AltPushToTalk"));

	qstiIcon = new QSystemTrayIcon(qApp->windowIcon(), this);
	qstiIcon->setObjectName(QLatin1String("Icon"));
	qstiIcon->show();
}

void MainWindow::setupGui()  {
	setWindowTitle(tr("Mumble -- %1").arg(QLatin1String(MUMBLE_RELEASE)));

	pmModel = new PlayerModel(qtvPlayers);

	qtvPlayers->setModel(pmModel);
	qtvPlayers->setItemDelegate(new PlayerDelegate(qtvPlayers));

	qaServerConnect->setShortcuts(QKeySequence::Open);
	qaServerDisconnect->setShortcuts(QKeySequence::Close);
	qaAudioMute->setChecked(g.s.bMute);
	qaAudioDeaf->setChecked(g.s.bDeaf);
	qaAudioTTS->setChecked(g.qs->value(QLatin1String("TextToSpeech"), true).toBool());
	qaAudioLocalDeafen->setChecked(g.s.bLocalDeafen);
	qaHelpWhatsThis->setShortcuts(QKeySequence::WhatsThis);


	connect(gsResetAudio, SIGNAL(down()), qaAudioReset, SLOT(trigger()));
	connect(gsMuteSelf, SIGNAL(down()), qaAudioMute, SLOT(trigger()));
	connect(gsDeafSelf, SIGNAL(down()), qaAudioDeaf, SLOT(trigger()));

	connect(gsUnlink, SIGNAL(down()), qaAudioUnlink, SLOT(trigger()));

	if (g.qs->value(QLatin1String("Horizontal"), true).toBool()) {
		qsSplit->setOrientation(Qt::Horizontal);
		qsSplit->addWidget(qteLog);
		qsSplit->addWidget(qtvPlayers);
	} else {
		qsSplit->setOrientation(Qt::Vertical);
		qsSplit->addWidget(qtvPlayers);
		qsSplit->addWidget(qteLog);
	}

	setCentralWidget(qsSplit);



	restoreGeometry(g.s.qbaMainWindowGeometry);
	restoreState(g.s.qbaMainWindowState);
	qsSplit->restoreState(g.s.qbaSplitterState);
}

void MainWindow::msgBox(QString msg) {
	MessageBoxEvent *mbe=new MessageBoxEvent(msg);
	QApplication::postEvent(this, mbe);
}

void MainWindow::closeEvent(QCloseEvent *e) {
	g.uiSession = 0;
	g.s.qbaMainWindowGeometry = saveGeometry();
	g.s.qbaMainWindowState = saveState();
	g.s.qbaSplitterState = qsSplit->saveState();
	QMainWindow::closeEvent(e);
	qApp->quit();
}

void MainWindow::hideEvent(QHideEvent *e) {
	if (qstiIcon->isSystemTrayAvailable())
		qApp->postEvent(this, new QEvent(static_cast<QEvent::Type>(TI_QEVENT)));
	QMainWindow::hideEvent(e);
}

void MainWindow::on_qtvPlayers_customContextMenuRequested(const QPoint &mpos) {
	QModelIndex idx = qtvPlayers->indexAt(mpos);
	if (! idx.isValid())
		idx = qtvPlayers->currentIndex();
	Player *p = pmModel->getPlayer(idx);

	if (p) {
		qmPlayer->popup(qtvPlayers->mapToGlobal(mpos), qaPlayerMute);
	} else {
		qmChannel->popup(qtvPlayers->mapToGlobal(mpos), qaChannelACL);
	}
}

void MainWindow::on_qtvPlayers_doubleClicked(const QModelIndex &idx) {
	Player *p = pmModel->getPlayer(idx);
	if (p) {
		on_qaPlayerTextMessage_triggered();
		return;
	}

	Channel *c = pmModel->getChannel(idx);
	if (!c)
		return;
	MessagePlayerMove mpm;
	mpm.uiVictim = g.uiSession;
	mpm.iChannelId = c->iId;
	g.sh->sendMessage(&mpm);
}

void MainWindow::openUrl(const QUrl &url) {
	g.l->log(Log::Information, tr("Opening URL %1").arg(url.toString()));
	if (url.scheme() != QLatin1String("mumble")) {
		g.l->log(Log::Warning, tr("URL scheme is not 'mumble'"));
		return;
	}
	QString host = url.host();
	int port = url.port(64738);
	QString user = url.userName();
	QString pw = url.password();
	qsDesiredChannel = url.path();

	if (user.isEmpty()) {
		bool ok;
		user = g.qs->value(QLatin1String("defUserName")).toString();
		user = QInputDialog::getText(this, tr("Connecting to %1").arg(url.toString()), tr("Enter username"), QLineEdit::Normal, user, &ok);
		if (! ok || user.isEmpty())
			return;

		g.qs->setValue(QLatin1String("defUserName"), user);
	}

	if (g.sh && g.sh->isRunning()) {
		on_qaServerDisconnect_triggered();
		g.sh->wait();
	}

	rtLast = MessageServerReject::None;
	qaServerDisconnect->setEnabled(true);
	g.sh->setConnectionInfo(host, port, user, pw);
	g.sh->start(QThread::TimeCriticalPriority);
}

void MainWindow::on_qaServerConnect_triggered() {
	ConnectDialog *cd = new ConnectDialog(this);
	int res = cd->exec();

	if (g.sh && g.sh->isRunning() && res == QDialog::Accepted) {
		on_qaServerDisconnect_triggered();
		g.sh->wait();
	}

	if (res == QDialog::Accepted) {
		qsDesiredChannel = QString();
		rtLast = MessageServerReject::None;
		qaServerDisconnect->setEnabled(true);
		g.sh->setConnectionInfo(cd->qsServer, cd->iPort, cd->qsUsername, cd->qsPassword);
		g.sh->start(QThread::TimeCriticalPriority);
	}
	delete cd;
}

void MainWindow::on_Reconnect_timeout() {
	g.l->log(Log::ServerDisconnected, tr("Reconnecting."));
	g.sh->start(QThread::TimeCriticalPriority);
}

void MainWindow::on_qaServerDisconnect_triggered() {
	if (qtReconnect->isActive())
		qtReconnect->stop();
	if (g.sh && g.sh->isRunning())
		g.sh->disconnect();
}

void MainWindow::on_qaServerBanList_triggered() {
	MessageServerBanList msbl;
	msbl.bQuery = true;
	g.sh->sendMessage(&msbl);

	if (banEdit) {
		banEdit->reject();
		delete banEdit;
		banEdit = NULL;
	}
}

void MainWindow::on_qaServerInformation_triggered() {
	ConnectionPtr c = g.sh->cConnection;

	if (! c)
		return;

	CryptState &cs = c->csCrypt;

	QSslCipher qsc = g.sh->qscCipher;

	QString qsControl=tr("<h2>Control channel</h2><p>Encrypted with %1 bit %2<br />%3 ms average latency (%4 variance)</p>").arg(qsc.usedBits()).arg(qsc.name()).arg(c->dTCPPingAvg, 0, 'f', 2).arg(c->dTCPPingVar / (c->uiTCPPackets - 1),0,'f',2);
	QString qsVoice, qsCrypt;
	if (g.s.bTCPCompat) {
		qsVoice = tr("Voice channel is sent over control channel.");
	} else {
		qsVoice = tr("<h2>Voice channel</h2><p>Encrypted with 128 bit OCB-AES128<br />%1 ms average latency (%4 variance)</p>").arg(c->dUDPPingAvg, 0, 'f', 2).arg(c->dUDPPingVar / (c->uiUDPPackets - 1),0,'f',2);
		qsCrypt = QString::fromLatin1("<h2>%1</h2><table><tr><th></th><th>%2</th><th>%3</th></tr>"
			     "<tr><th>%4</th><td>%8</td><td>%12</td></tr>"
			     "<tr><th>%5</th><td>%9</td><td>%13</td></tr>"
			     "<tr><th>%6</th><td>%10</td><td>%14</td></tr>"
			     "<tr><th>%7</th><td>%11</td><td>%15</td></tr>"
			     "</table>")
			     .arg(tr("UDP Statistics")).arg(tr("To Server")).arg(tr("From Server")).arg(tr("Good")).arg(tr("Late")).arg(tr("Lost")).arg(tr("Resync"))
			     .arg(cs.uiRemoteGood).arg(cs.uiRemoteLate).arg(cs.uiRemoteLost).arg(cs.uiRemoteResync)
			     .arg(cs.uiGood).arg(cs.uiLate).arg(cs.uiLost).arg(cs.uiResync);
	}

	QMessageBox qmb(QMessageBox::Information, tr("Mumble Server Information"), qsControl + qsVoice + qsCrypt, QMessageBox::Ok, this);
		qmb.setDefaultButton(QMessageBox::Ok);
		qmb.setEscapeButton(QMessageBox::Ok);

		QPushButton *qp = qmb.addButton(tr("&View Certificate"), QMessageBox::ActionRole);
		int res = qmb.exec();
		if ((res == 0) && (qmb.clickedButton() == qp)) {
			ViewCert vc(g.sh->qscCert, this);
			vc.exec();
		}
}

void MainWindow::on_qmPlayer_aboutToShow() {
	Player *p = pmModel->getPlayer(qtvPlayers->currentIndex());
	if (! p) {
		qaPlayerKick->setEnabled(false);
		qaPlayerBan->setEnabled(false);
		qaPlayerMute->setEnabled(false);
		qaPlayerLocalMute->setEnabled(false);
		qaPlayerDeaf->setEnabled(false);
		qaPlayerTextMessage->setEnabled(false);
	} else {
		qaPlayerKick->setEnabled(true);
		qaPlayerBan->setEnabled(true);
		qaPlayerMute->setEnabled(true);
		qaPlayerLocalMute->setEnabled(true);
		qaPlayerDeaf->setEnabled(true);
		qaPlayerMute->setChecked(p->bMute);
		qaPlayerLocalMute->setChecked(p->bLocalMute);
		qaPlayerDeaf->setChecked(p->bDeaf);
		qaPlayerTextMessage->setEnabled(true);
	}
}

void MainWindow::on_qaPlayerMute_triggered() {
	Player *p = pmModel->getPlayer(qtvPlayers->currentIndex());
	if (!p)
		return;

	MessagePlayerMute mpmMsg;
	mpmMsg.uiVictim = p->uiSession;
	mpmMsg.bMute = ! p->bMute;
	g.sh->sendMessage(&mpmMsg);
}

void MainWindow::on_qaPlayerLocalMute_triggered() {
	ClientPlayer *p = pmModel->getPlayer(qtvPlayers->currentIndex());
	if (!p)
		return;

	p->setLocalMute(qaPlayerLocalMute->isChecked());
}

void MainWindow::on_qaPlayerDeaf_triggered() {
	Player *p = pmModel->getPlayer(qtvPlayers->currentIndex());
	if (!p)
		return;

	MessagePlayerDeaf mpdMsg;
	mpdMsg.uiVictim = p->uiSession;
	mpdMsg.bDeaf = ! p->bDeaf;
	g.sh->sendMessage(&mpdMsg);
}

void MainWindow::on_qaPlayerKick_triggered() {
	Player *p = pmModel->getPlayer(qtvPlayers->currentIndex());
	if (!p)
		return;

	short session = p->uiSession;

	bool ok;
	QString reason = QInputDialog::getText(this, tr("Kicking player %1").arg(p->qsName), tr("Enter reason"), QLineEdit::Normal, QString(), &ok);

	p = ClientPlayer::get(session);
	if (!p)
		return;

	if (ok) {
		MessagePlayerKick mpkMsg;
		mpkMsg.uiVictim=p->uiSession;
		mpkMsg.qsReason = reason;
		g.sh->sendMessage(&mpkMsg);
	}
}

void MainWindow::on_qaPlayerBan_triggered() {
	Player *p = pmModel->getPlayer(qtvPlayers->currentIndex());
	if (!p)
		return;

	short session = p->uiSession;

	bool ok;
	QString reason = QInputDialog::getText(this, tr("Banning player %1").arg(p->qsName), tr("Enter reason"), QLineEdit::Normal, QString(), &ok);
	p = ClientPlayer::get(session);
	if (!p)
		return;

	if (ok) {
		MessagePlayerBan mpbMsg;
		mpbMsg.uiVictim=p->uiSession;
		mpbMsg.qsReason = reason;
		g.sh->sendMessage(&mpbMsg);
	}
}

void MainWindow::on_qaPlayerTextMessage_triggered() {
	Player *p = pmModel->getPlayer(qtvPlayers->currentIndex());

	if (!p)
		return;

	short session = p->uiSession;

	bool ok;
	QString message = QInputDialog::getText(this, tr("Sending message to %1").arg(p->qsName), tr("Enter message"), QLineEdit::Normal, QString(), &ok);
	p = ClientPlayer::get(session);
	if (!p)
		return;

	if (ok) {
		MessageTextMessage mtxt;
		mtxt.uiVictim = p->uiSession;
		mtxt.qsMessage = message;
		g.l->log(Log::TextMessage, tr("To %1: %2").arg(p->qsName).arg(mtxt.qsMessage), tr("Message to %1").arg(p->qsName));
		g.sh->sendMessage(&mtxt);
	}
}

void MainWindow::on_qaQuit_triggered() {
	qApp->closeAllWindows();
}

void MainWindow::on_qmChannel_aboutToShow() {
	QModelIndex idx = qtvPlayers->currentIndex();

	bool add, remove, acl, rename, link, unlink, unlinkall;

	add = remove = acl = rename = link = unlink = unlinkall = false;

	if (g.uiSession != 0) {
		add = true;
		acl = true;

		Channel *c = pmModel->getChannel(idx);
		Channel *home = ClientPlayer::get(g.uiSession)->cChannel;

		if (c && c->iId != 0) {
			rename = true;
			remove = true;
		}
		if (! c)
			c = Channel::get(0);

		unlinkall = (home->qhLinks.count() > 0);

		if (home != c) {
			if (c->allLinks().contains(home))
				unlink = true;
			else
				link = true;
		}
	}

	qaChannelAdd->setEnabled(add);
	qaChannelRemove->setEnabled(remove);
	qaChannelACL->setEnabled(acl);
	qaChannelRename->setEnabled(rename);
	qaChannelLink->setEnabled(link);
	qaChannelUnlink->setEnabled(unlink);
	qaChannelUnlinkAll->setEnabled(unlinkall);
}

void MainWindow::on_qaChannelAdd_triggered() {
	bool ok;
	Channel *c = pmModel->getChannel(qtvPlayers->currentIndex());
	int iParent = c ? c->iId : 0;
	QString name = QInputDialog::getText(this, tr("Mumble"), tr("Channel Name"), QLineEdit::Normal, QString(), &ok);

	c = Channel::get(iParent);
	if (! c)
		return;

	if (ok) {
		MessageChannelAdd mca;
		mca.qsName = name;
		mca.iParent = iParent;
		g.sh->sendMessage(&mca);
	}
}

void MainWindow::on_qaChannelRemove_triggered() {
	int ret;
	Channel *c = pmModel->getChannel(qtvPlayers->currentIndex());
	if (! c)
		return;

	int id = c->iId;

	ret=QMessageBox::question(this, tr("Mumble"), tr("Are you sure you want to delete %1 and all its sub-channels?").arg(c->qsName), QMessageBox::Yes, QMessageBox::No);

	c = Channel::get(id);
	if (!c)
		return;

	if (ret == QMessageBox::Yes) {
		MessageChannelRemove mcr;
		mcr.iId = c->iId;
		g.sh->sendMessage(&mcr);
	}
}

void MainWindow::on_qaChannelRename_triggered() {
	bool ok;
	Channel *c = pmModel->getChannel(qtvPlayers->currentIndex());
	if (! c)
		return;

	int id = c->iId;

	QString name = QInputDialog::getText(this, tr("Mumble"), tr("Channel Name"), QLineEdit::Normal, c->qsName, &ok);

	c = Channel::get(id);
	if (! c)
		return;

	if (ok) {
		MessageChannelRename mcr;
		mcr.iId = id;
		mcr.qsName = name;
		g.sh->sendMessage(&mcr);
	}
}

void MainWindow::on_qaChannelACL_triggered() {
	Channel *c = pmModel->getChannel(qtvPlayers->currentIndex());
	int id = c ? c->iId : 0;

	MessageEditACL mea;
	mea.iId = id;
	mea.bQuery = true;
	g.sh->sendMessage(&mea);

	if (aclEdit) {
		aclEdit->reject();
		delete aclEdit;
		aclEdit = NULL;
	}
}

void MainWindow::on_qaChannelLink_triggered() {
	Channel *c = ClientPlayer::get(g.uiSession)->cChannel;
	Channel *l = pmModel->getChannel(qtvPlayers->currentIndex());
	if (! l)
		l = Channel::get(0);

	MessageChannelLink mcl;
	mcl.iId = c->iId;
	mcl.qlTargets << l->iId;
	mcl.ltType = MessageChannelLink::Link;
	g.sh->sendMessage(&mcl);
}

void MainWindow::on_qaChannelUnlink_triggered() {
	Channel *c = ClientPlayer::get(g.uiSession)->cChannel;
	Channel *l = pmModel->getChannel(qtvPlayers->currentIndex());
	if (! l)
		l = Channel::get(0);

	MessageChannelLink mcl;
	mcl.iId = c->iId;
	mcl.qlTargets << l->iId;
	mcl.ltType = MessageChannelLink::Unlink;
	g.sh->sendMessage(&mcl);
}

void MainWindow::on_qaChannelUnlinkAll_triggered() {
	Channel *c = ClientPlayer::get(g.uiSession)->cChannel;

	MessageChannelLink mcl;
	mcl.iId = c->iId;
	mcl.ltType = MessageChannelLink::UnlinkAll;
	g.sh->sendMessage(&mcl);
}

void MainWindow::on_qaAudioReset_triggered() {
	AudioInputPtr ai = g.ai;
	if (ai)
		ai->bResetProcessor = true;
}

void MainWindow::on_qaAudioMute_triggered() {
	g.s.bMute = qaAudioMute->isChecked();
	if (! g.s.bMute && g.s.bDeaf) {
		g.s.bDeaf = false;
		qaAudioDeaf->setChecked(false);
		g.l->log(Log::SelfMute, tr("Unmuted and undeafened."));
	} else if (! g.s.bMute) {
		g.l->log(Log::SelfMute, tr("Unmuted."));
	} else {
		g.l->log(Log::SelfMute, tr("Muted."));
	}

	MessagePlayerSelfMuteDeaf mpsmd;
	mpsmd.bMute = g.s.bMute;
	mpsmd.bDeaf = g.s.bDeaf;
	g.sh->sendMessage(&mpsmd);
}

void MainWindow::on_qaAudioDeaf_triggered() {
	g.s.bDeaf = qaAudioDeaf->isChecked();
	if (g.s.bDeaf && ! g.s.bMute) {
		g.s.bMute = true;
		qaAudioMute->setChecked(true);
		g.l->log(Log::SelfMute, tr("Muted and deafened."));
	} else if (g.s.bDeaf) {
		g.l->log(Log::SelfMute, tr("Deafened."));
	} else {
		g.l->log(Log::SelfMute, tr("Undeafened."));
	}

	MessagePlayerSelfMuteDeaf mpsmd;
	mpsmd.bMute = g.s.bMute;
	mpsmd.bDeaf = g.s.bDeaf;
	g.sh->sendMessage(&mpsmd);
}

void MainWindow::on_qaAudioLocalDeafen_triggered() {
	g.s.bLocalDeafen = qaAudioLocalDeafen->isChecked();
	if (g.s.bLocalDeafen) {
		QMessageBox::information(this, tr("Mumble"), tr("You are now in local deafen mode. This mode is not relfected on the server, and you will still be transmitting "
				"voice to the server. This mode should only be used if there are several people in the same room and one of them have Mumble on loudspeakers."));
	}
}

void MainWindow::on_qaAudioTTS_triggered() {
	g.s.bTTS = qaAudioTTS->isChecked();
}

void MainWindow::on_qaAudioStats_triggered() {
	AudioStats *as=new AudioStats(this);
	as->show();
}

void MainWindow::on_qaAudioUnlink_triggered() {
	g.p->bUnlink = true;
}

void MainWindow::on_qaConfigDialog_triggered() {
	ConfigDialog dlg;
	dlg.exec();
}

void MainWindow::on_qaAudioWizard_triggered() {
#ifdef Q_OS_WIN
	AudioWizard aw;
	aw.exec();
#endif
}

void MainWindow::on_qaHelpWhatsThis_triggered() {
	QWhatsThis::enterWhatsThisMode();
}

void MainWindow::on_qaHelpAbout_triggered() {
	AboutDialog adAbout(this);
	adAbout.exec();
}

void MainWindow::on_qaHelpAboutSpeex_triggered() {
	AboutSpeexDialog adAbout(this);
	adAbout.exec();
}

void MainWindow::on_qaHelpAboutQt_triggered() {
	QMessageBox::aboutQt(this, tr("About Qt"));
}

void MainWindow::on_qaHelpVersionCheck_triggered() {
	new VersionCheck(this);
}

void MainWindow::on_PushToTalk_triggered(bool down) {
	if (down)
		g.iPushToTalk++;
	else if (g.iPushToTalk)
		g.iPushToTalk--;
}

void MainWindow::on_PushToMute_triggered(bool down) {
	g.bPushToMute = down;
}

void MainWindow::on_AltPushToTalk_triggered(bool down) {
	if (down) {
		g.iAltSpeak++;
		g.iPushToTalk++;
	} else if (g.iPushToTalk) {
		g.iAltSpeak--;
		g.iPushToTalk--;
	}
}

void MainWindow::on_CenterPos_triggered(bool down) {
	g.bCenterPosition = down;

	if (down) {
		g.iAltSpeak++;
		g.iPushToTalk++;
	} else if (g.iPushToTalk) {
		g.iAltSpeak--;
		g.iPushToTalk--;
	}
}

void MainWindow::pushLink(bool down) {
	if (down) {
		g.iAltSpeak++;
		g.iPushToTalk++;
	} else if (g.iPushToTalk) {
		g.iAltSpeak--;
		g.iPushToTalk--;
	}

	if (g.uiSession == 0)
		return;

	GlobalShortcut *gs = qobject_cast<GlobalShortcut *>(sender());
	int idx = gs->data().toInt();
	Channel *home = ClientPlayer::get(g.uiSession)->cChannel;

	Channel *target = NULL;
	switch (idx) {
		case 0:
			target = home->cParent;
			break;
		case 10:
			break;
		default:
			target = pmModel->getSubChannel(home, idx-1);
			break;
	}


	if (gsMetaChannel->active()) {
		if (! target || ! down)
			return;

		MessagePlayerMove mpm;
		mpm.uiVictim = g.uiSession;
		mpm.iChannelId = target->iId;
		g.sh->sendMessage(&mpm);
		g.l->log(Log::Information, tr("Joining %1.").arg(target->qsName));
	} else {
		MessageChannelLink mcl;
		mcl.iId = home->iId;
		if (down)
			mcl.ltType = MessageChannelLink::PushLink;
		else
			mcl.ltType = MessageChannelLink::PushUnlink;
		if (idx == 10) {
			foreach(Channel *l, home->qlChannels)
			mcl.qlTargets << l->iId;
		} else if (target) {
			mcl.qlTargets << target->iId;
		}
		if (mcl.qlTargets.count() == 0)
			return;
		g.sh->sendMessage(&mcl);
	}
}

void MainWindow::viewCertificate(bool) {
	ViewCert vc(g.sh->qscCert, this);
	vc.exec();
}

void MainWindow::serverConnected() {
	g.uiSession = 0;
	g.l->clearIgnore();
	g.l->setIgnore(Log::PlayerJoin);
	g.l->setIgnore(Log::OtherSelfMute);
	g.l->log(Log::ServerConnected, tr("Connected to server."));
	qaServerDisconnect->setEnabled(true);
	qaServerInformation->setEnabled(true);
	qaServerBanList->setEnabled(true);

	if (g.s.bMute || g.s.bDeaf) {
		MessagePlayerSelfMuteDeaf mpsmd;
		mpsmd.bMute = g.s.bMute;
		mpsmd.bDeaf = g.s.bDeaf;
		g.sh->sendMessage(&mpsmd);
	}
}

void MainWindow::serverDisconnected(QString reason) {
	g.uiSession = 0;
	qaServerDisconnect->setEnabled(false);
	qaServerInformation->setEnabled(false);
	qaServerBanList->setEnabled(false);

	QString uname, pw, host;
	int port;
	g.sh->getConnectionInfo(host, port, uname, pw);

	if (aclEdit) {
		aclEdit->reject();
		delete aclEdit;
		aclEdit = NULL;
	}

	if (banEdit) {
		banEdit->reject();
		delete banEdit;
		banEdit = NULL;
	}

	pmModel->removeAll();

	if (! g.sh->qlErrors.isEmpty()) {
		foreach(QSslError e, g.sh->qlErrors)
		g.l->log(Log::ServerDisconnected, tr("SSL Verification failed: %1").arg(e.errorString()));
		if (! g.sh->qscCert.isEmpty()) {
			QSslCertificate c = g.sh->qscCert.at(0);
			QString basereason;
			if (! Database::getDigest(host, port).isNull()) {
				basereason = tr("<b>WARNING:</b> The server presented a certificate that was different from the stored one.");
			} else {
				basereason = tr("Sever presented a certificate which failed verification.");
			}
			QStringList qsl;
			foreach(QSslError e, g.sh->qlErrors)
			qsl << QString::fromLatin1("<li>%1</li>").arg(e.errorString());

			QMessageBox qmb(QMessageBox::Warning, tr("Mumble"),
			                tr("<p>%1.<br />The specific errors with this certificate are: </p><ol>%2</ol>"
			                   "<p>Do you wish to accept this certificate anyway?<br />(It will also be stored so you won't be asked this again.)</p>"
			                  ).arg(basereason).arg(qsl.join(QString())), QMessageBox::Yes | QMessageBox::No, this);

			qmb.setDefaultButton(QMessageBox::No);
			qmb.setEscapeButton(QMessageBox::No);

			QPushButton *qp = qmb.addButton(tr("&View Certificate"), QMessageBox::ActionRole);
			forever {
				int res = qmb.exec();

				if ((res == 0) && (qmb.clickedButton() == qp)) {
					ViewCert vc(g.sh->qscCert, this);
					vc.exec();
					continue;
				} else if (res == QMessageBox::Yes) {
					Database::setDigest(host, port, QString::fromLatin1(c.digest(QCryptographicHash::Sha1).toHex()));
					qaServerDisconnect->setEnabled(true);
					g.sh->start(QThread::TimeCriticalPriority);
				}
				break;
			}
		}
	} else {
		bool ok = false;
		bool matched = false;

		if (! reason.isEmpty()) {
			g.l->log(Log::ServerDisconnected, tr("Server connection failed: %1.").arg(reason));
		}  else {
			g.l->log(Log::ServerDisconnected, tr("Disconnected from server."));
		}

		switch (rtLast) {
			case MessageServerReject::InvalidUsername:
			case MessageServerReject::UsernameInUse:
				matched = true;
				uname = QInputDialog::getText(this, tr("Invalid username"), (rtLast == MessageServerReject::InvalidUsername) ? tr("You connected with an invalid username, please try another one.") : tr("That username is already in use, please try another username."), QLineEdit::Normal, uname, &ok);
				break;
			case MessageServerReject::WrongUserPW:
			case MessageServerReject::WrongServerPW:
				matched = true;
				pw = QInputDialog::getText(this, tr("Wrong password"), (rtLast == MessageServerReject::WrongUserPW) ? tr("Wrong password for registered users, please try again.") : tr("Wrong server password for unregistered user account, please try again."), QLineEdit::Password, pw, &ok);
				break;
			default:
				break;
		}
		if (ok && matched) {
			qaServerDisconnect->setEnabled(true);
			g.sh->setConnectionInfo(host, port, uname, pw);
			g.sh->start(QThread::TimeCriticalPriority);
		} else if (!matched && g.s.bReconnect && ! reason.isEmpty()) {
			qaServerDisconnect->setEnabled(true);
			qtReconnect->start();
		}
	}
}

void MainWindow::on_Icon_activated(QSystemTrayIcon::ActivationReason) {
	if (! isVisible()) {
		show();
		showNormal();
	}
	activateWindow();
}


void MainWindow::customEvent(QEvent *evt) {
	if (evt->type() == TI_QEVENT) {
		hide();
		return;
	}
	if (evt->type() == MB_QEVENT) {
		MessageBoxEvent *mbe=static_cast<MessageBoxEvent *>(evt);
		QMessageBox::warning(NULL, tr("Mumble"), mbe->msg, QMessageBox::Ok, QMessageBox::NoButton);
		return;
	}
	if (evt->type() != SERVERSEND_EVENT)
		return;

	ServerHandlerMessageEvent *shme=static_cast<ServerHandlerMessageEvent *>(evt);

	Message *mMsg = Message::networkToMessage(shme->qbaMsg);
	if (mMsg) {
		dispatch(NULL, mMsg);
		delete mMsg;
	}
}

void MainWindow::on_qteLog_anchorClicked(const QUrl &url) {
	QDesktopServices::openUrl(url);
}

void MainWindow::on_qteLog_highlighted(const QUrl &url) {
	if (! url.isValid())
		QToolTip::hideText();
	else
		QToolTip::showText(QCursor::pos(), url.toString(), qteLog, QRect());
}
