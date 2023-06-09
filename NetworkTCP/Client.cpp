#include "Client.h"

bool client_Fire = false;
event_base* client_base;
bufferevent* client_bev;
ContinuousMap<evutil_socket_t, ClientPos> client(100);

//错误，超时 （连接断开会进入）
void client_event_cb(bufferevent* be, short events, void* arg)
{
	std::cout << "[client_E]" << std::flush;
	//读取超时时间发生后，数据读取停止
	if (events & BEV_EVENT_TIMEOUT && events & BEV_EVENT_READING)
	{
		std::cout << "BEV_EVENT_READING BEV_EVENT_TIMEOUT" << std::endl;
		//bufferevent_enable(be,EV_READ);
		//bufferevent_free(be);
		return;
	}
	else if (events & BEV_EVENT_ERROR)
	{
		//bufferevent_free(be);
		return;
	}
	//服务端的关闭事件
	if (events & BEV_EVENT_EOF)
	{
		std::cout << "BEV_EVENT_EOF" << std::endl;
		//bufferevent_free(be);
	}
	if (events & BEV_EVENT_CONNECTED)
	{
		std::cout << "BEV_EVENT_CONNECTED" << std::endl;
		//触发write
		bufferevent_trigger(be, EV_WRITE, 0);
	}

}
void client_write_cb(bufferevent* be, void* arg)
{
	//写入buffer
	ClientPos wanpos;
	wanpos.X = m_position.x;
	wanpos.Y = m_position.y;
	wanpos.ang = m_angle * 180.0f / 3.14159265359f;
	wanpos.Fire = client_Fire;
	client_Fire = false;
	bufferevent_write(be, &wanpos, sizeof(ClientPos));
}

void client_read_cb(bufferevent* be, void* arg)
{
	//读取输入缓冲数据
	ClientPos data[1000] = { 0 };
	int len = bufferevent_read(be, data, sizeof(data) - 1) / sizeof(ClientPos);
	if (len <= 0)return;
	ClientPos* px;
	for (size_t i = 0; i < len; i++)
	{
		//std::cout << "Key: " << data[i].Key << " X: " << data[i].X << " Y: " << data[i].Y << std::endl;
		px = client.Get(data[i].Key);
		if (px == nullptr)
		{
			px = client.New(data[i].Key);
		}
		px->X = data[i].X;
		px->Y = data[i].Y;
		px->ang = data[i].ang;
		px->Key = data[i].Key;
	}
	client.TimeoutDetection();
}

void client_init(std::string IPV, unsigned int Duan) {
#ifdef _WIN32 
	//初始化socket库
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#else
	//忽略管道信号，发送数据给已关闭的socket
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return 1;
#endif
	client_base = event_base_new();
	//调用客户端代码
	//-1内部创建socket 
	client_bev = bufferevent_socket_new(client_base, -1, BEV_OPT_CLOSE_ON_FREE);
	sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(Duan);
	evutil_inet_pton(AF_INET, IPV.c_str(), &sin.sin_addr.s_addr);

	//设置回调函数
	bufferevent_setcb(client_bev, client_read_cb, client_write_cb, client_event_cb, 0);
	//设置为读写
	bufferevent_enable(client_bev, EV_READ | EV_WRITE);
	int re = bufferevent_socket_connect(client_bev, (sockaddr*)&sin, sizeof(sin));
	if (re == 0)
	{
		std::cout << "connected" << std::endl;
	}
}

void client_delete() {
	event_base_free(client_base);
#ifdef _WIN32 
	WSACleanup();
#else
#endif
}