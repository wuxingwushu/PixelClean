#include "AttackMode.h"
#include "Arms.h"

namespace GAME {


	std::map<AttackModeEnum, AttackModeStruct> AttackModeMap{
		{ AttackModeEnum::Pistol, { PistolMode, 0.5f } },
		{ AttackModeEnum::Shrapnel, { ShrapnelMode, 1.0f } },
		{ AttackModeEnum::MachineGun, { MachineGunMode, 0.1f } },
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
		arms->ShootBullets(x, y, angle, speed, Type);
	}

	void ShrapnelMode(Arms* arms, float x, float y, float angle, float speed, unsigned int Type) {
		arms->ShootBullets(x, y, angle + 0.1, speed, Type);
		arms->ShootBullets(x, y, angle, speed, Type);
		arms->ShootBullets(x, y, angle - 0.1, speed, Type);
	}

	void MachineGunMode(Arms* arms, float x, float y, float angle, float speed, unsigned int Type) {
		arms->ShootBullets(x, y, angle, speed, Type);
	}
}