#include "MysqlConnPool.h"

MysqlConnPool::MysqlConnPool(){
    curConn =  0;
    freeConn = 0;
}

MysqlConnPool::~MysqlConnPool(){ this->destroyMysqlConnPool(); }

void MysqlConnPool::init(std::string _url, std::string _user, std::string _passWord, std::string _databasename, int _port, int _maxConn){
    mysqlUrl = _url;
    mysqlUser = _user;
    mysqlPassWord = _passWord;
    databaseName = _databasename;
    mysqlPort = _port;

    for (int i = 0; i < _maxConn; i++){
        MYSQL *conn = NULL;
		conn = mysql_init(conn);
        errif(conn == nullptr, "MySQL init Error");
                
        conn = mysql_real_connect(conn, _url.c_str(), _user.c_str(), _passWord.c_str(), _databasename.c_str(), _port, NULL, 0);
        errif(conn == nullptr, "MySQL connect Error");

        connList.push_back(conn);
		++freeConn;
    }
    
    maxConn = freeConn;
    sem_init(&reserve, 0, freeConn);
}

MYSQL* MysqlConnPool::getMysqlConn(){
    MYSQL *conn = NULL;

	if (0 == connList.size())
		return NULL;

	sem_wait(&reserve);
	
    {
        std::unique_lock<std::mutex> lock(handleMtx);
	    conn = connList.front();
	    connList.pop_front();

	    --freeConn;
	    ++curConn;
    }

	return conn;
}
bool MysqlConnPool::releaseMysqlConn(MYSQL *conn){
    if (NULL == conn)
		return false;

    {
        std::unique_lock<std::mutex> lock(handleMtx);
        connList.push_back(conn);
	    ++freeConn;
	    --curConn;
    }

	sem_post(&reserve);
	return true;
}

//当前空闲的连接数
int MysqlConnPool::getFreeConnSize(){
    return this->freeConn;
}

void MysqlConnPool::destroyMysqlConnPool(){
    std::unique_lock<std::mutex> lock(handleMtx);
    
	if (connList.size() > 0)
	{
		std::list<MYSQL*>::iterator it;
		for (it = connList.begin(); it != connList.end(); ++it)
		{
			MYSQL *con = *it;
			mysql_close(con);
		}
		freeConn = 0;
		curConn = 0;
		connList.clear();
	}
}


//双指针对MYSQL *con修改
MysqlConn::MysqlConn(MYSQL **con, MysqlConnPool *connPool){
    *con = connPool->getMysqlConn();
	
	mysqlConn = *con;
	mysqlPool = connPool;
}

MysqlConn::~MysqlConn(){
    mysqlPool->releaseMysqlConn(mysqlConn);
}