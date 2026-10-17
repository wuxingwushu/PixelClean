#include "ImGuiPhysics3D.hpp"
#include "../DebugLog.h"
#include <cstdio>
#include <exception>
#include <string>
#if TranslatorLocality3D
#include "../ImGui/imgui.h"
#include "../ini.h"
#else
#include "../extern/imgui/imgui.h"
#include "ini.h"
#endif

namespace PhysicsBlock3D
{
	// 实例化变量
	// 物理对象基础信息
	AuxiliaryBoolColor3D(Pos);		  // 位置
	AuxiliaryBoolColor3D(OldPos);	  // 旧位置
	AuxiliaryBoolColor3D(Angle);	  // 角度
	AuxiliaryBoolColor3D(Speed);	  // 速度
	AuxiliaryBoolColor3D(Force);	  // 受力
	AuxiliaryBoolColor3D(CentreMass); // 质心
	AuxiliaryBoolColor3D(CentreShape); // 几何中心
	AuxiliaryBoolColor3D(Outline);	  // 外骨骼点
	AuxiliaryBoolColor3D(MaxOutlineCentreMass); // 最大外骨骼点质心
	// 碰撞信息
	AuxiliaryBoolColor3D(CollisionDrop);				   // 碰撞点
	AuxiliaryBoolColor3D(SeparateNormalVector);			   // 分离法向量
	AuxiliaryBoolColor3D(CollisionDropToCenterOfGravity);  // 碰撞点到两个物体重心的连线
	AuxiliaryBoolColor3D(GridDivided);					   // 被选中的物体被划分网格到的网格位置

	// ═══════════════════════════════════════════════════════════════════════════
	// 辅助显示 ini 的健壮读写（3D 专属文件 PhysicsBlock3D.ini）
	// ═══════════════════════════════════════════════════════════════════════════
	// 这个 ini 是「运行时可写配置」：退出时由 AuxiliaryInfoStorage() 回写到当前工作
	// 目录，仓库根目录那份只是默认值，用户可以随意改。但 inih::INIReader 的构造函数
	// 在文件打不开时会直接抛 "ini file not found."（ini.h:302），异常从 PhysicsTest3D
	// 的构造函数一路冒到 main 的 catch（main.cpp:88），结果是**引擎启动失败**，只在
	// Logs/Error.txt 与 Logs/FatalException.txt 各留一行 "ini file not found."。
	// 一份 UI 配置不该有这个杀伤力，所以这里保证两点：
	//   1) 读取前文件一定存在 —— 不存在就按默认值先写一份（不依赖 CWD 预置文件，
	//      Android 的 assets 里没有这个文件也能正常起来）；
	//   2) 读写全程不抛异常 —— 失败就退回默认值 / 跳过保存，绝不让配置问题拖垮引擎。
	static const char *AuxiliaryIniPath3D = "PhysicsBlock3D.ini";

	// 键名规则与 AuxiliaryStorageBoolColor3D 完全一致（带 3D 后缀），默认值取自
	// 仓库根目录的 PhysicsBlock3D.ini，两处数值保持同步。
	static void EnsureAuxiliaryIniExists3D()
	{
		// 已存在就原样使用：那是用户上次退出时保存的选择，不能覆盖
		if (FILE *probe = fopen(AuxiliaryIniPath3D, "rb"))
		{
			fclose(probe);
			return;
		}

		FILE *fp = fopen(AuxiliaryIniPath3D, "wb");
		if (fp == nullptr)
			return; // 写不出来也不报错：读取端每个键都带默认值兜底

		fputs("[Auxiliary]\n", fp);

		// 临时宏负责「键名 + 默认值」的成对输出，省掉手写 26 行
#define AuxiliaryDefaultBoolColor3D(Name, Bool, C0, C1, C2, C3)          \
		fprintf(fp, TOSIRING3D(Name##Bool3D) "=%d\n", (Bool) ? 1 : 0);   \
		fprintf(fp, TOSIRING3D(Name##Color3D) "=%g %g %g %g\n",           \
			(double)(C0), (double)(C1), (double)(C2), (double)(C3));

		// 顺序与 AuxiliaryInfoRead() 里的读取顺序一致，便于对照
		AuxiliaryDefaultBoolColor3D(Pos, true, 0.599918, 0.230216, 0.814346, 1);
		AuxiliaryDefaultBoolColor3D(OldPos, false, 0, 0, 0, 1);
		AuxiliaryDefaultBoolColor3D(Angle, true, 1, 0.911392, 0, 1);
		AuxiliaryDefaultBoolColor3D(Speed, true, 1, 0.911392, 0, 1);
		AuxiliaryDefaultBoolColor3D(Force, true, 0.0264203, 0.619801, 0.894515, 1);
		AuxiliaryDefaultBoolColor3D(CentreMass, false, 0, 0, 0, 1);
		AuxiliaryDefaultBoolColor3D(CentreShape, false, 0, 0, 0, 1);
		AuxiliaryDefaultBoolColor3D(Outline, false, 1, 0.911392, 0, 1);
		AuxiliaryDefaultBoolColor3D(MaxOutlineCentreMass, false, 0.020652, 0.911392, 1, 1);
		AuxiliaryDefaultBoolColor3D(CollisionDrop, true, 0.978903, 0.020652, 0.020652, 1);
		AuxiliaryDefaultBoolColor3D(SeparateNormalVector, true, 0.970464, 0.970464, 0.970464, 1);
		AuxiliaryDefaultBoolColor3D(CollisionDropToCenterOfGravity, true, 0.0532678, 0.0881716, 0.742616, 1);
		AuxiliaryDefaultBoolColor3D(GridDivided, true, 1, 0, 0, 1);
#undef AuxiliaryDefaultBoolColor3D

		fclose(fp);
	}

	// 读取物理辅助显示信息
	void AuxiliaryInfoRead()
	{
		// 先补上缺失的 ini，否则下面的 INIReader 构造会抛 "ini file not found."
		EnsureAuxiliaryIniExists3D();

		try
		{
			// 3D 版使用专属 ini，避免与 2D 版的 PhysicsBlock.ini 相互覆盖
			inih::INIReader Ini{AuxiliaryIniPath3D};
			AuxiliaryReadBoolColor3D(Pos);						  // 位置
			AuxiliaryReadBoolColor3D(OldPos);					  // 旧位置
			AuxiliaryReadBoolColor3D(Angle);					  // 角度
			AuxiliaryReadBoolColor3D(Speed);					  // 速度
			AuxiliaryReadBoolColor3D(Force);					  // 受力
			AuxiliaryReadBoolColor3D(CentreMass);				  // 质心
			AuxiliaryReadBoolColor3D(CentreShape);				  // 几何中心
			AuxiliaryReadBoolColor3D(Outline);					  // 外骨骼点
			AuxiliaryReadBoolColor3D(MaxOutlineCentreMass);		  // 最大外骨骼点质心
			AuxiliaryReadBoolColor3D(CollisionDrop);			  // 碰撞点
			AuxiliaryReadBoolColor3D(SeparateNormalVector);		  // 分离法向量
			AuxiliaryReadBoolColor3D(CollisionDropToCenterOfGravity); // 碰撞点到两个物体重心的连线
			AuxiliaryReadBoolColor3D(GridDivided);				  // 被选中的物体被划分网格到的网格位置
		}
		catch (const std::exception &e)
		{
			// 读不出来就保持全局变量的初值（不显示），不影响引擎启动
			LOGE("PhysicsBlock3D::AuxiliaryInfoRead failed: %s", e.what());
		}
	}

	// 保存物理辅助显示信息
	void AuxiliaryInfoStorage()
	{
		try
		{
			inih::INIReader Ini{AuxiliaryIniPath3D};
			AuxiliaryStorageBoolColor3D(Pos);						  // 位置
			AuxiliaryStorageBoolColor3D(OldPos);					  // 旧位置
			AuxiliaryStorageBoolColor3D(Angle);						  // 角度
			AuxiliaryStorageBoolColor3D(Speed);						  // 速度
			AuxiliaryStorageBoolColor3D(Force);						  // 受力
			AuxiliaryStorageBoolColor3D(CentreMass);				  // 质心
			AuxiliaryStorageBoolColor3D(CentreShape);				  // 几何中心
			AuxiliaryStorageBoolColor3D(Outline);					  // 外骨骼点
			AuxiliaryStorageBoolColor3D(MaxOutlineCentreMass);		  // 最大外骨骼点质心
			AuxiliaryStorageBoolColor3D(CollisionDrop);				  // 碰撞点
			AuxiliaryStorageBoolColor3D(SeparateNormalVector);		  // 分离法向量
			AuxiliaryStorageBoolColor3D(CollisionDropToCenterOfGravity); // 碰撞点到两个物体重心的连线
			AuxiliaryStorageBoolColor3D(GridDivided);				  // 被选中的物体被划分网格到的网格位置
			inih::INIWriter::write_Gai(AuxiliaryIniPath3D, Ini);	  // 保存
		}
		catch (const std::exception &e)
		{
			// 本函数会在 ~PhysicsTest3D() 里被调用（析构里抛异常 = std::terminate）：
			// 保存失败最多损失下次启动的显示参数，绝不能崩在退出路径上
			LOGE("PhysicsBlock3D::AuxiliaryInfoStorage failed: %s", e.what());
		}
	}

	// 三分量拖拽（对应 2D 的 Dvec2ImGui；Vec2_ → Vec3_，多一个 z 分量）
	void Dvec3ImGui3D(std::string name, Vec3_ *val)
	{
		ImGui::DragScalar((name + ".x").c_str(), MyImGuiDataType3D, &val->x, 0.0005f, NULL, NULL, "%.10f");
		ImGui::DragScalar((name + ".y").c_str(), MyImGuiDataType3D, &val->y, 0.0005f, NULL, NULL, "%.10f");
		ImGui::DragScalar((name + ".z").c_str(), MyImGuiDataType3D, &val->z, 0.0005f, NULL, NULL, "%.10f");
	}

	bool GridImGui3D(std::string name, GridBlock3D *Grid)
	{
		static GridBlock3D *Static_Grid = nullptr;
		static FLOAT_ Static_mass;
		static bool Static_Entity;
		static bool Static_Collision;
		static bool Static_Event;

		if (Grid == nullptr)
			return false;

		// 更换对象,更新信息
		if (Static_Grid != Grid)
		{
			Static_Grid = Grid;
			Static_mass = Grid->mass;
			Static_Entity = Grid->Entity;
			Static_Collision = Grid->Collision;
			Static_Event = Grid->Event;
		}
#if TranslatorLocality3D
		ImGuiDataType_ T = ImGuiDataType_U8; // 这是正确的参数
#else
		ImGuiDataType_ T = ImGuiDataType_U64; // 这只是占位符而已（在另外一台电脑，防止报错，实际不使用）
#endif

		unsigned char min = 0, max = 255;
		ImGui::Text("%s", name.c_str());
		ImGui::DragScalar(("摩擦因数" + name).c_str(), MyImGuiDataType3D, &Grid->FrictionFactor, 0.0005f, NULL, NULL, "%.10f");
		if (Grid->Entity)
		{
			ImGui::SliderScalar(("血量" + name).c_str(), T, &Grid->Healthpoint, &min, &max);
			ImGui::DragScalar(("质量" + name).c_str(), MyImGuiDataType3D, &Grid->mass, 0.0005f, NULL, NULL, "%.10f");
		}
		else
		{
			ImGui::SliderScalar(("距离场" + name).c_str(), T, &Grid->DistanceField, &min, &max);
			ImGui::SliderScalar(("方向场" + name).c_str(), T, &Grid->DirectionField, &min, &max);
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

	void AuxiliaryColorUI3D(const char *Name, bool *Bool, float *Color)
	{
		ImGui::Checkbox(Name, Bool);
		ImGui::SameLine();
		ImGui::ColorEdit4((std::string(Name) + ":Color").c_str(), Color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf);
	}

	void PhysicsAuxiliaryColorUI()
	{
		AuxiliaryColorUI3D("位置##辅助", &Auxiliary_PosBool3D, Auxiliary_PosColor3D);
		AuxiliaryColorUI3D("旧位置", &Auxiliary_OldPosBool3D, Auxiliary_OldPosColor3D);
		// 2D 的"角度"在 3D 中改为显示物体局部 X 轴朝向（见 DrawShapeAuxiliary3D 等）
		AuxiliaryColorUI3D("角度##辅助", &Auxiliary_AngleBool3D, Auxiliary_AngleColor3D);
		AuxiliaryColorUI3D("速度##辅助", &Auxiliary_SpeedBool3D, Auxiliary_SpeedColor3D);
		AuxiliaryColorUI3D("受力##辅助", &Auxiliary_ForceBool3D, Auxiliary_ForceColor3D);
		AuxiliaryColorUI3D("质心##辅助", &Auxiliary_CentreMassBool3D, Auxiliary_CentreMassColor3D);
		AuxiliaryColorUI3D("几何中心##辅助", &Auxiliary_CentreShapeBool3D, Auxiliary_CentreShapeColor3D);
		AuxiliaryColorUI3D("外骨骼点", &Auxiliary_OutlineBool3D, Auxiliary_OutlineColor3D);
		AuxiliaryColorUI3D("最大外骨骼点质心", &Auxiliary_MaxOutlineCentreMassBool3D, Auxiliary_MaxOutlineCentreMassColor3D);
		AuxiliaryColorUI3D("碰撞碰撞点", &Auxiliary_CollisionDropBool3D, Auxiliary_CollisionDropColor3D);
		AuxiliaryColorUI3D("分离法向量", &Auxiliary_SeparateNormalVectorBool3D, Auxiliary_SeparateNormalVectorColor3D);
		AuxiliaryColorUI3D("碰撞点到两个物体重心的连线", &Auxiliary_CollisionDropToCenterOfGravityBool3D, Auxiliary_CollisionDropToCenterOfGravityColor3D);
		AuxiliaryColorUI3D("被选中物体被划分网格到的网格位置", &Auxiliary_GridDividedBool3D, Auxiliary_GridDividedColor3D);
	}

	bool PhysicsUI(PhysicsParticle3D *Object)
	{
		Dvec3ImGui3D("位置", &Object->pos);
		Dvec3ImGui3D("速度", &Object->speed);
		Dvec3ImGui3D("受力", &Object->force);
		ImGui::DragScalar("质量", MyImGuiDataType3D, &Object->mass, 0.0005f, NULL, NULL, "%.10f");

		return false;
	}

	bool PhysicsUI(PhysicsAngle3D *Object)
	{
		bool UpData = PhysicsUI((PhysicsParticle3D *)Object);
		// 2D 的标量 angle/angleSpeed/MomentInertia/torque 在 3D 分别变为
		// 四元数 rotation、Vec3_ angleSpeed、Mat3_ MomentInertia、Vec3_ torque。
		// 四元数与张量没有 DragScalar 的直接对应物，故这里显示只读摘要，
		// 数值本身交给 Draw*Auxiliary 的几何可视化去表达。
		ImGui::Text("朝向(四元数): w=%.6f x=%.6f y=%.6f z=%.6f",
					Object->rotation.w, Object->rotation.x, Object->rotation.y, Object->rotation.z);
		Dvec3ImGui3D("角速度", &Object->angleSpeed);
		Dvec3ImGui3D("扭矩", &Object->torque);
		ImGui::DragScalar("碰撞半径", MyImGuiDataType3D, &Object->radius, 0.0005f, NULL, NULL, "%.10f");
		return UpData;
	}

	bool PhysicsUI(PhysicsShape3D *Object)
	{
		bool UpData = PhysicsUI((PhysicsAngle3D *)Object);
		Dvec3ImGui3D("质心", &Object->CentreMass);
		Dvec3ImGui3D("几何中心", &Object->CentreShape);
		ImGui::Text("骨骼点: %u", Object->OutlineSize);
		ImGui::Text("骨骼棱: %u（棱线段补测用）", (unsigned int)Object->EdgeSet.size());
		if (ImGui::TreeNode("骨骼棱##edgetree"))
		{
			// 棱由 OutlineSet 派生：面片 4 点 → 4 条边，去重后只留折棱
			// （相邻面片法线不同），并剔除跨越凹口的弦。
			for (size_t i = 0; i < Object->EdgeSet.size(); ++i)
			{
				const Vec3_ A = Object->pos + QuatRotate(Object->rotation, Object->EdgeSet[i].A);
				const Vec3_ B = Object->pos + QuatRotate(Object->rotation, Object->EdgeSet[i].B);
				ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "(%f, %f, %f) → (%f, %f, %f) 摩擦 %.3f",
								   A.x, A.y, A.z, B.x, B.y, B.z, Object->EdgeSet[i].Friction);
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("骨骼点##tree"))
		{
			// 使用 map 统计每个坐标点出现的次数
			// （3D 的轮廓点结构为 OutlinePoint3D{LocalPos,Normal,Friction,SeedPos,Voxel,Corner}，
			//  替代 2D 的三条平行裸数组 OutlineSet/MaxOutlineCentreMass/FrictionSet。
			//  LocalPos 是方形面的顶点而非面中心，相邻面会共享同一条边上的顶点，
			//  因此按世界坐标去重仍能看出哪些顶点被多个面共用。）

			std::map<std::tuple<FLOAT_, FLOAT_, FLOAT_>, int> pointCount;

			for (size_t i = 0; i < Object->OutlineSet.size(); ++i)
			{
				Vec3_ World = Object->pos + QuatRotate(Object->rotation, Object->OutlineSet[i].LocalPos);
				auto point = std::make_tuple(World.x, World.y, World.z);
				pointCount[point]++;
				ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // 默认颜色：白色

				// 如果这个点出现超过一次，换成另一种颜色
				if (pointCount[point] > 1)
				{
					color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // 红色
				}

				ImGui::TextColored(color, "(%f, %f, %f) 法线(%.2f, %.2f, %.2f) 摩擦 %.3f 体素(%d, %d, %d) 角%d",
								   World.x, World.y, World.z,
								   Object->OutlineSet[i].Normal.x, Object->OutlineSet[i].Normal.y, Object->OutlineSet[i].Normal.z,
								   Object->OutlineSet[i].Friction,
								   Object->OutlineSet[i].Voxel.x, Object->OutlineSet[i].Voxel.y, Object->OutlineSet[i].Voxel.z,
								   (int)Object->OutlineSet[i].Corner);
			}

			ImGui::TreePop();
		}

		if (Object->width > 0 && Object->height > 0 && Object->depth > 0)
		{
			// 体素编辑网格：2D 是 width×height 的二维选择表，
			// 3D 若把 depth 也铺开需要 width*height*depth 个控件（256³ 时不可能），
			// 故矩阵只铺 X/Y，Z 层通过下方的 z 拖拽控件切换。
			static IVec3_ GridPos{0, 0, 0};
			if (GridPos.x >= (int)Object->width)
				GridPos.x = 0;
			if (GridPos.y >= (int)Object->height)
				GridPos.y = 0;
			if (GridPos.z >= (int)Object->depth)
				GridPos.z = 0;

			unsigned char zmin = 0;
			unsigned char zmax = (unsigned char)(Object->depth > 255 ? 255 : Object->depth - 1);
			ImGui::DragScalar("Z 层", ImGuiDataType_U8, &GridPos.z, 0.2f, &zmin, &zmax);

			for (int y = (int)Object->height - 1; y >= 0; --y)
			{
				for (unsigned int x = 0; x < Object->width; ++x)
				{
					ImGui::PushID((int)(x * Object->height * Object->depth + y * Object->depth + GridPos.z));
					if (ImGui::Selectable("", Object->at((int)x, y, GridPos.z).Collision, 0, ImVec2(5, 5)))
					{
						GridPos.x = (int)x;
						GridPos.y = y;
					}
					if (x < (Object->width - 1))
						ImGui::SameLine();
					ImGui::PopID();
				}
			}

			if (GridImGui3D("(" + std::to_string(GridPos.x) + "," + std::to_string(GridPos.y) + "," + std::to_string(GridPos.z) + ")", &Object->at(GridPos)))
			{
				UpData = true;
			}
		}

		// 更新对象信息
		if (UpData)
		{
			Object->UpdateAll();
		}
		return UpData;
	}

	bool PhysicsUI(PhysicsCircle3D *Object)
	{
		bool UpData = PhysicsUI((PhysicsAngle3D *)Object);
		return UpData;
	}

	bool PhysicsUI(PhysicsLine3D *Object)
	{
		bool UpData = PhysicsUI((PhysicsAngle3D *)Object);
		// 3D 把 2D 混用的 radius 拆成 HalfLength（半长）与 Thickness（胶囊厚度），
		// 这两项只读展示，避免误改导致惯量张量失真。
		ImGui::Text("半长度: %.10f  厚度: %.10f", Object->HalfLength, Object->Thickness);
		return UpData;
	}

	bool PhysicsUI(PhysicsWorld3D *Object)
	{
		Dvec3ImGui3D("重力加速度", &Object->GravityAcceleration);
		if (Object->WindBool)
		{
			Dvec3ImGui3D("风力", &Object->Wind);
		}
		ImGui::Text("物理网格：%d", (int)Object->PhysicsShapeS.size());
		ImGui::Text("物理点：%d", (int)Object->PhysicsParticleS.size());
		ImGui::Text("物理圆：%d", (int)Object->PhysicsCircleS.size());
		ImGui::Text("物理线：%d", (int)Object->PhysicsLineS.size());
		ImGui::Text("关节：%d", (int)Object->PhysicsJointS.size());
		ImGui::Text("绳子：%d", (int)Object->BaseJunctionS.size());
		ImGui::Text("组装体：%d", (int)Object->PhysicsAssemblyS.size());

		bool UpData = false;

		/*
		MapFormwork3D *Map = Object->GetMapFormwork();
		for (int z = Map->FMGetMapSize().z - 1; z >= 0; --z)
		{
			for (int y = Map->FMGetMapSize().y - 1; y >= 0; --y)
			{
				for (size_t x = 0; x < Map->FMGetMapSize().x; ++x)
				{
					ImGui::PushID((int)((x * Map->FMGetMapSize().y + y) * Map->FMGetMapSize().z + z));
					if (ImGui::Selectable("", Map->FMGetCollide(IVec3_{(int)x, y, z}), 0, ImVec2(25, 25)))
					{
						UpData = true;
						Map->FMGetGridBlock(IVec3_{(int)x, y, z}).Collision = !Map->FMGetGridBlock(IVec3_{(int)x, y, z}).Collision;
						Map->FMGetGridBlock(IVec3_{(int)x, y, z}).Entity = !Map->FMGetGridBlock(IVec3_{(int)x, y, z}).Entity;
					}
					if (x < (Map->FMGetMapSize().x - 1))
						ImGui::SameLine();
					ImGui::PopID();
				}
			}
		}
		*/
		return UpData;
	}

}
