// This file is part of QIpMsg.
//
// QIpMsg is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// QIpMsg is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with QIpMsg.  If not, see <http://www.gnu.org/licenses/>.
//

#include <QtGui>
#include <QtCore>
#include <QModelIndex>
#include <QHostAddress>
#include <QStringList>

#include "chat_window.h"
#include "user_manager.h"
#include "window_manager.h"
#include "msg_thread.h"
#include "send_msg.h"
#include "recv_msg.h"
#include "constants.h"
#include "ipmsg.h"
#include "send_file_window.h"
#include "systray.h"
#include "global.h"
#include "preferences.h"
#include "dir_dialog.h"
#include "send_file_manager.h"

quint32 ChatWindow::m_levelOneCount = 0;
quint32 ChatWindow::m_levelTwoCount = 0;

ChatWindow::ChatWindow(QString text, QString ip, QWidget *parent)
    : QMainWindow(parent), m_initText(text), m_selectedIp(ip), m_lastSearchDialog(0) 
{
    // delete when close
    setAttribute(Qt::WA_DeleteOnClose, true);

    setAcceptDrops(true);

    setSourceModel();

    createSendLayout();
    createPeerWidget();
    createInputWidget();
    createSplitterWidget();
    createActions();

    setCentralWidget(splitter);

    QItemSelectionModel *selectionModel
        = new QItemSelectionModel(proxyUserModel, this);
    userView->setSelectionModel(selectionModel);

    createConnections();

    inputEdit->setFocus(Qt::TabFocusReason);
    inputEdit->moveCursor(QTextCursor::End);

    QString title = (text == ""? "IP Messager" : text);
    setWindowTitle(title);
    setWindowIcon(*Global::iconSet.value("normal"));

    selectUser();

    adjustSize();
    move(Global::randomNearMiddlePoint());

    //cleare histroy message
    m_chatHistoryMessage = "";
}

ChatWindow::~ChatWindow()
{
    // XXX NOTE: m_sendFileMap will be managed by sendFileManager
    // nothing to do
}

QSize ChatWindow::sizeHint() const
{
    return QSize(400, 360);
}

void ChatWindow::createConnections()
{
    connect(sendButton, SIGNAL(clicked()),
            this, SLOT(sendMessage()));

    connect(new QShortcut(tr("Ctrl+Return"), inputEdit), SIGNAL(activated()),
            this, SLOT(sendMessage()));

    connect(Global::userManager, SIGNAL(userCountUpdated(int)),
            this, SLOT(updateUserCount(int)));
    connect(refreshButton, SIGNAL(clicked()),
            this, SLOT(refreshUserList()));

    connect(sendFileAct, SIGNAL(triggered()),
            this, SLOT(addSendFile()));
    connect(sendDirAct, SIGNAL(triggered()),
            this, SLOT(addSendDir()));
    connect(searchUserAct, SIGNAL(triggered()),
            this, SLOT(showSearchDialog()));

    connect(new QShortcut(tr("Ctrl+F"), this), SIGNAL(activated()),
            this, SLOT(showSearchDialog()));
}

void ChatWindow::createPeerWidget()
{
    peerWidget = new QWidget;

    proxyUserModel->setDynamicSortFilter(true);
    userView = new QTableView;
    userView->setModel(proxyUserModel);
    userView->setSortingEnabled(true);
    userView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    userView->horizontalHeader()->setMovable(true);
    userView->verticalHeader()->hide();
    userView->setSelectionBehavior(QAbstractItemView::SelectRows);
    userView->setTabKeyNavigation(false);
    userView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    QFontMetrics fm = fontMetrics();
    QHeaderView *hHeader = userView->horizontalHeader();
    hHeader->resizeSection(USER_VIEW_IP_COLUMN,
            fm.width(" 255.255.255.255 "));

    QHeaderView *vHeader = userView->verticalHeader();
    vHeader->setDefaultSectionSize(fm.height() + 6);
    vHeader->setStyleSheet(
                           "QHeaderView::section {"
                           "padding-bottom: 0px;"
                           "padding-top: 0px;"
                           "padding-left: 0px;"
                           "padding-right: 1px;"
                           "margin: 0px;"
                           "}"
                          );

    refreshButton = new QPushButton(tr("&Refresh"));
    userCountLabel = new QLabel(tr("User Count\n%1")
            .arg(Global::userManager->m_model->rowCount()));
    userCountLabel->setAlignment(Qt::AlignHCenter);

    QGridLayout *gridLayout = new QGridLayout;
    gridLayout->addWidget(userView, 0, 0, 5, 1);
    gridLayout->addWidget(userCountLabel, 1, 1);
    gridLayout->addWidget(refreshButton, 3, 1);

    peerWidget->setLayout(gridLayout);
}

void ChatWindow::createInputWidget()
{
    inputWidget = new QWidget;

    fileListButton = new QPushButton;
    connect(fileListButton, SIGNAL(clicked()),
            this, SLOT(showSendFileList()));

    inputEdit = new QTextEdit;
    inputEdit->setAcceptDrops(false);
    //inputEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    inputEdit->setAcceptRichText(false);
    inputEdit->setText("");
    inputEdit->setTabChangesFocus(true);

    chatHistoryEdit = new QTextEdit;
    chatHistoryEdit->setAcceptDrops(false);
    //chatHistoryEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    chatHistoryEdit->setAcceptRichText(false);
    chatHistoryEdit->setText("");
    chatHistoryEdit->setTabChangesFocus(true);
    chatHistoryEdit->setReadOnly(true);

    splitter = new QSplitter(Qt::Vertical);
    splitter->setChildrenCollapsible(false);

    splitter->addWidget(chatHistoryEdit);
    //splitter->addWidget(inputEdit);
    QList<int> l;
    l << peerWidget->maximumHeight() << inputWidget->minimumSizeHint().height();
    splitter->setSizes(l);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(fileListButton);
    //vbox->addWidget(chatHistoryEdit);
    vbox->addWidget(inputEdit);
    //vbox->addWidget(splitter);
    vbox->addLayout(sendLayout);
    //splitter->addWidget(vbox);

    fileListButton->hide();

    inputWidget->setLayout(vbox);
    splitter->addWidget(inputWidget);
}

void ChatWindow::createSplitterWidget()
{
    //splitter = new QSplitter(Qt::Vertical);
    //splitter->setChildrenCollapsible(false);
    //splitter->addWidget(peerWidget);
    //splitter->addWidget(inputWidget);

    //QList<int> l;
    // XXX TODO: How this work????????
    //l << peerWidget->maximumHeight() << inputWidget->minimumSizeHint().height();
    //splitter->setSizes(l);
}

void ChatWindow::createSendLayout()
{
    sendButton = new QPushButton(tr("&Send"));
    encapCheckBox = new QCheckBox(tr("&Encap"));
    if (Global::preferences->isSealed) {
        encapCheckBox->setCheckState(Qt::Checked);
    } else {
        encapCheckBox->setCheckState(Qt::Unchecked);
    }
    lockCheckBox = new QCheckBox(tr("Loc&k"));
    lockCheckBox->setEnabled(false);

    sendLayout = new QHBoxLayout;
    sendLayout->addStretch();
    sendLayout->addWidget(sendButton);
    sendLayout->addSpacing(20);
    sendLayout->addWidget(encapCheckBox);
    sendLayout->addSpacing(20);
    sendLayout->addWidget(lockCheckBox);
}

void ChatWindow::setSourceModel()
{
    proxyUserModel = new QSortFilterProxyModel(this);
    proxyUserModel->setSourceModel(Global::userManager->m_model);
}

void ChatWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    // XXX TODO: display level support
    // menu.addMenu(setDisplayLevelMenu);
    menu.addMenu(selectGroupMenu);
    menu.addAction(sendFileAct);
    menu.addAction(sendDirAct);
    menu.addAction(searchUserAct);

    updateSelectGroupMenu();

    menu.exec(event->globalPos());
}

void ChatWindow::createActions()
{
    sendFileAct = new QAction(tr("Send file"), this);
    sendDirAct = new QAction(tr("Sent directory"), this);
    searchUserAct = new QAction(tr("Search user Ctrl+F"), this);

    setLevelOneAct = new QAction(tr("Set level 1 (user %1 / %2)")
            .arg(m_levelOneCount)
            .arg(Global::userManager->m_model->rowCount()),
            this);
    setLevelTwoAct = new QAction(tr("Set level 2 (user %1 / %2)")
            .arg(m_levelTwoCount)
            .arg(Global::userManager->m_model->rowCount()),
            this);
    setDisplayLevelMenu = new QMenu(tr("Set display level"), this);
    setDisplayLevelMenu->addAction(setLevelOneAct);
    setDisplayLevelMenu->addAction(setLevelTwoAct);

    selectGroupMenu = new QMenu(tr("Select group"), this);

    updateSelectGroupMenu();
}

void ChatWindow::updateSelectGroupMenu()
{
    selectGroupMenu->clear();

    foreach (QString group, Global::preferences->groupNameList) {
        if (!group.isEmpty()) {
            QAction *action = new QAction(group, this);
            action->setData(group);
            connect(action, SIGNAL(triggered()), this, SLOT(selectGroup()));
            selectGroupMenu->addAction(action);
        }
    }
}

void ChatWindow::selectGroup()
{
    userView->clearSelection();

    QAction *action = qobject_cast<QAction *>(sender());
    QString group = action->data().toString();

    QList<QStandardItem *> list =
        Global::userManager->m_model
        ->findItems(group, Qt::MatchExactly,
                    USER_VIEW_GROUP_COLUMN);

    QFlags<QItemSelectionModel::SelectionFlag> flags;
    flags = QItemSelectionModel::Select | QItemSelectionModel::Rows;
    QItemSelectionModel *selections = userView->selectionModel();
    foreach (QStandardItem *item, list) {
        QModelIndex index = proxyUserModel->
            mapFromSource(Global::userManager->m_model->indexFromItem(item));
        selections->select(index, flags);
    }

    // scroll to first selected row
    if (!list.isEmpty()) {
        QStandardItem *item = list.at(0);
        QModelIndex index = proxyUserModel->
            mapFromSource(Global::userManager->m_model->indexFromItem(item));
        int row = index.row();
        userView->verticalScrollBar()->setValue(row);
    }
}

void ChatWindow::closeEvent(QCloseEvent *event)
{
    //Global::systray->mainWindowList.removeAll(this);
    Global::windowManager->removeMsgChatWindow(this);
    event->accept();
}

void ChatWindow::updateUserCount(int i)
{
    qDebug("ChatWindow::updateUserCount:");

    userCountLabel->setText(QString(tr("User Count\n%1").arg(i)));
    setLevelOneAct->setText(tr("set level 1 (user %1 / %2)")
            .arg(m_levelOneCount).arg(i));
    setLevelTwoAct->setText(tr("set level 2 (user %1 / %2)")
            .arg(m_levelTwoCount).arg(i));
}

void ChatWindow::sendMessage()
{
    quint32 flags = 0;
    flags |= IPMSG_SENDMSG | IPMSG_SENDCHECKOPT;
    if (encapCheckBox->isChecked()) {
        flags |= IPMSG_SECRETOPT | IPMSG_READCHECKOPT;
    }

    QItemSelectionModel *selections = userView->selectionModel();
    QModelIndexList modelIndexList =
        selections->selectedRows(USER_VIEW_IP_COLUMN);

    if (modelIndexList.count() < 1) { // No user selected
        return;
    } else if (modelIndexList.count() == 1) {
        ;
    } else {
        flags |= IPMSG_MULTICASTOPT;
    }

    foreach (QModelIndex index, modelIndexList) {
        QString ip = proxyUserModel->data(index).toString();
        QString recvUser = proxyUserModel->data(
                proxyUserModel->index(index.row(),
                    USER_VIEW_NAME_COLUMN)).toString();
        QString recvHostname = proxyUserModel->data(
                proxyUserModel->index(index.row(),
                    USER_VIEW_HOST_COLUMN)).toString();

        QString additionalInfo = inputEdit->toPlainText();
        if("" == additionalInfo.trimmed()) //no input content
        {
            break;
        }

        //echo message in histroy window
        Owner self = Global::userManager->ourself();
        addMessage(additionalInfo, self.name()); //show message

        additionalInfo.append(QChar('\0'));

        if (hasSendFile()) {
            // we need to realloc memory when send file to mulitple user.
            // we don't delete this, sendFileManager take care of it.
            m_sendFileMap = new SendFileMap;

            flags |= IPMSG_FILEATTACHOPT;

            QStringList pathList = m_sendFileModel.pathList();
            m_sendFileMap->addFile(pathList);
            additionalInfo.append(m_sendFileMap->packetString());
            additionalInfo.append(QChar('\0'));
        }

        if (Global::preferences->isLogMsg) {
            //logSendMsg(inputEdit->toPlainText(), index.row());
        }

        SendMsg sendMsg = SendMsg(QHostAddress(ip), IPMSG_DEFAULT_PORT,
                                  additionalInfo, ""/* extendedInfo */,
                                  flags);

        if (hasSendFile()) {
            m_sendFileMap->setRecvUser(recvUser);
            m_sendFileMap->setRecvHostname(recvHostname);
            m_sendFileMap->setPacketNoString(sendMsg.packetNoString());
            Global::sendFileManager
                ->addTransferLocked(m_sendFileMap->packetNoString(),
                         m_sendFileMap);

        }

        Global::msgThread->addSendMsg(Msg(sendMsg));
    }

    inputEdit->setText(""); //clear the input dialog
    Global::systray->clearNotify();
    emit messageReplyed();
}

void ChatWindow::dropEvent(QDropEvent *e)
{
    qDebug("ChatWindow::dropEvent");

    if (e->mimeData()->hasUrls()) {
        QList<QUrl> urlList = e->mimeData()->urls();

        foreach (QUrl url, urlList) {
            QString filePath;
            if (url.scheme() == "file") {
                filePath = url.toLocalFile();
            }

            m_sendFileModel.addFile(filePath);
        }
    }

    fileListButton->setText(QString(tr("Show send file list (%1)"))
            .arg(Global::fileCountString(m_sendFileModel.rowCount())));

    if (!fileListButton->isVisible()) {
        fileListButton->show();
    }
}

void ChatWindow::dragEnterEvent(QDragEnterEvent *e)
{
    qDebug("ChatWindow::dragEnterEvent");

    if (e->mimeData()->hasUrls()) {
        e->acceptProposedAction();
    }
}

void ChatWindow::showSendFileList()
{
    qDebug("ChatWindow::showSendFileList");

    sendFileWindow = new SendFileWindow(&m_sendFileModel, this);
    connect(sendFileWindow, SIGNAL(addFileFinished()),
            this, SLOT(updateFileCount()));
    sendFileWindow->setWindowFlags(Qt::Dialog);
    sendFileWindow->setWindowModality(Qt::WindowModal);

    // Disable drop events
    setAcceptDrops(false);

    sendFileWindow->show();
}

void ChatWindow::updateFileCount()
{
    setAcceptDrops(true);

    if (m_sendFileModel.rowCount() == 0) {
        if (fileListButton->isVisible()) {
            fileListButton->hide();
        }
    } else {
        fileListButton->setText(QString(tr("Show send file list (%1)"))
                .arg(Global::fileCountString(m_sendFileModel.rowCount())));
        if (!fileListButton->isVisible()) {
            fileListButton->show();
        }
    }
}

bool ChatWindow::hasSendFile()
{
    return m_sendFileModel.rowCount() > 0;
}

void ChatWindow::selectUser()
{
    if (m_selectedIp.isEmpty()) {
        return;
    }

    QList<QStandardItem *> list =
        Global::userManager->m_model
        ->findItems(m_selectedIp, Qt::MatchExactly,
                    USER_VIEW_IP_COLUMN);

    foreach (QStandardItem *item, list) {
        QModelIndex index = proxyUserModel->
            mapFromSource(Global::userManager->m_model->indexFromItem(item));
        int row = index.row();
        userView->selectRow(row);
    }

    // scroll to first selected user
    if (!list.isEmpty()) {
        QStandardItem *item = list.at(0);
        QModelIndex index = proxyUserModel->
            mapFromSource(Global::userManager->m_model->indexFromItem(item));
        int row = index.row();
        userView->verticalScrollBar()->setValue(row);
    }
}

void ChatWindow::logSendMsg(QString text, int row)
{
    if (!Global::preferences->logFile.isWritable()) {
        return;
    }

    QTextStream ts(&Global::preferences->logFile);

    QString s("=====================================\n");
    s.append(tr(" To: "));
    s.append(Global::userManager->name(row));
    if (Global::preferences->isLogLoginName) {
        s.append("[");
        s.append(Global::userManager->loginName(row));
        s.append("]");
    }

    s.append(" (");
    if (!Global::userManager->group(row).isEmpty()) {
        s.append(Global::userManager->group(row));
        s.append("/");
    }
    s.append(Global::userManager->host(row));
    if (Global::preferences->isLogIP) {
        s.append("/");
        s.append(Global::userManager->ip(row));
    }
    s.append(")\n");
    s.append(tr(" Time: "));
    QDateTime now = QDateTime::currentDateTime();
    s.append(now.toString("yyyy-MM-dd dddd hh:mm:ss"));
    // XXX TODO: if enable encrypt, need log encrypt method.
    if (encapCheckBox->isChecked()) {
        s.append(tr(" (Sealed)"));
    }
    s.append("\n");

    // Log attach file
    if (hasSendFile()) {
        QString files = "  ("
            + Global::fileCountString(m_sendFileModel.rowCount()) + ") ";
        files.append(m_sendFileModel.fileNameJoin(", "));

        s.append(files);
        s.append(QChar('\n'));
    } // End log attach file

    s.append("-------------------------------------\n");
    s.append(text);
    s.append("\n\n");

    ts << s << flush;
}

void ChatWindow::refreshUserList()
{
    Global::userManager->m_model
        ->removeRows(0, Global::userManager->m_model->rowCount());
    updateUserCount(0);

    Global::userManager->broadcastEntry();
}

void ChatWindow::addSendFile()
{
    // Disable drop events
    setAcceptDrops(false);

    QStringList pathList;

    pathList = QFileDialog::getOpenFileNames(this, tr("Add Files"),
            Global::preferences->lastSendFilePath,
            tr("All Files (*)"));

    m_sendFileModel.addFile(pathList);

    if (!pathList.isEmpty()) {
        Global::preferences->lastSendFilePath = pathList.at(0);
    }

    updateFileCount();
}

void ChatWindow::addSendDir()
{
    DirDialog *dirDialog = new DirDialog(&m_sendFileModel, this);

    connect(dirDialog, SIGNAL(dirSelected()),
            this, SLOT(updateFileCount()));

    dirDialog->show();
}

void ChatWindow::keyPressEvent(QKeyEvent *event)
{
#if 0
    switch (event->key()) {
    case Qt::CTRL + Qt::Key_F:
        showSearchDialog();
        break;
    default:
        QWidget::keyPressEvent(event);
        break;
    }
#endif
}

void ChatWindow::showSearchDialog()
{
    qDebug() << "ChatWindow::showSearchDialog";

    SearchUserDialog *searchUserDialog = new SearchUserDialog(this);

    connect(searchUserDialog, SIGNAL(searchUser(QString)),
            this, SLOT(search(QString)));

    searchUserDialog->show();
}

void ChatWindow::search(QString searchString)
{
    qDebug() << "ChatWindow::search";

    QItemSelectionModel *selections = userView->selectionModel();

    bool isContinueSearch = false;
    if (m_lastSearchDialog == sender() && m_lastSearch == searchString) {
        isContinueSearch = true;
    } else {
        m_lastSearchDialog = sender();
        m_lastSearch = searchString;
    }

    QList<QStandardItem*> itemList = searchItems(searchString);

    QList<int> rowList;
    foreach (QStandardItem *item, itemList) {
        QModelIndex index = proxyUserModel->
            mapFromSource(Global::userManager->m_model->indexFromItem(item));
        if (!rowList.contains(index.row())) {
            rowList << index.row();
        }
    }
    qSort(rowList.begin(), rowList.end());

    if (!isContinueSearch) {
        selections->clearSelection();
        foreach (int r, rowList) {
            userView->selectRow(r);
            break;
        }

        return;
    }

    QModelIndexList selectedRows = selections->selectedRows();

    userView->clearSelection();
    int row;
    if (selectedRows.isEmpty()) {
        foreach (int r, rowList) {
            row = r;
            break;
        }
    } else {    // have selected row
        if (selectedRows.last().row() >= rowList.last()) {
            row = rowList.first();
        } else {
            foreach (int r, rowList) {
                if (r > selectedRows.last().row()) {
                    row = r;
                    break;
                }
            }
        }
    }
    userView->selectRow(row);

    // scroll to selected row
    userView->verticalScrollBar()->setValue(row);
}

QList<QStandardItem*> ChatWindow::searchItems(QString searchString) const
{
    QList<QStandardItem*> itemList = Global::userManager->m_model
        ->findItems(searchString, Qt::MatchContains, USER_VIEW_NAME_COLUMN);

    if (Global::preferences->isSearchAllColumns) {
        itemList += Global::userManager->m_model->findItems(searchString, Qt::MatchContains, USER_VIEW_GROUP_COLUMN);
        itemList += Global::userManager->m_model->findItems(searchString, Qt::MatchContains, USER_VIEW_HOST_COLUMN);
        itemList += Global::userManager->m_model->findItems(searchString, Qt::MatchContains, USER_VIEW_IP_COLUMN);
        itemList += Global::userManager->m_model->findItems(searchString, Qt::MatchContains, USER_VIEW_LOGIN_NAME_COLUMN);
    }

    return itemList;
}

void ChatWindow::readMessage(Msg msg)
{
    QString sMsg = msg->additionalInfo();
    Owner o = msg->owner();
    QString name = o.name();

    addMessage(sMsg, name);
}

void ChatWindow::addMessage(QString msg, QString name)
{
    //time
    QDateTime now = QDateTime::currentDateTime();
    QString timeString(tr(""));
    timeString.append(now.toString("hh:mm:ss"));

    //sender info
    //Owner o = msg->owner();
    //QString name = o.name();
    //QString group = o.group();
    //QString host = o.host();
    //QString s = group.isEmpty() ? "" : group + "/";
    QString senderInfo  = name ;//+ " (" + s + host + ")";

    QString text = senderInfo + " (" + timeString + ") :\r\n" + msg + "\r\n\r\n";    
    m_chatHistoryMessage += text;

    QTextStream ts(&Global::preferences->logFile);
    if(Global::preferences->isLogMsg)
    {
        ts << text << flush;
    }
    
    chatHistoryEdit->setText(m_chatHistoryMessage);

    //scroll to bottom
    QTextCursor cursor = chatHistoryEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    chatHistoryEdit->setTextCursor(cursor);
}
