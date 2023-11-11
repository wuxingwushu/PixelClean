#include "ServerSynchronizeEvents.h"
#include "StructTCP.h"
#include "../GlobalStructural.h"
#include "../Tool/LimitUse.h"


//位置同步
void SGamePlayerSynchronize(bufferevent* be, void* Data) {
	evutil_socket_t fd = bufferevent_getfd(be);
	SynchronizeData* LD = (SynchronizeData*)Data;
	RoleSynchronizationData* PosData = (RoleSynchronizationData*)LD->Pointer;

	GAME::GamePlayer* LPlayer = server::GetServer()->GetCrowd()->GetGamePlayer(PosData->Key);
	LPlayer->GetObjectCollision()->SetAngle(PosData->ang);
	LPlayer->GetObjectCollision()->SetPos({ PosData->X, PosData->Y });
	LPlayer->setGamePlayerMatrix(TOOL::FPStime, 3, true);


	DataHeader DH;
	DH.Key = 1;
	DH.Size = server::GetServer()->GetServerData()->GetKeyDataSize();
	bufferevent_write(be, &DH, sizeof(DataHeader));//发送数据头
	bufferevent_write(be, server::GetServer()->GetServerData()->GetKeyData(fd), DH.Size);//发送数据

	SNPCSSynchronize(be, nullptr);

	SStrSend(be, nullptr);//发送给每一个客户端

	if (server::GetServer()->GetServerData()->Get(fd)->mBufferEventSingleData->mSubmitBullet->GetNumber() != 0) {
		DataHeader DH;
		DH.Key = 2;
		DH.Size = sizeof(SynchronizeBullet) * server::GetServer()->GetServerData()->Get(fd)->mBufferEventSingleData->mSubmitBullet->GetNumber();
		bufferevent_write(be, &DH, sizeof(DataHeader));//发送数据头
		bufferevent_write(be, server::GetServer()->GetServerData()->Get(fd)->mBufferEventSingleData->mSubmitBullet->GetData(), DH.Size);//发送数据
		server::GetServer()->GetServerData()->Get(fd)->mBufferEventSingleData->mSubmitBullet->ClearAll();
	}

	if (server::GetServer()->GetServerData()->Get(fd)->mBufferEventSingleData->mLabyrinthPixel->GetNumber() != 0) {
		DataHeader DH;
		DH.Key = 6;
		DH.Size = sizeof(PixelState) * server::GetServer()->GetServerData()->Get(fd)->mBufferEventSingleData->mLabyrinthPixel->GetNumber();
		bufferevent_write(be, &DH, sizeof(DataHeader));//发送数据头
		bufferevent_write(be, server::GetServer()->GetServerData()->Get(fd)->mBufferEventSingleData->mLabyrinthPixel->GetData(), DH.Size);//发送数据
		server::GetServer()->GetServerData()->Get(fd)->mBufferEventSingleData->mLabyrinthPixel->ClearAll();
	}
}

//子弹同步
void SArmsSynchronize(bufferevent* be, void* Data) {
	SynchronizeData* AData = (SynchronizeData*)Data;
	SynchronizeBullet* BData = (SynchronizeBullet*)AData->Pointer;
	unsigned char color[4] = { 0,255,0,125 };
	for (size_t i = 0; i < AData->Size; ++i)
	{
		server::GetServer()->GetArms()->ShootBullets(BData[i].X, BData[i].Y, BData[i].angle, 500, BData[i].Type);
	}
	RoleSynchronizationData* LServerPos = server::GetServer()->GetServerData()->GetKeyData(bufferevent_getfd(be));
	BufferEventSingleData* LBufferEventSingleData;
	for (size_t i = 0; i < server::GetServer()->GetServerData()->GetKeyNumber(); ++i)
	{
		if (LServerPos[i].Key == 0) {
			continue;//排除服务器自己
		}
		LBufferEventSingleData = LServerPos[i].mBufferEventSingleData;
		for (size_t j = 0; j < AData->Size; ++j)
		{
			LBufferEventSingleData->mSubmitBullet->add(BData[j]);
		}
	}
}

//玩家损伤成度同步
void SGamePlayerBroken(bufferevent* be, void* Data) {
	evutil_socket_t fd = bufferevent_getfd(be);
	RoleSynchronizationData* LServerPos = server::GetServer()->GetServerData()->GetData();
	DataHeader DH;
	DH.Key = 3;
	DH.Size = sizeof(PlayerBroken) * server::GetServer()->GetServerData()->GetNumber();
	bufferevent_write(be, &DH, sizeof(DataHeader));
	for (size_t i = 0; i < server::GetServer()->GetServerData()->GetNumber(); ++i)
	{
		bufferevent_write(be, &LServerPos[i].Key, sizeof(evutil_socket_t));
		bufferevent_write(be, LServerPos[i].mBufferEventSingleData->mBrokenData, 16*16);
	}
}

//返回玩家的初始信息
void SPlayerInformation(bufferevent* be, void* Data) {
	DataHeader DH;
	DH.Key = 4;
	DH.Size = sizeof(evutil_socket_t);
	bufferevent_write(be, &DH, sizeof(DataHeader));
	evutil_socket_t fd = bufferevent_getfd(be);
	bufferevent_write(be, &fd, DH.Size);//告诉客户端他自己的 evutil_socket_t
}

//地图初始化同步
void SInitLabyrinth(bufferevent* be, void* Data) {
	GAME::Labyrinth* LLabyrinth = server::GetServer()->GetLabyrinth();
	DataHeader DH;
	DH.Key = 5;
	DH.Size = (LLabyrinth->numberX * LLabyrinth->numberY * sizeof(unsigned int)) + (LLabyrinth->numberX * LLabyrinth->numberY * 16 * 16 * sizeof(int)) + (2 * sizeof(int));
	bufferevent_write(be, &DH, sizeof(DataHeader));
	bufferevent_write(be, &LLabyrinth->numberX, sizeof(int));
	bufferevent_write(be, &LLabyrinth->numberY, sizeof(int));
	for (size_t i = 0; i < LLabyrinth->numberX; ++i)
	{
		bufferevent_write(be, LLabyrinth->BlockTypeS[i], (LLabyrinth->numberY * sizeof(unsigned int)));
	}
	bufferevent_write(be, LLabyrinth->BlockPixelS, (LLabyrinth->numberX * LLabyrinth->numberY * 16 * 16 * sizeof(int)));
}

//地图破坏同步
void SLabyrinthPixel(bufferevent* be, void* Data) {
	SynchronizeData* AData = (SynchronizeData*)Data;
	PixelState* BData = (PixelState*)AData->Pointer;
	evutil_socket_t fd = bufferevent_getfd(be);
	for (size_t i = 0; i < AData->Size; ++i)
	{
		if (server::GetServer()->GetLabyrinth()->GetPixel(BData[i].X, BData[i].Y) != BData[i].State) {
			BData[i].State = !BData[i].State;
			server::GetServer()->GetServerData()->Get(fd)->mBufferEventSingleData->mLabyrinthPixel->add(BData[i]);
		}
	}
}

//NPC同步
void SNPCSSynchronize(bufferevent* be, void* Data) {
	ContinuousMap<evutil_socket_t, RoleSynchronizationData>* LRData = server::GetServer()->GetCrowd()->GetRoleSynchronizationData();
	if (LRData->GetNumber() > 0) {
		DataHeader DH;
		DH.Key = 7;
		DH.Size = sizeof(RoleSynchronizationData) * LRData->GetNumber();
		bufferevent_write(be, &DH, sizeof(DataHeader));
		bufferevent_write(be, LRData->GetData(), DH.Size);
	}
}

//Str发送
void SStrSend(bufferevent* be, void* Data) {
	ContinuousMap<evutil_socket_t, RoleSynchronizationData>* LRoleMap;
	RoleSynchronizationData* LRoleData;

	LRoleMap = server::GetServer()->GetServerData();
	LRoleData = LRoleMap->Get(bufferevent_getfd(be));

	Queue<LimitUse<ChatStrStruct*>*>* LStrQueue = LRoleData->mBufferEventSingleData->mStr;

	LimitUse<ChatStrStruct*>* LLimitUse;
	

	DataHeader DH;

	unsigned int LNumder = LStrQueue->GetNumber();
	for (size_t i = 0; i < LNumder; ++i)
	{
		LLimitUse = *LStrQueue->pop();
		DH.Key = 8;
		DH.Size = LLimitUse->Use()->size;
		bufferevent_write(be, &DH, sizeof(DataHeader));
		bufferevent_write(be, LLimitUse->Use()->str, DH.Size);
		if (LLimitUse->LimitDelete()) {
			delete LLimitUse;
		}
	}
}

//Str接收
void SStrReceive(bufferevent* be, void* Data) {
	SynchronizeData* AData = (SynchronizeData*)Data;
	char* BData = (char*)AData->Pointer;
	std::string LChat(BData);
	server::GetServer()->GetInterFace()->mChatBoxStr->add({ LChat, clock() });


	char* NewChar = new char[AData->Size];
	memcpy(NewChar, BData, AData->Size);


	LimitUse<ChatStrStruct*>* LUse;
	ContinuousMap<evutil_socket_t, RoleSynchronizationData>* LRoleMap;
	RoleSynchronizationData* LRoleData;

	LRoleMap = server::GetServer()->GetServerData();
	LRoleData = LRoleMap->GetKeyData(bufferevent_getfd(be));
	ChatStrStruct* LChatStrStruct = new ChatStrStruct;
	LChatStrStruct->str = NewChar;
	LChatStrStruct->size = AData->Size;
	LUse = new LimitUse<ChatStrStruct*>(LChatStrStruct, LRoleMap->GetNumber());
	for (size_t i = 0; i < LRoleMap->GetKeyNumber(); ++i)
	{
		LRoleData[i].mBufferEventSingleData->mStr->add(LUse);
	}
}
