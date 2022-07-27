#include "mytcpsocket.h"
#include "mytcpserver.h"
#include <QDir> // 操作文件夹的库

MyTcpSocket::MyTcpSocket()
{
    connect(this, SIGNAL(readyRead()), // 当接收到客户端的数据时，服务器会发送readyRead()信号
            this, SLOT(receiveMsg())); // 需要由服务器的相应receiveMsg槽函数进行处理
    connect(this, SIGNAL(disconnected()), this, SLOT(handleClientOffline())); // 关联Socket连接断开与客户端下线处理槽函数
}

QString MyTcpSocket::getStrName()
{
    return m_strName;
}

// 处理注册请求并返回响应PDU
PDU* handleRegistRequest(PDU* pdu)
{
    char caName[32] = {'\0'};
    char caPwd[32] = {'\0'};
    // 拷贝读取的信息
    strncpy(caName, pdu -> caData, 32);
    strncpy(caPwd, pdu -> caData + 32, 32);
    // qDebug() << pdu -> uiMsgType << " " << caName << " " << caPwd;
    bool ret = DBOperate::getInstance().handleRegist(caName, caPwd); // 处理请求，插入数据库

    // 响应客户端
    PDU *resPdu = mkPDU(0); // 响应消息
    resPdu -> uiMsgType = ENUM_MSG_TYPE_REGIST_RESPOND;
    if(ret)
    {
        strcpy(resPdu -> caData, REGIST_OK);
        // 注册成功，为新用户按用户名创建文件夹
        QDir dir;
        ret = dir.mkdir(QString("%1/%2").arg(MyTcpServer::getInstance().getStrRootPath()).arg(caName));
        qDebug() << "创建新用户文件夹 " << ret;
    }
    if(!ret)
    {
        strcpy(resPdu -> caData, REGIST_FAILED);
    }
    // qDebug() << resPdu -> uiMsgType << " " << resPdu ->caData;

    return resPdu;
}

// 处理登录请求并返回响应PDU
PDU* handleLoginRequest(PDU* pdu, QString& m_strName)
{
    char caName[32] = {'\0'};
    char caPwd[32] = {'\0'};
    // 拷贝读取的信息
    strncpy(caName, pdu -> caData, 32);
    strncpy(caPwd, pdu -> caData + 32, 32);
    // qDebug() << pdu -> uiMsgType << " " << caName << " " << caPwd;
    bool ret = DBOperate::getInstance().handleLogin(caName, caPwd); // 处理请求，插入数据库

    // 响应客户端
    PDU *resPdu = NULL; // 响应消息
    if(ret)
    {
        QString strUserRootPath = QString("%1/%2")
                .arg(MyTcpServer::getInstance().getStrRootPath()).arg(caName); // 用户文件系统根目录
        qDebug() << "登录用户的路径：" << strUserRootPath;
        resPdu = mkPDU(strUserRootPath.size() + 1);
        memcpy(resPdu -> caData, LOGIN_OK, 32);
        memcpy(resPdu -> caData + 32, caName, 32); // 将登录后的用户名传回，便于tcpclient确认已经登陆的用户名
        // 在登陆成功时，记录Socket对应的用户名
        m_strName = caName;
        // qDebug() << "m_strName: " << m_strName;
        // 返回用户的根目录
        strncpy((char*)resPdu -> caMsg, strUserRootPath.toStdString().c_str(), strUserRootPath.size() + 1);
    }
    else
    {
        resPdu = mkPDU(0);
        strcpy(resPdu -> caData, LOGIN_FAILED);
    }
    resPdu -> uiMsgType = ENUM_MSG_TYPE_LOGIN_RESPOND;
    qDebug() << "登录处理：" << resPdu -> uiMsgType << " " << resPdu ->caData << " " << resPdu ->caData + 32;

    return resPdu;
}

// 处理查询所有在线用户的请求
PDU* handleOnlineUsersRequest()
{
    QStringList strList = DBOperate::getInstance().handleOnlineUsers(); // 查询请求，查询数据库所有在线用户
    uint uiMsgLen = strList.size() * 32; // 消息报文的长度

    // 响应客户端
    PDU *resPdu = mkPDU(uiMsgLen); // 响应消息
    resPdu -> uiMsgType = ENUM_MSG_TYPE_ONLINE_USERS_RESPOND;
    // qDebug() << "在线用户数：" << strList.size();
    for(int i = 0; i < strList.size(); ++ i)
    {
        memcpy((char*)(resPdu -> caMsg) + 32 * i, strList[i].toStdString().c_str(), strList[i].size());
        // qDebug() << "所有在线用户有：" << (char*)(resPdu -> caMsg) + 32 * i;
    }

    return resPdu;
}

// 处理查找用户的请求
PDU* handleSearchUserRequest(PDU* pdu)
{
    char caName[32] = {'\0'};
    strncpy(caName, pdu -> caData, 32);
    int ret = DBOperate::getInstance().handleSearchUser(caName); // 处理请求

    // 响应客户端
    PDU *resPdu = mkPDU(0); // 响应消息
    resPdu -> uiMsgType = ENUM_MSG_TYPE_SEARCH_USER_RESPOND;
    if(ret == 1)
    {
        strcpy(resPdu -> caData, SEARCH_USER_OK);
    }
    else if(ret == 0)
    {
        strcpy(resPdu -> caData, SEARCH_USER_OFFLINE);
    }
    else
    {
        strcpy(resPdu -> caData, SEARCH_USER_EMPTY);
    }

    return resPdu;
}

// 处理添加好友请求
PDU* handleAddFriendRequest(PDU* pdu)
{
    char addedName[32] = {'\0'};
    char sourceName[32] = {'\0'};
    // 拷贝读取的信息
    strncpy(addedName, pdu -> caData, 32);
    strncpy(sourceName, pdu -> caData + 32, 32);
    // qDebug() << "handleAddFriendRequest  " << addedName << " " << sourceName;
    int iSearchUserStatus = DBOperate::getInstance().handleAddFriend(addedName, sourceName);
    // 0对方存在不在线，1对方存在在线，2不存在，3已是好友，4请求错误
    PDU* resPdu = NULL;

    switch (iSearchUserStatus) {
    case 0: // 0对方存在不在线
    {
        resPdu = mkPDU(0);
        resPdu -> uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_RESPOND;
        strcpy(resPdu -> caData, ADD_FRIEND_OFFLINE);
        break;
    }
    case 1: // 1对方存在在线
    {
        // 需要转发给对方请求添加好友消息
        MyTcpServer::getInstance().forwardMsg(addedName, pdu);

        resPdu = mkPDU(0);
        resPdu -> uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_RESPOND;
        strcpy(resPdu -> caData, ADD_FRIEND_OK); // 表示加好友请求已发送
        break;
    }
    case 2: // 2用户不存在
    {
        resPdu = mkPDU(0);
        resPdu -> uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_RESPOND;
        strcpy(resPdu -> caData, ADD_FRIEND_EMPTY);
        break;
    }
    case 3: // 3已是好友
    {
        resPdu = mkPDU(0);
        resPdu -> uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_RESPOND;
        strcpy(resPdu -> caData, ADD_FRIEND_EXIST);
        break;
    }
    case 4: // 4请求错误
    {
        resPdu = mkPDU(0);
        resPdu -> uiMsgType = ENUM_MSG_TYPE_ADD_FRIEND_RESPOND;
        strcpy(resPdu -> caData, UNKNOWN_ERROR);
        break;
    }
    default:
        break;
    }

    return resPdu;
}

// 同意加好友
void handleAddFriendAgree(PDU* pdu)
{
    char addedName[32] = {'\0'};
    char sourceName[32] = {'\0'};
    // 拷贝读取的信息
    strncpy(addedName, pdu -> caData, 32);
    strncpy(sourceName, pdu -> caData + 32, 32);

    // 将新的好友关系信息写入数据库
    DBOperate::getInstance().handleAddFriendAgree(addedName, sourceName);

    // 服务器需要转发给发送好友请求方其被同意的消息
    MyTcpServer::getInstance().forwardMsg(sourceName, pdu);
}

// 拒绝加好友
void handleAddFriendReject(PDU* pdu)
{
    char sourceName[32] = {'\0'};
    // 拷贝读取的信息
    strncpy(sourceName, pdu -> caData + 32, 32);
    // 服务器需要转发给发送好友请求方其被拒绝的消息
    MyTcpServer::getInstance().forwardMsg(sourceName, pdu);
}

// 刷新好友列表请求
PDU* handleFlushFriendRequest(PDU* pdu)
{
    char caName[32] = {'\0'};

    strncpy(caName, pdu -> caData, 32);

    QStringList strList = DBOperate::getInstance().handleFlushFriend(caName);
    uint uiMsgLen = strList.size() / 2 * 36; // 36 char[32] 好友名字+ 4 int 在线状态

    PDU* resPdu = mkPDU(uiMsgLen);
    resPdu -> uiMsgType = ENUM_MSG_TYPE_FLUSH_FRIEND_RESPOND;
    for(int i = 0; i * 2 < strList.size(); ++ i)
    {
        strncpy((char*)(resPdu -> caMsg) + 36 * i, strList.at(i * 2).toStdString().c_str(), 32);
        strncpy((char*)(resPdu -> caMsg) + 36 * i + 32, strList.at(i * 2 + 1).toStdString().c_str(), 4);
    }

    return resPdu;
}

// 删除好友请求
PDU* handleDeleteFriendRequest(PDU* pdu)
{
    char deletedName[32] = {'\0'};
    char sourceName[32] = {'\0'};
    // 拷贝读取的信息
    strncpy(deletedName, pdu -> caData, 32);
    strncpy(sourceName, pdu -> caData + 32, 32);
    bool ret = DBOperate::getInstance().handleDeleteFriend(deletedName, sourceName);

    // 给请求删除方消息提示，以返回值形式
    PDU *resPdu = mkPDU(0);
    resPdu -> uiMsgType = ENUM_MSG_TYPE_DELETE_FRIEND_RESPOND;
    if(ret)
    {
        strncpy(resPdu -> caData, DEL_FRIEND_OK, 32);
    }
    else
    {
        strncpy(resPdu -> caData, DEL_FRIEND_FAILED, 32);
    }

    // 给被删除方消息提示，如果在线的话
    MyTcpServer::getInstance().forwardMsg(deletedName, pdu);

    return resPdu;
}

// 私聊发送消息请求
PDU* handlePrivateChatRequest(PDU* pdu)
{
    char chatedName[32] = {'\0'};
    char sourceName[32] = {'\0'};
    // 拷贝读取的信息
    strncpy(chatedName, pdu -> caData, 32);
    strncpy(sourceName, pdu -> caData + 32, 32);
    qDebug() << "handlePrivateChatRequest  " << chatedName << " " << sourceName;

    PDU* resPdu = NULL;

    // 转发给对方消息  0对方存在不在线，1对方存在在线
    bool ret = MyTcpServer::getInstance().forwardMsg(chatedName, pdu);

    // 发送失败则给发送者消息
    if(!ret)// 0对方不在线
    {
        resPdu = mkPDU(0);
        resPdu -> uiMsgType = ENUM_MSG_TYPE_PRIVATE_CHAT_RESPOND;
        strcpy(resPdu -> caData, PRIVATE_CHAT_OFFLINE);
    }

    return resPdu;
}

// 群聊请求处理
void handleGroupChatRequest(PDU* pdu)
{
    QStringList strList = DBOperate::getInstance().handleFlushFriend(pdu->caData); // 查询请求，查询数据库所有在线用户

    for(QString strName:strList)
    {
        MyTcpServer::getInstance().forwardMsg(strName, pdu);
    }
}

// 创建文件夹请求处理
PDU* handleCreateDirRequest(PDU* pdu)
{
    char caDirName[32];
    char caCurPath[pdu -> uiMsgLen];
    strncpy(caDirName, pdu -> caData, 32);
    strncpy(caCurPath, (char*)pdu -> caMsg, pdu -> uiMsgLen);

    QString strDir = QString("%1/%2").arg(caCurPath).arg(caDirName);
    QDir dir;
    PDU *resPdu = mkPDU(0);
    resPdu -> uiMsgType = ENUM_MSG_TYPE_CREATE_DIR_RESPOND;

    qDebug() << "创建文件夹：" << strDir;
    if(dir.exists(caCurPath)) // 路径存在
    {
        if(dir.exists(strDir)) // 文件夹已经存在
        {
            strncpy(resPdu -> caData, CREATE_DIR_EXIST, 32);
        }
        else
        {
            dir.mkdir(strDir); // 创建文件夹
            strncpy(resPdu -> caData, CREATE_DIR_OK, 32);
        }
    }
    else // 路径不存在
    {
        strncpy(resPdu -> caData, PATH_NOT_EXIST, 32);
    }

    return resPdu;
}

void MyTcpSocket::receiveMsg()
{
    // qDebug() << this -> bytesAvailable(); // 输出接收到的数据大小
    uint uiPDULen = 0;
    this -> read((char*)&uiPDULen, sizeof(uint)); // 先读取uint大小的数据，首个uint正是总数据大小
    uint uiMsgLen = uiPDULen - sizeof(PDU); // 实际消息大小，sizeof(PDU)只会计算结构体大小，而不是分配的大小
    PDU *pdu = mkPDU(uiMsgLen);
    this -> read((char*)pdu + sizeof(uint), uiPDULen - sizeof(uint)); // 接收剩余部分数据（第一个uint已读取）
    // qDebug() << pdu -> uiMsgType << ' ' << (char*)pdu -> caMsg; // 输出

    // 根据不同消息类型，执行不同操作
    PDU* resPdu = NULL; // 响应报文
    switch(pdu -> uiMsgType)
    {
    case ENUM_MSG_TYPE_REGIST_REQUEST: // 注册请求
    {
        resPdu = handleRegistRequest(pdu); // 请求处理
        break;
    }
    case ENUM_MSG_TYPE_LOGIN_REQUEST: // 登录请求
    {
        resPdu = handleLoginRequest(pdu, m_strName);
        break;
    }
    case ENUM_MSG_TYPE_ONLINE_USERS_REQUEST: // 查询所有在线用户请求
    {
        resPdu = handleOnlineUsersRequest();
        break;
    }
    case ENUM_MSG_TYPE_SEARCH_USER_REQUEST: // 查找用户请求
    {
        resPdu = handleSearchUserRequest(pdu);
        break;
    }
    case ENUM_MSG_TYPE_ADD_FRIEND_REQUEST: // 添加好友请求
    {
        resPdu = handleAddFriendRequest(pdu);
        break;
    }
    case ENUM_MSG_TYPE_ADD_FRIEND_AGREE: // 同意加好友
    {
        handleAddFriendAgree(pdu);
        break;
    }
    case ENUM_MSG_TYPE_ADD_FRIEND_REJECT: // 拒绝加好友
    {
        handleAddFriendReject(pdu);
        break;
    }
    case ENUM_MSG_TYPE_FLSUH_FRIEND_REQUEST: // 刷新好友请求
    {
        resPdu = handleFlushFriendRequest(pdu);
        break;
    }
    case ENUM_MSG_TYPE_DELETE_FRIEND_REQUEST: // 删除好友请求
    {
        resPdu = handleDeleteFriendRequest(pdu);
        break;
    }
    case ENUM_MSG_TYPE_PRIVATE_CHAT_REQUEST: // 私聊请求
    {
        resPdu = handlePrivateChatRequest(pdu);
        break;
    }
    case ENUM_MSG_TYPE_GROUP_CHAT_REQUEST: // 群聊请求
    {
        handleGroupChatRequest(pdu);
        break;
    }
    case ENUM_MSG_TYPE_CREATE_DIR_REQUEST: // 创建文件夹请求
    {
        resPdu = handleCreateDirRequest(pdu);
        break;
    }
    default:
        break;
    }

    // 响应客户端
    if(NULL != resPdu)
    {
        // qDebug() << resPdu -> uiMsgType << " " << resPdu ->caData;
        this -> write((char*)resPdu, resPdu -> uiPDULen);
        // 释放空间
        free(resPdu);
        resPdu = NULL;
    }
    // 释放空间
    free(pdu);
    pdu = NULL;
}

void MyTcpSocket::handleClientOffline()
{
    DBOperate::getInstance().handleOffline(m_strName.toStdString().c_str());
    emit offline(this); // 发送给mytcpserver该socket删除信号
}



