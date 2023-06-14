#include "SquarePhysics.h"

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
		for (size_t i = 0; i < mObjectCollisionS->GetNumber(); i++)
		{
			delete mObjectCollisionS->GetData(i);
		}
		for (size_t i = 0; i < mPixelCollisionS->GetNumber(); i++)
		{
			delete mPixelCollisionS->GetData(i);
		}
		delete mObjectCollisionS;
		delete mPixelCollisionS;
	}

	ObjectCollision* SquarePhysics::ObjectS_PixelCollision(glm::ivec2 pos) {
		ObjectCollision** ObjectNumberS = mObjectCollisionS->Data();
		for (size_t i = 0; i < mObjectCollisionS->GetNumber(); i++)
		{
			if (ObjectNumberS[i]->PixelCollision(pos)) {
				return ObjectNumberS[i];
			}
		}
		return nullptr;
	}

	void SquarePhysics::PhysicsSimulation(float TimeStep) {
		ObjectCollision** ObjectNumberS = mObjectCollisionS->Data();
		PixelCollision** PixelCollisionS = mPixelCollisionS->Data();
		PixelCollision* LPixel;
		ObjectCollision* LObject;
		glm::dvec2 pos,dianpos,dian, zhuang, speed;
		glm::ivec2 Start, End;
		int XX, YY;
		//射线
		int dx, dy, sx, sy, err, e2;
		for (size_t i = 0; i < mPixelCollisionS->GetNumber(); i++)
		{
			LPixel = PixelCollisionS[i];
			pos = LPixel->GetPos();
			LPixel->FrameTimeStep(TimeStep, 0);
			dian = LPixel->GetPos();
			//dianpos = mFixedSizeTerrain->RadialCollisionDetection(pos, dian);
			End = dian;
			Start = pos;

			dx = abs(End.x - Start.x);
			dy = abs(End.y - Start.y);
			sx = (Start.x < End.x) ? 1 : -1;
			sy = (Start.y < End.y) ? 1 : -1;
			err = dx - dy;
			e2;
			LObject = nullptr;
			while (true) {
				if ((Start.x == End.x && Start.y == End.y) || mFixedSizeTerrain->GetFixedCollisionBool(Start.x, Start.y)) {
					break;
				}
				LObject = ObjectS_PixelCollision(Start);
				if (LObject != nullptr)
				{
					LPixel->CollisionCallback(LPixel);
					mPixelCollisionS->Delete(i);
					break;
				}
				e2 = 2 * err;
				if (e2 > -dy) {
					err -= dy;
					Start.x += sx;
				}
				if ((Start.x == End.x && Start.y == End.y) || mFixedSizeTerrain->GetFixedCollisionBool(Start.x, Start.y)) {
					break;
				}
				LObject = ObjectS_PixelCollision(Start);
				if (LObject != nullptr)
				{
					LPixel->CollisionCallback(LPixel);
					mPixelCollisionS->Delete(i);
					break;
				}
				if (e2 < dx) {
					err += dx;
					Start.y += sy;
				}
			}


			if (LObject != nullptr)
			{
				continue;
			}
			XX = Start.x;
			YY = Start.y;
			if (mFixedSizeTerrain->CrossingTheBoundary(XX, YY))
			{
				LPixel->CollisionCallback(LPixel);
				mPixelCollisionS->Delete(i);
				break;
			}
			if (mFixedSizeTerrain->GetFixedCollisionBool(XX, YY)) {
				LPixel->CollisionCallback(LPixel);
				mPixelCollisionS->Delete(i);
				mFixedSizeTerrain->CollisionCallback(XX, YY);
				mFixedSizeTerrain->SetFixedCollisionBool(XX, YY);
				break;
			}
		}


		unsigned int monicishu = 60;
		TimeStep = TimeStep / monicishu;
		//glm::dvec2 dianji[256], posL, pian;
		//glm::ivec2 chapos;
		for (size_t i = 0; i < mObjectCollisionS->GetNumber(); i++)
		{
			/*LObject = ObjectNumberS[i];
			pos = LObject->GetPos();
			XX = pos.x;	
			YY = pos.y;
			for (size_t iDD = 0; iDD < LObject->GetOutlinePointSize(); iDD++) {
				dianji[iDD] = vec2angle(LObject->GetOutlinePointSet(iDD), LObject->GetAngle());
			}
			LObject->FrameTimeStep(TimeStep, mFixedSizeTerrain->GetFixedFrictionCoefficient(XX, YY));
			posL = glm::dvec2(LObject->GetPos());
			pian = posL - pos;
			for (size_t iDD = 0; iDD < LObject->GetOutlinePointSize(); iDD++) {
				dianpos = mFixedSizeTerrain->RadialCollisionDetection(dianji[iDD], vec2angle(LObject->GetOutlinePointSet(iDD), LObject->GetAngle()));
				dian = dianpos - dianji[iDD];
				chapos = glm::ivec2(dian);
				if (chapos != glm::ivec2(pian)) {
					if (fabs(XX) < abs(chapos.x))
					{
						XX = chapos.x;
					}
					if (fabs(YY) < abs(chapos.y))
					{
						YY = chapos.y;
					}
				}
			}*/

			LObject = ObjectNumberS[i];
			for (size_t iFF = 0; iFF < monicishu; iFF++)
			{
				pos = LObject->GetPos();
				//LObject->SetPos(glm::vec2(pos) + LObject->GetSpeed() * TimeStep);
				XX = pos.x;
				YY = pos.y;
				LObject->FrameTimeStep(TimeStep, mFixedSizeTerrain->GetFixedFrictionCoefficient(XX, YY));
				for (size_t iDD = 0; iDD < LObject->GetOutlinePointSize(); iDD++)
				{
					dian = vec2angle(LObject->GetOutlinePointSet(iDD), LObject->GetAngle());
					dianpos = pos + dian;
					XX = dianpos.x;
					YY = dianpos.y;
					if (XX < 0)
					{
						XX--;
					}
					if (YY < 0)
					{
						YY--;
					}
					if (mFixedSizeTerrain->GetFixedCollisionBool(XX, YY)) {
						zhuang = SquareToDrop(-0.001f, 1.001f, -0.001f, 1.001f, { (dianpos.x - 0.0f) - XX, (dianpos.y - 0.0f) - YY }, { -dian.x,-dian.y });
						speed = LObject->GetSpeed();
						if (ModulusLength(zhuang) != 0) {
							if (pow(zhuang.x, 2) > pow(zhuang.y, 2)) {
								speed.x = 0;
							}
							else {
								speed.y = 0;
							}
						}
						LObject->SetSpeed(speed);
						LObject->SetPos(zhuang + pos);
					}
				}
			}
			
		}
	}


	bool SquarePhysics::PixelCollisionPhysicsSimulation(float TimeStep, PixelCollision* LPixelCollision) {
		glm::vec2 pos;
		int XX, YY;
		TimeStep = TimeStep / 60.0f;
		for (size_t i = 0; i < 60; i++)
		{
			pos = LPixelCollision->GetPos();
			XX = pos.x;
			YY = pos.y;
			LPixelCollision->FrameTimeStep(TimeStep, mFixedSizeTerrain->GetFixedFrictionCoefficient(XX, YY));
			if (mFixedSizeTerrain->GetFixedCollisionBool(XX, YY)) {
				return true;
			}
		}
		return false;
	}

	
}