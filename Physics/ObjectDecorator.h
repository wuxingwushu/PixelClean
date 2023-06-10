#pragma once
#include <glm/glm.hpp>

namespace SquarePhysics {

	class ObjectDecorator //装饰器模式
	{
	public:

		


		void SetQuality(float quality) { mQuality = quality; }

		[[nodiscard]] float GetQuality() { return mQuality; }



		
		void SetPosX(float x) { mPos.x = x; }

		void SetPosY(float y) { mPos.y = y; }

		void SetPos(glm::vec2 pos) { mPos = pos; }

		[[nodiscard]] float GetPosX() { return mPos.x; }

		[[nodiscard]] float GetPosY() { return mPos.y; }

		[[nodiscard]] glm::vec2 GetPos() { return mPos; }




		void SetForceX(float x) { mForce.x = x; }

		void SetForceY(float y) { mForce.y = y; }

		void SetForce(glm::vec2 force) { mForce = force; }

		[[nodiscard]] float GetForceX() { return mForce.x; }

		[[nodiscard]] float GetForceY() { return mForce.y; }

		[[nodiscard]] glm::vec2 GetForce() { return mForce; }




		void SetAngleX(float x) { mAngle.x = x; }

		void SetAngleY(float y) { mAngle.y = y; }

		void SetAngle(glm::vec2 angle) { mAngle = angle; }

		void SetAngle(float angle) { 
			mAngleFloat = angle;
			mAngle = { cos(angle),sin(angle) }; 
		}

		[[nodiscard]] float GetAngleX() { return mAngle.x; }

		[[nodiscard]] float GetAngleY() { return mAngle.y; }

		[[nodiscard]] glm::vec2 GetAngle() { return mAngle; }

		[[nodiscard]] float GetAngleFloat() { return mAngleFloat; }




		void SetSpeedX(float x) { 
			mSpeed.x = x;
			mSpeedBack[0] = (mSpeed.x > 0.0f);
		}

		void SetSpeedY(float y) { 
			mSpeed.y = y;
			mSpeedBack[1] = (mSpeed.y > 0.0f);
		}

		void SetSpeed(glm::vec2 speed) { 
			mSpeed = speed;
			UpDataSpeedBack();
		}

		void SetSpeed(float speed) { 
			mSpeed = { speed * mAngle.x, speed * mAngle.y };
			UpDataSpeedBack();
		}

		void SetSpeed(float angle, float speed) { 
			SetAngle(angle);
			SetSpeed(speed);
			UpDataSpeedBack();
		}

		void SetSpeed(float angle, glm::vec2 speed) {
			SetAngle(angle);
			SetSpeed(speed);
			//angle -= 1.57f;
			//SetSpeed({ speed .x * cos(angle) - speed.y * sin(angle), speed.x * sin(angle) + speed.y * cos(angle)});
			UpDataSpeedBack();
		}

		void SetSpeed(glm::vec2 angle, float speed) {
			SetAngle(angle);
			SetSpeed(speed);
			UpDataSpeedBack();
		}

		[[nodiscard]] float GetSpeedX() { return mSpeed.x; }

		[[nodiscard]] float GetSpeedY() { return mSpeed.y; }

		[[nodiscard]] glm::vec2 GetSpeed() { return mSpeed; }


		void SetFrictionCoefficient(float frictionCoefficient) { mFrictionCoefficient = frictionCoefficient; }

		[[nodiscard]] float GetFrictionCoefficient() { return mFrictionCoefficient; }



		//帧时间步长模拟
		void FrameTimeStep(float TimeStep, float FrictionCoefficient){
			glm::vec2 Resistance = mAngle * (mQuality * 9.8f * FrictionCoefficient * mFrictionCoefficient);
			if ((mForce.x != 0.0f) || (mForce.y != 0.0f)) {
				if ((mSpeed.x != 0.0f) || (mSpeed.y != 0.0f)) {
					mSpeed += ((mForce - Resistance) / mQuality) * TimeStep;
					mPos += mSpeed * TimeStep;
					SpeedReversalJudge();
				}
				else
				{
					if (ModulusLength(Resistance) < ModulusLength(mForce))
					{
						mSpeed += ((mForce - Resistance) / mQuality) * TimeStep;
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
					mSpeed -= (Resistance / mQuality) * TimeStep;
					mPos += mSpeed * TimeStep;
					SpeedReversalJudge();
				}
				else
				{
					return;
				}
			}
		}

		//获取模的长度
		float ModulusLength(glm::vec2 Modulus) {
			return ((Modulus.x * Modulus.x) + (Modulus.y * Modulus.y));
		}

		//和上一时刻速度的Bool进行对比，判断速度是否要取反。
		void SpeedReversalJudge() {
			bool SpeedX = (mSpeed.x > 0.0f);
			bool SpeedY = (mSpeed.y > 0.0f);
			if (SpeedX != mSpeedBack[0])
			{
				mSpeed.x = 0.0f;
				mAngle.x = 0.0f;
			}

			if (SpeedY != mSpeedBack[1])
			{
				mSpeed.y = 0.0f;
				mAngle.y = 0.0f;
			}
			mSpeedBack[0] = SpeedX;
			mSpeedBack[1] = SpeedY;
		}

		//更新各个方向速度的Bool
		void UpDataSpeedBack() {
			mSpeedBack[0] = (mSpeed.x > 0.0f);
			mSpeedBack[1] = (mSpeed.y > 0.0f);
		}

	private:
		float		mQuality = 1.0f;				//质量（KG）
		float		mFrictionCoefficient = 0.6f;	//摩檫力系数
		float		mAngleFloat = 0.0f;				//朝向角度
		glm::vec2	mAngle{};						//朝向角度
		glm::vec2	mPos{};							//位置
		glm::vec2	mSpeed{};						//速度
		bool		mSpeedBack[2];					//用于判断速度方向反转
		glm::vec2	mForce{};						//受力
	};

}