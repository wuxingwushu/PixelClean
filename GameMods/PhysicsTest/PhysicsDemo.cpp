#include "PhysicsDemo.h"

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
		int MapSize = 160;

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
			PhysicsShape2->at(i).mass = DBL_MAX;
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
			PhysicsShape2->at(i).mass = DBL_MAX;
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
	}

}
