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
		inline void SetQuality(double quality) noexcept { mQuality = quality; }

		[[nodiscard]] inline double GetQuality() const noexcept { return mQuality; }

		inline void SetFrictionCoefficient(double frictionCoefficient) noexcept { mFrictionCoefficient = frictionCoefficient; }

		[[nodiscard]] inline double GetFrictionCoefficient() const noexcept { return mFrictionCoefficient; }

		[[nodiscard]] inline glm::dvec2 GetBarycenter() noexcept { return mBarycenter; }



		
		inline void SetPosX(double x) noexcept { mPos.x = x; }

		inline void SetPosY(double y) noexcept { mPos.y = y; }

		inline void SetPos(glm::dvec2 pos) noexcept { mPos = pos; }

		[[nodiscard]] inline double GetPosX() noexcept { return mPos.x; }

		[[nodiscard]] inline double GetPosY() noexcept { return mPos.y; }

		[[nodiscard]] inline glm::dvec2 GetPos() const noexcept { return mPos; }
		[[nodiscard]] constexpr inline glm::dvec2* GetPosPointer() noexcept { return &mPos; }



		inline void SetAngle(double angle) noexcept {
			mAngledouble = angle;
			mAngle = AngleFloatToAngleVec(angle);
		}

		[[nodiscard]] inline  double GetAngleX() const noexcept { return mAngle.x; }

		[[nodiscard]] inline  double GetAngleY() const noexcept { return mAngle.y; }

		[[nodiscard]] inline  glm::dvec2 GetAngle() const noexcept { return mAngle; }

		[[nodiscard]] inline  double GetAngleFloat() const noexcept { return mAngledouble; }








		//设置受力
		inline void SetForce(glm::dvec2 force) {
			mForce = force;
			mForcedouble = Modulus(force);
			mForceAngledouble = EdgeVecToCosAngleFloat(force);
			mForceAngle = AngleFloatToAngleVec(mForceAngledouble);
		}

		//设置受力
		inline void SetForce(double angle, double Force) {
			mForcedouble = Force;
			mForceAngledouble = angle;
			mForceAngle = AngleFloatToAngleVec(angle);
			mForce = mForcedouble * mForceAngle;
		}

		//受到的力
		inline void SufferForce(glm::dvec2 force) {
			mForce += force;
			mForcedouble = Modulus(force);
			mForceAngledouble = EdgeVecToCosAngleFloat(force);
			mForceAngle = AngleFloatToAngleVec(mForceAngledouble);
		}

		//受到的力
		inline void SufferForce(double angle, double Force) {
			mForce += AngleFloatToAngleVec(angle) * Force;
			mForcedouble = Modulus(mForce);
			mForceAngledouble = EdgeVecToCosAngleFloat(mForce);
			mForceAngle = AngleFloatToAngleVec(mForceAngledouble);
		}

		[[nodiscard]] inline  double GetForceX() const noexcept { return mForce.x; }

		[[nodiscard]] inline  double GetForceY() const noexcept { return mForce.y; }

		[[nodiscard]] inline  double GetForceAngleX() const noexcept { return mForceAngle.x; }

		[[nodiscard]] inline  double GetForceAngleY() const noexcept { return mForceAngle.y; }

		[[nodiscard]] inline  glm::dvec2 GetForce() const noexcept { return mForce; }
		[[nodiscard]] constexpr inline  glm::dvec2* GetForcePointer() noexcept { return &mForce; }

		[[nodiscard]] inline  double GetForcedouble() const noexcept { return mForcedouble; }

		[[nodiscard]] inline  double GetForceAngledouble() const noexcept { return mForceAngledouble; }

		[[nodiscard]] inline  glm::dvec2 GetForceAngle() const noexcept { return mForceAngle; }




		

		//设置速度
		inline void SetSpeed(glm::dvec2 speed) {
			mSpeed = speed;
			mSpeeddouble = Modulus(mSpeed);
			mSpeedAngledouble = EdgeVecToCosAngleFloat(mSpeed);
			mSpeedAngle = AngleFloatToAngleVec(mSpeedAngledouble);
		}

		//设置速度
		inline void SetSpeed(double speed, double angle) {
			mSpeeddouble = speed;
			mSpeedAngledouble = angle;
			mSpeedAngle = AngleFloatToAngleVec(mSpeedAngledouble);
			mSpeed = mSpeedAngle * mSpeeddouble;
		}

		//期望速度
		inline void ExpectSpeed(double speed, double angle, double time) {
			glm::dvec2 Lspeed = speed * AngleFloatToAngleVec(angle);
			Lspeed -= mSpeed;
			mSpeed += Lspeed * time;
			mSpeeddouble = Modulus(mSpeed);
			mSpeedAngledouble = EdgeVecToCosAngleFloat(mSpeed);
			mSpeedAngle = AngleFloatToAngleVec(mSpeedAngledouble);
		}


		[[nodiscard]] inline double GetSpeedX() const noexcept { return mSpeed.x; }

		[[nodiscard]] inline double GetSpeedY() const noexcept { return mSpeed.y; }

		[[nodiscard]] inline double GetSpeedAngleX() const noexcept { return mSpeedAngle.x; }

		[[nodiscard]] inline double GetSpeedAngleY() const noexcept { return mSpeedAngle.y; }

		[[nodiscard]] inline glm::dvec2 GetSpeed() const noexcept { return mSpeed; }
		[[nodiscard]] constexpr inline  glm::dvec2* GetSpeedPointer() noexcept { return &mSpeed; }

		[[nodiscard]] inline double GetSpeedFloat() const noexcept { return mSpeeddouble; }

		[[nodiscard]] inline double GetSpeedAngleFloat() const noexcept { return mSpeedAngledouble; }

		[[nodiscard]] inline glm::dvec2 GetSpeedAngle() const noexcept { return mSpeedAngle; }



		//破坏的回调函数
		inline void SetDestroyModeCallback(_DestroyModeCallback DestroyModeCallback) noexcept {
			mDestroyModeCallback = DestroyModeCallback;
		}

		inline bool DestroyModeCallback(int x, int y, bool Bool, double Angle, ObjectDecorator* Object, GridDecorator* Grid, void* Data) noexcept {
			if (mDestroyModeCallback != nullptr) {
				return mDestroyModeCallback(x, y, Bool, Angle, Object, Grid, Data);
			}
		}




		//帧时间步长模拟
		void FrameTimeStep(double TimeStep, double FrictionCoefficient){
			glm::dvec2 Resistance = { 0, 0};
			if (mFrictionCoefficient != 0) {
				Resistance = mSpeedAngle * ((mQuality * 9.8f * FrictionCoefficient * mFrictionCoefficient) + (mSpeeddouble * mSpeeddouble * 0.01f));//受到摩檫力
			}

			mPos += mSpeed * TimeStep;//计算移动距离
			//解算速度
			if (mSpeeddouble != 0) {//速度不为 O
				glm::dvec2 LForce = mForce - Resistance;
				glm::dvec2 mAcceleration = (LForce / mQuality);
				SetSpeed(mSpeed + (mAcceleration * TimeStep));
			}
			else if(ModulusLength(Resistance) < ModulusLength(mForce))
			{
				glm::dvec2 LForce = mForce - Resistance;
				glm::dvec2 mAcceleration = (LForce / mQuality);
				SetSpeed(mSpeed + (mAcceleration * TimeStep));
			}
			SetForce({ 0,0 });

			double AngleAcceleration = 0;
			double LTorque = (mQuality * 9.8f * FrictionCoefficient * mFrictionCoefficient) * mRadius + (mAngleSpeed * mAngleSpeed * 0.1f);
			mAngledouble += mAngleSpeed * TimeStep;//计算旋转角度
			SetAngle(mAngledouble);
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
		void PlayerTargetAngle(double TargetAngle) {
			mTargetAngle = TargetAngle;
			bool B = false;
			if (mAngledouble == 3.14159f) {
				B = true;
			}
			TargetAngle = TargetAngle - mAngledouble;
			if (TargetAngle == 0) {
				return;
			}

			double AngleForce = sin(TargetAngle) * 0.05f;
			if (cos(TargetAngle) < -0.1) {
				if (AngleForce < 0) {
					AngleForce += cos(TargetAngle) * 0.5f;
				}
				else {
					AngleForce -= cos(TargetAngle) * 0.5f;
				}
				
			}
			mAngledouble += (AngleForce == 0.0f ? 0.1f : AngleForce);
			if (B) {
				std::cout << (AngleForce == 0.0f ? 0.1f : AngleForce) << std::endl;
			}
			mAngle = AngleFloatToAngleVec(mAngledouble);
		}

		//受力解算
		void ForceSolution(glm::dvec2 ArmOfForce, glm::dvec2 Force, double tiem) {
			DecompositionForce LSDecompositionForce = CalculateDecompositionForce(ArmOfForce, Force);
			double Angle = EdgeVecToCosAngleFloat(ArmOfForce) - EdgeVecToCosAngleFloat(LSDecompositionForce.Vertical);
			double angletime = Modulus(ArmOfForce) * Modulus(LSDecompositionForce.Vertical);
			angletime = angletime * (sin(Angle) < 0 ? -1 : 1);
			SufferTorque(angletime);
			SufferForce(LSDecompositionForce.Parallel);
		}

		inline void SetTorque(double Torque) noexcept { mTorque = Torque; }

		inline void SufferTorque(double Torque) noexcept { mTorque += Torque; }


	
		double		mQuality = 1.0f;				//质量（KG）
		double		mFrictionCoefficient = 0.6f;	//摩檫力系数
		glm::dvec2	mBarycenter{ 0.0f, 0.0f };		//重心
		
		glm::dvec2	mPos{ 0.0f, 0.0f };				//位置
		glm::dvec2	mAngle{ 0.0f, 0.0f };			//朝向角度
		double		mAngledouble = 0.0f;				//朝向角度double
		
		glm::dvec2	mForce{ 0.0f, 0.0f };			//受力
		double		mForcedouble = 0.0f;				//受力大小double
		glm::dvec2	mForceAngle{ 0.0f, 0.0f };		//受力角度
		double		mForceAngledouble = 0.0f;		//受力角度double

		glm::dvec2	mSpeed{ 0.0f, 0.0f };			//速度
		double		mSpeeddouble = 0.0f;				//速度大小double
		glm::dvec2	mSpeedAngle{ 0.0f, 0.0f };		//速度角度
		double		mSpeedAngledouble = 0.0f;		//速度角度double

		double		mAngleSpeed = 0.0f;				//角速度
		double		mTorque = 0.0f;					//扭矩
		double		mRadius = 8.0f;					//半径

		double		mTargetAngle = 0;				//目标角度
		bool		mSpeedBack[2]{};				//用于判断速度方向反转


		_DestroyModeCallback mDestroyModeCallback = nullptr;
	};

}