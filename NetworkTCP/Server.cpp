#include "Server.h"

event_base* server_base;
evconnlistener* server_ev;

ContinuousMap<evutil_socket_t, ServerPos> server(100);

//错误，超时 （连接断开会进入）
void server_event_cb(bufferevent* be, short events, void* arg)
{
	std::cout << "[E]" << std::flush;
	//读取超时时间发生后，数据读取停止
	if (events & BEV_EVENT_TIMEOUT && events & BEV_EVENT_READING)
	{
		std::cout << "BEV_EVENT_READING BEV_EVENT_TIMEOUT" << std::endl;
		evutil_socket_t fd = bufferevent_getfd(be);
		server.Delete(fd);
		bufferevent_free(be);
	}
	else if (events & BEV_EVENT_ERROR)
	{
		std::cout << "BEV_EVENT_ERROR" << std::endl;
		evutil_socket_t fd = bufferevent_getfd(be);
		server.Delete(fd);
		bufferevent_free(be);
	}
	else
	{
		std::cout << "OTHERS" << std::endl;
	}
}
void server_write_cb(bufferevent* be, void* arg)
{
	//std::cout << "[W]" << std::flush;	
}

void server_read_cb(bufferevent* be, void* arg)
{
	bool boolFireS = false;
	ServerPos data[100] = { 0 };
	////读取输入缓冲数据
	int len = (bufferevent_read(be, data, sizeof(data) - 1) / sizeof(ServerPos));
	if (len <= 0)return;
	for (size_t i = 0; i < len; i++)
	{
		boolFireS |= data[i].Fire;
	}
	len--;
	evutil_socket_t fd = bufferevent_getfd(be);
	ServerPos* px = server.Get(fd);
	px = server.Get(fd);
	px->X = data[len].X;
	px->Y = data[len].Y;
	px->ang = data[len].ang;
	px->Key = fd;
	px->Fire = boolFireS;
	//std::cout << "ServerPos server: " << px->Key << " X: " << px->X << " Y: " << px->Y << std::endl;
	bufferevent_write(be, server.GetKeyData(fd), server.GetKeyDataSize());
}

void server_listen_cb(evconnlistener* ev, evutil_socket_t s, sockaddr* sin, int slen, void* arg)
{
	std::cout << "新连接接入:" << s << std::endl;
	
	ServerPos* pos = server.New(s);
	pos->Key = s;
	pos->X = 0;
	pos->Y = 0;
	pos->ang = 0;

	//char ipstr[INET6_ADDRSTRLEN];
	//int port = -1;
	//if (sin->sa_family == AF_INET) { // IPv4
	//	struct sockaddr_in* addr = (struct sockaddr_in*)sin;
	//	inet_ntop(AF_INET, &(addr->sin_addr), ipstr, INET_ADDRSTRLEN);
	//	port = ntohs(addr->sin_port);
	//}
	//else { // IPv6
	//	struct sockaddr_in6* addr = (struct sockaddr_in6*)sin;
	//	inet_ntop(AF_INET6, &(addr->sin6_addr), ipstr, INET6_ADDRSTRLEN);
	//	port = ntohs(addr->sin6_port);
	//}
	//printf("New connection from %s:%d\n", ipstr, port);
	event_base* base = (event_base*)arg;

	//创建bufferevent上下文 BEV_OPT_CLOSE_ON_FREE清理bufferevent时关闭socket
	bufferevent* bev = bufferevent_socket_new(base, s, BEV_OPT_CLOSE_ON_FREE);

	//添加监控事件
	bufferevent_enable(bev, EV_READ | EV_WRITE);

	//设置水位
	//读取水位
	//bufferevent_setwatermark(bev, EV_READ,
	//	5,	//低水位 0就是无限制 默认是0
	//	10	//高水位 0就是无限制 默认是0
	//);

	//bufferevent_setwatermark(bev, EV_WRITE,
	//	5,	//低水位 0就是无限制 默认是0 缓冲数据低于5 写入回调被调用
	//	0	//高水位无效
	//);

	//超时时间的设置
	timeval t1 = { 3,0 };
	bufferevent_set_timeouts(bev, &t1, 0);

	//设置回调函数
	bufferevent_setcb(bev, server_read_cb, server_write_cb, server_event_cb, base);
}


void server_init(unsigned int Duan) {
#ifdef _WIN32 
	//初始化socket库
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#else
	//忽略管道信号，发送数据给已关闭的socket
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return 1;
#endif
	server_base = event_base_new();
	//创建网络服务器

	//设定监听的端口和地址
	sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(Duan);

	server_ev = evconnlistener_new_bind(
		server_base,
		server_listen_cb,		//回调函数
		server_base,			//回调函数的参数arg
		LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE,
		10,				//listen back
		(sockaddr*)&sin,
		sizeof(sin)
	);

	ServerPos* pos = server.New(0);
	pos->Key = 0;
	pos->ang = 0;
	pos->X = 0;
	pos->Y = 0;
}

void server_delete() {
	evconnlistener_free(server_ev);
	event_base_free(server_base);

#ifdef _WIN32 
	WSACleanup();
#else
#endif
}