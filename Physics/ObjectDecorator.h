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
		
		//设置质量
		inline void SetQuality(float quality) noexcept { mQuality = quality; }

		[[nodiscard]] inline float GetQuality() const noexcept { return mQuality; }

		inline void SetFrictionCoefficient(float frictionCoefficient) noexcept { mFrictionCoefficient = frictionCoefficient; }

		[[nodiscard]] inline float GetFrictionCoefficient() const noexcept { return mFrictionCoefficient; }

		[[nodiscard]] inline glm::vec2 GetBarycenter() noexcept { return mBarycenter; }



		
		inline void SetPosX(float x) noexcept { mPos.x = x; }

		inline void SetPosY(float y) noexcept { mPos.y = y; }

		inline void SetPos(glm::vec2 pos) noexcept { mPos = pos; }

		[[nodiscard]] inline float GetPosX() noexcept { return mPos.x; }

		[[nodiscard]] inline float GetPosY() noexcept { return mPos.y; }

		[[nodiscard]] inline glm::vec2 GetPos() const noexcept { return mPos; }
		[[nodiscard]] constexpr inline glm::vec2* GetPosPointer() noexcept { return &mPos; }



		inline void SetAngle(float angle) noexcept {
			mAngleFloat = angle;
			mAngle = AngleFloatToAngleVec(angle);
		}

		[[nodiscard]] inline  float GetAngleX() const noexcept { return mAngle.x; }

		[[nodiscard]] inline  float GetAngleY() const noexcept { return mAngle.y; }

		[[nodiscard]] inline  glm::vec2 GetAngle() const noexcept { return mAngle; }

		[[nodiscard]] inline  float GetAngleFloat() const noexcept { return mAngleFloat; }








		//设置受力
		inline void SetForce(glm::vec2 force) {
			mForce = force;
			mForceFloat = Modulus(force);
			mForceAngleFloat = EdgeVecToCosAngleFloat(force);
			mForceAngle = AngleFloatToAngleVec(mForceAngleFloat);
		}

		//设置受力
		inline void SetForce(float angle, float Force) {
			mForceFloat = Force;
			mForceAngleFloat = angle;
			mForceAngle = AngleFloatToAngleVec(angle);
			mForce = mForceFloat * mForceAngle;
		}

		//受到的力
		inline void SufferForce(glm::vec2 force) {
			mForce += force;
			mForceFloat = Modulus(force);
			mForceAngleFloat = EdgeVecToCosAngleFloat(force);
			mForceAngle = AngleFloatToAngleVec(mForceAngleFloat);
		}

		//受到的力
		inline void SufferForce(float angle, float Force) {
			mForce += AngleFloatToAngleVec(angle) * Force;
			mForceFloat = Modulus(mForce);
			mForceAngleFloat = EdgeVecToCosAngleFloat(mForce);
			mForceAngle = AngleFloatToAngleVec(mForceAngleFloat);
		}

		[[nodiscard]] inline  float GetForceX() const noexcept { return mForce.x; }

		[[nodiscard]] inline  float GetForceY() const noexcept { return mForce.y; }

		[[nodiscard]] inline  float GetForceAngleX() const noexcept { return mForceAngle.x; }

		[[nodiscard]] inline  float GetForceAngleY() const noexcept { return mForceAngle.y; }

		[[nodiscard]] inline  glm::vec2 GetForce() const noexcept { return mForce; }
		[[nodiscard]] constexpr inline  glm::vec2* GetForcePointer() noexcept { return &mForce; }

		[[nodiscard]] inline  float GetForceFloat() const noexcept { return mForceFloat; }

		[[nodiscard]] inline  float GetForceAngleFloat() const noexcept { return mForceAngleFloat; }

		[[nodiscard]] inline  glm::vec2 GetForceAngle() const noexcept { return mForceAngle; }




		

		//设置速度
		inline void SetSpeed(glm::vec2 speed) {
			mSpeed = speed;
			mSpeedFloat = Modulus(mSpeed);
			mSpeedAngleFloat = EdgeVecToCosAngleFloat(mSpeed);
			mSpeedAngle = AngleFloatToAngleVec(mSpeedAngleFloat);
		}

		//设置速度
		inline void SetSpeed(float speed, float angle) {
			mSpeedFloat = speed;
			mSpeedAngleFloat = angle;
			mSpeedAngle = AngleFloatToAngleVec(mSpeedAngleFloat);
			mSpeed = mSpeedAngle * mSpeedFloat;
		}

		//期望速度
		inline void ExpectSpeed(float speed, float angle, float time) {
			glm::vec2 Lspeed = speed * AngleFloatToAngleVec(angle);
			Lspeed -= mSpeed;
			mSpeed += Lspeed * time;
			mSpeedFloat = Modulus(mSpeed);
			mSpeedAngleFloat = EdgeVecToCosAngleFloat(mSpeed);
			mSpeedAngle = AngleFloatToAngleVec(mSpeedAngleFloat);
		}


		[[nodiscard]] inline float GetSpeedX() const noexcept { return mSpeed.x; }

		[[nodiscard]] inline float GetSpeedY() const noexcept { return mSpeed.y; }

		[[nodiscard]] inline float GetSpeedAngleX() const noexcept { return mSpeedAngle.x; }

		[[nodiscard]] inline float GetSpeedAngleY() const noexcept { return mSpeedAngle.y; }

		[[nodiscard]] inline glm::vec2 GetSpeed() const noexcept { return mSpeed; }
		[[nodiscard]] constexpr inline  glm::vec2* GetSpeedPointer() noexcept { return &mSpeed; }

		[[nodiscard]] inline float GetSpeedFloat() const noexcept { return mSpeedFloat; }

		[[nodiscard]] inline float GetSpeedAngleFloat() const noexcept { return mSpeedAngleFloat; }

		[[nodiscard]] inline glm::vec2 GetSpeedAngle() const noexcept { return mSpeedAngle; }



		//破坏的回调函数
		inline void SetDestroyModeCallback(_DestroyModeCallback DestroyModeCallback) noexcept {
			mDestroyModeCallback = DestroyModeCallback;
		}

		inline bool DestroyModeCallback(int x, int y, bool Bool, float Angle, ObjectDecorator* Object, GridDecorator* Grid, void* Data) noexcept {
			if (mDestroyModeCallback != nullptr) {
				return mDestroyModeCallback(x, y, Bool, Angle, Object, Grid, Data);
			}
		}




		//帧时间步长模拟
		void FrameTimeStep(float TimeStep, float FrictionCoefficient){
			glm::vec2 Resistance = { 0, 0};
			if (mFrictionCoefficient != 0) {
				Resistance = mSpeedAngle * ((mQuality * 9.8f * FrictionCoefficient * mFrictionCoefficient) + (mSpeedFloat * mSpeedFloat * 0.01f));//受到摩檫力
			}

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
		inline void UpDataSpeedBack() noexcept {
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

		inline void SetTorque(float Torque) noexcept { mTorque = Torque; }

		inline void SufferTorque(float Torque) noexcept { mTorque += Torque; }


	
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