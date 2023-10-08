#pragma once
#include <glm/glm.hpp>
#include "PhysicsCalculate.h"
#include "StructuralComponents.h"
#include "Callback.h"
#include <iostream>

namespace SquarePhysics {

	class ObjectDecorator //装饰器模式
	{
	public:

		ObjectDecorator() {

		}

		virtual ~ObjectDecorator() {

		}
		
		void SetQuality(float quality) { mQuality = quality; }

		[[nodiscard]] constexpr float GetQuality() const noexcept { return mQuality; }

		void SetFrictionCoefficient(float frictionCoefficient) { mFrictionCoefficient = frictionCoefficient; }

		[[nodiscard]] constexpr float GetFrictionCoefficient() const noexcept { return mFrictionCoefficient; }

		[[nodiscard]] glm::vec2 GetBarycenter() { return mBarycenter; }



		
		void SetPosX(float x) { mPos.x = x; }

		void SetPosY(float y) { mPos.y = y; }

		void SetPos(glm::vec2 pos) { mPos = pos; }

		[[nodiscard]] float GetPosX() { return mPos.x; }

		[[nodiscard]] float GetPosY() { return mPos.y; }

		[[nodiscard]] constexpr glm::vec2 GetPos() const noexcept { return mPos; }
		[[nodiscard]] constexpr glm::vec2* GetPosPointer() noexcept { return &mPos; }



		void SetAngle(float angle) {
			mAngleFloat = angle;
			mAngle = AngleFloatToAngleVec(angle);
		}

		[[nodiscard]] constexpr float GetAngleX() const noexcept { return mAngle.x; }

		[[nodiscard]] constexpr float GetAngleY() const noexcept { return mAngle.y; }

		[[nodiscard]] constexpr glm::vec2 GetAngle() const noexcept { return mAngle; }

		[[nodiscard]] constexpr float GetAngleFloat() const noexcept { return mAngleFloat; }








		//设置受力
		void SetForce(glm::vec2 force) { 
			mForce = force;
			mForceFloat = Modulus(force);
			mForceAngleFloat = EdgeVecToCosAngleFloat(force);
			mForceAngle = AngleFloatToAngleVec(mForceAngleFloat);
		}

		//设置受力
		void SetForce(float angle, float Force) {
			mForceFloat = Force;
			mForceAngleFloat = angle;
			mForceAngle = AngleFloatToAngleVec(angle);
			mForce = mForceFloat * mForceAngle;
		}

		//受到的力
		void SufferForce(glm::vec2 force) {
			mForce += force;
			mForceFloat = Modulus(force);
			mForceAngleFloat = EdgeVecToCosAngleFloat(force);
			mForceAngle = AngleFloatToAngleVec(mForceAngleFloat);
		}

		//受到的力
		void SufferForce(float angle, float Force) {
			mForce += AngleFloatToAngleVec(angle) * Force;
			mForceFloat = Modulus(mForce);
			mForceAngleFloat = EdgeVecToCosAngleFloat(mForce);
			mForceAngle = AngleFloatToAngleVec(mForceAngleFloat);
		}

		[[nodiscard]] constexpr float GetForceX() const noexcept { return mForce.x; }

		[[nodiscard]] constexpr float GetForceY() const noexcept { return mForce.y; }

		[[nodiscard]] constexpr float GetForceAngleX() const noexcept { return mForceAngle.x; }

		[[nodiscard]] constexpr float GetForceAngleY() const noexcept { return mForceAngle.y; }

		[[nodiscard]] constexpr glm::vec2 GetForce() const noexcept { return mForce; }
		[[nodiscard]] constexpr glm::vec2* GetForcePointer() noexcept { return &mForce; }

		[[nodiscard]] constexpr float GetForceFloat() const noexcept { return mForceFloat; }

		[[nodiscard]] constexpr float GetForceAngleFloat() const noexcept { return mForceAngleFloat; }

		[[nodiscard]] constexpr glm::vec2 GetForceAngle() const noexcept { return mForceAngle; }




		

		//设置速度
		void SetSpeed(glm::vec2 speed) { 
			mSpeed = speed;
			mSpeedFloat = Modulus(mSpeed);
			mSpeedAngleFloat = EdgeVecToCosAngleFloat(mSpeed);
			mSpeedAngle = AngleFloatToAngleVec(mSpeedAngleFloat);
		}

		//设置速度
		void SetSpeed(float speed, float angle) { 
			mSpeedFloat = speed;
			mSpeedAngleFloat = angle;
			mSpeedAngle = AngleFloatToAngleVec(mSpeedAngleFloat);
			mSpeed = mSpeedAngle * mSpeedFloat;
		}


		[[nodiscard]] constexpr float GetSpeedX() const noexcept { return mSpeed.x; }

		[[nodiscard]] constexpr float GetSpeedY() const noexcept { return mSpeed.y; }

		[[nodiscard]] constexpr float GetSpeedAngleX() const noexcept { return mSpeedAngle.x; }

		[[nodiscard]] constexpr float GetSpeedAngleY() const noexcept { return mSpeedAngle.y; }

		[[nodiscard]] constexpr glm::vec2 GetSpeed() const noexcept { return mSpeed; }
		[[nodiscard]] constexpr glm::vec2* GetSpeedPointer() noexcept { return &mSpeed; }

		[[nodiscard]] constexpr float GetSpeedFloat() const noexcept { return mSpeedFloat; }

		[[nodiscard]] constexpr float GetSpeedAngleFloat() const noexcept { return mSpeedAngleFloat; }

		[[nodiscard]] constexpr glm::vec2 GetSpeedAngle() const noexcept { return mSpeedAngle; }



		//破坏的回调函数
		void SetDestroyModeCallback(_DestroyModeCallback DestroyModeCallback) {
			mDestroyModeCallback = DestroyModeCallback;
		}

		bool DestroyModeCallback(int x, int y, bool Bool, ObjectDecorator* Object, GridDecorator* Grid, void* Data) {
			if (mDestroyModeCallback != nullptr) {
				return mDestroyModeCallback(x, y, Bool, Object, Grid, Data);
			}
		}




		//帧时间步长模拟
		void FrameTimeStep(float TimeStep, float FrictionCoefficient){
			glm::vec2 Resistance = mSpeedAngle * ((mQuality * 9.8f * FrictionCoefficient * mFrictionCoefficient) + (mSpeedFloat * mSpeedFloat * 0.01f));//受到摩檫力

			mPos += mSpeed * TimeStep;//计算移动距离
			//解算速度
			if (mSpeedFloat != 0) {//速度不为 O
				glm::vec2 LForce = mForce - Resistance;
				glm::vec2 mAcceleration = (LForce / mQuality);
				SetSpeed(mSpeed + (mAcceleration * TimeStep));
			}
			else if(ModulusLength(Resistance) < ModulusLength(mForce))
			{
				glm::vec2 LForce = mForce - Resistance;
				glm::vec2 mAcceleration = (LForce / mQuality);
				SetSpeed(mSpeed + (mAcceleration * TimeStep));
			}
			SetForce({ 0,0 });

			float AngleAcceleration = 0;
			float LTorque = (mQuality * 9.8f * FrictionCoefficient * mFrictionCoefficient) * mRadius + (mAngleSpeed * mAngleSpeed * 0.1f);
			mAngleFloat += mAngleSpeed * TimeStep;//计算旋转角度
			SetAngle(mAngleFloat);
			//解算角速度
			if (mAngleSpeed != 0) {//角速度不为 O
				int ZF = (mAngleSpeed < 0 ? 1 : -1);
				AngleAcceleration += ((LTorque * ZF + mTorque) / mQuality);
				mAngleSpeed += AngleAcceleration * TimeStep;
				if ((mAngleSpeed < 0 ? 1 : -1) != ZF) { mAngleSpeed = 0; }
			}
			else if (LTorque < fabs(mTorque) && mTorque != 0)
			{
				if (mTorque < 0) {
					mTorque += LTorque;
				}
				else {
					mTorque -= LTorque;
				}
				AngleAcceleration += mTorque / mQuality;
				mAngleSpeed += AngleAcceleration * TimeStep;
			}
			mTorque = 0;
		}

		

		//和上一时刻速度的Bool进行对比，判断速度是否要取反。
		void SpeedReversalJudge() {
			bool SpeedX = (mSpeed.x > 0.0f);
			bool SpeedY = (mSpeed.y > 0.0f);
			if (SpeedX != mSpeedBack[0])
			{
				mSpeed.x = 0.0f;
				mSpeedAngle.x = 0.0f;
				mSpeedBack[0] = SpeedX;
			}

			if (SpeedY != mSpeedBack[1])
			{
				mSpeed.y = 0.0f;
				mSpeedAngle.y = 0.0f;
				mSpeedBack[1] = SpeedY;
			}
			SetSpeed(mSpeed);
		}

		//更新各个方向速度的Bool
		void UpDataSpeedBack() {
			mSpeedBack[0] = (mSpeed.x > 0.0f);
			mSpeedBack[1] = (mSpeed.y > 0.0f);
		}

		//期望玩家物理角度
		void PlayerTargetAngle(float TargetAngle) {
			mTargetAngle = TargetAngle;
			bool B = false;
			if (mAngleFloat == 3.14159f) {
				B = true;
			}
			TargetAngle = TargetAngle - mAngleFloat;
			if (TargetAngle == 0) {
				return;
			}

			float AngleForce = sin(TargetAngle) * 0.05f;
			if (cos(TargetAngle) < -0.1) {
				if (AngleForce < 0) {
					AngleForce += cos(TargetAngle) * 0.5f;
				}
				else {
					AngleForce -= cos(TargetAngle) * 0.5f;
				}
				
			}
			mAngleFloat += (AngleForce == 0.0f ? 0.1f : AngleForce);
			if (B) {
				std::cout << (AngleForce == 0.0f ? 0.1f : AngleForce) << std::endl;
			}
			mAngle = AngleFloatToAngleVec(mAngleFloat);
		}

		//受力解算
		void ForceSolution(glm::vec2 ArmOfForce, glm::vec2 Force, float tiem) {
			DecompositionForce LSDecompositionForce = CalculateDecompositionForce(ArmOfForce, Force);
			float Angle = EdgeVecToCosAngleFloat(ArmOfForce) - EdgeVecToCosAngleFloat(LSDecompositionForce.Vertical);
			float angletime = Modulus(ArmOfForce) * Modulus(LSDecompositionForce.Vertical);
			angletime = angletime * (sin(Angle) < 0 ? -1 : 1);
			SufferTorque(angletime);
			SufferForce(LSDecompositionForce.Parallel);
		}

		void SetTorque(float Torque) { mTorque = Torque; }

		void SufferTorque(float Torque) { mTorque += Torque; }


	
		float		mQuality = 1.0f;				//质量（KG）
		float		mFrictionCoefficient = 0.6f;	//摩檫力系数
		glm::vec2	mBarycenter{ 0.0f, 0.0f };		//重心
		
		glm::vec2	mPos{ 0.0f, 0.0f };				//位置
		glm::vec2	mAngle{ 0.0f, 0.0f };			//朝向角度
		float		mAngleFloat = 0.0f;				//朝向角度Float
		
		glm::vec2	mForce{ 0.0f, 0.0f };			//受力
		float		mForceFloat = 0.0f;				//受力大小Float
		glm::vec2	mForceAngle{ 0.0f, 0.0f };		//受力角度
		float		mForceAngleFloat = 0.0f;		//受力角度Float

		glm::vec2	mSpeed{ 0.0f, 0.0f };			//速度
		float		mSpeedFloat = 0.0f;				//速度大小Float
		glm::vec2	mSpeedAngle{ 0.0f, 0.0f };		//速度角度
		float		mSpeedAngleFloat = 0.0f;		//速度角度Float

		float		mAngleSpeed = 0.0f;				//角速度
		float		mTorque = 0.0f;					//扭矩
		float		mRadius = 8.0f;					//半径

		float		mTargetAngle = 0;				//目标角度
		bool		mSpeedBack[2]{};				//用于判断速度方向反转


		_DestroyModeCallback mDestroyModeCallback = nullptr;
	};

}