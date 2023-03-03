#include <HCNL/TcpServer.h>
#include <HCNL/Logger.h>

#include <string>
#include <functional>

class EchoServer
{
public:
        EchoServer(EventLoop *loop, 
                const InetAddress &listenAddr,
                const std::string &nameArg)
                : server_(loop, listenAddr, nameArg)
                , loop_(loop)
        {
                // 注册回调函数
                server_.setConnectionCallback(
                        std::bind(&EchoServer::onConnection, this, std::placeholders::_1)
                );
                
                server_.setMessageCallback(
                        std::bind(&EchoServer::onMessage, this, 
                                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
                );
                // 设置合适的loop线程数量
                server_.setThreadNum(3);
        }
        
        void start()
        {
                server_.start();
        }

private:
        // 连接建立或者断开的回调
        void onConnection(const TcpConnectionPtr &conn)
        {
                if (conn->connected())
                {
                        LOG_INFO("Connection UP: %s",conn->peerAddress().toIpPort().c_str());
                }
                else
                {
                        LOG_INFO("Connection DOWN: %s",conn->peerAddress().toIpPort().c_str());
                } 
        }

        // 可读写事件回调
        void onMessage(const TcpConnectionPtr &conn, 
                Buffer *buf, 
                Timestamp time)
        {
                std::string msg(buf->retrieveAllAsString());
                conn->send(msg);
                conn->shutdown();
        }
        EventLoop *loop_;
        TcpServer server_;
};

int main()
{
        EventLoop loop;
        InetAddress listenAddr(8000);
        EchoServer server(&loop, listenAddr, "EchoServer-01"); // Acceptor non-blocking listenfd create bind
        server.start(); // listen loopthread listenfd => acceptChannel => mainLoop =>
        loop.loop(); // 启动mainLoop的底层Poller
        return 0;
}