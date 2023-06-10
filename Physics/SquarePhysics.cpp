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
	}

	void SquarePhysics::PhysicsSimulation(float TimeStep) {
		ObjectCollision** ObjectNumberS = mObjectCollisionS->Data();
		PixelCollision** PixelCollisionS = mPixelCollisionS->Data();
		PixelCollision* LPixel;
		ObjectCollision* LObject;
		glm::dvec2 pos,dianpos,dian;
		glm::dvec2 dianji[256], posL, pian;
		glm::ivec2 chapos;
		int XX, YY;
		for (size_t i = 0; i < mPixelCollisionS->GetNumber(); i++)
		{
			LPixel = PixelCollisionS[i];
			pos = LPixel->GetPos();
			LPixel->FrameTimeStep(TimeStep, 0);
			dian = LPixel->GetPos();
			dianpos = mFixedSizeTerrain->RadialCollisionDetection(pos, dian);
			XX = dianpos.x;
			YY = dianpos.y;
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
			/*for (size_t iFF = 0; iFF < monicishu; iFF++)
			{
				pos = LPixel->GetPos();
				XX = pos.x;
				YY = pos.y;
				if (mFixedSizeTerrain->CrossingTheBoundary(XX, YY))
				{
					LPixel->CollisionCallback(LPixel);
					mPixelCollisionS->Delete(i);
					break;
				}
				LPixel->FrameTimeStep(TimeStep, mFixedSizeTerrain->GetFixedFrictionCoefficient(XX, YY));
				if (mFixedSizeTerrain->GetFixedCollisionBool(XX, YY)) {
					LPixel->CollisionCallback(LPixel);
					mPixelCollisionS->Delete(i);
					mFixedSizeTerrain->CollisionCallback(XX, YY);
					mFixedSizeTerrain->SetFixedCollisionBool(XX, YY);
					break;
				}
			}*/
		}


		unsigned int monicishu = 60;
		TimeStep = TimeStep / monicishu;
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
				LObject->SetPos(glm::vec2(pos) + LObject->GetSpeed() * TimeStep);
				XX = pos.x;
				YY = pos.y;
				//LObject->FrameTimeStep(TimeStep, mFixedSizeTerrain->GetFixedFrictionCoefficient(XX, YY));
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
						//LObject->SetSpeed(0);
						//LObject->SetForce({0,0});
						LObject->SetPos(SquareToDrop(-0.001f, 1.001f, -0.001f, 1.001f, { (dianpos.x - 0.0f) - XX, (dianpos.y - 0.0f) - YY }, { -dian.x,-dian.y }) + pos);
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

	//vec2旋转
	glm::dvec2 vec2angle(glm::dvec2 pos, double angle) {
		double cosangle = cos(angle);
		double sinangle = sin(angle);
		return glm::dvec2((pos.x * cosangle) - (pos.y * sinangle), (pos.x * sinangle) + (pos.y * cosangle));
	}

	//vec2旋转
	glm::dvec2 vec2angle(glm::dvec2 pos, glm::dvec2 angle) {
		return glm::dvec2((pos.x * angle.x) - (pos.y * angle.y), (pos.x * angle.y) + (pos.y * angle.x));
	}

	//正方形和正方形的碰撞检测（A为静态刚体，B为动态刚体）
	glm::dvec2 SquareToSquare(glm::vec2 posA, unsigned int dA, double angleA, glm::vec2 posB, unsigned int dB, double angleB) {
		double rA = double(dA) / 2;//边长的一半
		double rB = double(dB) / 2;
		glm::dvec2 Pos{ posB.x - posA.x, posB.y - posA.y };//两个正方形的  X  Y  的间距
		double R = sqrt(pow(Pos.x, 2) + pow(Pos.y, 2));//两个正方形的距离
		if (R > ((rA + rB) * 1.42f)) {
			return glm::dvec2(0, 0);
		}
		double Angle = angleB - angleA;//两个正方形的角度差
		double cosangle = cos(Angle);
		double sinangle = sin(Angle);
		//if ((abs(sinangle) >= 0.997) || (abs(cosangle) >= 0.997))//当两个正方形边贴着边时
		//{
		//	if (R < (rA + rB)) {
		//		double cha = (rA + rB) - R;
		//		return glm::vec2(cha * (Pos.x / R), cha * (Pos.y / R));
		//	}
		//}
		

		double A1 = - rA;
		double A2 = + rA;
		double B1 = - rA;
		double B2 = + rA;
		double Acosangle = cos(angleA);
		double Asinangle = sin(angleA);
		double Bcosangle = cos(angleB);
		double Bsinangle = sin(angleB);

		glm::dvec2 QPYpos{ 0.0f,0.0f };//补偿距离
		glm::dvec2 QPos = vec2angle(Pos, { Acosangle, -Asinangle });//以 A 为坐标系，B 正方形的四个点进行判断是否在内
		QPYpos += SquareToDrop(A1, A2, B1, B2, (QPos + QPYpos + vec2angle(glm::vec2(-rB, -rB), {cosangle, sinangle})), QPos);
		QPYpos += SquareToDrop(A1, A2, B1, B2, (QPos + QPYpos + vec2angle(glm::vec2(+rB, -rB), {cosangle, sinangle})), QPos);
		QPYpos += SquareToDrop(A1, A2, B1, B2, (QPos + QPYpos + vec2angle(glm::vec2(-rB, +rB), {cosangle, sinangle})), QPos);
		QPYpos += SquareToDrop(A1, A2, B1, B2, (QPos + QPYpos + vec2angle(glm::vec2(+rB, +rB), {cosangle, sinangle})), QPos);

		QPYpos = vec2angle(QPYpos, { Acosangle, Asinangle });

		A1 = - rB;
		A2 = + rB;
		B1 = - rB;
		B2 = + rB;
		sinangle = -sinangle;

		glm::dvec2 HPYpos{ 0.0f,0.0f };//补偿距离
		glm::dvec2 HPos = vec2angle(-(QPYpos + Pos), { Bcosangle, -Bsinangle });//以 B 为坐标系，A 正方形的四个点进行判断是否在内
		HPYpos -= SquareToDrop(A1, A2, B1, B2, (HPos + HPYpos + vec2angle(glm::vec2(-rA, -rA), {cosangle, sinangle})), HPos);
		HPYpos -= SquareToDrop(A1, A2, B1, B2, (HPos + HPYpos + vec2angle(glm::vec2(+rA, -rA), {cosangle, sinangle})), HPos);
		HPYpos -= SquareToDrop(A1, A2, B1, B2, (HPos + HPYpos + vec2angle(glm::vec2(-rA, +rA), {cosangle, sinangle})), HPos);
		HPYpos -= SquareToDrop(A1, A2, B1, B2, (HPos + HPYpos + vec2angle(glm::vec2(+rA, +rA), {cosangle, sinangle})), HPos);

		return QPYpos + vec2angle(HPYpos, { Bcosangle, Bsinangle });//返回需要矫正的距离
	}

	//正方形和点的碰撞检测
	glm::dvec2 SquareToDrop(float A1, float A2, float B1, float B2, glm::dvec2 Drop, glm::dvec2 PY) {
		glm::dvec2 PYpos{ 0.0f,0.0f };
		if (((Drop.x >= A1) && (Drop.x <= A2)) && ((Drop.y >= B1) && (Drop.y <= B2))) {//判断这个点是否在另外一个正方形里面
			if (PY.x > 0) {
				PYpos.x = A2 - Drop.x;
			}
			else {
				PY.x = -PY.x;
				PYpos.x = A1 - Drop.x;
			}

			if (PY.y > 0) {
				PYpos.y = B2 - Drop.y;
			}
			else {
				PY.y = -PY.y;
				PYpos.y = B1 - Drop.y;
			}

			if (PY.x < PY.y) {
				PYpos.x = 0;
			}
			else {
				PYpos.y = 0;
			}
		}
		return PYpos;
	}
}