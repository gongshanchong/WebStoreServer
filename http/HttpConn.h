#pragma once
#include "message.h"
#include "../tcpserver/TcpServer.h"

class HttpConn
{
public:
    Request httpRequset;
    Response httpResponse;

public:
    HttpConn(){};
    ~HttpConn(){};

    HTTP_CODE processRead(std::string &recvMsg);
    bool processWrite(HTTP_CODE ret, std::string &sendMsg);

    void handleConn(Connection *conn);
};