#include "ImGuiPhysics.hpp"
#include <string>

namespace PhysicsBlock
{
	void Dvec2ImGui(std::string name, glm::dvec2* val) {
		ImGui::InputDouble((name + ".x").c_str(), &val->x);
		ImGui::InputDouble((name + ".y").c_str(), &val->y);
	}

	void GridImGui(std::string name, GridBlock* Grid) {
		unsigned char min = 0, max = 255;
		ImGui::Text(name.c_str());
		ImGui::InputDouble(("摩擦因数" + name).c_str(), &Grid->FrictionFactor);
		if (Grid->Entity) {
			ImGui::SliderScalar(("血量" + name).c_str(), ImGuiDataType_U8, &Grid->Healthpoint, &min, &max);
			ImGui::InputDouble(("质量" + name).c_str(), &Grid->mass);
		}
		else {
			ImGui::SliderScalar(("距离场" + name).c_str(), ImGuiDataType_U8, &Grid->DistanceField, &min, &max);
			ImGui::SliderScalar(("方向场" + name).c_str(), ImGuiDataType_U8, &Grid->DirectionField, &min, &max);
		}
		bool val = Grid->Entity;
		ImGui::Checkbox(("实体" + name).c_str(), &val);
		Grid->Entity = val;
		val = Grid->Collision;
		ImGui::Checkbox(("碰撞" + name).c_str(), &val);
		Grid->Collision = val;
		val = Grid->Event;
		ImGui::Checkbox(("碰撞事件" + name).c_str(), &val);
		Grid->Event = val;
	}

	void PhysicsShapeUI(PhysicsShape* Object) {
		if (ImGui::Button("重新计算对象参数")) {
			Object->UpdateInfo();
			Object->UpdateOutline();
		}
		PhysicsShapeUI((PhysicsParticle*)Object);
		ImGui::InputDouble("角度", &Object->angle);
		ImGui::InputDouble("角速度", &Object->angleSpeed);
		Dvec2ImGui("质心", &Object->CentreMass);
		ImGui::InputDouble("碰撞半径", &Object->CollisionR);
		ImGui::InputDouble("转动惯量", &Object->MomentInertia);
		ImGui::InputDouble("扭矩", &Object->torque);
		Dvec2ImGui("几何中心", &Object->CentreShape);

		for (size_t x = 0; x < Object->width; ++x)
		{
			for (size_t y = 0; y < Object->height; ++y)
			{
				GridImGui("(" + std::to_string(x) + "," + std::to_string(y) + ")", &Object->at(x, y));
			}
		}
	}

	void PhysicsShapeUI(PhysicsParticle* Object) {
		Dvec2ImGui("位置", &Object->pos);
		Dvec2ImGui("速度", &Object->speed);
		Dvec2ImGui("受力", &Object->force);
		ImGui::InputDouble("质量", &Object->mass);
	}

	void PhysicsShapeUI(PhysicsWorld* Object) {
		Dvec2ImGui("重力加速度", &Object->GravityAcceleration);
		if (Object->WindBool) {
			Dvec2ImGui("风力", &Object->Wind);
		}
	}

}