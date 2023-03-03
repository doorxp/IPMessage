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

#include "main_list_window.h"
#include "chat_window.h"
#include "user_manager.h"
#include "window_manager.h"
#include "msg_thread.h"
#include "constants.h"
#include "ipmsg.h"
#include "systray.h"
#include "global.h"
#include "preferences.h"
#include "dir_dialog.h"

quint32 MainListWindow::m_levelOneCount = 0;
quint32 MainListWindow::m_levelTwoCount = 0;

MainListWindow::MainListWindow(QString text, QString ip,
        QWidget *parent)
    : QMainWindow(parent), m_initText(text), m_selectedIp(ip),
    m_lastSearchDialog(0)
{
    // delete when close
    setAttribute(Qt::WA_DeleteOnClose, true);

    setAcceptDrops(true);

    setSourceModel();

    createPeerWidget();
    createActions();

    setCentralWidget(peerWidget);

    QItemSelectionModel *selectionModel
        = new QItemSelectionModel(proxyUserModel, this);
    userView->setSelectionModel(selectionModel);

    createConnections();

    setWindowTitle(tr("IP Messenger"));
    setWindowIcon(*Global::iconSet.value("normal"));

    selectUser();

    adjustSize();
    move(Global::randomNearMiddlePoint());
}

MainListWindow::~MainListWindow()
{
    // XXX NOTE: m_sendFileMap will be managed by sendFileManager
    // nothing to do
}

QSize MainListWindow::sizeHint() const
{
    return QSize(400, 360);
}

void MainListWindow::createConnections()
{
    connect(Global::userManager, SIGNAL(userCountUpdated(int)),
            this, SLOT(updateUserCount(int)));
    connect(refreshButton, SIGNAL(clicked()),
            this, SLOT(refreshUserList()));

    connect(searchUserAct, SIGNAL(triggered()),
            this, SLOT(showSearchDialog()));

    connect(new QShortcut(tr("Ctrl+F"), this), SIGNAL(activated()),
            this, SLOT(showSearchDialog()));

    connect(userView, SIGNAL(doubleClicked(const QModelIndex &)),
            this, SLOT(rowDoubleClicked(const QModelIndex &)));  
}

void MainListWindow::createPeerWidget()
{
    peerWidget = new QWidget;

    proxyUserModel->setDynamicSortFilter(true);
    userView = new QTableView;
    userView->setModel(proxyUserModel);
    userView->setSortingEnabled(true);
    userView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // userView->horizontalHeader()->setSectionsMovable(true);
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

void MainListWindow::setSourceModel()
{
    proxyUserModel = new QSortFilterProxyModel(this);
    proxyUserModel->setSourceModel(Global::userManager->m_model);
}

void MainListWindow::rowDoubleClicked(const QModelIndex& index)  
{  
    //QModelIndex index = userView->currentIndex();  
    if (index.isValid())  
    {  
        QString ip = proxyUserModel->data(proxyUserModel->index(index.row(), USER_VIEW_IP_COLUMN)).toString();
        QString user = proxyUserModel->data(proxyUserModel->index(index.row(), USER_VIEW_LOGIN_NAME_COLUMN)).toString();
        //Global::windowManager->visibleAllMsgReadedWindow();
        Global::windowManager->createChatWindow(ip, user);
    }  
} 

void MainListWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    // XXX TODO: display level support
    // menu.addMenu(setDisplayLevelMenu);
    menu.addMenu(selectGroupMenu);
    menu.addAction(searchUserAct);

    updateSelectGroupMenu();

    menu.exec(event->globalPos());
}

void MainListWindow::createActions()
{
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

void MainListWindow::updateSelectGroupMenu()
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

void MainListWindow::selectGroup()
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

void MainListWindow::closeEvent(QCloseEvent *event)
{
    //Global::systray->mainWindowList.removeAll(this);

    event->accept();
}

void MainListWindow::updateUserCount(int i)
{
    qDebug("MainListWindow::updateUserCount:");

    userCountLabel->setText(QString(tr("User Count\n%1").arg(i)));
    setLevelOneAct->setText(tr("set level 1 (user %1 / %2)")
            .arg(m_levelOneCount).arg(i));
    setLevelTwoAct->setText(tr("set level 2 (user %1 / %2)")
            .arg(m_levelTwoCount).arg(i));
}

void MainListWindow::dropEvent(QDropEvent *e)
{
    qDebug("MainListWindow::dropEvent");

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

void MainListWindow::dragEnterEvent(QDragEnterEvent *e)
{
    qDebug("MainListWindow::dragEnterEvent");

    if (e->mimeData()->hasUrls()) {
        e->acceptProposedAction();
    }
}

void MainListWindow::updateFileCount()
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

bool MainListWindow::hasSendFile()
{
    return m_sendFileModel.rowCount() > 0;
}

void MainListWindow::selectUser()
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

void MainListWindow::refreshUserList()
{
    Global::userManager->m_model
        ->removeRows(0, Global::userManager->m_model->rowCount());
    updateUserCount(0);

    Global::userManager->broadcastEntry();
}

void MainListWindow::keyPressEvent(QKeyEvent *event)
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

void MainListWindow::showSearchDialog()
{
    qDebug() << "MainListWindow::showSearchDialog";

    SearchUserDialog *searchUserDialog = new SearchUserDialog(this);

    connect(searchUserDialog, SIGNAL(searchUser(QString)),
            this, SLOT(search(QString)));

    searchUserDialog->show();
}

void MainListWindow::search(QString searchString)
{
    qDebug() << "MainListWindow::search";

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

QList<QStandardItem*> MainListWindow::searchItems(QString searchString) const
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

