#include "PhysicsDemo.h"
#include "PhysicsLogBuffer.h"
#include "../../DebugLog.h"
#include "../../PhysicsBlock/MapDynamic.hpp"
#include "../../Tool/PerlinNoise.h"
#include <cstdio>

namespace PhysicsBlock
{

	void PhysicsDemo0(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 10;

		// 设置摄像机位置
		mCamera->setCameraPos({0, 0, 20});

		// 创建地图
		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		PhysicsBlock::PhysicsShape *PhysicsShape1 = new PhysicsBlock::PhysicsShape({-3, 0}, {5, 5});
		for (size_t i = 0; i < (PhysicsShape1->width * PhysicsShape1->height); ++i)
		{
			PhysicsShape1->at(i).Collision = false;
			PhysicsShape1->at(i).Entity = false;
			PhysicsShape1->at(i).mass = 1;
		}
		for (size_t x = 1; x < PhysicsShape1->width - 1; ++x)
		{
			for (size_t y = 1; y < PhysicsShape1->height - 1; ++y)
			{
				PhysicsShape1->at(x, y).Collision = true;
				PhysicsShape1->at(x, y).Entity = true;
				PhysicsShape1->at(x, y).mass = 1;
			}
		}
		PhysicsShape1->at(2, 2).Collision = false;
		PhysicsShape1->at(2, 2).Entity = false;
		PhysicsShape1->at(3, 1).Collision = false;
		PhysicsShape1->at(3, 1).Entity = false;
		PhysicsShape1->at(0, 1).Collision = true;
		PhysicsShape1->at(0, 1).Entity = true;
		PhysicsShape1->at(1, 0).Collision = true;
		PhysicsShape1->at(1, 0).Entity = true;
		PhysicsShape1->at(4, 4).Collision = true;
		PhysicsShape1->at(4, 4).Entity = true;
		PhysicsShape1->UpdateAll();
		PhysicsShape1->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape1);
	}

	void PhysicsDemo1(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 10;

		// 设置摄像机位置
		mCamera->setCameraPos({0, 0, 20});

		// 创建地图
		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		PhysicsBlock::PhysicsShape *PhysicsShape1 = new PhysicsBlock::PhysicsShape({0.97, -2}, {2, 2});
		for (size_t i = 0; i < (PhysicsShape1->width * PhysicsShape1->height); ++i)
		{
			PhysicsShape1->at(i).Collision = true;
			PhysicsShape1->at(i).Entity = true;
			PhysicsShape1->at(i).mass = 1;
		}
		PhysicsShape1->UpdateAll();
		PhysicsShape1->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape1);
		PhysicsBlock::PhysicsShape *PhysicsShape2 = new PhysicsBlock::PhysicsShape({0, -4}, {2, 2});
		for (size_t i = 0; i < (PhysicsShape2->width * PhysicsShape2->height); ++i)
		{
			PhysicsShape2->at(i).Collision = true;
			PhysicsShape2->at(i).Entity = true;
			PhysicsShape2->at(i).mass = 1;
		}
		PhysicsShape2->UpdateAll();
		PhysicsShape2->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape2);
	}

	void PhysicsDemo2(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 10;

		// 设置摄像机位置
		mCamera->setCameraPos({0, 5, 50});

		// 创建地图
		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		for (int j = 0; j < 10; ++j)
		{
			PhysicsBlock::PhysicsShape *PhysicsShape1 = new PhysicsBlock::PhysicsShape({Random(-0.1f, 0.1f), -4 + j * 2.1}, {2, 2});
			for (size_t i = 0; i < (PhysicsShape1->width * PhysicsShape1->height); ++i)
			{
				PhysicsShape1->at(i).Collision = true;
				PhysicsShape1->at(i).Entity = true;
				PhysicsShape1->at(i).mass = 1;
			}
			PhysicsShape1->UpdateAll();
			PhysicsShape1->angle = 0;
			(*myPhysicsWorld)->AddObject(PhysicsShape1);
		}
	}

	void PhysicsDemo3(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 240;

		// 设置摄像机位置
		mCamera->setCameraPos({0, 0, MapSize * 2});

		// 创建地图
		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		int JZMapSize = MapSize / 4;
		for (int xpos = 0; xpos < JZMapSize; ++xpos)
		{
			for (int ypos = xpos; ypos < JZMapSize; ++ypos)
			{
				PhysicsBlock::PhysicsShape *PhysicsShape3 = new PhysicsBlock::PhysicsShape({(ypos * 2.2) - JZMapSize - (JZMapSize / 2) - (xpos) + 5, (xpos * 3) - (MapSize / 2 - 4)}, {2, 2});
				for (size_t i = 0; i < (PhysicsShape3->width * PhysicsShape3->height); i++)
				{
					PhysicsShape3->at(i).Collision = true;
					PhysicsShape3->at(i).Entity = true;
					PhysicsShape3->at(i).mass = 1;
				}
				PhysicsShape3->UpdateAll();
				PhysicsShape3->angle = 0;
				(*myPhysicsWorld)->AddObject(PhysicsShape3);
			}
		}
	}

	void PhysicsDemo4(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 10;

		// 设置摄像机位置
		mCamera->setCameraPos({0, 0, 20});

		// 创建地图
		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		PhysicsBlock::PhysicsParticle *PhysicsParticle1 = new PhysicsBlock::PhysicsParticle({-0.9, 0.1}, 1);
		(*myPhysicsWorld)->AddObject(PhysicsParticle1);
		PhysicsParticle1 = new PhysicsBlock::PhysicsParticle({0.9, 0.1}, 1);
		(*myPhysicsWorld)->AddObject(PhysicsParticle1);
		PhysicsBlock::PhysicsShape *PhysicsShape1 = new PhysicsBlock::PhysicsShape({-1, -2}, {2, 2});
		for (size_t i = 0; i < (PhysicsShape1->width * PhysicsShape1->height); ++i)
		{
			PhysicsShape1->at(i).Collision = true;
			PhysicsShape1->at(i).Entity = true;
			PhysicsShape1->at(i).mass = 1;
		}
		PhysicsShape1->UpdateAll();
		PhysicsShape1->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape1);
	}

	void PhysicsDemo5(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 20;

		// 设置摄像机位置
		mCamera->setCameraPos({0, 0, 40});

		// 创建地图
		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		PhysicsBlock::PhysicsShape *PhysicsShape1 = new PhysicsBlock::PhysicsShape({4, 3}, {2, 2});
		for (size_t i = 0; i < (PhysicsShape1->width * PhysicsShape1->height); ++i)
		{
			PhysicsShape1->at(i).Collision = true;
			PhysicsShape1->at(i).Entity = true;
			PhysicsShape1->at(i).mass = 1;
		}
		PhysicsShape1->UpdateAll();
		PhysicsShape1->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape1);
		PhysicsBlock::PhysicsShape *PhysicsShape2 = new PhysicsBlock::PhysicsShape({0, -4}, {2, 2});
		for (size_t i = 0; i < (PhysicsShape2->width * PhysicsShape2->height); ++i)
		{
			PhysicsShape2->at(i).Collision = true;
			PhysicsShape2->at(i).Entity = true;
			PhysicsShape2->at(i).mass = FLOAT_MAX;
		}
		PhysicsShape2->UpdateAll();
		PhysicsShape2->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape2);

		PhysicsBlock::PhysicsJoint *PhysicsJoint1 = new PhysicsBlock::PhysicsJoint;
		PhysicsJoint1->Set(PhysicsShape1, PhysicsShape2, {PhysicsShape2->pos.x, PhysicsShape1->pos.y});
		(*myPhysicsWorld)->AddObject(PhysicsJoint1);

		PhysicsBlock::PhysicsShape *PhysicsShape3 = new PhysicsBlock::PhysicsShape({0, -1.5}, {2, 2});
		for (size_t i = 0; i < (PhysicsShape3->width * PhysicsShape3->height); ++i)
		{
			PhysicsShape3->at(i).Collision = true;
			PhysicsShape3->at(i).Entity = true;
			PhysicsShape3->at(i).mass = 1;
		}
		PhysicsShape3->UpdateAll();
		PhysicsShape3->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape3);
	}

	void PhysicsDemo6(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 30;

		// 设置摄像机位置
		mCamera->setCameraPos({0, 0, 60});

		// 创建地图
		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		std::vector<PhysicsBlock::PhysicsShape *> PhysicsShapeVector;
		PhysicsBlock::PhysicsShape *PhysicsShape2 = new PhysicsBlock::PhysicsShape({0, 0}, {2, 2});
		for (size_t i = 0; i < (PhysicsShape2->width * PhysicsShape2->height); ++i)
		{
			PhysicsShape2->at(i).Collision = true;
			PhysicsShape2->at(i).Entity = true;
			PhysicsShape2->at(i).mass = FLOAT_MAX;
		}
		PhysicsShape2->UpdateAll();
		PhysicsShape2->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape2);
		PhysicsShapeVector.push_back(PhysicsShape2);

		for (size_t i = 0; i < MapSize / 8; ++i)
		{
			PhysicsBlock::PhysicsShape *PhysicsShape1 = new PhysicsBlock::PhysicsShape({PhysicsShapeVector[PhysicsShapeVector.size() - 1]->pos.x + 2, 0.5}, {2, 1});
			for (size_t x = 0; x < PhysicsShape1->width; ++x)
			{
				for (size_t y = 0; y < PhysicsShape1->height; ++y)
				{
					PhysicsShape1->at(x, y).Collision = true;
					PhysicsShape1->at(x, y).Entity = true;
					PhysicsShape1->at(x, y).mass = 1;
				}
			}
			PhysicsShape1->UpdateAll();
			PhysicsShape1->angle = 0;
			(*myPhysicsWorld)->AddObject(PhysicsShape1);

			PhysicsBlock::PhysicsJoint *PhysicsJoint1 = new PhysicsBlock::PhysicsJoint;
			if (i == 0)
			{
				PhysicsJoint1->Set(PhysicsShape1, PhysicsShapeVector[PhysicsShapeVector.size() - 1], {1, 1});
			}
			else
			{
				PhysicsJoint1->Set(PhysicsShape1, PhysicsShapeVector[PhysicsShapeVector.size() - 1], {(PhysicsShape1->pos.x + PhysicsShapeVector[PhysicsShapeVector.size() - 1]->pos.x) / 2, PhysicsShape1->pos.y});
			}
			(*myPhysicsWorld)->AddObject(PhysicsJoint1);

			PhysicsShapeVector.push_back(PhysicsShape1);
		}
	}

	void PhysicsDemo7(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 20;

		// 设置摄像机位置
		mCamera->setCameraPos({0, 0, MapSize * 2});

		// 创建地图
		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		PhysicsBlock::PhysicsParticle *PhysicsParticle1 = new PhysicsBlock::PhysicsParticle({4, 1}, 10);
		(*myPhysicsWorld)->AddObject(PhysicsParticle1);

		PhysicsBlock::PhysicsParticle *PhysicsParticle2 = new PhysicsBlock::PhysicsParticle({5, 1}, 10);
		(*myPhysicsWorld)->AddObject(PhysicsParticle2);

		PhysicsBlock::PhysicsJunctionP *PhysicsJunctionP1 = new PhysicsBlock::PhysicsJunctionP(PhysicsParticle1, {0, 1}, PhysicsBlock::lever);
		(*myPhysicsWorld)->AddObject(PhysicsJunctionP1);
		PhysicsBlock::PhysicsJunctionPP *PhysicsJunctionP2 = new PhysicsBlock::PhysicsJunctionPP(PhysicsParticle1, PhysicsParticle2, PhysicsBlock::lever);
		(*myPhysicsWorld)->AddObject(PhysicsJunctionP2);

		PhysicsParticle1 = new PhysicsBlock::PhysicsParticle({6, -4}, 10);
		(*myPhysicsWorld)->AddObject(PhysicsParticle1);
		PhysicsJunctionP1 = new PhysicsBlock::PhysicsJunctionP(PhysicsParticle1, {6, 0}, PhysicsBlock::lever);
		(*myPhysicsWorld)->AddObject(PhysicsJunctionP1);

		PhysicsBlock::PhysicsShape *PhysicsShape2 = new PhysicsBlock::PhysicsShape({0, -5}, {2, 6});
		for (size_t i = 0; i < (PhysicsShape2->width * PhysicsShape2->height); ++i)
		{
			PhysicsShape2->at(i).Collision = true;
			PhysicsShape2->at(i).Entity = true;
			PhysicsShape2->at(i).mass = 1;
		}
		PhysicsShape2->UpdateAll();
		PhysicsShape2->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape2);

		PhysicsShape2 = new PhysicsBlock::PhysicsShape({3, 6}, {2, 2});
		for (size_t i = 0; i < (PhysicsShape2->width * PhysicsShape2->height); ++i)
		{
			PhysicsShape2->at(i).Collision = true;
			PhysicsShape2->at(i).Entity = true;
			PhysicsShape2->at(i).mass = 1;
		}
		PhysicsShape2->UpdateAll();
		PhysicsShape2->angle = 3.14 / 4;
		(*myPhysicsWorld)->AddObject(PhysicsShape2);

		PhysicsBlock::PhysicsJunctionS *PhysicsJunction3 = new PhysicsBlock::PhysicsJunctionS(PhysicsShape2, {-0.75, 0.75}, {0, PhysicsShape2->pos.y}, PhysicsBlock::lever);
		(*myPhysicsWorld)->AddObject(PhysicsJunction3);

		PhysicsBlock::PhysicsShape *PhysicsShape3 = new PhysicsBlock::PhysicsShape({6, 6}, {2, 2});
		for (size_t i = 0; i < (PhysicsShape3->width * PhysicsShape3->height); ++i)
		{
			PhysicsShape3->at(i).Collision = true;
			PhysicsShape3->at(i).Entity = true;
			PhysicsShape3->at(i).mass = 1;
		}
		PhysicsShape3->UpdateAll();
		PhysicsShape3->angle = 3.14 / 4;
		(*myPhysicsWorld)->AddObject(PhysicsShape3);

		PhysicsBlock::PhysicsJunctionSS *PhysicsJunction4 = new PhysicsBlock::PhysicsJunctionSS(PhysicsShape2, {0.75, -0.75}, PhysicsShape3, {-0.75, 0.75}, PhysicsBlock::lever);
		(*myPhysicsWorld)->AddObject(PhysicsJunction4);

		PhysicsShape2 = new PhysicsBlock::PhysicsShape({5.1, -2}, {2, 2});
		for (size_t i = 0; i < (PhysicsShape2->width * PhysicsShape2->height); ++i)
		{
			PhysicsShape2->at(i).Collision = true;
			PhysicsShape2->at(i).Entity = true;
			PhysicsShape2->at(i).mass = 1;
		}
		PhysicsShape2->UpdateAll();
		PhysicsShape2->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape2);
	}

	void PhysicsDemo8(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if ((*myPhysicsWorld) != nullptr)
		{
			delete (*myPhysicsWorld);
		}
		(*myPhysicsWorld) = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 10;

		// 设置摄像机位置
		mCamera->setCameraPos({0, 0, MapSize * 2});

		// 创建地图
		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		const double Mass = 5;
		const double Gao = 15;
		std::vector<PhysicsBlock::PhysicsParticle *> ParticleS;

		PhysicsBlock::PhysicsParticle *PhysicsParticle1 = new PhysicsBlock::PhysicsParticle({0, Gao - 1}, Mass);
		(*myPhysicsWorld)->AddObject(PhysicsParticle1);
		ParticleS.push_back(PhysicsParticle1);
		PhysicsBlock::PhysicsJunctionP *PhysicsJunctionP1 = new PhysicsBlock::PhysicsJunctionP(PhysicsParticle1, {0, Gao}, PhysicsBlock::rubber);
		(*myPhysicsWorld)->AddObject(PhysicsJunctionP1);

		for (size_t i = 0; i < MapSize - 4; ++i)
		{
			PhysicsParticle1 = new PhysicsBlock::PhysicsParticle({0, ParticleS[ParticleS.size() - 1]->pos.y - 1}, Mass);
			(*myPhysicsWorld)->AddObject(PhysicsParticle1);
			PhysicsBlock::PhysicsJunctionPP *PhysicsJunctionPP = new PhysicsBlock::PhysicsJunctionPP(PhysicsParticle1, ParticleS[ParticleS.size() - 1], PhysicsBlock::rubber);
			(*myPhysicsWorld)->AddObject(PhysicsJunctionPP);
			ParticleS.push_back(PhysicsParticle1);
		}
	}

	void PhysicsDemo9(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if ((*myPhysicsWorld) != nullptr)
		{
			delete (*myPhysicsWorld);
		}
		(*myPhysicsWorld) = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 20;

		// 设置摄像机位置
		mCamera->setCameraPos({0, 0, MapSize * 2});

		// 创建地图
		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->at(MapSize / 2, MapSize / 2).Entity = true;
		mMapStatic->at(MapSize / 2, MapSize / 2).Collision = true;
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		const double Mass = 2;
		const double Gao = 1;
		std::vector<PhysicsBlock::PhysicsParticle *> ParticleS;

		PhysicsBlock::PhysicsParticle *PhysicsParticle1 = new PhysicsBlock::PhysicsParticle({0 - (MapSize / 2) + 1, Gao}, Mass);
		(*myPhysicsWorld)->AddObject(PhysicsParticle1);
		ParticleS.push_back(PhysicsParticle1);

		for (size_t i = 0; i < ((MapSize - 2) * 2 - 1); ++i)
		{
			PhysicsParticle1 = new PhysicsBlock::PhysicsParticle({ParticleS[ParticleS.size() - 1]->pos.x + 0.5, Gao}, Mass);
			(*myPhysicsWorld)->AddObject(PhysicsParticle1);
			PhysicsBlock::PhysicsJunctionPP *PhysicsJunctionPP = new PhysicsBlock::PhysicsJunctionPP(PhysicsParticle1, ParticleS[ParticleS.size() - 1], PhysicsBlock::cord);
			(*myPhysicsWorld)->AddObject(PhysicsJunctionPP);
			ParticleS.push_back(PhysicsParticle1);
		}

		PhysicsBlock::PhysicsCircle *PhysicsCircle1 = new PhysicsBlock::PhysicsCircle({-2, -2}, 1, FLOAT_MAX);
		(*myPhysicsWorld)->AddObject(PhysicsCircle1);
		PhysicsCircle1 = new PhysicsBlock::PhysicsCircle({2, -2}, 1, FLOAT_MAX);
		(*myPhysicsWorld)->AddObject(PhysicsCircle1);
	}

	void PhysicsDemo10(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if ((*myPhysicsWorld) != nullptr)
		{
			delete (*myPhysicsWorld);
		}
		(*myPhysicsWorld) = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 10;

		// 设置摄像机位置
		mCamera->setCameraPos({0, 0, MapSize * 2});

		// 创建地图
		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->at(MapSize / 2 - 1, 1).Entity = true;
		mMapStatic->at(MapSize / 2 - 1, 1).Collision = true;
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		PhysicsBlock::PhysicsCircle *PhysicsCircle1 = new PhysicsBlock::PhysicsCircle({-1.5, 0}, 1, 1);
		(*myPhysicsWorld)->AddObject(PhysicsCircle1);
	}

	void PhysicsDemo11(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if ((*myPhysicsWorld) != nullptr)
		{
			delete (*myPhysicsWorld);
		}
		(*myPhysicsWorld) = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 50;

		// 设置摄像机位置
		mCamera->setCameraPos({0, 0, MapSize * 2});

		// 创建地图
		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		PhysicsBlock::PhysicsCircle *PhysicsCircle1;

		for (int x = 0; x < MapSize / 2; ++x)
		{
			for (int y = 0; y < MapSize * 1.5; ++y)
			{
				PhysicsCircle1 = new PhysicsBlock::PhysicsCircle({x - (MapSize / 4), (y * 1.2) - (MapSize / 3)}, 0.5, 1);
				(*myPhysicsWorld)->AddObject(PhysicsCircle1);
			}
		}
	}

	void PhysicsDemo12(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if ((*myPhysicsWorld) != nullptr)
		{
			delete (*myPhysicsWorld);
		}
		(*myPhysicsWorld) = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 50;

		// 设置摄像机位置
		mCamera->setCameraPos({0, 0, MapSize * 2});

		// 创建地图
		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		PhysicsBlock::PhysicsCircle *PhysicsCircle1;
		PhysicsBlock::PhysicsJunctionS *PhysicsJunctionS1;
		for (int x = 0; x < (MapSize / 2 - 6); ++x)
		{
			PhysicsCircle1 = new PhysicsBlock::PhysicsCircle({x - (MapSize / 4 - 3), -MapSize / 4}, 0.5, 1);
			(*myPhysicsWorld)->AddObject(PhysicsCircle1);
			PhysicsJunctionS1 = new PhysicsBlock::PhysicsJunctionS(PhysicsCircle1, {0, 0}, {x - (MapSize / 4 - 3), 0}, PhysicsBlock::cord);
			(*myPhysicsWorld)->AddObject(PhysicsJunctionS1);
		}
		PhysicsCircle1 = new PhysicsBlock::PhysicsCircle({-(MapSize / 2 - 3), 0}, 0.5, 1);
		(*myPhysicsWorld)->AddObject(PhysicsCircle1);
		PhysicsJunctionS1 = new PhysicsBlock::PhysicsJunctionS(PhysicsCircle1, {0, 0}, {-(MapSize / 4 - 2), 0}, PhysicsBlock::cord);
		(*myPhysicsWorld)->AddObject(PhysicsJunctionS1);
	}

	void PhysicsDemo13(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if ((*myPhysicsWorld) != nullptr)
		{
			delete (*myPhysicsWorld);
		}
		(*myPhysicsWorld) = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 50;

		// 设置摄像机位置
		mCamera->setCameraPos({0, 0, MapSize * 2});

		// 创建地图
		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		PhysicsBlock::PhysicsShape *PhysicsShape2;
		PhysicsBlock::PhysicsShape *PhysicsShape3;
		PhysicsBlock::PhysicsJoint *PhysicsJoint1;
		for (int x = 0; x < (MapSize / 2 - 6); ++x)
		{
			PhysicsShape2 = new PhysicsBlock::PhysicsShape({x - (MapSize / 4 - 3), 0}, {1, 1});
			for (size_t i = 0; i < (PhysicsShape2->width * PhysicsShape2->height); ++i)
			{
				PhysicsShape2->at(i).Collision = true;
				PhysicsShape2->at(i).Entity = true;
				PhysicsShape2->at(i).mass = FLOAT_MAX;
			}
			PhysicsShape2->mass = FLOAT_MAX;
			PhysicsShape2->UpdateAll();
			PhysicsShape2->angle = 0;
			(*myPhysicsWorld)->AddObject(PhysicsShape2);
			PhysicsShape3 = new PhysicsBlock::PhysicsShape({x - (MapSize / 4 - 3), -MapSize / 4}, {1, 1});
			for (size_t i = 0; i < (PhysicsShape3->width * PhysicsShape3->height); ++i)
			{
				PhysicsShape3->at(i).Collision = true;
				PhysicsShape3->at(i).Entity = true;
				PhysicsShape3->at(i).mass = 1;
			}
			PhysicsShape3->UpdateAll();
			PhysicsShape3->angle = 0;
			(*myPhysicsWorld)->AddObject(PhysicsShape3);
			PhysicsJoint1 = new PhysicsBlock::PhysicsJoint;
			PhysicsJoint1->Set(PhysicsShape2, PhysicsShape3, {PhysicsShape2->pos + Vec2_{0, -1}});
			(*myPhysicsWorld)->AddObject(PhysicsJoint1);
		}
		PhysicsShape2 = new PhysicsBlock::PhysicsShape({-(MapSize / 4 - 2), 0}, {1, 1});
		for (size_t i = 0; i < (PhysicsShape2->width * PhysicsShape2->height); ++i)
		{
			PhysicsShape2->at(i).Collision = true;
			PhysicsShape2->at(i).Entity = true;
			PhysicsShape2->at(i).mass = FLOAT_MAX;
		}
		PhysicsShape2->mass = FLOAT_MAX;
		PhysicsShape2->UpdateAll();
		PhysicsShape2->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape2);
		PhysicsShape3 = new PhysicsBlock::PhysicsShape({-(MapSize / 2 - 4), -1}, {1, 1});
		for (size_t i = 0; i < (PhysicsShape3->width * PhysicsShape3->height); ++i)
		{
			PhysicsShape3->at(i).Collision = true;
			PhysicsShape3->at(i).Entity = true;
			PhysicsShape3->at(i).mass = 1;
		}
		PhysicsShape3->UpdateAll();
		PhysicsShape3->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape3);
		PhysicsJoint1 = new PhysicsBlock::PhysicsJoint;
		PhysicsJoint1->Set(PhysicsShape2, PhysicsShape3, {PhysicsShape2->pos + Vec2_{0, -1}});
		(*myPhysicsWorld)->AddObject(PhysicsJoint1);
	}

	void PhysicsDemo14(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if ((*myPhysicsWorld) != nullptr)
		{
			delete (*myPhysicsWorld);
		}
		(*myPhysicsWorld) = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 20;

		// 设置摄像机位置
		mCamera->setCameraPos({0, 0, MapSize * 2});

		// 创建地图
		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		PhysicsBlock::PhysicsCircle *PhysicsCircle1;
		PhysicsCircle1 = new PhysicsBlock::PhysicsCircle({-MapSize / 3, MapSize / 3}, 0.5, 1, 0.2);
		PhysicsCircle1->angleSpeed = -100;
		(*myPhysicsWorld)->AddObject(PhysicsCircle1);

		int Y = 0;
		int X = 0;

		PhysicsBlock::PhysicsShape *PhysicsShape2;
		PhysicsShape2 = new PhysicsBlock::PhysicsShape({0, Y}, {MapSize - 4, 1});
		for (size_t i = 0; i < (PhysicsShape2->width * PhysicsShape2->height); ++i)
		{
			PhysicsShape2->at(i).Collision = true;
			PhysicsShape2->at(i).Entity = true;
			PhysicsShape2->at(i).mass = FLOAT_MAX;
			PhysicsShape2->at(i).FrictionFactor = 5;
		}
		PhysicsShape2->mass = FLOAT_MAX;
		PhysicsShape2->UpdateAll();
		PhysicsShape2->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape2);
		/*PhysicsShape2 = new PhysicsBlock::PhysicsShape({(-MapSize / (6 * 1.414)) - (MapSize / 6) - X, MapSize / (6 * 1.414) + Y}, {MapSize / 3, 1});
		for (size_t i = 0; i < (PhysicsShape2->width * PhysicsShape2->height); ++i)
		{
			PhysicsShape2->at(i).Collision = true;
			PhysicsShape2->at(i).Entity = true;
			PhysicsShape2->at(i).mass = FLOAT_MAX;
			PhysicsShape2->at(i).FrictionFactor = 5;
		}
		PhysicsShape2->mass = FLOAT_MAX;
		PhysicsShape2->UpdateAll();
		PhysicsShape2->angle = -M_PI_4;
		(*myPhysicsWorld)->AddObject(PhysicsShape2);*/
		PhysicsShape2 = new PhysicsBlock::PhysicsShape({(MapSize / (6 * 1.414)) + MapSize / 6 + X, MapSize / (6 * 1.414) + Y}, {MapSize / 3, 1});
		for (size_t i = 0; i < (PhysicsShape2->width * PhysicsShape2->height); ++i)
		{
			PhysicsShape2->at(i).Collision = true;
			PhysicsShape2->at(i).Entity = true;
			PhysicsShape2->at(i).mass = FLOAT_MAX;
			PhysicsShape2->at(i).FrictionFactor = 5;
		}
		PhysicsShape2->mass = FLOAT_MAX;
		PhysicsShape2->UpdateAll();
		PhysicsShape2->angle = M_PI / 4;
		(*myPhysicsWorld)->AddObject(PhysicsShape2);
	}

	void PhysicsDemo15(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if ((*myPhysicsWorld) != nullptr)
		{
			delete (*myPhysicsWorld);
		}
		(*myPhysicsWorld) = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 500;

		// 设置摄像机位置
		mCamera->setCameraPos({0, 0, MapSize * 2});

		// 创建地图
		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		PhysicsBlock::PhysicsCircle *PhysicsCircle1;

		PhysicsCircle1 = new PhysicsBlock::PhysicsCircle({0, -(MapSize / 3)}, 1, 1);
		(*myPhysicsWorld)->AddObject(PhysicsCircle1);

		for (int x = 0; x < MapSize / 16; ++x)
		{
			PhysicsCircle1->pos.y = -(MapSize / 3);
			PhysicsCircle1->radius = 0;
			for (int i = 0; i < MapSize / 3; ++i)
			{
				FLOAT_ R = PhysicsBlock::Random(1.0, 4.0);
				PhysicsCircle1 = new PhysicsBlock::PhysicsCircle({-(MapSize / 2) + (MapSize / 4) + PhysicsBlock::Random(-0.2, 0.2) + (x * 8), PhysicsCircle1->pos.y + PhysicsCircle1->radius + R + 1.0}, R, 1);
				(*myPhysicsWorld)->AddObject(PhysicsCircle1);
			}
		}
	}

	void PhysicsDemo16(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if ((*myPhysicsWorld) != nullptr)
		{
			delete (*myPhysicsWorld);
		}
		(*myPhysicsWorld) = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 10;

		// 设置摄像机位置
		mCamera->setCameraPos({0, 0, MapSize * 2});

		// 创建地图
		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->at(MapSize / 2, MapSize / 2).Entity = true;
		mMapStatic->at(MapSize / 2, MapSize / 2).Collision = true;
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		const double Mass = 2;
		const double Gao = 1;

		PhysicsBlock::PhysicsParticle *PhysicsParticle1 = new PhysicsBlock::PhysicsParticle({3, 4}, 1);
		(*myPhysicsWorld)->AddObject(PhysicsParticle1);

		PhysicsBlock::PhysicsShape *PhysicsShape3 = new PhysicsBlock::PhysicsShape({0, 4}, {1, 1});
		for (size_t i = 0; i < (PhysicsShape3->width * PhysicsShape3->height); ++i)
		{
			PhysicsShape3->at(i).Collision = true;
			PhysicsShape3->at(i).Entity = true;
			PhysicsShape3->at(i).mass = 1;
		}
		PhysicsShape3->UpdateAll();
		PhysicsShape3->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape3);

		PhysicsBlock::PhysicsLine *PhysicsLine1 = new PhysicsBlock::PhysicsLine({MapSize / 2 - 1, 2}, {-MapSize / 2 + 2, 2}, 1);
		(*myPhysicsWorld)->AddObject(PhysicsLine1);

		PhysicsLine1 = new PhysicsBlock::PhysicsLine({MapSize / 2 - 1, -4}, {-MapSize / 2 + 2, -4}, 1);
		(*myPhysicsWorld)->AddObject(PhysicsLine1);

		PhysicsBlock::PhysicsCircle *PhysicsCircle1 = new PhysicsBlock::PhysicsCircle({2, -2}, 1, FLOAT_MAX);
		(*myPhysicsWorld)->AddObject(PhysicsCircle1);
		PhysicsCircle1 = new PhysicsBlock::PhysicsCircle({-2, -2}, 1, FLOAT_MAX);
		(*myPhysicsWorld)->AddObject(PhysicsCircle1);
	}

	void PhysicsDemo17(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if ((*myPhysicsWorld) != nullptr)
		{
			delete (*myPhysicsWorld);
		}
		(*myPhysicsWorld) = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 20;

		// 设置摄像机位置
		mCamera->setCameraPos({0, 0, MapSize * 2});

		// 创建地图
		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->at(MapSize / 2, MapSize / 2).Entity = true;
		mMapStatic->at(MapSize / 2, MapSize / 2).Collision = true;
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		const double Mass = 2;
		const double Gao = 1;
		std::vector<PhysicsBlock::PhysicsLine *> LineS;

		Vec2_ begin{0 - (MapSize / 2) + 2, Gao}, end{0 - (MapSize / 2) + 3, Gao};

		PhysicsBlock::PhysicsLine *PhysicsLine1 = new PhysicsBlock::PhysicsLine(begin, end, 1);
		(*myPhysicsWorld)->AddObject(PhysicsLine1);
		LineS.push_back(PhysicsLine1);
		PhysicsBlock::PhysicsJoint *PhysicsJoint1;
		for (size_t i = 0; i < (MapSize - 6); ++i)
		{
			++begin.x;
			++end.x;
			PhysicsLine1 = new PhysicsBlock::PhysicsLine(begin, end, 1);
			(*myPhysicsWorld)->AddObject(PhysicsLine1);
			PhysicsJoint1 = new PhysicsBlock::PhysicsJoint;
			PhysicsJoint1->Set(PhysicsLine1, LineS[LineS.size() - 1], begin);
			(*myPhysicsWorld)->AddObject(PhysicsJoint1);
			LineS.push_back(PhysicsLine1);
		}

		PhysicsBlock::PhysicsCircle *PhysicsCircle1 = new PhysicsBlock::PhysicsCircle({2, -2}, 1, FLOAT_MAX);
		(*myPhysicsWorld)->AddObject(PhysicsCircle1);
		PhysicsCircle1 = new PhysicsBlock::PhysicsCircle({-2, -2}, 1, FLOAT_MAX);
		(*myPhysicsWorld)->AddObject(PhysicsCircle1);
	}

	void PhysicsDemo18(PhysicsWorld** myPhysicsWorld, Camera* mCamera)
	{
		if ((*myPhysicsWorld) != nullptr)
		{
			delete (*myPhysicsWorld);
		}
		(*myPhysicsWorld) = new PhysicsBlock::PhysicsWorld({ 0.0, -9.8 }, false);
		int MapSize = 20;

		// 设置摄像机位置
		mCamera->setCameraPos({ 0, 0, MapSize * 2 });

		PhysicsBlock::MapStatic* mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->SetCentrality({ MapSize / 2, MapSize / 2 });
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		// === PhysicsAssembly Demo ===
		PhysicsBlock::PhysicsAssembly* assembly = new PhysicsBlock::PhysicsAssembly();

		// 1. 一个矩形形状 — 主体
		const FLOAT_ BR = 1.5;
		PhysicsBlock::PhysicsShape* bodyShape = new PhysicsBlock::PhysicsShape({ 0, 0 }, { 3, 3 });
		for (size_t i = 0; i < (bodyShape->width * bodyShape->height); ++i)
		{
			bodyShape->at(i).Entity = false;
			bodyShape->at(i).Collision = false;
			bodyShape->at(i).mass = 1;
		}
		for (int x = 0; x < 3; ++x)
		{
			for (int y = 0; y < 3; ++y)
			{
				if (x == 1 && y == 1) continue;
				bodyShape->at(x, y).Entity = true;
				bodyShape->at(x, y).Collision = true;
			}
		}
		bodyShape->UpdateAll();
		bodyShape->pos = { 0, -4 };
		bodyShape->angle = 0;
		assembly->Add(bodyShape);

		// 2. 一个圆形 — 顶部轮子
		PhysicsBlock::PhysicsCircle* topCircle = new PhysicsBlock::PhysicsCircle({ 0, -1.5 }, BR, 1);
		assembly->Add(topCircle);

		// 3. 小型圆 — 分布在主体周围（用作 PhysicsJunctionSS 的端点）
		PhysicsBlock::PhysicsCircle* p1 = new PhysicsBlock::PhysicsCircle({ -BR, -4 }, 0.2, 1);
		PhysicsBlock::PhysicsCircle* p2 = new PhysicsBlock::PhysicsCircle({ BR, -4 }, 0.2, 1);
		PhysicsBlock::PhysicsCircle* p3 = new PhysicsBlock::PhysicsCircle({ 0, -7 }, 0.2, 1);
		assembly->Add(p1);
		assembly->Add(p2);
		assembly->Add(p3);

		// 4. 线段 — 连接两个小圆
		PhysicsBlock::PhysicsLine* line1 = new PhysicsBlock::PhysicsLine({ -BR, -4 }, { BR, -4 }, 1);
		assembly->Add(line1);

		// 5. 用 PhysicsJoint 将顶部圆和主体刚性连接
		PhysicsBlock::PhysicsJoint* jointCS = new PhysicsBlock::PhysicsJoint;
		jointCS->Set(topCircle, bodyShape, { 0, -2.5 });

		// 6. 用 PhysicsJunctionSS 将小圆连接到主体（绳索约束）
		PhysicsBlock::PhysicsJunctionSS* juncP1 = new PhysicsBlock::PhysicsJunctionSS(bodyShape, { 0, -1.0 }, p1, { 0, 0 }, PhysicsBlock::cord);
		PhysicsBlock::PhysicsJunctionSS* juncP2 = new PhysicsBlock::PhysicsJunctionSS(bodyShape, { 1.0, 0 }, p2, { 0, 0 }, PhysicsBlock::cord);

		// 将组装体注册到物理世界
		(*myPhysicsWorld)->AddObject(assembly);

		// 注册关节和连接
		(*myPhysicsWorld)->AddObject(jointCS);
		(*myPhysicsWorld)->AddObject(juncP1);
		(*myPhysicsWorld)->AddObject(juncP2);

		// 7. 外部测试物体 — 会对组装体产生碰撞
		PhysicsBlock::PhysicsCircle* externalCircle = new PhysicsBlock::PhysicsCircle({ 5, -1 }, BR * 0.6, 1);
		(*myPhysicsWorld)->AddObject(externalCircle);
		PhysicsBlock::PhysicsShape* externalShape = new PhysicsBlock::PhysicsShape({ -5, -1 }, { 2, 2 });
		for (size_t i = 0; i < (externalShape->width * externalShape->height); ++i)
		{
			externalShape->at(i).Entity = true;
			externalShape->at(i).Collision = true;
			externalShape->at(i).mass = 1;
		}
		externalShape->UpdateAll();
		externalShape->angle = 0.5;
		(*myPhysicsWorld)->AddObject(externalShape);

		PhysicsBlock::PhysicsParticle* externalParticle = new PhysicsBlock::PhysicsParticle({ 5, 2 }, 1);
		(*myPhysicsWorld)->AddObject(externalParticle);
	}

	void PhysicsDemo19(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 30;

		mCamera->setCameraPos({0, 0, MapSize * 2});

		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		LayerMask layerAll = LayerMaskAll;
		LayerMask layerObjA = LayerMaskDefault;
		LayerMask layerObjB = LayerMaskDefault;
		LayerMask layerGhost = 1 << 3;

		PhysicsBlock::PhysicsShape *floor = new PhysicsBlock::PhysicsShape({0, -(FLOAT_)(MapSize / 2 - 3)}, {MapSize - 2, 4});
		for (size_t k = 0; k < (floor->width * floor->height); ++k)
		{
			floor->at(k).Entity = true;
			floor->at(k).Collision = true;
			floor->at(k).mass = FLOAT_MAX;
		}
		floor->UpdateAll();
		floor->angle = 0;
		(*myPhysicsWorld)->AddObject(floor);
		(*myPhysicsWorld)->mCollision.SetCollisionLayers(floor, layerAll);
		(*myPhysicsWorld)->mCollision.SetCollisionPriority(floor, 100);

		PhysicsBlock::PhysicsShape *divider = new PhysicsBlock::PhysicsShape({0, 0}, {2, 10});
		for (size_t k = 0; k < (divider->width * divider->height); ++k)
		{
			divider->at(k).Entity = true;
			divider->at(k).Collision = true;
			divider->at(k).mass = FLOAT_MAX;
		}
		divider->UpdateAll();
		divider->angle = 0;
		(*myPhysicsWorld)->AddObject(divider);
		(*myPhysicsWorld)->mCollision.SetCollisionLayers(divider, layerAll);
		(*myPhysicsWorld)->mCollision.SetCollisionPriority(divider, 20);

		PhysicsBlock::PhysicsShape *platform = new PhysicsBlock::PhysicsShape({-6, -6}, {8, 2});
		for (size_t k = 0; k < (platform->width * platform->height); ++k)
		{
			platform->at(k).Entity = true;
			platform->at(k).Collision = true;
			platform->at(k).mass = FLOAT_MAX;
		}
		platform->UpdateAll();
		platform->angle = 0;
		(*myPhysicsWorld)->AddObject(platform);
		(*myPhysicsWorld)->mCollision.SetCollisionLayers(platform, layerAll);
		(*myPhysicsWorld)->mCollision.SetCollisionPriority(platform, 10);

		(*myPhysicsWorld)->mCollision.AddCollisionEnterListener(floor,
			[](const PhysicsFormwork *a, const PhysicsFormwork *b, const BaseArbiter *arbiter) {
				PhysicsLog("[FLOOR Enter] Object hit floor\n");
			});

		(*myPhysicsWorld)->mCollision.AddCollisionEnterListener(divider,
			[](const PhysicsFormwork *a, const PhysicsFormwork *b, const BaseArbiter *arbiter) {
				PhysicsLog("[DIVIDER Enter] Object hit divider\n");
			});

		(*myPhysicsWorld)->mCollision.AddCollisionEnterListener(platform,
			[](const PhysicsFormwork *a, const PhysicsFormwork *b, const BaseArbiter *arbiter) {
				PhysicsLog("[PLATFORM Enter] Object landed on platform\n");
			});

		for (int i = 0; i < 4; ++i)
		{
			PhysicsBlock::PhysicsShape *box = new PhysicsBlock::PhysicsShape(
				{-10 + i * 2.5f + Random(-0.1f, 0.1f), 10 + i * 1.5f},
				{1, 1});
			for (size_t k = 0; k < (box->width * box->height); ++k)
			{
				box->at(k).Entity = true;
				box->at(k).Collision = true;
				box->at(k).mass = 1;
			}
			box->UpdateAll();
			box->angle = Random(-0.15f, 0.15f);
			(*myPhysicsWorld)->AddObject(box);
			(*myPhysicsWorld)->mCollision.SetCollisionLayers(box, layerObjA);

			int idx = i;
			(*myPhysicsWorld)->mCollision.AddCollisionEnterListener(box,
				[idx](const PhysicsFormwork *a, const PhysicsFormwork *b, const BaseArbiter *arbiter) {
					if (arbiter->numContacts > 0)
						PhysicsLog("[ENTER] Box#%d hit at (%.1f,%.1f) depth=%.3f normal=(%.2f,%.2f)\n",
							idx,
							arbiter->contacts[0].position.x, arbiter->contacts[0].position.y,
							-arbiter->contacts[0].separation,
							arbiter->contacts[0].normal.x, arbiter->contacts[0].normal.y);
				});
		}

		for (int i = 0; i < 6; ++i)
		{
			PhysicsBlock::PhysicsCircle *ball = new PhysicsBlock::PhysicsCircle(
				{4 + i * 1.8f + Random(-0.1f, 0.1f), 10 + i * 1.2f},
				0.6f + Random(0.0f, 0.3f), 1);
			(*myPhysicsWorld)->AddObject(ball);
			(*myPhysicsWorld)->mCollision.SetCollisionLayers(ball, layerObjB);

			int idx = i;
			(*myPhysicsWorld)->mCollision.AddCollisionEnterListener(ball,
				[idx](const PhysicsFormwork *a, const PhysicsFormwork *b, const BaseArbiter *arbiter) {
					if (arbiter->numContacts > 0)
						PhysicsLog("[ENTER] Ball#%d at (%.1f,%.1f) depth=%.3f\n",
							idx,
							arbiter->contacts[0].position.x, arbiter->contacts[0].position.y,
							-arbiter->contacts[0].separation);
				});
			(*myPhysicsWorld)->mCollision.AddCollisionExitListener(ball,
				[idx](const PhysicsFormwork *a, const PhysicsFormwork *b, const BaseArbiter *arbiter) {
					PhysicsLog("[EXIT]  Ball#%d separated\n", idx);
				});
		}

		for (int i = 0; i < 3; ++i)
		{
			PhysicsBlock::PhysicsCircle *ghost = new PhysicsBlock::PhysicsCircle(
				{-4 + i * 2.0f + Random(-0.1f, 0.1f), 14 + i * 2.0f},
				0.7f, 1);
			(*myPhysicsWorld)->AddObject(ghost);
			(*myPhysicsWorld)->mCollision.SetCollisionLayers(ghost, layerGhost);

			int idx = i;
			(*myPhysicsWorld)->mCollision.AddCollisionEnterListener(ghost,
				[idx](const PhysicsFormwork *a, const PhysicsFormwork *b, const BaseArbiter *arbiter) {
					PhysicsLog("[GHOST#%d] Ghost touched something (no effect)\n", idx);
				});
		}

		(*myPhysicsWorld)->mCollision.SetCollisionPriority(
			(*myPhysicsWorld)->PhysicsCircleS[0], 90);
		(*myPhysicsWorld)->mCollision.SetCollisionPriority(
			(*myPhysicsWorld)->PhysicsCircleS[3], 30);
	}

	void PhysicsDemo20(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 30;

		mCamera->setCameraPos({0, 5, MapSize * 2});

		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		PhysicsBlock::PhysicsShape *platform = new PhysicsBlock::PhysicsShape({-5, -8}, {8, 2});
		for (size_t i = 0; i < (platform->width * platform->height); ++i)
		{
			platform->at(i).Entity = true;
			platform->at(i).Collision = true;
			platform->at(i).mass = 1;
		}
		platform->UpdateAll();
		platform->angle = 0;
		(*myPhysicsWorld)->AddObject(platform);

		(*myPhysicsWorld)->mKinematic.SetIsKinematic(platform, true);
		(*myPhysicsWorld)->mKinematic.MoveTo(platform, {-5, 6}, 3.0f);
		(*myPhysicsWorld)->mKinematic.SetMoveMode(platform, KinematicMoveMode::PingPong);

		for (int i = 0; i < 4; ++i)
		{
			PhysicsBlock::PhysicsCircle *ball = new PhysicsBlock::PhysicsCircle(
				{-6 + i * 1.2f + Random(-0.1f, 0.1f), -5 + i * 1.5f},
				0.5, 1);
			(*myPhysicsWorld)->AddObject(ball);
		}

		PhysicsBlock::PhysicsShape *arm = new PhysicsBlock::PhysicsShape({7, 6}, {8, 3});
		for (size_t i = 0; i < (arm->width * arm->height); ++i)
		{
			arm->at(i).Entity = true;
			arm->at(i).Collision = true;
			arm->at(i).mass = 1;
		}
		arm->UpdateAll();
		arm->angle = 0;
		(*myPhysicsWorld)->AddObject(arm);

		(*myPhysicsWorld)->mKinematic.SetIsKinematic(arm, true);
		(*myPhysicsWorld)->mKinematic.RotateTo(arm, M_PI / 3, 2.0f);
		(*myPhysicsWorld)->mKinematic.SetRotateMode(arm, KinematicMoveMode::PingPong);

		for (int i = 0; i < 3; ++i)
		{
			PhysicsBlock::PhysicsShape *box = new PhysicsBlock::PhysicsShape(
				{-(MapSize / 2) + 3 + i * 2.2f, 10 + i * 2.5f},
				{2, 2});
			for (size_t k = 0; k < (box->width * box->height); ++k)
			{
				box->at(k).Entity = true;
				box->at(k).Collision = true;
				box->at(k).mass = 1;
			}
			box->UpdateAll();
			box->angle = 0;
			(*myPhysicsWorld)->AddObject(box);
		}
	}

	void PhysicsDemo21(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 30;

		mCamera->setCameraPos({0, 0, MapSize * 2});

		PhysicsBlock::MapStatic *mMapStatic = new PhysicsBlock::MapStatic(MapSize, MapSize);
		for (int i = 0; i < (MapSize * MapSize); ++i)
		{
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
			mMapStatic->at(i).Healthpoint = 24;
		}
		for (int i = 0; i < MapSize; ++i)
		{
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MapSize - 1, i).Entity = true;
			mMapStatic->at(MapSize - 1, i).Collision = true;
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
		}
		mMapStatic->SetCentrality({MapSize / 2, MapSize / 2});
		(*myPhysicsWorld)->SetMapFormwork(mMapStatic);

		PhysicsBlock::PhysicsShape *triggerObj1 = new PhysicsBlock::PhysicsShape({-6, 5}, {2, 2});
		for (size_t i = 0; i < (triggerObj1->width * triggerObj1->height); ++i)
		{
			triggerObj1->at(i).Entity = true;
			triggerObj1->at(i).Collision = false;
			triggerObj1->at(i).mass = FLOAT_MAX;
		}
		triggerObj1->UpdateAll();
		triggerObj1->angle = 0;
		(*myPhysicsWorld)->AddObject(triggerObj1);

		Bounds triggerZone1({-6, 7}, {10, 4});
		(*myPhysicsWorld)->mTrigger.SetTriggerBounds(triggerObj1, triggerZone1);

		(*myPhysicsWorld)->mTrigger.AddTriggerListener(triggerObj1, TriggerEventType::Enter,
			[](PhysicsFormwork *other) {
				PhysicsLog("[Trigger Enter] Object entered Zone-1 at y=%.1f\n", other->PFGetPos().y);
			});
		(*myPhysicsWorld)->mTrigger.AddTriggerListener(triggerObj1, TriggerEventType::Exit,
			[](PhysicsFormwork *other) {
				PhysicsLog("[Trigger Exit] Object left Zone-1 at y=%.1f\n", other->PFGetPos().y);
			});

		PhysicsBlock::PhysicsShape *triggerObj2 = new PhysicsBlock::PhysicsShape({6, -2}, {2, 2});
		for (size_t i = 0; i < (triggerObj2->width * triggerObj2->height); ++i)
		{
			triggerObj2->at(i).Entity = true;
			triggerObj2->at(i).Collision = false;
			triggerObj2->at(i).mass = FLOAT_MAX;
		}
		triggerObj2->UpdateAll();
		triggerObj2->angle = 0;
		(*myPhysicsWorld)->AddObject(triggerObj2);

		Bounds triggerZone2({6, 0}, {8, 4});
		(*myPhysicsWorld)->mTrigger.SetTriggerBounds(triggerObj2, triggerZone2);
		(*myPhysicsWorld)->mTrigger.SetTriggerLayers(triggerObj2, 1 << 0);

		(*myPhysicsWorld)->mTrigger.AddTriggerListener(triggerObj2, TriggerEventType::Enter,
			[](PhysicsFormwork *other) {
				PhysicsLog("[Trigger Enter] Object entered Zone-2 (Layer-0 only) at y=%.1f\n",
					other->PFGetPos().y);
			});
		(*myPhysicsWorld)->mTrigger.AddTriggerListener(triggerObj2, TriggerEventType::Stay,
			[](PhysicsFormwork *other) {
				static int frameCount = 0;
				if (++frameCount % 30 == 0)
					PhysicsLog("[Trigger Stay] Object staying in Zone-2 at (%.1f, %.1f)\n",
						other->PFGetPos().x, other->PFGetPos().y);
			});

		PhysicsBlock::PhysicsShape *triggerObj3 = new PhysicsBlock::PhysicsShape({0, -9}, {2, 2});
		for (size_t i = 0; i < (triggerObj3->width * triggerObj3->height); ++i)
		{
			triggerObj3->at(i).Entity = true;
			triggerObj3->at(i).Collision = false;
			triggerObj3->at(i).mass = FLOAT_MAX;
		}
		triggerObj3->UpdateAll();
		triggerObj3->angle = 0;
		(*myPhysicsWorld)->AddObject(triggerObj3);

		Bounds triggerZone3({0, -5}, {14, 6});
		(*myPhysicsWorld)->mTrigger.SetTriggerBounds(triggerObj3, triggerZone3);

		(*myPhysicsWorld)->mTrigger.AddTriggerListener(triggerObj3, TriggerEventType::Enter,
			[](PhysicsFormwork *other) {
				PhysicsLog("[Trigger Enter] Object entered Catch-Zone at (%.1f, %.1f)\n",
					other->PFGetPos().x, other->PFGetPos().y);
			});
		(*myPhysicsWorld)->mTrigger.AddTriggerListener(triggerObj3, TriggerEventType::Exit,
			[](PhysicsFormwork *other) {
				PhysicsLog("[Trigger Exit] Object escaped Catch-Zone\n");
			});

		for (int g = 0; g < 4; ++g)
		{
			PhysicsBlock::PhysicsShape *box = new PhysicsBlock::PhysicsShape(
				{-(MapSize / 2) + 3 + g * 3.5f, 12 + g * 2.0f},
				{2, 2});
			for (size_t k = 0; k < (box->width * box->height); ++k)
			{
				box->at(k).Entity = true;
				box->at(k).Collision = true;
				box->at(k).mass = 1;
			}
			box->UpdateAll();
			box->angle = Random(0, M_PI);
			(*myPhysicsWorld)->AddObject(box);
		}

		for (int g = 0; g < 4; ++g)
		{
			PhysicsBlock::PhysicsCircle *ball = new PhysicsBlock::PhysicsCircle(
				{MapSize / 2 - 3 - g * 3.5f, 12 + g * 1.5f},
				0.9, 1);
			(*myPhysicsWorld)->AddObject(ball);
		}

		PhysicsBlock::PhysicsCircle *centerBall = new PhysicsBlock::PhysicsCircle({0, 10}, 1.2, 3);
		(*myPhysicsWorld)->AddObject(centerBall);
	}

	void PhysicsDemo22(PhysicsWorld **myPhysicsWorld, Camera *mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete *myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);

		const unsigned int PlateCountX = 50;
		const unsigned int PlateCountY = 50;
		const int MapPixelWidth = PlateCountX * PixelBlockEdgeSize;
		const int MapPixelHeight = PlateCountY * PixelBlockEdgeSize;

		mCamera->setCameraPos({0, 0, MapPixelWidth * 2.0f});

		PhysicsBlock::MapDynamic *mMapDynamic = new PhysicsBlock::MapDynamic(PlateCountX, PlateCountY);

		static PerlinNoise mPerlinNoise;

		mMapDynamic->SetCallback(
			[](PhysicsBlock::BaseGrid** mT, int x, int y, void* Data) {
				PerlinNoise* noise = (PerlinNoise*)Data;
				PhysicsBlock::BaseGrid* grid = *mT;
				for (unsigned int ix = 0; ix < PixelBlockEdgeSize; ++ix)
				{
					for (unsigned int iy = 0; iy < PixelBlockEdgeSize; ++iy)
					{
						double nx = (x * PixelBlockEdgeSize + ix) * 0.05;
						double ny = (y * PixelBlockEdgeSize + iy) * 0.05;
						double val = noise->noise(nx, ny, 0.5);
						bool collision = (val > 0.6);
						grid->at(ix, iy).Entity = collision;
						grid->at(ix, iy).Collision = collision;
						grid->at(ix, iy).mass = 1.0;
						grid->at(ix, iy).Healthpoint = 24;
					}
				}
			},
			&mPerlinNoise,
			[](PhysicsBlock::BaseGrid** mT, void* Data) {
				PhysicsBlock::BaseGrid* grid = *mT;
				for (unsigned int ix = 0; ix < PixelBlockEdgeSize; ++ix)
				{
					for (unsigned int iy = 0; iy < PixelBlockEdgeSize; ++iy)
					{
						grid->at(ix, iy).Entity = false;
						grid->at(ix, iy).Collision = false;
					}
				}
			},
			nullptr
		);

		mMapDynamic->SetPos(0, 0);
		mMapDynamic->Updata({MapPixelWidth / 2.0f, MapPixelHeight / 2.0f});

		(*myPhysicsWorld)->SetMapFormwork(mMapDynamic);

		for (int i = 0; i < 25; ++i)
		{
			float bx = Random(MapPixelWidth * 0.2f, MapPixelWidth * 0.8f);
			float by = Random(MapPixelHeight * 0.55f, MapPixelHeight * 0.9f);
			PhysicsBlock::PhysicsShape *box = new PhysicsBlock::PhysicsShape({bx, by}, {2, 2});
			for (size_t k = 0; k < (box->width * box->height); ++k)
			{
				box->at(k).Entity = true;
				box->at(k).Collision = true;
				box->at(k).mass = 1;
			}
			box->UpdateAll();
			box->angle = Random(0.0f, (float)M_PI);
			(*myPhysicsWorld)->AddObject(box);
		}

		for (int i = 0; i < 15; ++i)
		{
			float cx = Random(MapPixelWidth * 0.2f, MapPixelWidth * 0.8f);
			float cy = Random(MapPixelHeight * 0.55f, MapPixelHeight * 0.9f);
			PhysicsBlock::PhysicsCircle *ball = new PhysicsBlock::PhysicsCircle({cx, cy}, Random(0.5f, 1.5f), 1);
			(*myPhysicsWorld)->AddObject(ball);
		}
	}

}
