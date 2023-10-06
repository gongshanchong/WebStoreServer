#include "HttpConn.h"
#include <cstddef>

HTTP_CODE HttpConn::processRead(std::string &recvMsg){
    // 保存字符串查找结果，每次查找都可以用该变量暂存查找结果
    // 在HTTP报文中，每一行的数据由\r\n作为结束字符，空行则是仅仅是字符\r\n。因此，可以通过查找\r\n将报文拆解成单独的行进行解析
    std::string::size_type endIndex = 0;
    std::string line;
    HTTP_CODE ret = NO_REQUEST;
    
    // 在HTTP报文中，每一行的数据由\r\n作为结束字符，空行则是仅仅是字符\r\n
    // 解析消息体以前的http报文数据
    // 给报文末尾添加字符\r\n，便于后续POST中的消息体处理
    if(httpRequset.fileMsgStatus == CHECK_STATE_FILE_ZERO)recvMsg += "\r\n";
    while(httpRequset.status != CHECK_STATE_CONTENT){
        endIndex = recvMsg.find("\r\n");                    // 查找请求行的结束边界
        line = recvMsg.substr(0, endIndex + 2);             // 获取请求报文的一行

        // 状态机的三种状态转移逻辑
        switch (httpRequset.status){
        case CHECK_STATE_REQUESTLINE:
        {
            // 解析请求行
            // 从HTTP 1.1开始支持持久连接，也就是一次TCP连接可以发送多次的HTTP请求
            ret = httpRequset.parseRequestLine(line, httpResponse);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            httpRequset.status = CHECK_STATE_HEADER;
            LOG_INFO("%s", line.substr(0, endIndex).c_str());
            break;
        }
        case CHECK_STATE_HEADER:
        {
            // 解析请求头
            ret = httpRequset.parseHeaders(line, httpResponse);

            // 请求头部和请求数据之间有空行
            if(line == "\r\n"){
                // 如果是空行，将状态修改为等待解析消息体
                httpRequset.status = CHECK_STATE_CONTENT;                                              
                if(httpRequset.requestMethod == "GET"){
                    // 初始化状态
                    httpRequset.status = CHECK_STATE_REQUESTLINE;
                    // GET 操作时表示请求数据，将请求的资源路径交给 processWrite 处理
                    httpResponse.bodyFileName = httpRequset.rquestResourse;
                    return GET_REQUEST;
                }
                // 文件上传分几次请求，第一次的请求为请求状态、请求头，第一次请求设置文件的解析状态
                if(httpRequset.msgHeader["Content-Type"] == "multipart/form-data"){         
                    // 如果接收的是文件，设置消息体中文件的处理状态
                    httpRequset.fileMsgStatus = CHECK_STATE_FILE_BEGIN;
                    httpResponse.bodyFileName = "/redirect";
                    return NO_REQUEST;
                }
            }
            else LOG_INFO("%s", line.substr(0, endIndex).c_str());
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
        // 删除收到的数据中的请求行
        recvMsg.erase(0, endIndex + 2);                 
    }
    // 解析请求体
    // POST 表示上传数据，执行接收数据的操作
    // 上传文件的请求消息头和文件是分两次发送的，通过这个条件来接收文件
    // GET产生一个TCP数据包；POST产生两个TCP数据包
    ret = httpRequset.parseContent(recvMsg, httpResponse);
    // 完整解析POST请求后，跳转到报文响应函数
    if(ret == GET_REQUEST) 
        // 初始化状态
        httpRequset.status = CHECK_STATE_REQUESTLINE;

    return ret;
}

bool HttpConn::processWrite(HTTP_CODE ret, std::string &sendMsg){
    // 清空上次发送的消息
    httpResponse.clear();
    
    switch (ret){
    // 内部错误，500
    case INTERNAL_ERROR:
    {
        httpResponse.addStatus("500", error_500_title);
        httpResponse.addHaeders(std::to_string(error_500_form.size()));
        sendMsg += httpResponse.beforeBodyMsg;
        sendMsg += error_500_form;
        break;
    }
    // 报文语法有误，400
    case BAD_REQUEST:
    {
        httpResponse.addStatus("400", error_400_title);
        httpResponse.addHaeders(std::to_string(error_400_form.size()));
        sendMsg += httpResponse.beforeBodyMsg;
        sendMsg += error_400_form;
        break;
    }
    // 依据完整的http请求报文构建响应报文
    case GET_REQUEST:
    {   
        // 解析出请求报文中的文件路径
        httpResponse.getFilePath();
        // 获取响应报文中的响应正文
        HTTP_CODE res = httpResponse.getContent();
        if(res == FILE_REQUEST){
            // 文件存在，200
            httpResponse.addStatus("200", ok_200_title);
            httpResponse.addHaeders(std::to_string(httpResponse.msgBodyLen), httpResponse.bodyType, std::to_string(httpResponse.msgBodyLen-1));
            sendMsg += httpResponse.beforeBodyMsg;
            sendMsg += httpResponse.msgBody;
        }
        else if(res == REDIRECR){
            // 构建重定向
            httpResponse.addStatus("302", move_302_title);
            httpResponse.addHaeders("0", httpResponse.bodyType, "", httpResponse.realFile);
            sendMsg += httpResponse.beforeBodyMsg;
        }
        else{
            // 请求的资源大小为0或者权限不够
            httpResponse.addStatus("404", error_404_title);
            httpResponse.addHaeders(std::to_string(error_400_form.size()));
            sendMsg += httpResponse.beforeBodyMsg;
            sendMsg += error_404_form;
        }
        break;
    }
    // 上传文件未完全接收时，不发送任何数据
    case NO_REQUEST:
        break;
    default:
        return false;
    }
    LOG_INFO("request:%s", httpResponse.beforeBodyMsg.c_str());

    return true;
}