#include "ClientSynchronizeEvents.h"

void CGamePlayerSynchronize(bufferevent* be, void* Data) {
	ClientPos* px;
	SynchronizeData* LD = (SynchronizeData*)Data;
	ClientPos* LData = (ClientPos*)LD->Pointer;
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