#include "ServerSynchronizeEvents.h"

void SGamePlayerSynchronize(bufferevent* be, void* Data) {
	evutil_socket_t fd = bufferevent_getfd(be);
	SynchronizeData* LD = (SynchronizeData*)Data;
	PlayerPos* PosData = (PlayerPos*)LD->Pointer;
	//std::cout << "fd: " << fd << std::endl;
	PlayerPos* px = server::GetServer()->GetServerData()->Get(fd);
	px = server::GetServer()->GetServerData()->Get(fd);
	px->X = PosData->X;
	px->Y = PosData->Y;
	px->ang = PosData->ang;
	px->Key = fd;

	GAME::GamePlayer* LPlayer = server::GetServer()->GetCrowd()->GetGamePlayer(px->Key);
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


	DataHeader DH;
	DH.Key = 1;
	DH.Size = server::GetServer()->GetServerData()->GetKeyDataSize();
	bufferevent_write(be, &DH, sizeof(DataHeader));//发送数据头
	bufferevent_write(be, server::GetServer()->GetServerData()->GetKeyData(fd), DH.Size);//发送数据

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
		DH.Size = sizeof(PixelSynchronize) * server::GetServer()->GetServerData()->Get(fd)->mBufferEventSingleData->mLabyrinthPixel->GetNumber();
		bufferevent_write(be, &DH, sizeof(DataHeader));//发送数据头
		bufferevent_write(be, server::GetServer()->GetServerData()->Get(fd)->mBufferEventSingleData->mLabyrinthPixel->GetData(), DH.Size);//发送数据
		server::GetServer()->GetServerData()->Get(fd)->mBufferEventSingleData->mLabyrinthPixel->ClearAll();
	}
}

void SArmsSynchronize(bufferevent* be, void* Data) {
	SynchronizeData* AData = (SynchronizeData*)Data;
	SynchronizeBullet* BData = (SynchronizeBullet*)AData->Pointer;
	unsigned char color[4] = { 0,255,0,125 };
	for (size_t i = 0; i < AData->Size; i++)
	{
		server::GetServer()->GetArms()->ShootBullets(BData[i].X, BData[i].Y, color, BData[i].angle, 500);
	}
	PlayerPos* LServerPos = server::GetServer()->GetServerData()->GetKeyData(bufferevent_getfd(be));
	BufferEventSingleData* LBufferEventSingleData;
	for (size_t i = 0; i < server::GetServer()->GetServerData()->GetKeyNumber(); i++)
	{
		if (LServerPos[i].Key == 0) {
			continue;//排除服务器自己
		}
		LBufferEventSingleData = LServerPos[i].mBufferEventSingleData;
		for (size_t j = 0; j < AData->Size; j++)
		{
			LBufferEventSingleData->mSubmitBullet->add(BData[j]);
		}
	}
}

void SGamePlayerBroken(bufferevent* be, void* Data) {
	evutil_socket_t fd = bufferevent_getfd(be);
	PlayerPos* LServerPos = server::GetServer()->GetServerData()->GetKeyData(fd);
	DataHeader DH;
	DH.Key = 3;
	DH.Size = sizeof(PlayerBroken) * server::GetServer()->GetServerData()->GetNumber();
	bufferevent_write(be, &DH, sizeof(DataHeader));
	for (size_t i = 0; i < server::GetServer()->GetServerData()->GetNumber(); i++)
	{
		bufferevent_write(be, &LServerPos[i].Key, sizeof(evutil_socket_t));
		bufferevent_write(be, LServerPos[i].mBufferEventSingleData->mBrokenData, 16*16);
	}
}

void SPlayerInformation(bufferevent* be, void* Data) {
	DataHeader DH;
	DH.Key = 4;
	DH.Size = sizeof(evutil_socket_t);
	bufferevent_write(be, &DH, sizeof(DataHeader));
	evutil_socket_t fd = bufferevent_getfd(be);
	bufferevent_write(be, &fd, DH.Size);//告诉客户端他自己的 evutil_socket_t
}

void SInitLabyrinth(bufferevent* be, void* Data) {
	GAME::Labyrinth* LLabyrinth = server::GetServer()->GetLabyrinth();
	DataHeader DH;
	DH.Key = 5;
	DH.Size = (LLabyrinth->numberX * LLabyrinth->numberY * sizeof(unsigned int)) + (LLabyrinth->numberX * LLabyrinth->numberY * 16 * 16 * sizeof(int)) + (2 * sizeof(int));
	bufferevent_write(be, &DH, sizeof(DataHeader));
	bufferevent_write(be, &LLabyrinth->numberX, sizeof(int));
	bufferevent_write(be, &LLabyrinth->numberY, sizeof(int));
	for (size_t i = 0; i < LLabyrinth->numberX; i++)
	{
		bufferevent_write(be, LLabyrinth->BlockTypeS[i], (LLabyrinth->numberY * sizeof(unsigned int)));
	}
	bufferevent_write(be, LLabyrinth->BlockPixelS, (LLabyrinth->numberX * LLabyrinth->numberY * 16 * 16 * sizeof(int)));
}

void SLabyrinthPixel(bufferevent* be, void* Data) {
	SynchronizeData* AData = (SynchronizeData*)Data;
	PixelSynchronize* BData = (PixelSynchronize*)AData->Pointer;
	evutil_socket_t fd = bufferevent_getfd(be);
	for (size_t i = 0; i < AData->Size; i++)
	{
		if (server::GetServer()->GetLabyrinth()->GetPixel(BData[i].X, BData[i].Y) != BData[i].State) {
			BData[i].State = !BData[i].State;
			server::GetServer()->GetServerData()->Get(fd)->mBufferEventSingleData->mLabyrinthPixel->add(BData[i]);
		}
	}
}
