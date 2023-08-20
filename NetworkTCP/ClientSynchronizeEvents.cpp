#include "ClientSynchronizeEvents.h"
#include "StructTCP.h"
#include "../GlobalStructural.h"
#include "../Tool/LimitUse.h"

//位置同步
void CGamePlayerSynchronize(bufferevent* be, void* Data) {
	RoleSynchronizationData* px;
	GAME::GamePlayer* LPlayer;
	SynchronizeData* LD = (SynchronizeData*)Data;
	RoleSynchronizationData* LData = (RoleSynchronizationData*)LD->Pointer;
	for (size_t i = 0; i < LD->Size; i++)
	{
		px = client::GetClient()->GetClientData()->Get(LData[i].Key);
		LPlayer = client::GetClient()->GetCrowd()->GetGamePlayer(LData[i].Key);
		if (px == nullptr)
		{
			px = client::GetClient()->GetClientData()->New(LData[i].Key);
			px->Key = LData[i].Key;
			px->mBufferEventSingleData = new BufferEventSingleData(100);
			LPlayer->SetRoleSynchronizationData(px);
			client::GetClient()->GetClientData()->SetPointerData(LData[i].Key, LPlayer);
		}

		LPlayer->GetObjectCollision()->SetAngle(LData[i].ang);
		LPlayer->GetObjectCollision()->SetPos({ LData[i].X, LData[i].Y });
		LPlayer->setGamePlayerMatrix(3, true);
	}
	client::GetClient()->GetClientData()->TimeoutDetection();
}

//子弹同步
void CArmsSynchronize(bufferevent* be, void* Data) {
	SynchronizeData* AData = (SynchronizeData*)Data;
	SynchronizeBullet* BData = (SynchronizeBullet*)AData->Pointer;
	for (size_t i = 0; i < AData->Size; i++)
	{
		client::GetClient()->GetArms()->ShootBullets(BData[i].X, BData[i].Y, BData[i].angle, 500, BData[i].Type);
	}
}

//玩家损伤成度同步
void CGamePlayerBroken(bufferevent* be, void* Data) {
	SynchronizeData* AData = (SynchronizeData*)Data;
	PlayerBroken* BData = (PlayerBroken*)AData->Pointer;
	for (size_t i = 0; i < (AData->Size - 1); i++)//去除最后一个自己
	{
		GAME::GamePlayer* LPlayer = client::GetClient()->GetCrowd()->GetGamePlayer(BData[i].Key);
		LPlayer->UpDataBroken(BData[i].Broken);
	}
	client::GetClient()->GetGamePlayer()->UpDataBroken(BData[AData->Size - 1].Broken);
}

//返回玩家的初始信息
void CPlayerInformation(bufferevent* be, void* Data) {
	SynchronizeData* AData = (SynchronizeData*)Data;
	evutil_socket_t* BData = (evutil_socket_t*)AData->Pointer;
	client::GetClient()->fd = *BData;
	if (client::GetClient()->GetGamePlayer()->GetRoleSynchronizationData() == nullptr) {
		RoleSynchronizationData* LRole = new RoleSynchronizationData;
		LRole->Key = *BData;
		LRole->mBufferEventSingleData = new BufferEventSingleData(100);
		client::GetClient()->GetGamePlayer()->SetRoleSynchronizationData(LRole);
	}
}

//地图初始化同步
void CInitLabyrinth(bufferevent* be, void* Data) {
	SynchronizeData* AData = (SynchronizeData*)Data;
	int X = ((int*)AData->Pointer)[0];
	int Y = ((int*)AData->Pointer)[1];
	client::GetClient()->GetLabyrinth()->LoadLabyrinth(
		X, Y, 
		&(((int*)AData->Pointer)[2 + (X * Y)]),
		&(((unsigned int*)AData->Pointer)[2])
	);
	client::GetClient()->GetArms()->GetSquarePhysics()->SetFixedSizeTerrain(client::GetClient()->GetLabyrinth()->mFixedSizeTerrain);
	client::GetClient()->InitbuffereventTO = false;
}

//地图破坏同步
void CLabyrinthPixel(bufferevent* be, void* Data) {
	SynchronizeData* AData = (SynchronizeData*)Data;
	PixelState* BData = (PixelState*)AData->Pointer;
	for (size_t i = 0; i < AData->Size; i++)
	{
		client::GetClient()->GetLabyrinth()->mPixelQueue->add({ BData[i].X, BData[i].Y, BData[i].State });
	}
}

//NPC同步
void CNPCSSynchronize(bufferevent* be, void* Data) {
	SynchronizeData* AData = (SynchronizeData*)Data;
	RoleSynchronizationData* BData = (RoleSynchronizationData*)AData->Pointer;
	GAME::Crowd* LCrowd = client::GetClient()->GetCrowd();
	GAME::NPC* LNPC = nullptr;
	for (size_t i = 0; i < AData->Size; i++)
	{
		LNPC = LCrowd->GetNPC(BData[i].Key);
		LNPC->SetNPC(BData[i].X, BData[i].Y, BData[i].ang);
	}
}

//Str发送
void CStrSend(bufferevent* be, void* Data) {
	QueueData<LimitUse<ChatStrStruct*>*>* LStrQueue = client::GetClient()->GetGamePlayer()->GetRoleSynchronizationData()->mBufferEventSingleData->mStr;
	LimitUse<ChatStrStruct*>* LLimitUse;

	DataHeader DH;

	unsigned int LNumder = LStrQueue->GetNumber();
	for (size_t i = 0; i < LNumder; i++)
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
void CStrReceive(bufferevent* be, void* Data) {
	SynchronizeData* AData = (SynchronizeData*)Data;
	char* BData = (char*)AData->Pointer;
	std::string LChat(BData);
	client::GetClient()->GetInterFace()->mChatBoxStr->add({ LChat, clock() });
}
