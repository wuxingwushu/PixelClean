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

		~ObjectDecorator() {

		}
		
		void SetQuality(float quality) { mQuality = quality; }

		[[nodiscard]] constexpr float GetQuality() const noexcept { return mQuality; }

		void SetFrictionCoefficient(float frictionCoefficient) { mFrictionCoefficient = frictionCoefficient; }

		[[nodiscard]] constexpr float GetFrictionCoefficient() const noexcept { return mFrictionCoefficient; }



		
		void SetPosX(float x) { mPos.x = x; }

		void SetPosY(float y) { mPos.y = y; }

		void SetPos(glm::vec2 pos) { mPos = pos; }

		[[nodiscard]] float GetPosX() { return mPos.x; }

		[[nodiscard]] float GetPosY() { return mPos.y; }

		[[nodiscard]] glm::vec2 GetPos() { return mPos; }



		void SetAngle(float angle) {
			mAngleFloat = angle;
			mAngle = AngleFloatToAngleVec(angle);
		}

		[[nodiscard]] constexpr float GetAngleX() const noexcept { return mAngle.x; }

		[[nodiscard]] constexpr float GetAngleY() const noexcept { return mAngle.y; }

		[[nodiscard]] constexpr glm::vec2 GetAngle() const noexcept { return mAngle; }

		[[nodiscard]] constexpr float GetAngleFloat() const noexcept { return mAngleFloat; }









		void SetForce(glm::vec2 force) { 
			mForce = force;
			mForceFloat = Modulus(force);
			mForceAngleFloat = EdgeVecToCosAngleFloat(force);
			mForceAngle = AngleFloatToAngleVec(mForceAngleFloat);
		}

		void SetForce(float angle, float Force) {
			mForceFloat = Force;
			mForceAngleFloat = angle;
			mForceAngle = AngleFloatToAngleVec(angle);
			mForce = mForceFloat * mForceAngle;
		}

		[[nodiscard]] constexpr float GetForceX() const noexcept { return mForce.x; }

		[[nodiscard]] constexpr float GetForceY() const noexcept { return mForce.y; }

		[[nodiscard]] constexpr float GetForceAngleX() const noexcept { return mForceAngle.x; }

		[[nodiscard]] constexpr float GetForceAngleY() const noexcept { return mForceAngle.y; }

		[[nodiscard]] constexpr glm::vec2 GetForce() const noexcept { return mForce; }

		[[nodiscard]] constexpr float GetForceFloat() const noexcept { return mForceFloat; }

		[[nodiscard]] constexpr float GetForceAngleFloat() const noexcept { return mForceAngleFloat; }

		[[nodiscard]] constexpr glm::vec2 GetForceAngle() const noexcept { return mForceAngle; }




		


		void SetSpeed(glm::vec2 speed) { 
			mSpeed = speed;
			mSpeedFloat = Modulus(mSpeed);
			mSpeedAngleFloat = EdgeVecToCosAngleFloat(mSpeed);
			mSpeedAngle = AngleFloatToAngleVec(mSpeedAngleFloat);
		}

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

		[[nodiscard]] constexpr float GetSpeedFloat() const noexcept { return mSpeedFloat; }

		[[nodiscard]] constexpr float GetSpeedAngleFloat() const noexcept { return mSpeedAngleFloat; }

		[[nodiscard]] constexpr glm::vec2 GetSpeedAngle() const noexcept { return mSpeedAngle; }



		void SetAccelerationX(float X) {
			mAcceleration.x = X;
		}

		void SetAccelerationY(float Y) {
			mAcceleration.y = Y;
		}

		void SetAcceleration(glm::vec2 Acceleration) {
			mAcceleration = Acceleration;
		}



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
			glm::vec2 Resistance = mSpeedAngle * (mQuality * 9.8f * FrictionCoefficient * mFrictionCoefficient);//受到摩檫力

			/*mPos += mSpeed * TimeStep;
			if (mSpeedFloat != 0) {
				if (mForce.x == 0) {
					if (fabs(mSpeed.x) < 0.2f || (mSpeed.x > 0 != mSpeedBack[0])) {
						SetSpeed({ 0,mSpeed.y });
						mAcceleration.x = 0.0f;
					}
				}
				if (mForce.y == 0) {
					if (fabs(mSpeed.y) < 0.2f || (mSpeed.y > 0 != mSpeedBack[1])) {
						SetSpeed({ mSpeed.x, 0 });
						mAcceleration.y = 0.0f;
					}
				}
				if (mSpeedFloat == 0) {
					return;
				}
				glm::vec2 LForce = mForce - Resistance;
				mAcceleration += (LForce / mQuality) * TimeStep;
				UpDataSpeedBack();
				SetSpeed(mSpeed + (mAcceleration * TimeStep));
			}
			else if(ModulusLength(Resistance) < ModulusLength(mForce))
			{
				glm::vec2 LForce = mForce - Resistance;
				mAcceleration += (LForce / mQuality) * TimeStep;
				UpDataSpeedBack();
				SetSpeed(mSpeed + (mAcceleration * TimeStep));
			}*/
			

			if ((mForce.x != 0.0f) || (mForce.y != 0.0f)) {
				if ((mSpeed.x != 0.0f) || (mSpeed.y != 0.0f)) {
					SetSpeed(mSpeed + (((mForce - Resistance) / mQuality) * TimeStep));
					mPos += mSpeed * TimeStep;
				}
				else
				{
					if (ModulusLength(Resistance) < ModulusLength(mForce))
					{
						SetSpeed(mSpeed + (((mForce - Resistance) / mQuality) * TimeStep));
						mPos += mSpeed * TimeStep;
					}
					else
					{
						return;
					}
				}
			}
			else {
				if (mSpeedFloat < 0.2f) {
					SetSpeed({ 0,0 });
				}
				if ((mSpeed.x != 0.0f) || (mSpeed.y != 0.0f)) {
					SetSpeed(mSpeed - ((Resistance / mQuality) * TimeStep * 1.5f));
					mPos += mSpeed * TimeStep;
				}
				else
				{
					return;
				}
			}
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

	
		float		mQuality = 1.0f;				//质量（KG）
		float		mFrictionCoefficient = 0.6f;	//摩檫力系数
		
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
		
		glm::vec2	mAcceleration{ 0.0f, 0.0f };		//加速度
		float		mAccelerationFloat = 0.0f;			//加速度大小Float
		glm::vec2	mAccelerationAngle{ 0.0f, 0.0f };	//加速度角度
		float		mAccelerationAngleFloat = 0.0f;		//加速度角度Float

		float		mTargetAngle = 0;				//目标角度
		float		mAngleSpeed = 0;				//角速度
		float		mTorque = 0.0f;					//扭矩
		bool		mSpeedBack[2];					//用于判断速度方向反转


		_DestroyModeCallback mDestroyModeCallback = nullptr;
	};

}