#include "ImGuiPhysics.hpp"
#include "../ini.h"
#include <string>
#if TranslatorLocality
#include "../ImGui/imgui.h"
#else
#include "../extern/imgui/imgui.h"
#endif

namespace PhysicsBlock
{
	// 实例化变量
	// 物理对象基础信息
	AuxiliaryBoolColor(Pos);		 // 位置
	AuxiliaryBoolColor(OldPos);		 // 旧位置
	AuxiliaryBoolColor(Speed);		 // 速度
	AuxiliaryBoolColor(Force);		 // 受力
	AuxiliaryBoolColor(CentreMass);	 // 质心
	AuxiliaryBoolColor(CentreShape); // 几何中心
	AuxiliaryBoolColor(Outline);	 // 外骨骼点
	// 碰撞信息
	AuxiliaryBoolColor(CollisionDrop);					// 碰撞点
	AuxiliaryBoolColor(SeparateNormalVector);			// 分离法向量
	AuxiliaryBoolColor(CollisionDropToCenterOfGravity); // 碰撞点到两个物体重心的连线

	// 读取物理辅助显示信息
	void AuxiliaryInfoRead()
	{
		inih::INIReader Ini{"PhysicsBlock.ini"};
		AuxiliaryReadBoolColor(Pos);						// 位置
		AuxiliaryReadBoolColor(OldPos);						// 旧位置
		AuxiliaryReadBoolColor(Speed);						// 速度
		AuxiliaryReadBoolColor(Force);						// 受力
		AuxiliaryReadBoolColor(CentreMass);					// 质心
		AuxiliaryReadBoolColor(CentreShape);				// 几何中心
		AuxiliaryReadBoolColor(Outline);	 // 外骨骼点
		AuxiliaryReadBoolColor(CollisionDrop);					// 碰撞点
		AuxiliaryReadBoolColor(SeparateNormalVector);			// 分离法向量
		AuxiliaryReadBoolColor(CollisionDropToCenterOfGravity); // 碰撞点到两个物体重心的连线
	}

	// 保存物理辅助显示信息
	void AuxiliaryInfoStorage()
	{
		inih::INIReader Ini{"PhysicsBlock.ini"};
		AuxiliaryStorageBoolColor(Pos);						 // 位置
		AuxiliaryStorageBoolColor(OldPos);					 // 旧位置
		AuxiliaryStorageBoolColor(Speed);					 // 速度
		AuxiliaryStorageBoolColor(Force);					 // 受力
		AuxiliaryStorageBoolColor(CentreMass);				 // 质心
		AuxiliaryStorageBoolColor(CentreShape);				 // 几何中心
		AuxiliaryStorageBoolColor(Outline);	 // 外骨骼点
		AuxiliaryStorageBoolColor(CollisionDrop);					 // 碰撞点
		AuxiliaryStorageBoolColor(SeparateNormalVector);			 // 分离法向量
		AuxiliaryStorageBoolColor(CollisionDropToCenterOfGravity);	 // 碰撞点到两个物体重心的连线
		inih::INIWriter::write_Gai("PhysicsBlock.ini", Ini); // 保存
	}

	void Dvec2ImGui(std::string name, glm::dvec2 *val)
	{
		ImGui::InputDouble((name + ".x").c_str(), &val->x);
		ImGui::SameLine();
		ImGui::InputDouble((name + ".y").c_str(), &val->y);
	}

	bool GridImGui(std::string name, GridBlock *Grid)
	{
		static GridBlock *Static_Grid = nullptr;
		static double Static_mass;
		static bool Static_Entity;
		static bool Static_Collision;
		static bool Static_Event;

		// 更换对象,更新信息
		if (Static_Grid != Grid)
		{
			Static_Grid = Grid;
			Static_mass = Grid->mass;
			Static_Entity = Grid->Entity;
			Static_Collision = Grid->Collision;
			Static_Event = Grid->Event;
		}

		unsigned char min = 0, max = 255;
		ImGui::Text(name.c_str());
		ImGui::InputDouble(("摩擦因数" + name).c_str(), &Grid->FrictionFactor);
		if (Grid->Entity)
		{
			ImGui::SliderScalar(("血量" + name).c_str(), ImGuiDataType_U64, &Grid->Healthpoint, &min, &max);
			ImGui::InputDouble(("质量" + name).c_str(), &Grid->mass);
		}
		else
		{
			ImGui::SliderScalar(("距离场" + name).c_str(), ImGuiDataType_U64, &Grid->DistanceField, &min, &max);
			ImGui::SliderScalar(("方向场" + name).c_str(), ImGuiDataType_U64, &Grid->DirectionField, &min, &max);
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

		// 判断关键信息是否发生变化，请求更新物体信息
		bool UpData = false;
		if (Static_mass != Grid->mass)
		{
			UpData = true;
		}
		if (Static_Entity != Grid->Entity)
		{
			UpData = true;
		}
		if (Static_Collision != Grid->Collision)
		{
			UpData = true;
		}
		if (Static_Event != Grid->Event)
		{
			UpData = true;
		}
		// 更新用于判断的数据
		if (UpData)
		{
			Static_mass = Grid->mass;
			Static_Entity = Grid->Entity;
			Static_Collision = Grid->Collision;
			Static_Event = Grid->Event;
		}
		return UpData;
	}

	void AuxiliaryColorUI(const char *Name, bool *Bool, float *Color)
	{
		ImGui::Checkbox(Name, Bool);
		ImGui::SameLine();
		ImGui::ColorEdit4((std::string(Name) + ":Color").c_str(), Color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
	}

	void PhysicsAuxiliaryColorUI()
	{
		AuxiliaryColorUI("位置", &Auxiliary_PosBool, Auxiliary_PosColor);
		AuxiliaryColorUI("旧位置", &Auxiliary_OldPosBool, Auxiliary_OldPosColor);
		AuxiliaryColorUI("速度", &Auxiliary_SpeedBool, Auxiliary_SpeedColor);
		AuxiliaryColorUI("受力", &Auxiliary_ForceBool, Auxiliary_ForceColor);
		AuxiliaryColorUI("质心", &Auxiliary_CentreMassBool, Auxiliary_CentreMassColor);
		AuxiliaryColorUI("几何中心", &Auxiliary_CentreShapeBool, Auxiliary_CentreShapeColor);
		AuxiliaryColorUI("外骨骼点", &Auxiliary_OutlineBool, Auxiliary_OutlineColor);
		AuxiliaryColorUI("碰撞点", &Auxiliary_CollisionDropBool, Auxiliary_CollisionDropColor);
		AuxiliaryColorUI("分离法向量", &Auxiliary_SeparateNormalVectorBool, Auxiliary_SeparateNormalVectorColor);
		AuxiliaryColorUI("碰撞点到两个物体重心的连线", &Auxiliary_CollisionDropToCenterOfGravityBool, Auxiliary_CollisionDropToCenterOfGravityColor);
	}

	bool PhysicsShapeUI(PhysicsShape *Object)
	{
		bool UpData = PhysicsShapeUI((PhysicsParticle *)Object);
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

		if (GridImGui("(" + std::to_string(GridPos.x) + "," + std::to_string(GridPos.y) + ")", &Object->at(GridPos)))
		{
			UpData = true;
		}

		// 更新对象信息
		if (UpData)
		{
			Object->UpdateInfo();
			Object->UpdateOutline();
		}
		return UpData;
	}

	bool PhysicsShapeUI(PhysicsParticle *Object)
	{
		Dvec2ImGui("位置", &Object->pos);
		Dvec2ImGui("速度", &Object->speed);
		Dvec2ImGui("受力", &Object->force);
		ImGui::InputDouble("质量", &Object->mass);

		return false;
	}

	bool PhysicsShapeUI(PhysicsWorld *Object)
	{
		Dvec2ImGui("重力加速度", &Object->GravityAcceleration);
		if (Object->WindBool)
		{
			Dvec2ImGui("风力", &Object->Wind);
		}

		bool UpData = false;

		MapFormwork *Map = Object->GetMapFormwork();
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