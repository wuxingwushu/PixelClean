#include "OpcodeFunction.h"
#include <map>
#include <algorithm>
#include <unordered_map>
#include <stdexcept>
#include <sstream>

#include "../Labyrinth/Labyrinth.h"
#include "../Character/Crowd.h"
#include "../application.h"

namespace Opcode {

	//指令对象数据
	GAME::Labyrinth* OpLabyrinth = nullptr;
	GAME::Crowd* OpCrowd = nullptr;
	GAME::GamePlayer* OpGamePlayer = nullptr;
	GAME::Application* OpApplication = nullptr;

	void OPNULLPTR(Queue<const char*>* CodeS) {
		return;
	}

	OpcodeMapping* OpcodeMapping::mOpcodeMapping = nullptr;
	OpcodeMapping::OpcodeMapping()
	{
		AddCommand(AddNPC, "AddNPC", "AddNPC() 随机位置生成一个 NPC ");
		AddCommand(AddNPCPos, "AddNPCPos", "AddNPCPos(int, int) 设置位置生成一个 NPC ");
		AddCommand(AddNPCS, "AddNPCS", "AddNPCS(int) 随机位置生成 N 个 NPC "); 
		AddCommand(KillAllNPC, "KillAllNPC", "KillAllNPC() 清空所有 NPC ");
		AddCommand(ReplaceMap, "ReplaceMap", "ReplaceMap(int, int) 更换地图(X,Y 都要 >= 5 )");
		AddCommand(SetMistSwitch, "SetMistSwitch", "SetMistSwitch(bool) 设置迷雾开关");
		AddCommand(SetPipelineLinesMode, "SetPipelineLinesMode", "SetPipelineLinesMode(bool) 设置线框渲染开关");
	}

	OpcodeMapping::~OpcodeMapping()
	{
		mOpcodeMapping = nullptr;
	}

	template <typename T>
	inline T OpConverter(const std::string& s) {
		try {
			T v{};
			std::istringstream _{ s };
			_.exceptions(std::ios::failbit);
			_ >> v;
			return v;
		}
		catch (std::exception& e) {
			throw std::runtime_error("cannot parse value '" + s + "' to type<T>.");
		};
	}

	bool OpBoolConverter(std::string s) {
		std::transform(s.begin(), s.end(), s.begin(), ::tolower);
		static const std::unordered_map<std::string, bool> s2b{
			{"1", true}, { "true", true }, { "yes", true }, { "on", true },
			{ "0", false }, { "false", false }, { "no", false }, { "off", false },
		};
		auto const value = s2b.find(s);
		if (value == s2b.end()) {
			throw std::runtime_error("'" + s + "' is not a valid boolean value.");
		}
		return value->second;
	}

	
	//添加NPC
	void AddNPC(Queue<const char*>* CodeS) {
		glm::ivec2 pos = OpLabyrinth->GetLegitimateGeneratePos();
		OpCrowd->AddNPC(pos.x, pos.y);
	}

	//添加设置了位置的NPC
	void AddNPCPos(Queue<const char*>* CodeS) {
		if (CodeS->GetNumber() == 2) {
			OpCrowd->AddNPC(OpConverter<int>(*CodeS->pop()), OpConverter<int>(*CodeS->pop()));
		}
		else
		{
			std::cout << "[Opcode][Error]: AddNPCPos Generate Error ! " << std::endl;
		}
	}

	//添加多个NPC
	void AddNPCS(Queue<const char*>* CodeS) {
		if (CodeS->GetNumber() == 1) {
			int S = OpConverter<int>(*CodeS->pop());
			for (size_t i = 0; i < S; i++)
			{
				glm::ivec2 pos = OpLabyrinth->GetLegitimateGeneratePos();
				OpCrowd->AddNPC(pos.x, pos.y);
			}
		}
		else
		{
			std::cout << "[Opcode][Error]: AddNPCS Generate Error ! " << std::endl;
		}
	}

	//清除所有NPC
	void KillAllNPC(Queue<const char*>* CodeS) {
		OpCrowd->KillAll();
	}

	//更换地图
	void ReplaceMap(Queue<const char*>* CodeS) {
		if (CodeS->GetNumber() == 2) {
			OpCrowd->KillAll();
			OpLabyrinth->AgainGenerateLabyrinth(OpConverter<int>(*CodeS->pop()), OpConverter<int>(*CodeS->pop()));
			OpGamePlayer->GetObjectCollision()->SetPos(OpLabyrinth->GetLegitimateGeneratePos());
		}
		else
		{
			std::cout << "[Opcode][Error]: ReplaceMap Generate Error ! " << std::endl;
		}
		
	}

	//迷雾开关
	void SetMistSwitch(Queue<const char*>* CodeS) {
		if (CodeS->GetNumber() == 1) {
			Global::MistSwitch = OpBoolConverter(*CodeS->pop());
			Global::MainCommandBufferUpdateRequest();
		}
		else
		{
			std::cout << "[Opcode][Error]: SetMistSwitch Generate Error ! " << std::endl;
		}
	}

	//线框模式
	void SetPipelineLinesMode(Queue<const char*>* CodeS) {
		if (CodeS->GetNumber() == 1) {
			Global::DrawLinesMode = OpBoolConverter(*CodeS->pop());
			OpApplication->recreateSwapChain();
		}
		else
		{
			std::cout << "[Opcode][Error]: SetPipelineLinesMode Generate Error ! " << std::endl;
		}
	}
}