#include "AttackMode.h"
#include "Arms.h"
#include "../DebugLog.h"

namespace GAME {


	std::map<AttackModeEnum, AttackModeStruct> AttackModeMap{
		{ AttackModeEnum::Pistol, { PistolMode, 0.5f } },
		{ AttackModeEnum::Shrapnel, { ShrapnelMode, 1.0f } },
		{ AttackModeEnum::MachineGun, { MachineGunMode, 0.1f } },
		{ AttackModeEnum::Rocket, { RocketMode, 1.5f } },
	};

	AttackModeStruct GetAttackMode(AttackModeEnum Enum) {
		if (AttackModeMap.find(Enum) == AttackModeMap.end()) {
			return { PistolMode, 0.5f };
		}
		else {
			return AttackModeMap[Enum];
		}
	}

	void PistolMode(Arms* arms, float x, float y, float angle, float speed, unsigned int Type) {
		LOGD("PistolMode() called");
		arms->ShootBullets(x, y, angle, speed, Type);
	}

	void ShrapnelMode(Arms* arms, float x, float y, float angle, float speed, unsigned int Type) {
		LOGD("ShrapnelMode() called");
		arms->ShootBullets(x, y, angle + 0.15, speed, Type);
		arms->ShootBullets(x, y, angle + 0.07, speed, Type);
		arms->ShootBullets(x, y, angle, speed, Type);
		arms->ShootBullets(x, y, angle - 0.07, speed, Type);
		arms->ShootBullets(x, y, angle - 0.15, speed, Type);
	}

	void MachineGunMode(Arms* arms, float x, float y, float angle, float speed, unsigned int Type) {
		LOGD("MachineGunMode() called");
		// 轻微随机散射，模拟机枪精度
		float spread = ((rand() % 100) / 100.0f - 0.5f) * 0.1f;
		arms->ShootBullets(x, y, angle + spread, speed, Type);
	}

	void RocketMode(Arms* arms, float x, float y, float angle, float speed, unsigned int Type) {
		LOGD("RocketMode() called");
		// 火箭弹：速度较慢、命中即爆炸（爆炸参数由 Arms::GetWeaponConfig 设置）
		arms->ShootBullets(x, y, angle, speed * 0.6f, Type);
	}
}