#include "ClientSynchronizeEvents.h"

void CGamePlayerSynchronize(bufferevent* be, void* Data) {
	PlayerPos* px;
	SynchronizeData* LD = (SynchronizeData*)Data;
	PlayerPos* LData = (PlayerPos*)LD->Pointer;
	for (size_t i = 0; i < LD->Size; i++)
	{
		//std::cout << "Key: " << data[i].Key << " X: " << data[i].X << " Y: " << data[i].Y << std::endl;
		px = client::GetClient()->GetClientData()->Get(LData[i].Key);
		if (px == nullptr)
		{
			px = client::GetClient()->GetClientData()->New(LData[i].Key);
		}
		px->X = LData[i].X;
		px->Y = LData[i].Y;
		px->ang = LData[i].ang;
		px->Key = LData[i].Key;

		GAME::GamePlayer* LPlayer = client::GetClient()->GetCrowd()->GetGamePlayer(px->Key);
		LPlayer->setGamePlayerMatrix(glm::rotate(
			glm::translate(glm::mat4(1.0f), glm::vec3(px->X, px->Y, 0.0f)),
			glm::radians(px->ang),
			glm::vec3(0.0f, 0.0f, 1.0f)
		),
			3,
			true
		);
		LPlayer->mObjectCollision->SetAngle(px->ang / 180.0f * 3.14159265359f);
		LPlayer->mObjectCollision->SetPos({ px->X, px->Y });
	}
	client::GetClient()->GetClientData()->TimeoutDetection();
}

void CArmsSynchronize(bufferevent* be, void* Data) {
	SynchronizeData* AData = (SynchronizeData*)Data;
	SynchronizeBullet* BData = (SynchronizeBullet*)AData->Pointer;
	unsigned char color[4] = { 0,255,0,125 };
	for (size_t i = 0; i < AData->Size; i++)
	{
		client::GetClient()->GetArms()->ShootBullets(BData[i].X, BData[i].Y, color, BData[i].angle, 500);
	}
}

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

void CPlayerInformation(bufferevent* be, void* Data) {
	SynchronizeData* AData = (SynchronizeData*)Data;
	evutil_socket_t* BData = (evutil_socket_t*)AData->Pointer;
	client::GetClient()->fd = *BData;
	/*PlayerPos* LData = client::GetClient()->GetClientData()->New(*BData);
	LData->mBufferEventSingleData = new BufferEventSingleData(100);
	LData->mBufferEventSingleData->mBrokenData = client::GetClient()->GetGamePlayer()->GetBrokenData();*/
}