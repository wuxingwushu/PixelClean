#pragma once
#include <glm/glm.hpp>
#include "PhysicsCalculate.h"
#include "StructuralComponents.h"

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
			UpDataSpeedBack();
		}

		void SetSpeed(float speed, float angle) { 
			mSpeedFloat = speed;
			mSpeedAngleFloat = angle;
			mSpeedAngle = AngleFloatToAngleVec(mSpeedAngleFloat);
			mSpeed = mSpeedAngle * mSpeedFloat;
			UpDataSpeedBack();
		}


		[[nodiscard]] constexpr float GetSpeedX() const noexcept { return mSpeed.x; }

		[[nodiscard]] constexpr float GetSpeedY() const noexcept { return mSpeed.y; }

		[[nodiscard]] constexpr float GetSpeedAngleX() const noexcept { return mSpeedAngle.x; }

		[[nodiscard]] constexpr float GetSpeedAngleY() const noexcept { return mSpeedAngle.y; }

		[[nodiscard]] constexpr glm::vec2 GetSpeed() const noexcept { return mSpeed; }

		[[nodiscard]] constexpr float GetSpeedFloat() const noexcept { return mSpeedFloat; }

		[[nodiscard]] constexpr float GetSpeedAngleFloat() const noexcept { return mSpeedAngleFloat; }

		[[nodiscard]] constexpr glm::vec2 GetSpeedAngle() const noexcept { return mSpeedAngle; }



		//破坏的回调函数
		void SetDestroyModeCallback(_DestroyModeCallback DestroyModeCallback) {
			mDestroyModeCallback = DestroyModeCallback;
		}

		void DestroyModeCallback(int x, int y, void* mclass, bool Bool) {
			if (mDestroyModeCallback != nullptr) {
				mDestroyModeCallback(x, y, mclass, Bool);
			}
		}




		//帧时间步长模拟
		void FrameTimeStep(float TimeStep, float FrictionCoefficient){
			glm::vec2 Resistance = mSpeedAngle * (mQuality * 9.8f * FrictionCoefficient * mFrictionCoefficient);
			if ((mForce.x != 0.0f) || (mForce.y != 0.0f)) {
				if ((mSpeed.x != 0.0f) || (mSpeed.y != 0.0f)) {
					SetSpeed(mSpeed + (((mForce - Resistance) / mQuality) * TimeStep));
					mPos += mSpeed * TimeStep;
					SpeedReversalJudge();
				}
				else
				{
					if (ModulusLength(Resistance) < ModulusLength(mForce))
					{
						SetSpeed(mSpeed + (((mForce - Resistance) / mQuality) * TimeStep));
						mPos += mSpeed * TimeStep;
						SpeedReversalJudge();
					}
					else
					{
						return;
					}
				}
			}
			else {
				if ((mSpeed.x != 0.0f) || (mSpeed.y != 0.0f)) {
					SetSpeed(mSpeed - ((Resistance / mQuality) * TimeStep));
					mPos += mSpeed * TimeStep;
					SpeedReversalJudge();
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

	
		float		mQuality = 1.0f;				//质量（KG）
		float		mFrictionCoefficient = 0.6f;	//摩檫力系数
		
		glm::vec2	mPos{ 0.0f, 0.0f };				//位置
		glm::vec2	mAngle{ 0.0f, 0.0f };			//朝向角度
		float		mAngleFloat = 0.0f;				//朝向角度Float
		
		glm::vec2	mForce{ 0.0f, 0.0f };			//受力
		float		mForceFloat = 0.0f;				//受力Float
		glm::vec2	mForceAngle{ 0.0f, 0.0f };		//受力角度
		float		mForceAngleFloat = 0.0f;		//受力角度Float

		glm::vec2	mSpeed{ 0.0f, 0.0f };			//速度
		float		mSpeedFloat = 0.0f;				//速度Float
		glm::vec2	mSpeedAngle{ 0.0f, 0.0f };		//速度角度
		float		mSpeedAngleFloat = 0.0f;		//速度角度Float
		bool		mSpeedBack[2];					//用于判断速度方向反转

		_DestroyModeCallback mDestroyModeCallback = nullptr;
	};

}