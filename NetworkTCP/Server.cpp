#include "Server.h"

event_base* server_base;
evconnlistener* server_ev;

struct Zip {
	z_stream* y;
	z_stream* j;
};

struct DataHeader
{
	unsigned int Key;
	unsigned int Size;
};

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

ServerPos server_read_data;
DataHeader server_RDH;

void server_read_cb(bufferevent* be, void* arg)
{
	while (evbuffer_get_length(bufferevent_get_input(be)) > 0)//判断输入缓存是否读完了
	{
		bool boolFireS = false;
		if (server_RDH.Size == 0) {
			int len = bufferevent_read(be, &server_RDH, sizeof(DataHeader));
			if (len != 8)
			{
				std::cerr << "[Error DataHeader != 8]" << std::endl;
				return;
			}
			else {
				//std::cerr << "[Client Data OK!]" << std::endl;
			}
		}

		char* Pointer = nullptr;
		Pointer = (char*)&server_read_data;
		Pointer += sizeof(ServerPos) - server_RDH.Size;
		////读取输入缓冲数据
		int len = bufferevent_read(be, Pointer, server_RDH.Size);
		if (len <= 0)return;
		server_RDH.Size -= len;

		if (server_RDH.Size == 0) {
			int lenData[1000];
			len = bufferevent_read(be, lenData, sizeof(lenData));

			evutil_socket_t fd = bufferevent_getfd(be);
			ServerPos* px = server.Get(fd);
			px = server.Get(fd);
			px->X = server_read_data.X;
			px->Y = server_read_data.Y;
			px->ang = server_read_data.ang;
			px->Key = fd;
			px->Fire = boolFireS;

			DataHeader DH;
			DH.Key = 1;
			DH.Size = server.GetKeyDataSize();
			bufferevent_write(be, &DH, sizeof(DataHeader));//发送数据头
			bufferevent_write(be, server.GetKeyData(fd), server.GetKeyDataSize());//发送数据
		}
	}
	//std::cout << "ServerPos server: " << px->Key << " X: " << px->X << " Y: " << px->Y << std::endl;
}

void server_listen_cb(evconnlistener* ev, evutil_socket_t s, sockaddr* sin, int slen, void* arg)
{
	std::cout << "新连接接入:" << s << std::endl;
	
	ServerPos* pos = server.New(s);
	pos->Key = s;
	pos->X = 0;
	pos->Y = 0;
	pos->ang = 0;

	event_base* base = (event_base*)arg;

	//创建bufferevent上下文 BEV_OPT_CLOSE_ON_FREE清理bufferevent时关闭socket
	bufferevent* bev = bufferevent_socket_new(base, s, BEV_OPT_CLOSE_ON_FREE);

	Zip* zip = new Zip();
	zip->y = new z_stream();
	deflateInit(zip->y, Z_DEFAULT_COMPRESSION);
	zip->j = new z_stream();
	inflateInit(zip->j);

	//2 添加过滤 并设置输入回调
	bufferevent* bev_filter = bufferevent_filter_new(bev,
		server_filter_in,//输入过滤函数
		server_filter_out,//输出过滤
		BEV_OPT_CLOSE_ON_FREE,//关闭filter同时管理bufferevent
		0, //清理回调
		zip	//传递参数
	);

	//添加监控事件
	bufferevent_enable(bev_filter, EV_READ | EV_WRITE);

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
	bufferevent_set_timeouts(bev_filter, &t1, 0);

	//设置回调函数
	bufferevent_setcb(bev_filter, server_read_cb, server_write_cb, server_event_cb, base);
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

//输入过滤器
bufferevent_filter_result server_filter_in(
	evbuffer* s,
	evbuffer* d,
	ev_ssize_t limit,
	bufferevent_flush_mode mode,
	void* arg
) {
	//解压上下文
	Zip* z = (Zip*)arg;
	z_stream* p = z->j;

	if (!p)
	{
		return BEV_ERROR;
	}
	//解压
	evbuffer_iovec v_in[1];
	//读取数据 不清理缓冲
	int n = evbuffer_peek(s, -1, NULL, v_in, 1);
	if (n <= 0)
		return BEV_NEED_MORE;

	//zlib 输入数据大小
	p->avail_in = v_in[0].iov_len;
	//zlib  输入数据地址
	p->next_in = (Byte*)v_in[0].iov_base;

	//申请输出空间大小
	evbuffer_iovec v_out[1];
	evbuffer_reserve_space(d, 4096, v_out, 1);

	//zlib 输出空间大小
	p->avail_out = v_out[0].iov_len;

	//zlib 输出空间地址
	p->next_out = (Byte*)v_out[0].iov_base;
	//解压数据
	int re = inflate(p, Z_SYNC_FLUSH);

	if (re != Z_OK)
	{
		std::cerr << "in inflate failed!" << std::endl;
	}

	//解压用了多少数据，从source evbuffer中移除
	//p->avail_in 未处理的数据大小
	int nread = v_in[0].iov_len - p->avail_in;

	//解压后数据大小 传入 des evbuffer
	//p->avail_out 剩余空间大小
	int nwrite = v_out[0].iov_len - p->avail_out;

	//移除source evbuffer中数据
	evbuffer_drain(s, nread);

	//cout << "filter_out" << endl;
	//传入 des evbuffer
	v_out[0].iov_len = nwrite;
	evbuffer_commit_space(d, v_out, 1);

	return BEV_OK;
}

//输出过滤器
bufferevent_filter_result server_filter_out(
	evbuffer* s,
	evbuffer* d,
	ev_ssize_t limit,
	bufferevent_flush_mode mode,
	void* arg
) {
	Zip* z = (Zip*)arg;
	z_stream* p = z->y;
	if (!p)
	{
		return BEV_ERROR;
	}
	//取出buffer中数据的引用
	evbuffer_iovec v_in[1];
	int n = evbuffer_peek(s, -1, 0, v_in, 1);
	if (n <= 0)
	{
		//没有数据 BEV_NEED_MORE 不会进入写入回调
		return BEV_NEED_MORE;
	}
	//zlib 输入数据大小
	p->avail_in = v_in[0].iov_len;
	//zlib  输入数据地址
	p->next_in = (Byte*)v_in[0].iov_base;

	//申请输出空间大小
	evbuffer_iovec v_out[1];
	evbuffer_reserve_space(d, 4096, v_out, 1);

	//zlib 输出空间大小
	p->avail_out = v_out[0].iov_len;

	//zlib 输出空间地址
	p->next_out = (Byte*)v_out[0].iov_base;

	//zlib 压缩
	int re = deflate(p, Z_SYNC_FLUSH);
	if (re != Z_OK)
	{
		std::cerr << "out deflate failed!" << std::endl;
	}


	//压缩用了多少数据，从source evbuffer中移除
	//p->avail_in 未处理的数据大小
	int nread = v_in[0].iov_len - p->avail_in;

	//压缩后数据大小 传入 des evbuffer
	//p->avail_out 剩余空间大小
	int nwrite = v_out[0].iov_len - p->avail_out;

	//std::cout << "size: " << nread << " - " << float(nwrite) / nread << "%" << std::endl;


	//移除source evbuffer中数据
	evbuffer_drain(s, nread);

	//cout << "filter_out" << endl;
	//传入 des evbuffer
	v_out[0].iov_len = nwrite;
	evbuffer_commit_space(d, v_out, 1);
	return BEV_OK;
}