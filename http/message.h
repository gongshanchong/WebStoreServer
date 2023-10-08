/*  文件说明：
 *  1. 该文件中定义的类用于在事件处理中表示 “请求消息” 和 “响应消息”
 *  2. Request 表示浏览器的请求消息，用来记录其中的重要字段，及事件处理中对请求消息处理了多少
 *  3. Response 表示向客户端回复的响应消息，记录响应消息中待发送的数据，如状态行、首部、消息体、响应消息已发送多少
 */
#pragma once
#ifndef MESSAGE_H
#define MESSAGE_H
#include <iostream>
#include <algorithm>
#include <dirent.h>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <unordered_map>
#include <mutex>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h> 
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <cstdio>
#include "../CGImysql/MysqlConnPool.h"

// 文件路径
const std::string fileRoot = "./root";

//定义http响应的一些状态信息
const std::string ok_200_title = "OK";
const std::string move_302_title = "Moved Temporarily";
const std::string error_400_title = "Bad Request";
const std::string error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.";

const std::string error_403_title = "Forbidden";
const std::string error_403_form = "You do not have permission to get file form this server.";

const std::string error_404_title = "Not Found";
const std::string error_404_form = "The requested file was not found on this server.";

const std::string  error_500_title = "Internal Error";
const std::string error_500_form = "There was an unusual problem serving the request file.";

// 表示 Request中数据的处理状态
enum CHECK_STATE
{
    CHECK_STATE_REQUESTLINE = 0,            // 解析请求行
    CHECK_STATE_HEADER,                     // 解析请求头
    CHECK_STATE_CONTENT                     // 解析消息体
};

// 报文解析的结果
enum HTTP_CODE
{
    NO_REQUEST = 0,                         // 请求不完整，需要继续读取请求报文数据
    GET_REQUEST,                            // 获得了完整的HTTP请求
    BAD_REQUEST,                            // HTTP请求报文有语法错误
    NO_RESOURCE,
    FORBIDDEN_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,                         // 服务器内部错误，该结果在主状态机逻辑switch的default下，一般不会触发
    CLOSED_CONNECTION,
    REDIRECR,                               // 重定向
};

// 表示消息体的类型
enum MSGBODYTYPE{
    FILE_TYPE_DONLOAD,      // 消息体是文件下载
    FILE_TYPE_PREVIEW,      // 消息体是文件预览
    HTML_TYPE,              // 消息体是 HTML 页面
    IMG_TYPE,               // 消息体为图像
    EMPTY_TYPE,             // 消息体为空
};

// 当接收文件时，消息体会分不同的部分，用该类型表示文件消息体已经处理到哪个部分
enum FILEMSGBODYSTATUS{
    CHECK_STATE_FILE_ZERO,          // 起始状态
    CHECK_STATE_FILE_BEGIN,         // 正在获取并处理表示文件开始的标志行
    CHECK_STATE_FILE_HEAD,          // 正在获取并处理文件属性部分
    CHECK_STATE_FILE_CONTENT,       // 正在获取并处理文件内容的部分
};

// 对于状态行修改和获取，设置要发送的首部选项
class Response{
public:
    std::string responseMethod;                           // 请求消息的请求方法
    // 以下成员主要用于在发送响应消息时暂存相关的数据
    MSGBODYTYPE bodyType;                                 // 消息的类型
    std::string bodyFileName;                             // 请求报文中请求资源路径
    std::string opera;                                    // 操作方法
    std::string realFile;                                 // 文件
    std::string fileType;                                 // 请求文件的类型

    std::string beforeBodyMsg;                            // 消息体之前的所有数据

    std::string msgBody;                                  // 在字符串中保存 HTML 类型的消息体
    unsigned long msgBodyLen;                             // 消息体的长度

    std::string userName;                                 // 用户的登录名
    bool lingerConn = false;                              // 记录是否是长连接

    int fileMsgFd;                                        // 文件类型的消息体保存文件描述符
private:
    // 非文本类型（依据浏览器可以显示的文件类型进行区分，参考MIME类型，列举一些常见的文件后缀）
    const std::string nonTextType = ".gz .rar .exe .bz2 .tar .xz .Z .rpm zip .a .so .o .jar .dll .lib .deb .I .png .jpg .jpeg .mp3 .mp4 .m4a .flv .mkv .rmvb .avi .pcap .pdf .docx .xlsx .pptx .ram .mid .dwg";
    // 对url中带中文参数进行解码
    unsigned char NUM_2_HEX(const char h, const char l){
        std::string str = "0123456789ABCDEF";
        unsigned char hh = std::find(str.begin(), str.end(), h) - str.begin();
        unsigned char ll = std::find(str.begin(), str.end(), l) - str.begin();
        return (hh << 4) + ll;
    }

    std::string Decode(const std::string & url){
        std::string ret;
        for (auto it = url.begin(); it != url.end(); ++it)
        {
            if (*it == '%')
            {
                if (std::next(it++) == url.end())
                {
                    throw std::invalid_argument("url is invalid");
                }
                ret.push_back(NUM_2_HEX(*it, *std::next(it)));
                if (std::next(it++) == url.end())
                {
                    throw std::invalid_argument("url is invalid");
                }
            }
            else
            {
                ret.push_back(*it);
            }
        }
        return ret;
    }


public:
    void clear(){
        if(!beforeBodyMsg.empty()){
            beforeBodyMsg.clear();
        }
        if(!msgBody.empty()){
            msgBody.clear();
        }
    }

    // 添加状态行：http/1.1 状态码 状态消息
    // 用于构建状态行，参数分别表示状态行的三个部分
    void addStatus(const std::string &statusCode, const std::string &statusDes){
        // 构建状态行
        beforeBodyMsg = "HTTP/1.1 ";
        beforeBodyMsg += statusCode + " ";
        beforeBodyMsg += statusDes + "\r\n";
    }

    // 构建头部字段：
    // contentLength        : 指定消息体的长度
    // contentType          : 指定消息体的类型
    // Connection:          : 是否保持连接
    // redirectLoction = "" : 如果是重定向报文，可以指定重定向的地址。空字符串表示不添加该首部。
    // contentRange = ""    : 如果是下载文件的响应报文，指定当前发送的文件范围。空字符串表示不添加该首部
    // 添加消息报头，具体的添加文本长度、连接状态和空行
    bool addHaeders(const std::string &contentLength, const MSGBODYTYPE contentType = EMPTY_TYPE, const std::string &contentRange = "", const std::string &redirectLoction = ""){
        std::string headerOpt;

        // 添加消息体长度字段
        if(contentLength != ""){
            beforeBodyMsg += "Content-Length: " + contentLength + "\r\n";
        }

        // 添加消息体类型字段
        // 如果是浏览器支持的文件类型，一般会默认使用浏览器打开，比如txt、jpg等
        if(contentType == HTML_TYPE){
            // 发送网页时指定的类型
            beforeBodyMsg += "Content-Type: text/html;charset=UTF-8\r\n";     
        }else if(contentType == IMG_TYPE){
            // 发送信息时指定的类型
            beforeBodyMsg += "Content-Type: image/png\r\n";
        // application/octet-stream(二进制文件的默认值，与Content-Disposition: attachment效果等同)
        // 类型一般会配合另一个响应头Content-Disposition,该响应头指示回复的内容该以何种形式展示
        // 是以内联的形式（即网页或者网页的一部分），还是以附件的形式下载并保存到本地
        }else if(contentType == FILE_TYPE_DONLOAD || contentType == FILE_TYPE_PREVIEW){
            // 设置编码，为避免文本中中文乱码问题，依据请求报文中的文件后缀来进行判断是否需要设置文件类型
            // 对于那些没有明确子类型的文本文档，应使用 text/plain。
            // 没有明确子类型或子类型未知的二进制文件，应使用 application/octet-stream
            // 其他类型的文件，浏览器会默认进行显示
            if(contentType == FILE_TYPE_PREVIEW && nonTextType.find(fileType) == std::string::npos) 
                beforeBodyMsg += "Content-Type: text/plain; charset=UTF-8\r\n";
            // 是以附件的形式下载并保存到本地 或者 是以内联的形式(浏览器无法显示的会直接下载到本地)
            beforeBodyMsg += "Content-Disposition: ";
            beforeBodyMsg += (contentType == FILE_TYPE_DONLOAD)? "attachment; " : "inline; ";
            beforeBodyMsg +=  "filename=";
            beforeBodyMsg += bodyFileName + "\r\n";

            // 添加文件范围的字段
            if(contentRange != ""){
                beforeBodyMsg += "Content-Range: 0-" + contentRange + "\r\n";
            }
        }

        // 添加重定向位置字段
        if(redirectLoction != ""){
            beforeBodyMsg += "Location: " + redirectLoction + "\r\n";
        }

        // 是否保持连接
        beforeBodyMsg += "Connection: ";
        beforeBodyMsg += (lingerConn? "keep-alive" : "close");
        beforeBodyMsg += "\r\n";

        // 添加空行
        beforeBodyMsg += "\r\n";

        return true;
    }

    // 首先分辨响应报文类型
    // 然后分离操作方法和文件，请求报文中解析出的请求资源，以/开头，也就是/xxx
    // 如果不是访问根目录，根据 / 对URL中的路径（如 /delete/filename）(/log、/register)进行分隔，找到要执行的操作和操作的文件
    void getFilePath(){
        // 根据 / 对URL中的路径(/log、/register、/redirect、/filelist)进行分隔
        std::string::size_type endIndex = bodyFileName.find("/", 1);
        if(endIndex == std::string::npos){
            opera = "html";
            if(bodyFileName == "/"){ realFile = fileRoot + "/htmls/start.html";}
            else if(bodyFileName == "/log" || bodyFileName == "/log?") { realFile = fileRoot + "/htmls/log.html";}
            else if(bodyFileName == "/logError"){ realFile = fileRoot + "/htmls/logError.html";}
            else if(bodyFileName == "/register" || bodyFileName == "/register?") { realFile = fileRoot + "/htmls/register.html";}
            else if(bodyFileName == "/registerError"){ realFile = fileRoot + "/htmls/registerError.html";}
            else if(bodyFileName == "/favicon.ico"){
                opera = "image/png";
                realFile = fileRoot + bodyFileName;
            }
            else if(bodyFileName == "/filelist"){
                // 构造重定向
                if(userName.size() == 0){
                    opera = "redirect";
                    realFile = "/";
                }
                else{
                    opera = "filelist";
                    realFile = "/filelist";
                }
            }
            // 构造重定向
            else{ 
                opera = "redirect";
                realFile = "/filelist";
            }
        }
        else{
            // 构造重定向
            if(userName.size() == 0){
                opera = "redirect";
                realFile = "/";
                return;
            }

            // 根据 / 对URL中的路径（如 /delete/filename、/download/filename）进行分隔
            // 需要对请求的url进行解码，防止有中文转换而来的乱码
            opera = bodyFileName.substr(1, endIndex-1);
            bodyFileName = Decode(bodyFileName.substr(endIndex+1));
            realFile = fileRoot + "/filedir/" + userName + "/" + bodyFileName;
            // 获取文件类型，从后往前查找文件后缀
            endIndex = bodyFileName.rfind(".");
            fileType = (endIndex != std::string::npos)?bodyFileName.substr(endIndex):"";

            if(opera == "delete"){
                // 在本地删除文件
                int ret = remove(realFile.c_str());
                errif(ret!=0, ("HTTP delete file " + realFile + " error").c_str());
                opera = "redirect";
                realFile = "/filelist";
            }
        }
    }

    // 以下两个函数用来构建文件列表的页面，最终结果保存到 fileListHtml 中
    void getFileListPage(std::string &fileListHtml){
        // 将指定目录内的所有文件保存到 fileVec 中
        std::vector<std::string> fileVec;

        // 使用 dirent 获取文件目录下的所有文件
        std::string dirName = fileRoot + "/filedir/" + userName;
        DIR *dir = opendir(dirName.c_str());   // 目录指针
        struct dirent *stdinfo;
        if(dir == NULL){
            int ret = mkdir(dirName.c_str(), 0777);
            errif(ret && errno != EEXIST, ("create dir error: " + dirName).c_str());
            dir = opendir(dirName.c_str());
        }
    
        while(true){
            // 获取文件夹中的一个文件
            stdinfo = readdir(dir);
            if (stdinfo == nullptr){ break;}

            if(strcmp(stdinfo->d_name, ".") != 0 && strcmp(stdinfo->d_name, "..") != 0){
                fileVec.push_back(stdinfo->d_name);
            }
        }

        // 构建页面
        std::ifstream fileListStream(fileRoot + "/htmls/filelist.html", std::ios::in);
        std::string tempLine;
        errif(!fileListStream.is_open(), "http get filelist error");
        // 首先读取文件列表的 <!--filelist_label--> 注释前的语句
        while(1){
            getline(fileListStream, tempLine);
            if(tempLine == "<!--filelist_label-->"){
                break;
            }
            fileListHtml += tempLine + "\n";
        }

        // 根据如下标签，将将文件夹中的所有文件项添加到返回页面中
        //             <tr><td class="col1">filenamename</td> <td class="col2"><a href="file/filename">下载</a></td> <td class="col3"><a href="delete/filename">删除</a></td></tr>
        for(const auto &filename : fileVec){
            fileListHtml += "            <tr><td class=\"col1\">" + filename +
                        "</td> <td class=\"col2\"><a href=\"preview/" + filename +
                        "\">预览</a></td> <td class=\"col3\"><a href=\"download/" + filename +
                        "\">下载</a></td> <td class=\"col4\"><a href=\"delete/" + filename +
                        "\" onclick=\"return confirmDelete();\">删除</a></td></tr>" + "\n";
        }

        // 将文件列表注释后的语句加入后面
        while(getline(fileListStream, tempLine)){
            fileListHtml += tempLine + "\n";
        }
        fileListStream.close();
    }
 
    // 添加content
    HTTP_CODE getContent(){
        if(opera == "filelist"){
            getFileListPage(msgBody);
            // 获取文件长度，作为消息体长度
            bodyType = HTML_TYPE;
            msgBodyLen = msgBody.size();
        }
        else if(opera == "html"){
            std::ifstream fileStream(realFile, std::ios::in);
            std::string tempLine;
            errif(!fileStream.is_open(), "http get file open error");

            // 读取文件
            // 消息体为 HTML 页面时的发送方法
            while(getline(fileStream, tempLine)){ msgBody += (tempLine + "\n");}
            fileStream.close();
            // 获取文件长度，作为消息体长度
            bodyType = HTML_TYPE;
            msgBodyLen = msgBody.size();
        }
        else if(opera == "download" || opera == "preview" || opera == "image/png"){
            fileMsgFd = open(realFile.c_str(), O_RDONLY);
            errif(fileMsgFd == -1, ("http get file " + realFile + " open error").c_str());

            // 获取文件信息
            struct stat fileStat;
            if(fstat(fileMsgFd, &fileStat) < 0) return NO_RESOURCE;
            // 判断文件的权限，是否可读，不可读则返回FORBIDDEN_REQUEST状态
            if (!(fileStat.st_mode & S_IROTH))  return FORBIDDEN_REQUEST;
            // 获取文件长度，作为消息体长度
            msgBodyLen = fileStat.st_size;

            if(opera == "download") bodyType = FILE_TYPE_DONLOAD;
            else if(opera == "preview") bodyType = FILE_TYPE_PREVIEW;
            else if(opera == "image/png") bodyType = IMG_TYPE;
            
            // 进行文件的读
            // 字符串数组初始值太大会报错段错误
            char fileData[2048];
            ssize_t bytes_read;
            while((bytes_read = read(fileMsgFd, fileData, 2048)) > 0)
                msgBody.append(fileData, bytes_read);
            
            close(fileMsgFd);
        }
        // 构造重定向
        else if(opera == "redirect"){
            bodyType = HTML_TYPE;
            return REDIRECR;
        }
        // 表示请求文件存在，且可以访问
        return FILE_REQUEST;
    }
};

// 对请求行的修改和获取，保存收到的首部选项
class Request{
public:
    std::mutex handleMtx;                       // 互斥锁,也成互斥量,可以保护关键代码段,以确保独占式访问

    CHECK_STATE status;                         // 记录请求消息的解析状态
    std::string requestMethod;                  // 请求消息的请求方法
    std::string rquestResourse;                 // 请求的资源
    std::string httpVersion;                    // 请求的HTTP版本
    long long contentLength = 0;                // 记录消息体的长度　
    std::unordered_map<std::string, std::string> msgHeader;  // 保存消息首部
    std::string fileData;                       // 保存文件数据

    std::string userName;                        // 用户的登录名
    std::string userPassword;                    // 用户的密码
    std::string recvFileName;                    // 如果客户端发送的是文件，记录文件的名字
    FILEMSGBODYSTATUS fileMsgStatus;             // 记录表示文件的消息体已经处理到哪些部分
private:
    HTTP_CODE parseFileContent(std::string &text, Response &httpResponse){
        // 对这请求消息体进行解析
        std::string::size_type endIndex;
        std::string line;

        // 读取文件的状态转移逻辑
        while(fileMsgStatus != CHECK_STATE_FILE_CONTENT){
            endIndex = text.find("\r\n");                    // 查找请求行的结束边界
            line = text.substr(0, endIndex + 2);             // 获取请求报文的一行

            switch (fileMsgStatus){
            // 如果处于等待处理文件开始标志的状态，查找 \r\n 判断标志部分是否已经接收
            case CHECK_STATE_FILE_BEGIN:
            {
                // 当前状态下，\r\n 前的数据必然是文件信息开始的标志
                // 判断文件开始
                // 文件开始的标志
                // post 请求上传文件的时候是不需要自己设置 Content-Type，会自动给你添加一个 boundary ，用来分割消息主体中的每个字段
                std::string flagStr = line.substr(0, endIndex);
                // 如果等于 "--" 加边界，进入下一个状态
                if(flagStr == "--" + msgHeader["boundary"]){
                    fileMsgStatus = CHECK_STATE_FILE_HEAD;
                }
                // 如果和边界不同，表示出错，重新请求文件列表
                else{
                    fileMsgStatus = CHECK_STATE_FILE_ZERO;
                    httpResponse.bodyFileName = "/redirect";
                    return GET_REQUEST;
                }
                break;
            }
            // 如果处于等待接收并处理消息体中文件头部信息的状态，从中提取文件名
            case CHECK_STATE_FILE_HEAD:
            {
                // 对这行进行解析
                // 文件消息头和文件之间有个空行
                // 检测是否为空行，如果是空行,退出
                if(line == "\r\n"){
                    fileMsgStatus = CHECK_STATE_FILE_CONTENT;
                    break;
                }

                // 查找 strLine 是否包含 filename
                // 文件名格式：“filename”
                std::string::size_type fileIndex = line.find("filename");
                if(fileIndex != std::string::npos){
                    // 保存文件名
                    int i;
                    recvFileName.clear();
                    for(i = fileIndex; line[i] != '\"'; ++i){}
                    for(i = i+1 ; line[i] != '\"'; ++i) recvFileName += line[i];
                }
                break;
            }
            default:
                return INTERNAL_ERROR;
            }
            // 删除收到的文件数据中的文件信息
            text.erase(0, endIndex + 2);                 
        }
        
        // 如果处于等待并处理消息体中文件内容部分
        // 循环检索是否有 \r\n ，将 \r\n 之前的内容全部保存，并判断边界
        // 正文中每一部分用换行符代表结束
        // 文件结尾有htpp添加的\r\n（即文件数据和结束边界）
        // 整个报文使用 “–-boundary-–”表示结束
        // 文件结束的标志：\r\n + "--" + boundary + "--" + \r\n(前面的为http报文所加，最后的\r\n为程序添加的)
        // 判断后面这部分数据是否为结束边界"\r\n"
        endIndex = text.find("--" + msgHeader["boundary"]);
        // 如果找到边界，进行写
        if(endIndex != std::string::npos){
            // 首先以二进制追加的方式打开文件
            // 文件存储在filedir文件夹中，以每个用户的名称进行区分存储
            fileData += text.substr(0, endIndex-2);
            std::ofstream ofs(fileRoot + "/filedir/" + userName + "/" + recvFileName, std::ios::out | std::ios::binary | std::ios_base::trunc);
            errif(!ofs, ("open file of " + userName + " error").c_str());

            // 数据存入文件
            ofs.write(fileData.c_str(), fileData.size());
            fileMsgStatus = CHECK_STATE_FILE_ZERO;
            fileData.clear();
            ofs.close();

            // 设置响应消息的资源路径，在 Response 中根据请求资源构建整个响应消息并发送
            httpResponse.bodyFileName = "/redirect";
            return GET_REQUEST;
        }
        else
            fileData += text;

        return NO_REQUEST;
    }

    // 如果sql中定义的类型是int型的可以不用加引号，但是如果是字符串类型的，必须加引号
    HTTP_CODE parseFormContent(std::string &text, Response &httpResponse){
        //先从连接池中取一个连接
        MYSQL *mysql = NULL;
        MysqlConn mysqlcon(&mysql, MysqlConnPool::getInstance());
        // sql语句
        std::string sql;
        // 获取用户名和密码
        int i;
        userName.clear();
        userPassword.clear();
        for(i = 5; text[i] != '&'; ++i)
            userName.push_back(text[i]);
        for(i = i+10; text[i] != '\r'; ++i)
            userPassword.push_back(text[i]);
        httpResponse.userName = userName;

        // 同步线程登录校验
        if(rquestResourse.find("log") != std::string::npos){
            sql = "SELECT username, passwd FROM user WHERE username = ";
            sql += "'" + userName + "'";
            sql += " AND passwd = ";
            sql += "'" +  userPassword + "'";

            // 如果查询成功，返回0。如果出现错误，返回非0值
            int res = mysql_query(mysql, sql.c_str());
            errif(res!=0, mysql_error(mysql));
            //从表中检索完整的结果集
            MYSQL_RES *result = mysql_store_result(mysql);
            //返回结果集中的行数
            int num_rows = mysql_num_rows(result);
            // 释放结果占用的内存
            mysql_free_result(result);

            if (num_rows)
                httpResponse.bodyFileName = "/redirect";
            else
                httpResponse.bodyFileName = "/logError";
        }
        // 同步线程注册校验
        else if(rquestResourse.find("register") != std::string::npos){
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据
            sql = "SELECT username FROM user WHERE username = ";
            sql += "'" + userName + "'";

            // 如果查询成功，返回0。如果出现错误，返回非0值
            int res = mysql_query(mysql, sql.c_str());
            errif(res!=0, mysql_error(mysql));
            //从表中检索完整的结果集
            MYSQL_RES *result = mysql_store_result(mysql);
            //返回结果集中的行数
            int num_rows = mysql_num_rows(result);
            // 释放结果占用的内存
            mysql_free_result(result);

            if (num_rows)
                httpResponse.bodyFileName = "/registerError";
            else{
                sql = "INSERT INTO user(username, passwd) VALUES(";
                sql += ("'" + userName + "', '");
                sql += (userPassword + "')");
                {
                    std::unique_lock<std::mutex> lock(handleMtx);
                    int res = mysql_query(mysql, sql.c_str());
                }
                //校验成功，跳转登录页面
                if (!res)
                    httpResponse.bodyFileName =  "/log";
                //校验失败，跳转注册失败页面
                else
                    httpResponse.bodyFileName =  "/registerError";
            }
        }

        return GET_REQUEST;
    }


public:
    Request(): status(CHECK_STATE_REQUESTLINE), fileMsgStatus(CHECK_STATE_FILE_ZERO){}

    // 主状态机解析报文中的请求行数据
    HTTP_CODE parseRequestLine(const std::string &text, Response &httpResponse){
        std::istringstream lineStream(text);
        // 获取请求方法
        lineStream >> requestMethod;
        httpResponse.responseMethod = requestMethod;
        // 获取请求资源
        lineStream >> rquestResourse;
        // 获取http版本
        lineStream >> httpVersion;

        // 仅支持HTTP/1.1
        return (httpVersion.find("HTTP/1.1") == std::string::npos)? BAD_REQUEST : NO_REQUEST;
    }

    // 主状态机解析报文中的请求头数据
    HTTP_CODE parseHeaders(const std::string &text, Response &httpResponse){
        // 检测是否为空行，如果是空行，修改状态，退出
        if(text == "\r\n") return NO_REQUEST;

        static std::istringstream lineStream;
        lineStream.str(text);         // 以 istringstream 的方式处理头部选项

        std::string key, value;      // 保存键和值的临时量

        lineStream >> key;           // 获取 key
        key.pop_back();              // 删除键中的冒号 
        lineStream.get();            // 删除冒号后的空格

        // 读取空格之后所有的数据，遇到 \n 停止，所以 value 中还包含一个 \r
        getline(lineStream, value);
        value.pop_back();            // 删除其中的 \r
        
        if(key == "Connection"){
            httpResponse.lingerConn = (value == "keep-alive")? true : false;
        }
        else if(key == "Content-Length"){
            // 保存消息体的长度
            contentLength = std::stoll(value);
        }
        else if(key == "Content-Type"){
            // 分离消息体类型。消息体类型可能是复杂的消息体，类似 Content-Type: multipart/form-data; boundary=---------------------------24436669372671144761803083960
            
            // 先找出值中分号的位置
            std::string::size_type semIndex = value.find(';');
            // 根据分号查找的结果，保存类型的结果
            if(semIndex != std::string::npos){
                msgHeader[key] = value.substr(0, semIndex);

                std::string::size_type eqIndex = value.find('=', semIndex);
                key = value.substr(semIndex + 2, eqIndex - semIndex - 2);
                msgHeader[key] = value.substr(eqIndex + 1);
            }else{
                msgHeader[key] = value;
            }
            
        }else{
            msgHeader[key] = value;
        }

        return NO_REQUEST;
    }   

    // 主状态机解析报文中的请求内容
    // 拆分为多种函数的结合
    HTTP_CODE parseContent(std::string &text, Response &httpResponse){
        // 对这请求消息体进行解析
        // 如果客户端发送的是文件
        if(msgHeader["Content-Type"] == "multipart/form-data"){
            msgHeader.clear();
            return parseFileContent(text, httpResponse);
        }
        // POST 其他类型的数据，本项目是登录时产生的数据，类型为提交表单，并且依据本项目的html页面来验证该post的请求是否为注册登陆
        // 将用户名和密码提取出来
        // 数据格式：user=123&password=123\r\n(最后的\r\n为程序添加的)
        else if (msgHeader["Content-Type"] == "application/x-www-form-urlencoded" && rquestResourse.find("CGI") != std::string::npos){
            msgHeader.clear();
            return parseFormContent(text, httpResponse);
        }
        // 其他 POST 类型的数据时，直接返回，获取文件列表
        else{
            httpResponse.bodyFileName = "/redirect";
            return GET_REQUEST;
        }
    }
};


#endif