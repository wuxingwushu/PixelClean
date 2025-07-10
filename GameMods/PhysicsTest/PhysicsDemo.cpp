#include "PhysicsDemo.h"

namespace PhysicsBlock
{

	void PhysicsDemo1(PhysicsWorld **myPhysicsWorld, Camera* mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete* myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);
		int MapSize = 10;

		// 设置摄像机位置
		mCamera->setCameraPos({0,0,20});

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
		PhysicsShape1->UpdateInfo();
		PhysicsShape1->UpdateOutline();
		PhysicsShape1->CollisionR = 1.414;
		PhysicsShape1->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape1);
		PhysicsBlock::PhysicsShape *PhysicsShape2 = new PhysicsBlock::PhysicsShape({0, -4}, {2, 2});
		for (size_t i = 0; i < (PhysicsShape2->width * PhysicsShape2->height); ++i)
		{
			PhysicsShape2->at(i).Collision = true;
			PhysicsShape2->at(i).Entity = true;
			PhysicsShape2->at(i).mass = 1;
		}
		PhysicsShape2->UpdateInfo();
		PhysicsShape2->UpdateOutline();
		PhysicsShape2->CollisionR = 1.414;
		PhysicsShape2->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape2);
	}

	void PhysicsDemo2(PhysicsWorld **myPhysicsWorld, Camera* mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete* myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({ 0.0, -9.8 }, false);
		int MapSize = 10;

		// 设置摄像机位置
		mCamera->setCameraPos({0,5,50});

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
			PhysicsShape1->UpdateInfo();
			PhysicsShape1->UpdateOutline();
			PhysicsShape1->CollisionR = 1.414;
			PhysicsShape1->angle = 0;
			(*myPhysicsWorld)->AddObject(PhysicsShape1);
		}
	}

	void PhysicsDemo3(PhysicsWorld **myPhysicsWorld, Camera* mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete* myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({ 0.0, -9.8 }, false);
		int MapSize = 50;

		// 设置摄像机位置
		mCamera->setCameraPos({0,0,80});

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
				PhysicsBlock::PhysicsShape *PhysicsShape3 = new PhysicsBlock::PhysicsShape({(ypos * 2.2) - JZMapSize - (JZMapSize / 2) - (xpos) + 5, (xpos * 4)}, {2, 2});
				for (size_t i = 0; i < (PhysicsShape3->width * PhysicsShape3->height); i++)
				{
					PhysicsShape3->at(i).Collision = true;
					PhysicsShape3->at(i).Entity = true;
					PhysicsShape3->at(i).mass = 1;
				}
				PhysicsShape3->UpdateInfo();
				PhysicsShape3->UpdateOutline();
				PhysicsShape3->CollisionR = 1.414;
				PhysicsShape3->angle = 0;
				(*myPhysicsWorld)->AddObject(PhysicsShape3);
			}
		}
	}

	void PhysicsDemo4(PhysicsWorld **myPhysicsWorld, Camera* mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete* myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({ 0.0, -9.8 }, false);
		int MapSize = 10;

		// 设置摄像机位置
		mCamera->setCameraPos({0,0,20});

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
		PhysicsShape1->UpdateInfo();
		PhysicsShape1->UpdateOutline();
		PhysicsShape1->CollisionR = 1.414;
		PhysicsShape1->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape1);
	}

	void PhysicsDemo5(PhysicsWorld **myPhysicsWorld, Camera* mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete* myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({ 0.0, -9.8 }, false);
		int MapSize = 20;

		// 设置摄像机位置
		mCamera->setCameraPos({0,0,40});

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
		PhysicsShape1->UpdateInfo();
		PhysicsShape1->UpdateOutline();
		PhysicsShape1->CollisionR = 1.414;
		PhysicsShape1->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape1);
		PhysicsBlock::PhysicsShape *PhysicsShape2 = new PhysicsBlock::PhysicsShape({0, -4}, {2, 2});
		for (size_t i = 0; i < (PhysicsShape2->width * PhysicsShape2->height); ++i)
		{
			PhysicsShape2->at(i).Collision = true;
			PhysicsShape2->at(i).Entity = true;
			PhysicsShape2->at(i).mass = DBL_MAX;
		}
		PhysicsShape2->UpdateInfo();
		PhysicsShape2->UpdateOutline();
		PhysicsShape2->CollisionR = 1.414;
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
		PhysicsShape3->UpdateInfo();
		PhysicsShape3->UpdateOutline();
		PhysicsShape3->CollisionR = 1.414;
		PhysicsShape3->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape3);
	}

	void PhysicsDemo6(PhysicsWorld **myPhysicsWorld, Camera* mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete* myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({ 0.0, -9.8 }, false);
		int MapSize = 30;

		// 设置摄像机位置
		mCamera->setCameraPos({0,0,60});

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
		PhysicsShape2->UpdateInfo();
		PhysicsShape2->UpdateOutline();
		PhysicsShape2->CollisionR = 1.414;
		PhysicsShape2->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape2);
		PhysicsShapeVector.push_back(PhysicsShape2);

		for (size_t i = 0; i < MapSize / 8; i++)
		{
			PhysicsBlock::PhysicsShape *PhysicsShape1 = new PhysicsBlock::PhysicsShape({PhysicsShapeVector[PhysicsShapeVector.size() - 1]->pos.x + 2, 0}, {2, 1});
			for (size_t i = 0; i < (PhysicsShape1->width * PhysicsShape1->height); ++i)
			{
				PhysicsShape1->at(i).Collision = true;
				PhysicsShape1->at(i).Entity = true;
				PhysicsShape1->at(i).mass = 1;
			}
			PhysicsShape1->UpdateInfo();
			PhysicsShape1->UpdateOutline();
			PhysicsShape1->CollisionR = 1.414;
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

	void PhysicsDemo7(PhysicsWorld **myPhysicsWorld, Camera* mCamera)
	{
		if (*myPhysicsWorld != nullptr)
		{
			delete* myPhysicsWorld;
		}
		*myPhysicsWorld = new PhysicsBlock::PhysicsWorld({ 0.0, -9.8 }, false);
		int MapSize = 10;

		// 设置摄像机位置
		mCamera->setCameraPos({0,0,MapSize});

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

		PhysicsBlock::PhysicsShape *PhysicsShape1 = new PhysicsBlock::PhysicsShape({4, 0}, {2, 2});
		for (size_t i = 0; i < (PhysicsShape1->width * PhysicsShape1->height); ++i)
		{
			PhysicsShape1->at(i).Collision = true;
			PhysicsShape1->at(i).Entity = true;
			PhysicsShape1->at(i).mass = 1;
		}
		PhysicsShape1->UpdateInfo();
		PhysicsShape1->UpdateOutline();
		PhysicsShape1->CollisionR = 1.414;
		PhysicsShape1->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape1);

		PhysicsBlock::PhysicsShape *PhysicsShape2 = new PhysicsBlock::PhysicsShape({0, 0}, {2, 2});
		for (size_t i = 0; i < (PhysicsShape2->width * PhysicsShape2->height); ++i)
		{
			PhysicsShape2->at(i).Collision = true;
			PhysicsShape2->at(i).Entity = true;
			PhysicsShape2->at(i).mass = DBL_MAX;
		}
		PhysicsShape2->UpdateInfo();
		PhysicsShape2->UpdateOutline();
		PhysicsShape2->CollisionR = 1.414;
		PhysicsShape2->angle = 0;
		(*myPhysicsWorld)->AddObject(PhysicsShape2);

		PhysicsBlock::PhysicsJunction *PhysicsJunction1 = new PhysicsBlock::PhysicsJunction({PhysicsShape1, {0, 0}}, {PhysicsShape2, {1, 1}}, PhysicsBlock::cord);
		(*myPhysicsWorld)->AddObject(PhysicsJunction1);
	}

}
