#include "Client.h"

client* client::mClient = nullptr;
clock_t Ctime = 0;

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
		Zip* zip = new Zip();
		

		//创建输出过滤
		bufferevent* bev_filter = bufferevent_filter_new(be,
			client_filter_in, //输入过滤函数
			client_filter_out,//输出过滤
			BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS,
			0,//清理回调
			zip//传递参数
		);

		//设置读取、写入和事件的回调
		bufferevent_setcb(bev_filter, client_read_cb, client_write_cb, client_event_cb, 0);
		bufferevent_enable(bev_filter, EV_READ | EV_WRITE);

		//触发write
		bufferevent_trigger(bev_filter, EV_WRITE, 0);
	}

}


bool InitbuffereventFD = true;

void client_write_cb(bufferevent* be, void* arg)
{
	DataHeader DHArms;
	if (client::GetClient()->InitbuffereventTO) {
		DHArms.Key = 4;
		DHArms.Size = sizeof(int);
		bufferevent_write(be, &DHArms, sizeof(DataHeader));
		bufferevent_write(be, &DHArms.Key, DHArms.Size);
		if (InitbuffereventFD) {
			DHArms.Key = 5;
			DHArms.Size = sizeof(int);
			bufferevent_write(be, &DHArms, sizeof(DataHeader));
			bufferevent_write(be, &DHArms.Key, DHArms.Size);
			InitbuffereventFD = false;
		}
		return;
	}
	
	
	DHArms.Key = 1;
	DHArms.Size = sizeof(RoleSynchronizationData);
	bufferevent_write(be, &DHArms, sizeof(DataHeader));
	bufferevent_write(be, client::GetClient()->GetGamePlayer()->GetRoleSynchronizationData(), DHArms.Size);//发送玩家位置数据

	CStrSend(be, nullptr);//发送客户端发出的消息

	if (client::GetClient()->GetGamePlayer()->GetRoleSynchronizationData()->mBufferEventSingleData->mSubmitBullet->GetNumber() != 0) {//子弹情况
		DataHeader DHArms;
		DHArms.Key = 2;
		DHArms.Size = sizeof(SynchronizeBullet) * client::GetClient()->GetGamePlayer()->GetRoleSynchronizationData()->mBufferEventSingleData->mSubmitBullet->GetNumber();
		bufferevent_write(be, &DHArms, sizeof(DataHeader));
		bufferevent_write(be, client::GetClient()->GetGamePlayer()->GetRoleSynchronizationData()->mBufferEventSingleData->mSubmitBullet->GetData(), DHArms.Size);//发送客户端发射的子弹
		client::GetClient()->GetGamePlayer()->GetRoleSynchronizationData()->mBufferEventSingleData->mSubmitBullet->ClearAll();//客户端清空发射
	}

	if ((clock() - Ctime) >= 1000) {//请求玩家破损情况
		Ctime = clock();
		DataHeader DHArms;
		DHArms.Key = 3;
		DHArms.Size = sizeof(unsigned int);
		bufferevent_write(be, &DHArms, sizeof(DataHeader));
		bufferevent_write(be, &DHArms.Key, DHArms.Size);
	}

	if (client::GetClient()->GetGamePlayer()->GetRoleSynchronizationData()->mBufferEventSingleData->mLabyrinthPixel->GetNumber() != 0) {//地图情况
		DataHeader DHArms;
		DHArms.Key = 6;
		DHArms.Size = sizeof(PixelState) * client::GetClient()->GetGamePlayer()->GetRoleSynchronizationData()->mBufferEventSingleData->mLabyrinthPixel->GetNumber();
		bufferevent_write(be, &DHArms, sizeof(DataHeader));
		bufferevent_write(be, client::GetClient()->GetGamePlayer()->GetRoleSynchronizationData()->mBufferEventSingleData->mLabyrinthPixel->GetData(), DHArms.Size);//发送客户端发射的子弹
		client::GetClient()->GetGamePlayer()->GetRoleSynchronizationData()->mBufferEventSingleData->mLabyrinthPixel->ClearAll();//客户端清空发射
	}

	//Sleep(10);
	//bufferevent_disable(be);
	event_base_loopbreak(client::GetClient()->GetEvent_Base());//跳出循环
}


unsigned client_read_data_daxiao = 0;
DataHeader client_RDH = { 0 };
_Synchronize Client_mSynchronize;
SynchronizeData Cdata;

void client_read_cb(bufferevent* be, void* arg)
{
	while (evbuffer_get_length(bufferevent_get_input(be)) > 0)//判断输入缓存是否读完了
	{
		if (client_RDH.Size == 0) {
			int len = bufferevent_read(be, &client_RDH, sizeof(DataHeader));
			if (len != 8)
			{
				std::cerr << "[Error DataHeader != 8]" << std::endl;
				return;
			}
			else {
				//std::cerr << "[Server Data OK!]" << std::endl;
				Client_mSynchronize = client::GetClient()->GetSynchronizeMap(client_RDH.Key);
				Cdata.Size = client_RDH.Size / Client_mSynchronize.Size;
				Cdata.Pointer = Client_mSynchronize.mPointer;
				client_read_data_daxiao = 0;
			}
		}

		////读取输入缓冲数据
		int len = bufferevent_read(be, Client_mSynchronize.mPointer, client_RDH.Size);
		if (len <= 0)return;
		Client_mSynchronize.mPointer = (char*)Client_mSynchronize.mPointer + len;
		client_RDH.Size -= len;

		if (client_RDH.Size == 0) {
			Client_mSynchronize.mSynchronizeCallback(be, &Cdata);
		}
	}
}

client::client(std::string IPV, unsigned int Duan) {
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
	bufferevent_setcb(client_bev, 0, 0, client_event_cb, 0);
	//设置为读写
	bufferevent_enable(client_bev, EV_READ | EV_WRITE);
	int re = bufferevent_socket_connect(client_bev, (sockaddr*)&sin, sizeof(sin));
	if (re == 0)
	{
		std::cout << "connected" << std::endl;
	}

	mClientData = new ContinuousMap<evutil_socket_t, RoleSynchronizationData>(100);
	mClientData->SetPointerCallback(GAME::PointerGamePlayer);//（调整储存连续时，同时更新引用者）更新指针

	InitSynchronizeMap();
}

void client::InitSynchronizeMap() {
	char* DataBuffer = new char[100000000];
	AddSynchronizeMap(1, { DataBuffer, sizeof(RoleSynchronizationData), CGamePlayerSynchronize});		//位置同步
	AddSynchronizeMap(2, { DataBuffer, sizeof(SynchronizeBullet), CArmsSynchronize });					//子弹同步
	AddSynchronizeMap(3, { DataBuffer, sizeof(PlayerBroken), CGamePlayerBroken });						//玩家损伤成度同步
	AddSynchronizeMap(4, { DataBuffer, sizeof(evutil_socket_t), CPlayerInformation });					//返回玩家的初始信息
	AddSynchronizeMap(5, { DataBuffer, sizeof(char), CInitLabyrinth });									//地图初始化同步
	AddSynchronizeMap(6, { DataBuffer, sizeof(PixelState), CLabyrinthPixel });							//地图破坏同步
	AddSynchronizeMap(7, { DataBuffer, sizeof(RoleSynchronizationData), CNPCSSynchronize });			//NPC同步
	AddSynchronizeMap(8, { DataBuffer, sizeof(char), CStrReceive });									//聊天窗口同步
}

client::~client() {
	delete mClientData;

	event_base_free(client_base);
#ifdef _WIN32 
	WSACleanup();
#else
#endif
}


//输入过滤器
bufferevent_filter_result client_filter_in(
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
		std::cerr << "inflate failed!" << std::endl;
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
bufferevent_filter_result client_filter_out(
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
		std::cerr << "deflate failed! : " << re << std::endl;
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