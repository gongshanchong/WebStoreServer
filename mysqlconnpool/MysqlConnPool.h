#pragma once
#ifndef _MYSQLCONNPOOL_
#define _MYSQLCONNPOOL_

#include <stdio.h>
#include <list>
#include <memory>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include <mutex>
#include <semaphore.h>
#include "../utils/utils.h"

// 在程序初始化的时候，集中创建多个数据库连接，并把他们集中管理，供程序使用，可以保证较快的数据库读写速度，更加安全可靠。
class MysqlConnPool
{
private:
    std::string mysqlUrl;			        // 主机地址
	std::string mysqlPort;		            // 数据库端口号
	std::string mysqlUser;		            // 登陆数据库用户名
	std::string mysqlPassWord;	            // 登陆数据库密码
	std::string databaseName;               // 使用数据库名
	int maxConn;                            // 最大连接数
	int curConn;                            // 当前已使用的连接数
	int freeConn;                           // 当前空闲的连接数
	std::mutex handleMtx;                   // 互斥锁,也成互斥量,可以保护关键代码段,以确保独占式访问
	sem_t reserve; 							// 信号量
	std::list<MYSQL*> connList;             // 连接池

public:
    //单例模式
	static MysqlConnPool *getInstance(){
		static MysqlConnPool mysqlConnPool;
		return &mysqlConnPool;
	}
    void init(std::string _url, std::string _user, std::string _passWord, std::string _databasename, int _port, int _maxConn = 8);

    MysqlConnPool();
	~MysqlConnPool();

	MYSQL *getMysqlConn();				    //获取数据库连接
	bool releaseMysqlConn(MYSQL *conn);     //释放连接
	int getFreeConnSize();					//获取空闲连接数
	void destroyMysqlConnPool();		    //销毁所有连接

};

//在获取连接时，通过有参构造对传入的参数进行修改。
//其中数据库连接本身是指针类型，所以参数需要通过双指针才能对其进行修改
class MysqlConn{

public:
	//双指针对MYSQL *con修改
	MysqlConn(MYSQL **conn, MysqlConnPool *connPool);
	// RALL机制
	~MysqlConn();
	
private:
	MYSQL *mysqlConn;
	MysqlConnPool *mysqlPool;
};

#endif