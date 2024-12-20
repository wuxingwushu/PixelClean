#include "ImGuiPhysics.hpp"

namespace PhysicsBlock
{

	void PhysicsShapeUI(PhysicsShape* Object) {
		PhysicsShapeUI((PhysicsParticle*)Object);
		ImGui::InputDouble("角度:", &Object->angle);
		ImGui::InputDouble("角速度:", &Object->angleSpeed);
		ImGui::InputDouble("质心.x:", &Object->CentreMass.x);
		ImGui::InputDouble("质心.y:", &Object->CentreMass.y);
		ImGui::InputDouble("碰撞半径:", &Object->CollisionR);
		ImGui::InputDouble("转动惯量:", &Object->MomentInertia);
		ImGui::InputDouble("扭矩:", &Object->torque);
		ImGui::InputDouble("几何中心.x:", &Object->CentreShape.x);
		ImGui::InputDouble("几何中心.y:", &Object->CentreShape.y);
	}

	void PhysicsShapeUI(PhysicsParticle* Object) {
		ImGui::InputDouble("位置.x:", &Object->pos.x);
		ImGui::InputDouble("位置.y:", &Object->pos.y);
		ImGui::InputDouble("速度.x:", &Object->speed.x);
		ImGui::InputDouble("速度.y:", &Object->speed.y);
		ImGui::InputDouble("受力.x:", &Object->force.x);
		ImGui::InputDouble("受力.y:", &Object->force.y);
		ImGui::InputDouble("质量:", &Object->mass);
	}

}