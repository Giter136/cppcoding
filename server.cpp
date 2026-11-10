#include<iostream>
#include<cstring>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<fcntl.h>
#include<cerrno>
#include<cstdio>

constexpr int MAX_EVENTS = 1024;
constexpr int PORT = 8888;

bool set_nonblocking(int fd){
    int flags = fcntl(fd,F_GETFL,0);
    if(flags==-1) return false;
    return fcntl(fd,F_SETFL,flags | O_NONBLOCK) != -1;
}

//创建监听的 socket 
int create_listen_fd(int port){
    int lfd = socket(AF_INET,SOCK_STREAM,0);
    if(lfd==-1) {
        perror("socket");
        return -1;
    }
    int opt = 1;
    setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    if(!set_nonblocking(lfd)){
        perror("fcntl");
        close(lfd);
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if(bind(lfd,(sockaddr*)&addr,sizeof(addr)) == -1){
        perror("bind");
        close(lfd);
        return -1;
    }

    if(listen(lfd,SOMAXCONN) == -1){
        perror("listen");
        close(lfd);
        return -1;
    }

    return lfd;
}

void handle_accept(int lfd,int epfd){
    while(true){
        int cfd = accept(lfd,nullptr,nullptr);//??
        if(cfd == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;
            if(errno == EINTR) continue;
            perror("accept");
            break;
        }

        set_nonblocking(cfd);

        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = cfd;//??
        epoll_ctl(epfd,EPOLL_CTL_ADD,cfd,&ev);

        std::cout<<"[+] 新连接 fd = "<<cfd<<std::endl;
    }
}

void handle_client(int cfd,int epfd){
    char buf[4096];
    while(true){
        ssize_t n =recv(cfd,buf,sizeof(buf),0);

        if(n>0){
            ssize_t sent = 0;
            while(sent < n){
                ssize_t s = send(cfd,buf+sent,n-sent,0);
                if(s == -1){
                    if(errno == EINTR) continue;
                    break;
                }
                sent += s;
            }
        }
        else if(n == 0){
            std::cout<<"[-] 客户端断开连接 fd = "<< cfd << std::endl;
            epoll_ctl(epfd,EPOLL_CTL_DEL,cfd,nullptr);
            close(cfd);
            return;
        }
        else{
            if(errno == EAGAIN || errno ==EWOULDBLOCK) return;
            if(errno == EINTR) continue;
            perror("recv");
            epoll_ctl(epfd,EPOLL_CTL_DEL,cfd,nullptr);
            close(cfd);
            return;
        }
    }
}

int main(){
    int lfd = create_listen_fd(PORT);
    if(lfd == -1) return 1;
    int epfd = epoll_create1(0);
    if(epfd == -1){
        perror("epoll_create1");
        close(lfd);
        return 1;
    }
    
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = lfd;

    if(epoll_ctl(epfd,EPOLL_CTL_ADD,lfd,&ev) == -1){
        perror("epoll_ctl"); 
        close(lfd); 
        close(epfd); 
        return 1;
    }

    std::cout<<"服务器启动，监听端口 "<< PORT;

    epoll_event events[MAX_EVENTS];

    while(true){
        int n = epoll_wait(epfd,events,MAX_EVENTS,-1);
        if(n==-1){
            if(errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for(int i = 0; i< n ;i++){
            int fd = events[i].data.fd;
            if(fd == lfd){
                handle_accept(lfd,epfd);
            }
            else{
                handle_client(fd,epfd);
            }
        }
    }

    close(lfd);
    close(epfd);
}




//飞升是个骗局
