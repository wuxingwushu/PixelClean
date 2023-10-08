#include "SquarePhysics.h"
#include <iostream>
#include <vector>

//VoxelPhysics
//CheckPhysics
//SquarePhysics

namespace SquarePhysics {


	SquarePhysics::SquarePhysics(unsigned int ObjectNumber, unsigned int PixelNumber)
	{
		mObjectCollisionS = new ContinuousData<ObjectCollision*>(ObjectNumber);
		mPixelCollisionS = new ContinuousData<PixelCollision*>(PixelNumber);
	}

	SquarePhysics::~SquarePhysics()
	{
		for (size_t i = 0; i < mObjectCollisionS->GetNumber(); ++i)
		{
			delete *mObjectCollisionS->GetData(i);
		}
		for (size_t i = 0; i < mPixelCollisionS->GetNumber(); ++i)
		{
			delete *mPixelCollisionS->GetData(i);
		}
		delete mObjectCollisionS;
		delete mPixelCollisionS;
	}

	ObjectSufferForce SquarePhysics::GetGoods(glm::vec2 pos) {
		glm::vec2 Lpos;
		ObjectCollision* LObjectDecorator = nullptr;
		for (size_t i = 0; i < mObjectCollisionS->GetNumber(); ++i)
		{
			LObjectDecorator = mObjectCollisionS->Data()[i];
			Lpos = pos - LObjectDecorator->GetPos();
			Lpos = vec2angle(Lpos, -LObjectDecorator->GetAngleFloat());
			if (LObjectDecorator->GetFixedCollisionBool(Lpos)) {
				return { LObjectDecorator, Lpos };
			}
		}
		return { nullptr, glm::vec2{ 0 } };
	}

	void SquarePhysics::PhysicsSimulation(float TimeStep) {
		ObjectCollision** ObjectNumberS = mObjectCollisionS->Data();
		PixelCollision** PixelCollisionS = mPixelCollisionS->Data();
		PixelCollision* LPixel;
		ObjectCollision* LObject;
		ObjectCollision* LObject2;

		glm::dvec2 Jpos, Xpos, Dian, Speed, Deviation;
		glm::ivec2 Start, End;
		float DeviationDistance;//偏移距离
		glm::dvec2 DStart, DEnd;
		CollisionInfo LCollisionInfo;

		glm::vec2 CoordinateDianJi{ 0, 0 };
		glm::vec2 LiBi{ 0, 0 };
		int CoordinateDianJiShu = 0;
		
		bool Once;

		//玩家碰撞玩家
		for (size_t i = 0; i < (mObjectCollisionS->GetNumber() - 1); ++i)
		{
			LObject = ObjectNumberS[i];
			Xpos = LObject->GetPos();//位置
			for (size_t i2 = (i + 1); i2 < mObjectCollisionS->GetNumber(); ++i2)
			{
				LObject2 = ObjectNumberS[i2];
				Once = false;
				if (Modulus(LObject->GetPos() - LObject2->GetPos()) < 22.7f) {//到了碰撞范围
					Deviation = LObject->GetPos() - LObject2->GetPos();
					CoordinateDianJi = { 0, 0 };
					LiBi = { 0, 0 };
					CoordinateDianJiShu = 0;
					for (size_t i3 = 0; i3 < LObject->GetOutlinePointSize(); ++i3)
					{
						Dian = vec2angle(LObject->GetOutlinePointSet(i3), LObject->GetAngle());//获取骨骼点
						LCollisionInfo = LObject2->SquarePhysicsCoordinateSystemRadialCollisionDetection(Xpos, Xpos + Dian, Deviation);//获取碰撞点
						if (LCollisionInfo.Collision) {
							Once = true;
							LiBi += Dian;
							if (fabs(Deviation.x) < fabs(Deviation.y))//偏向玩家位置移动
							{
								CoordinateDianJi.y += LCollisionInfo.Pos.y - Dian.y - Xpos.y;
							}
							else {
								CoordinateDianJi.x += LCollisionInfo.Pos.x - Dian.x - Xpos.x;
							}
							++CoordinateDianJiShu;
						}
					}
					if (Once) {
						CoordinateDianJi /= CoordinateDianJiShu;
						LiBi /= CoordinateDianJiShu;
						LObject->SetPos((CoordinateDianJi / 2.0f) + LObject->GetPos());
						LObject2->SetPos((-CoordinateDianJi / 2.0f) + LObject2->GetPos());
						LObject->ForceSolution((LiBi + LObject->GetPos()) - LObject2->GetPos(), LObject->GetSpeed() * 0.1f, 0);
						LObject2->ForceSolution(LiBi, -LObject->GetSpeed() * 0.1f, 0);
					}
				}
			}
		}

		//玩家对地图碰撞事件
		for (size_t i = 0; i < mObjectCollisionS->GetNumber(); ++i)
		{
			LObject = ObjectNumberS[i];
			Jpos = LObject->GetPos();//久位置
			LObject->FrameTimeStep(TimeStep, mTerrain->GetFrictionCoefficient(Jpos));//物理模拟
			Xpos = LObject->GetPos();//新位置
			Deviation = Xpos - Jpos;//移动方向和大小
			for (size_t i2 = 0; i2 < LObject->GetOutlinePointSize(); ++i2)
			{
				Dian = vec2angle(LObject->GetOutlinePointSet(i2), LObject->GetAngle());//获取骨骼点
				Start = Jpos;//久位置的骨骼点位置
				if (Start.x < 0)Start.x -= 1;//负值偏移
				if (Start.y < 0)Start.y -= 1;//负值偏移
				End = Xpos + Dian;//新位置的骨骼点位置
				if (End.x < 0)End.x -= 1;//负值偏移
				if (End.y < 0)End.y -= 1;//负值偏移
				LCollisionInfo = mTerrain->RadialCollisionDetection(Start, End);//对地图射线碰撞进行判断
				if (LCollisionInfo.Collision) {//是否碰撞
					Speed = LObject->GetSpeed();
					if (fabs(Dian.x) < fabs(Dian.y))//偏向玩家位置移动
					{
						if (Dian.y < 0)//以 1 * 1 的正方形为判断
						{
							LCollisionInfo.Pos.y += 1;//偏移
						}
						LObject->SetAngle(LObject->GetAngleFloat() + TorqueCalculate(Xpos, End, {0,-LObject->GetForceY()}) * 0.0001f);//角度
						Speed.y = 0.1f * Speed.y;
						Xpos.y = glm::dvec2(LCollisionInfo.Pos).y - Dian.y;
					}
					else
					{
						if (Dian.x < 0)//以 1 * 1 的正方形为判断
						{
							LCollisionInfo.Pos.x += 1;//偏移
						}
						LObject->SetAngle(LObject->GetAngleFloat() + TorqueCalculate(Xpos, End, { -LObject->GetForceX(), 0}) * 0.0001f);//角度
						Speed.x = 0.1f * Speed.x;
						Xpos.x = glm::dvec2(LCollisionInfo.Pos).x - Dian.x;
					}
					LObject->SetSpeed(Speed);//设置速度
					LObject->SetPos(Xpos);//设置位置
				}
			}

			LObject->SetForce({ 0,0 });
		}

		//子弹碰撞
		for (size_t i = 0; i < mPixelCollisionS->GetNumber(); ++i)//遍历所有子弹
		{
			LPixel = PixelCollisionS[i];//子弹
			Jpos = LPixel->GetPos();//现在位置
			LPixel->FrameTimeStep(TimeStep, 0);//更新物理状态
			Xpos = LPixel->GetPos();//新位置
			LCollisionInfo = mTerrain->RadialCollisionDetection(//对地图碰撞进行判断
				{ (Jpos.x < 0 ? Jpos.x - 1 : Jpos.x), (Jpos.y < 0 ? Jpos.y - 1 : Jpos.y) },
				{ (Xpos.x < 0 ? Xpos.x - 1 : Xpos.x), (Xpos.y < 0 ? Xpos.y - 1 : Xpos.y) }
			);
			if (LCollisionInfo.Collision) {//是否碰撞
				LPixel->SetPos(LCollisionInfo.Pos);//设置为碰撞位置
				if (LPixel->DestroyModeCallback(LCollisionInfo.Pos.x, LCollisionInfo.Pos.y, true, LPixel, mTerrain, nullptr)) {
					LPixel->CollisionCallback(LPixel);//调用像素销毁回调函数
					mPixelCollisionS->Delete(i);//销毁对应的像素
					--i;//因为销毁了，I--，这样就不会少遍历对应的像素事件
				}
				continue;
			}

			for (size_t i2 = 0; i2 < mObjectCollisionS->GetNumber(); ++i2)
			{
				LObject = ObjectNumberS[i2];
				LCollisionInfo = LObject->RelativeCoordinateSystemRadialCollisionDetection(Jpos, Xpos);
				if (LCollisionInfo.Collision)
				{
					//玩家受力
					LObject->ForceSolution(
						vec2angle(LCollisionInfo.Pos, LObject->GetAngle()),//力臂
						LPixel->GetSpeedAngle() * (LPixel->GetSpeedFloat() * LPixel->GetQuality()),//力
						TimeStep
					);
					if (LPixel->DestroyModeCallback(LCollisionInfo.Pos.x, LCollisionInfo.Pos.y, true, LPixel, LObject, nullptr)) {
						LPixel->CollisionCallback(LPixel);//调用像素销毁回调函数
						mPixelCollisionS->Delete(i);//销毁对应的像素
						LObject->OutlineCalculate();//重新生成碰撞骨骼点
						--i;//因为销毁了，I--，这样就不会少遍历对应的像素事件
					}
					break;
				}
			}
		}
	}
}