#include "ImGuiPhysics.hpp"

namespace PhysicsBlock
{

	void PhysicsShapeUI(PhysicsShape* Object) {
		ImGui::InputDouble("Double:", &Object->angle);
	}

	void PhysicsShapeUI(PhysicsParticle* Object) {

	}

}