#include "Opcode.h"
#include "OpcodeFunction.h"

namespace Opcode {

	std::string RemoveExcessiveSpaces(std::string str) {
		int jie;
		if (str.size() < 1) {
			return str;
		}
		for (size_t i = 0; i < str.size() - 1; ++i)
		{
			if (str[i] == ' ') {
				for (size_t x = i; x < str.size(); ++x) {
					if (str[x] != ' ') {
						i++;
						jie = x;
						break;
					}
					if (x == (str.size() - 1)) {
						jie = str.size();
					}
				}
				str = str.substr(0, i) + str.substr(jie, str.size() - jie);
			}
		}
		if (str[0] == ' ')
		{
			str = str.substr(1, str.size() - 1);
		}
		return str;
	}

	//操作码读取
	void CodeExplain(Queue<const char*>* CodeS) {
		if (CodeS->GetNumber() > 0) {
			OpcodeMapping::GetOpcodeMapping()->GetCommandFunction(*CodeS->pop())(CodeS);
		}
	}

	//操作码读取
	void Opcode(std::string Code) {
		if (Code.size() != 0) {//判断存在指令
			std::vector<std::string> CodeBuffer;
			Queue<const char*> CodeS(10);
			Code = RemoveExcessiveSpaces(Code);//把间隔空格处理的只剩下一个
			for (size_t i = 0; i < Code.size(); ++i)
			{
				if (Code[i] == ' ') {
					CodeBuffer.push_back(Code.substr(0, i));//把内容提取出来
					Code = Code.substr(i + 1, Code.size() - 1 - i);//把已经提取的内容出除
					i = 0;//从头开始处理
				}
			}
			CodeBuffer.push_back(Code);//把最后的指令加入
			for (size_t i = 0; i < CodeBuffer.size(); ++i)
			{
				CodeS.add(CodeBuffer[i].c_str());
			}
			CodeExplain(&CodeS);
		}
	}

	std::vector<const std::string*> GetOpCodePrompt(std::string str) {
		return OpcodeMapping::GetOpcodeMapping()->GetCodePrompt(str);
	}

	const char* GetOpAnnotation(std::string str) {
		return OpcodeMapping::GetOpcodeMapping()->GetAnnotation(str);
	}
}