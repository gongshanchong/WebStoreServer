#include "./storeserver/StoreServer.h"
#include "tcpserver/TcpServer.h"
#include <memory>

int main() {
  // 参数
  // 服务器端口
  uint16_t port = 80;
  const char *ip_add = nullptr;
  // 数据库相关
  std::string mysqlUrl = "localhost";			        // 主机地址
  std::string mysqlUser = "root";		            // 登陆数据库用户名
	std::string mysqlPassWord = "1106";	            // 登陆数据库密码
	std::string databaseName = "login";               // 使用数据库名
  int mysqlPort = 3306;		                    // 数据库端口号
  int mSqlNum = 8;                            // 数据库连接池的大小
  // 日志相关
  std::string logFileName = "./log/serverLog/serverLog";  // 日志存储的文件名
  int maxLines = 5000000;                                 // 日志

  // 服务端初始化
  std::unique_ptr<StoreServer> storeserver = std::make_unique<StoreServer>(port, ip_add, mysqlUrl, mysqlUser, mysqlPassWord, databaseName, mysqlPort, mSqlNum, logFileName, maxLines);

  storeserver->Start();

  return 0;
}
