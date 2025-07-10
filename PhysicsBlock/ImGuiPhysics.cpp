#include "ImGuiPhysics.hpp"
#include <string>

namespace PhysicsBlock
{
	void Dvec2ImGui(std::string name, glm::dvec2* val) {
		ImGui::InputDouble((name + ".x").c_str(), &val->x);
		ImGui::SameLine();
		ImGui::InputDouble((name + ".y").c_str(), &val->y);
	}

	void GridImGui(std::string name, GridBlock* Grid) {
		static GridBlock* Static_Grid = nullptr;
		// 更换对象,更新信息
		if (Static_Grid != Grid) {
			Static_Grid = Grid;
		}

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

	bool PhysicsShapeUI(PhysicsShape* Object) {
		if (ImGui::Button("重新计算对象参数")) {
			Object->UpdateInfo();
			Object->UpdateOutline();
		}
		bool UpData = PhysicsShapeUI((PhysicsParticle*)Object);
		ImGui::InputDouble("角度", &Object->angle);
		ImGui::InputDouble("角速度", &Object->angleSpeed);
		Dvec2ImGui("质心", &Object->CentreMass);
		ImGui::InputDouble("碰撞半径", &Object->CollisionR);
		ImGui::InputDouble("转动惯量", &Object->MomentInertia);
		ImGui::InputDouble("扭矩", &Object->torque);
		Dvec2ImGui("几何中心", &Object->CentreShape);

		static glm::ivec2 GridPos{0};
		for (int y = Object->height - 1; y >= 0; --y)
		{
			for (size_t x = 0; x < Object->width; ++x)
			{
				ImGui::PushID(x * Object->height + y);
				if (ImGui::Selectable("", Object->at(x, y).Collision, 0, ImVec2(25, 25)))
				{
					GridPos = {x, y};
				}
				if (x < (Object->width - 1))
					ImGui::SameLine();
				ImGui::PopID();
			}
		}

		GridImGui("(" + std::to_string(GridPos.x) + "," + std::to_string(GridPos.y) + ")", &Object->at(GridPos));

		return UpData;
	}

	bool PhysicsShapeUI(PhysicsParticle* Object) {
		Dvec2ImGui("位置", &Object->pos);
		Dvec2ImGui("速度", &Object->speed);
		Dvec2ImGui("受力", &Object->force);
		ImGui::InputDouble("质量", &Object->mass);

		return false;
	}

	bool PhysicsShapeUI(PhysicsWorld* Object) {
		Dvec2ImGui("重力加速度", &Object->GravityAcceleration);
		if (Object->WindBool) {
			Dvec2ImGui("风力", &Object->Wind);
		}

		bool UpData = false;

		MapFormwork* Map = Object->GetMapFormwork();
		for (int y = Map->FMGetMapSize().y - 1; y >= 0; --y)
		{
			for (size_t x = 0; x < Map->FMGetMapSize().x; ++x)
			{
				ImGui::PushID(x * Map->FMGetMapSize().y + y);
				if (ImGui::Selectable("", Map->FMGetCollide(glm::ivec2{x, y}), 0, ImVec2(25, 25)))
				{
					UpData = true;
					Map->FMGetGridBlock(glm::ivec2{x, y}).Collision = !Map->FMGetGridBlock(glm::ivec2{x, y}).Collision;
					Map->FMGetGridBlock(glm::ivec2{x, y}).Entity = !Map->FMGetGridBlock(glm::ivec2{x, y}).Entity;
				}
				if (x < (Map->FMGetMapSize().x - 1))
					ImGui::SameLine();
				ImGui::PopID();
			}
		}
		return UpData;
	}

}