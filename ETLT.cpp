#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <pthread.h>
#include <iostream>
#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 10
using namespace std;

int setnonblocking(int fd)
{ //设置文件描述符为非阻塞
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option; //返回旧描述符以便以后恢复
}
void addfd(int epollfd, int fd, bool enable_et)
{ //添加文件描述符fd到事件表epollfd中
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN; //可读事件
    if (enable_et)
    { //支持ET模式
        event.events |= EPOLLET;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event); //添加事件
    setnonblocking(fd);
}
void lt(epoll_event *events, int number, int epollfd, int listenfd)
{ //LT模式
    char buf[BUFFER_SIZE];
    for (int i = 0; i < number; i++)
    { //就绪事件
        int sockfd = events[i].data.fd;
        if (sockfd == listenfd)
        { //若为服务端监听端口表示事件(有新连接)发生则接受连接
            struct sockaddr_in client_address;
            socklen_t client_addrlength = sizeof(client_address);
            int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            addfd(epollfd, connfd, false); //将新连接添加到事件表且设置为非ET
        }
        else if (events[i].events & EPOLLIN)
        { //客户端连接事件(有数据发送到服务端)发生则服务端接收数据
            cout << "event trigger once" << endl;
            memset(buf, '\0', BUFFER_SIZE);
            int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
            if (ret <= 0)
            { //客户连接已关闭
                close(sockfd);
                continue;
            }
            cout << "receive data:" << ret << " " << buf << endl; //输出服务端接收到的数据
        }
        else
        {
            cout << "something wrong" << endl;
        }
    }
}
void et(epoll_event *events, int number, int epollfd, int listenfd)
{ //ET模式
    char buf[BUFFER_SIZE];
    for (int i = 0; i < number; i++)
    { //就绪事件
        int sockfd = events[i].data.fd;
        if (sockfd == listenfd)
        { //监听端口有新连接
            struct sockaddr_in client_address;
            socklen_t client_addrlength = sizeof(client_address);
            int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            addfd(epollfd, connfd, true); //新连接设置为ET模式
        }
        else if (events[i].events & EPOLLIN)
        { //客户端有数据发送到服务端
            cout << "event trigger once" << endl;
            while (1)
            { //这里要一次性读完客户端发送来的数据因为ET是不会重复触发
                memset(buf, '\0', BUFFER_SIZE);
                int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
                if (ret < 0)
                { //非阻塞IO中recv返回-1并不一定是出错
                    if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                    { //errno为EAGAIN和EWOULDBLOCK时表示数据已经读取完毕可以进行下一次epoll
                        cout << "epoll again" << endl;
                        break;
                    }
                    close(sockfd);
                    break;
                }
                else if (ret == 0)
                { //连接已关闭
                    close(sockfd);
                }
                else
                {
                    cout << "receive data:" << ret << " " << buf << endl; //输出接收到的数据
                }
            }
        }
        else
        {
            cout << "somethig wrong" << endl;
        }
    }
}
int main(int argc, char *argv[])
{
    if (argc <= 2)
    {
        cout << "argc<=2" << endl;
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(listenfd, 5);
    assert(ret != -1);
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5); //这里的5只是给内核一个提示事件表的大小无实际作用
    assert(epollfd != -1);
    addfd(epollfd, listenfd, true); //###1###将监听端口设置为非阻塞的理由见后面
    while (1)
    {
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1); //等待事件就绪
        if (ret < 0)
        {
            cout << "epoll error" << endl;
            break;
        }
        //lt(events,ret,epollfd,listenfd);//采用LT模式
        et(events, ret, epollfd, listenfd); //采用ET模式
    }
    close(listenfd);
    return 0;
}