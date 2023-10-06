server: 
	g++ -std=c++17 -lpthread -g ./log/Log.cpp ./timer/Timer.cpp ./utils/utils.cpp ./CGImysql/MysqlConnPool.cpp ./acceptor/Acceptor.cpp ./buffer/Buffer.cpp ./channel/Channel.cpp ./connection/Connection.cpp ./eventloop/EventLoop.cpp ./poller/Poller.cpp ./socket/Socket.cpp ./threadpool/ThreadPool.cpp ./tcpserver/TcpServer.cpp ./http/HttpConn.cpp ./storeserver/StoreServer.cpp main.cpp -o server -lmysqlclient

clean:
	rm server