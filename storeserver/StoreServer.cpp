#include "StoreServer.h"

StoreServer::StoreServer(uint16_t port, const char *ip, std::string _url, std::string _user, std::string _passWord, std::string _databasename, int _port, int _maxConn, std::string _logFileName, int _maxLines){
    // 网络路TCP服务端初始化
    tcpServer = std::make_unique<TcpServer>(port, ip, _logFileName, _maxLines);
    std::function<void(Connection*, int)> handleCb = std::bind(&StoreServer::handleHttpConn, this, std::placeholders::_1, std::placeholders::_2);
    tcpServer->setOnHandle(handleCb);

    // 初始化http中要使用的数据库，数据库连接池(单例模式)
    MysqlConnPool::getInstance()->init(_url, _user, _passWord, _databasename, _port, _maxConn);
}

StoreServer::~StoreServer(){}

// 自定义业务逻辑，设置回调函数
// 在进入该函数前，服务器已经完成了接受客户端数据并保存在读缓冲区里，
// 业务逻辑只需要将读缓冲区里的数据发送回即可，这样的设计更加符合服务器的功能准则与设计准则
// 现在Connection类只有从socket读写数据的逻辑，与具体业务没有任何关系，业务完全由用户自定义
void StoreServer::handleHttpConn(Connection *conn, const int _sockFd){
        if (conn->getStateConn() == Connection::State::Closed) {
            conn->closeConn();
            httpConns.erase(_sockFd);
        }
        else{
            // 对tcp和http进行绑定，http在tcp的基础上进行封装处理
            if(httpConns.find(_sockFd) == httpConns.end())
                httpConns.emplace(_sockFd, std::make_unique<HttpConn>());
            
            // 读取客户端发送的数据
            // 因为会有文件等传输，使用指针/引用来操作避免复制
            // 业务逻辑对从客户端读取的数据进行处理
            HTTP_CODE ret = httpConns[_sockFd]->processRead(conn->getReadBuf()->getBuf());
        
            // 处理完毕，设置发送数据
            bool flag = httpConns[_sockFd]->processWrite(ret, conn->getSendBuf()->getBuf());
            if(!flag) conn->closeConn();
        }
}