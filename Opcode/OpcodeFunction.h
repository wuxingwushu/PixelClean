#pragma once
#include "../Tool/Queue.h"
#include <string>
#include "../Tool/trie.h"
#include <iostream>

namespace GAME {
	class Labyrinth;
	class Crowd;
	class GamePlayer;
	class Application;
}

namespace Opcode {

	//指令对象数据
	extern GAME::Labyrinth* OpLabyrinth;
	extern GAME::Crowd* OpCrowd;
	extern GAME::GamePlayer* OpGamePlayer;
	extern GAME::Application* OpApplication;

	void OPNULLPTR(Queue<const char*>* CodeS);

	typedef void (*_CommandFunction)(Queue<const char*>*);

	class OpcodeMapping
	{
	private:
		static OpcodeMapping* mOpcodeMapping;
		OpcodeMapping();

		std::map<std::string, _CommandFunction> CommandFunction;//函数映射
		trie<std::string> CommandPrompt;//代码名多叉树
		std::map<std::string, std::string> CommandAnnotation;//注释映射

		//添加
		void AddCommand(_CommandFunction Function, const std::string code, const std::string Annotation) {
			CommandPrompt.insert(code);
			CommandFunction.insert(std::make_pair(code, Function));
			CommandAnnotation.insert(std::make_pair(code, Annotation));
		}
	public:
		static OpcodeMapping* GetOpcodeMapping() {
			if (mOpcodeMapping == nullptr) {
				mOpcodeMapping = new OpcodeMapping();
			}
			return mOpcodeMapping;
		}
		~OpcodeMapping();

		//获取对应函数回调
		_CommandFunction GetCommandFunction(std::string Command) {
			if (CommandFunction.find(Command) != CommandFunction.end())//判断是否存在键
			{
				return CommandFunction[Command];
			}
			else
			{
				std::cerr << "不存在: " << Command << std::endl;
				return OPNULLPTR;
			}
		}

		//获取代码提示
		std::vector<const std::string*> GetCodePrompt(std::string str) {
			return CommandPrompt.complete(str);
		}

		//获取注释
		const char* GetAnnotation(std::string code) {
			return CommandAnnotation[code].c_str();
		}
	};


	

	//添加NPC
	void AddNPC(Queue<const char*>* CodeS);
	//添加设置了位置的NPC
	void AddNPCPos(Queue<const char*>* CodeS);
	//添加多个NPC
	void AddNPCS(Queue<const char*>* CodeS);
	//清除所有NPC
	void KillAllNPC(Queue<const char*>* CodeS);
	//更换地图
	void ReplaceMap(Queue<const char*>* CodeS);
	//迷雾开关
	void SetMistSwitch(Queue<const char*>* CodeS);
	//线框模式
	void SetPipelineLinesMode(Queue<const char*>* CodeS);
}