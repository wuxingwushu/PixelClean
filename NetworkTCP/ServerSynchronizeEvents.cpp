#include "ServerSynchronizeEvents.h"

void SGamePlayerSynchronize(bufferevent* be, void* Data) {
	evutil_socket_t fd = bufferevent_getfd(be);
	ServerPos* PosData = server::GetServer()->GetServerPos();
	//std::cout << "fd: " << fd << std::endl;
	ServerPos* px = server::GetServer()->GetServerData()->Get(fd);
	px = server::GetServer()->GetServerData()->Get(fd);
	px->X = PosData->X;
	px->Y = PosData->Y;
	px->ang = PosData->ang;
	px->Key = fd;
	px->Fire = false;

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
}