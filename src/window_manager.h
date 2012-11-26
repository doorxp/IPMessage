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

#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QMapIterator>
#include <QMutex>

#include "owner.h"
#include "msg.h"
#include "chat_window.h"

class MsgWindow;
class MsgReadedWindow;
class ChatWindow;

class WindowManager : public QObject
{
    Q_OBJECT

public:
    WindowManager(QObject *parent = 0);
    ~WindowManager();

    void visibleMsgWindow();
    void visibleAllMsgWindow();
    void visibleAllMsgReadedWindow();

    void createChatWindow(QString, QString);

    void removeMsgWindow(MsgWindow *w) {
        m_msgWindowList.removeAll(w);
    }

    void removeMsgReadedWindow(MsgReadedWindow *w) {
        m_msgReadedWindowList.removeAll(w);
    }

    void removeMsgChatWindow(ChatWindow *w) 
    {
        QMapIterator<QString, ChatWindow *> it(m_msgChatWindowList); 
        while(it.hasNext())
        {
            it.next();
            if(it.value() == w)
            {
                m_msgChatWindowList.remove(it.key());
                break;
            }
        }
    }

    int hidedMsgWindowCount() const;

private slots:
    void newMsg(Msg msg);
    void destroyMsgReadedWindowList();

private:
    void createMsgWindow(Msg msg);
    void createMsgReadedWindow(Msg msg);

    void destroyMsgWindowList();

    QList<MsgWindow *> m_msgWindowList;
    QList<MsgReadedWindow *> m_msgReadedWindowList;
    QMap<QString, ChatWindow *> m_msgChatWindowList; //chat dialog
};

#endif // !WINDOW_MANAGER_H
