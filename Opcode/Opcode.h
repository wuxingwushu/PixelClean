#pragma once
#include <string>
#include <vector>

namespace Opcode {

	//操作码读取
	void Opcode(std::string Code);
	
	//代码提示
	std::vector<const std::string*> GetOpCodePrompt(std::string str);

	const char* GetOpAnnotation(std::string str);
}








