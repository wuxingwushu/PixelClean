#pragma once


namespace GAME {
	class Arms;
	typedef void (*_ShootCallback)(Arms* arms, float x, float y, float angle, float speed, unsigned int Type);//武器开火 回调函数类型

	enum class AttackModeEnum
	{
		Pistol = 0,
		Shrapnel,
		MachineGun
	};

	constexpr int AttackModeEnumNumber = static_cast<int>(AttackModeEnum::MachineGun);

	struct AttackModeStruct {
		_ShootCallback Mode = nullptr;
		float Interval = 0.5f;
	};

	AttackModeStruct GetAttackMode(AttackModeEnum Enum);

	void PistolMode(Arms* arms, float x, float y, float angle, float speed, unsigned int Type);

	void ShrapnelMode(Arms* arms, float x, float y, float angle, float speed, unsigned int Type);

	void MachineGunMode(Arms* arms, float x, float y, float angle, float speed, unsigned int Type);
}