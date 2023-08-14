#pragma once
#include "../http/HttpConn.h"

class StoreServer{
private:
    // 网络路TCP服务端初始化
    std::unique_ptr<TcpServer> tcpServer;
    // http连接
    std::unordered_map<int, std::unique_ptr<HttpConn>> httpConns;

public:
    StoreServer(uint16_t port, const char *ip, std::string _url, std::string _user, std::string _passWord, std::string _databasename, int _port, int _maxConn, std::string _logFileName, int _maxLines);
    ~StoreServer();

    void Start() { tcpServer->Start(); }

    // 自定义业务逻辑
    void handleHttpConn(Connection* conn, const int _sockFd);
};