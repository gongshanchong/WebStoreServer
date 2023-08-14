server: 
	g++ -std=c++17 -lpthread -lmysqlclient -g ./log/Log.cpp ./timer/Timer.cpp ./CGImysql/MysqlConnPool.cpp ./acceptor/Acceptor.cpp ./buffer/Buffer.cpp ./channel/Channel.cpp ./connection/Connection.cpp ./eventloop/EventLoop.cpp ./poller/Poller.cpp ./socket/Socket.cpp ./threadpool/ThreadPool.cpp ./utils/utils.cpp ./tcpserver/TcpServer.cpp ./http/HttpConn.cpp ./storeserver/StoreServer.cpp main.cpp -o server

clean:
	rm server