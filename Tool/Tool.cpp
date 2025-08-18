#include "Tool.h"
#include <iostream>
#include <filesystem>


namespace TOOL {
	Timer* mTimer = nullptr;
	void InitTimer() {
		if (mTimer == nullptr)
		{
			mTimer = new Timer();
		}
	}



#ifdef TOOL_ThreadPool
	ThreadPool* mThreadPool = nullptr;//声明全局类（唯一）
	unsigned int mThreadCount;

	void InitThreadPool(unsigned int ThreadCount) {
		if (mThreadPool == nullptr) {
			if (ThreadCount == 0)
			{
				mThreadCount = std::thread::hardware_concurrency() - 1;
				mThreadPool = new ThreadPool(mThreadCount);
			}
			else
			{
				mThreadCount = ThreadCount;
				mThreadPool = new ThreadPool(mThreadCount);
			}
		}
	}

	void DeleteThreadPool() {
		if (mThreadPool != nullptr) {
			delete mThreadPool;
		}
	}
#endif

#ifdef TOOL_SpdLog
	spdlog::logger* Error = nullptr;
	spdlog::logger* VulKanError = nullptr;

	void InitSpdLog() {
		if (Error == nullptr) {
			auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
			console_sink->set_level(spdlog::level::warn);//设置警报等级
			console_sink->set_pattern("[Error] [%^%l%$] %v");//打印显示

			auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("Logs/Error.txt", false);//日志文件路径，是否覆写
			file_sink->set_level(spdlog::level::trace);//设置警报等级

			Error = new spdlog::logger("Error", { console_sink, file_sink }); // 日志保存
			Error->set_level(spdlog::level::debug);//设置日志警报保存等级
		}
		
		if (VulKanError == nullptr) {
			auto VulKanconsole_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
			VulKanconsole_sink->set_level(spdlog::level::warn);//设置警报等级
			VulKanconsole_sink->set_pattern("[VulKanError] [%^%l%$] %v");//打印显示

			auto VulKanfile_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("Logs/VulKanError.txt", false);//日志文件路径，是否覆写
			VulKanfile_sink->set_level(spdlog::level::trace);//设置警报等级

			VulKanError = new spdlog::logger("VulKanError", { VulKanconsole_sink, VulKanfile_sink }); // 日志保存
			VulKanError->set_level(spdlog::level::debug);//设置日志警报保存等级
		}
	}

	void DeleteSpdLog() {
		if (Error != nullptr) {
			Error = nullptr;
			delete Error;
		}
		if (VulKanError != nullptr) {
			VulKanError = nullptr;
			delete VulKanError;
		}
	}
#endif

#ifdef TOOL_Log
	FILE* fpError;
	FILE* fpVulKanError;

	void InitLog() {
		log_set_level(0);
		log_set_quiet(0);//是否打印到控制台

		fpError = fopen("./Logs/log_info.txt", "ab+");
		fpVulKanError = fopen("./Logs/log_debug.txt", "ab+");

		log_add_fp(fpError, LOG_INFO);
		log_add_fp(fpVulKanError, LOG_DEBUG);
	}

	void DeleteLog() {
		fclose(fpError);
		fclose(fpVulKanError);
	}
#endif

#ifdef TOOL_PerlinNoise
	PerlinNoise* mPerlinNoise = nullptr;

	void InitPerlinNoise() {
		if (mPerlinNoise == nullptr) {
			mPerlinNoise = new PerlinNoise();
		}
	}
	void DeletePerlinNoise() {
		if (mPerlinNoise != nullptr) {
			delete mPerlinNoise;
		}
	}
#endif

#ifdef TOOL_Convert
	std::string StrTolower(std::string Str) {
		std::string str;
		for (size_t i = 0; i < Str.size(); ++i)
		{
			str += tolower(Str[i]);
		}
		return str;
	}

	std::string StrName(std::string Str) {
		size_t dianI = Str.size();
		size_t xieI = 0;
		for (size_t i = Str.size() - 1; i > 0; --i) {
			if (Str[i] == '.') {
				dianI = i - 1;
			}
			if (Str[i] == '\\') {
				dianI -= i;
				xieI = i + 1;
				break;
			}
		}
		return Str.substr(xieI, dianI);
	}

	void FilePath(const char* path, std::vector<std::string>* strS, const char* Suffix, const char* Name, int* Index) {
		std::string ModelFileName;
		for (const auto& entry : std::filesystem::directory_iterator(path)) {
			ModelFileName = entry.path().filename().string();//获取文件名字
			for (size_t i = 0; i < ModelFileName.size(); ++i)
			{
				if ((ModelFileName[i] == '.') && (StrTolower(ModelFileName.substr(i + 1, ModelFileName.size() - i - 1)) == StrTolower(Suffix))) {
					ModelFileName = ModelFileName.substr(0, i);
					strS->push_back(ModelFileName);
					if (ModelFileName == Name) {
						Index[0] = strS->size() - 1;
					}
				}
			}
		}
	}


	void* memcpyf(void* dest, const void* src, size_t n, size_t b) { //n 是有多少个数据， b 是数据多少个字节 sizeof()获得有多少个字节
		char* pDest = (char*)dest;
		const char* pSrc = (const char*)src;
		pDest += (b - 1);
		pSrc += (b - 1);
		n *= b;
		while (n--) {
			*pDest-- = *pSrc--;
		}
		return dest;
	}

	template<typename T, typename U, size_t>void TypenameMemcpy(T* t, U* u, size_t n) {
		while (n--) {
			*t++ = *u++;
		}
	}

	//ostringstream对象用来进行格式化的输出，常用于将各种类型转换为string类型
	//ostringstream只支持<<操作符
	template<typename T>std::string toString(const T& t)
	{
		std::ostringstream oss;  //创建一个格式化输出流
		oss << t;             //把值传递如流中
		return oss.str();
	}

	//将一个int转换为4个char
	void ConvertIntTo4Char(const int IntValue, char* Chars)
	{
		memcpy(Chars, (char*)&IntValue, 4);
	}

	//将4个char转换位一个int
	void Convert4CharToInt(const char* Chars, int* IntValue)
	{
		memcpy((char*)IntValue, Chars, 4);
	}

	//string 转 wstring
	std::string ws2s(const std::wstring& ws)
	{
		size_t i;
		std::string curLocale = setlocale(LC_ALL, NULL);
		setlocale(LC_ALL, "chs");
		const wchar_t* _source = ws.c_str();
		size_t _dsize = 2 * ws.size() + 1;
		char* _dest = new char[_dsize];
		memset(_dest, 0x0, _dsize);
		wcstombs_s(&i, _dest, _dsize, _source, _dsize);
		std::string result = _dest;
		delete[] _dest;
		setlocale(LC_ALL, curLocale.c_str());
		return result;
	}

	//wstring 转 string
	std::wstring s2ws(const std::string& s)
	{
		size_t i;
		std::string curLocale = setlocale(LC_ALL, NULL);
		setlocale(LC_ALL, "chs");
		const char* _source = s.c_str();
		size_t _dsize = s.size() + 1;
		wchar_t* _dest = new wchar_t[_dsize];
		wmemset(_dest, 0x0, _dsize);
		mbstowcs_s(&i, _dest, _dsize, _source, _dsize);
		std::wstring result = _dest;
		delete[] _dest;
		setlocale(LC_ALL, curLocale.c_str());
		return result;
	}

	//Unicode 转到 utf8
	std::string UnicodeToUtf8(const std::string& str) {
		const std::wstring wstr = s2ws(str);
		std::vector<char> utf8buf;
		utf8buf.reserve(wstr.length() * 4);

		for (const auto& wc : wstr) {
			if (wc < 0x80) {
				utf8buf.push_back(static_cast<char>(wc));
			}
			else if (wc < 0x800) {
				utf8buf.push_back(static_cast<char>((wc >> 6) | 0xC0));
				utf8buf.push_back(static_cast<char>((wc & 0x3F) | 0x80));
			}
			else if (wc < 0x10000) {
				utf8buf.push_back(static_cast<char>((wc >> 12) | 0xE0));
				utf8buf.push_back(static_cast<char>(((wc >> 6) & 0x3F) | 0x80));
				utf8buf.push_back(static_cast<char>((wc & 0x3F) | 0x80));
			}
			else {
				utf8buf.push_back(static_cast<char>((wc >> 18) | 0xF0));
				utf8buf.push_back(static_cast<char>(((wc >> 12) & 0x3F) | 0x80));
				utf8buf.push_back(static_cast<char>(((wc >> 6) & 0x3F) | 0x80));
				utf8buf.push_back(static_cast<char>((wc & 0x3F) | 0x80));
			}
		}

		return std::string(utf8buf.data(), utf8buf.size());
	}

	//utf8 转到 Unicode
	std::string Utf8ToUnicode(const std::string& utf8_str) {
		std::wstring result;
		int i = 0;

		while (i < utf8_str.size()) {
			wchar_t ch = 0;
			unsigned char byte = utf8_str[i];

			if (byte <= 0x7F) {
				// Single-byte character
				ch = byte;
				i += 1;
			}
			else if ((byte & 0xE0) == 0xC0) {
				// Two-byte character
				ch |= (byte & 0x1F) << 6;
				ch |= (utf8_str[i + 1] & 0x3F);
				i += 2;
			}
			else if ((byte & 0xF0) == 0xE0) {
				// Three-byte character
				ch |= (byte & 0x0F) << 12;
				ch |= (utf8_str[i + 1] & 0x3F) << 6;
				ch |= (utf8_str[i + 2] & 0x3F);
				i += 3;
			}
			else if ((byte & 0xF8) == 0xF0) {
				// Four-byte character
				ch |= (byte & 0x07) << 18;
				ch |= (utf8_str[i + 1] & 0x3F) << 12;
				ch |= (utf8_str[i + 2] & 0x3F) << 6;
				ch |= (utf8_str[i + 3] & 0x3F);
				i += 4;
			}
			else {
				// Invalid byte
				i += 1;
			}

			result += ch;
		}

		return TOOL::ws2s(result);
	}
#endif

#ifdef TOOL_FPS
	/*********************************************- FPS -*********************************************/
	clock_t kaishi_time, jieshu_time;//储存时间戳
	int number_time = 0;//当前第几帧
	const int number = 60;//多少帧刷新一次
	const double miao_time = (number + 1) * CLOCKS_PER_SEC;//用来计算FPS的数
	double FPSNumber = 0.0f;//帧数
	double FPStime = 0.0f;//帧时间

	float values[values_number] = {};//储存FPS数据
	float Max_values; //FPS数据 最大值
	float Min_values;//FPS数据 最小值
	double Mean_values;//平均帧数



	float LMax_FrameAmplitude;
	float LMin_FrameAmplitude;
	float Max_FrameAmplitude; //帧振幅 最大值
	float Min_FrameAmplitude;//帧振幅 最小值
	float FrameAmplitude;//帧振幅
	float FrameAmplitudeAccumulate;//帧振幅累积


	clock_t zhenfu_time;

	void FPS()
	{
		if (number_time >= number) {
			FrameAmplitude = FrameAmplitudeAccumulate;
			Max_FrameAmplitude = LMax_FrameAmplitude;
			Min_FrameAmplitude = LMin_FrameAmplitude;
			LMax_FrameAmplitude = 0;
			LMin_FrameAmplitude = 100.0f;
			FrameAmplitudeAccumulate = 0;
			number_time = 0;
			jieshu_time = clock();
			FPSNumber = miao_time / double(jieshu_time - kaishi_time);
			FPStime = 1.0f / FPSNumber;
			Max_values = 0.0f;
			Min_values = 10000.0f;
			Mean_values = 0.0f;
			for (int i = 0; i < values_number; ++i) {
				Mean_values += values[i];

				if (values_number == i + 1) {
					values[i] = FPSNumber;
				}
				else {
					values[i] = values[i + 1];
				}

				if (values[i] > Max_values) {
					Max_values = values[i];
				}
				if (values[i] < Min_values) {
					Min_values = values[i];
				}
			}
			Mean_values = Mean_values / values_number;
			kaishi_time = clock();
		}
		else {
			float Ltame = (float(clock() - zhenfu_time) / CLOCKS_PER_SEC) - FPStime;
			zhenfu_time = clock();
			Ltame = (Ltame < 0 ? -Ltame : Ltame);
			if (LMax_FrameAmplitude < Ltame) {
				LMax_FrameAmplitude = Ltame;
			}
			if (LMin_FrameAmplitude > Ltame) {
				LMin_FrameAmplitude = Ltame;
			}
			FrameAmplitudeAccumulate += Ltame;
			++number_time;
		}
	}
#endif



#ifdef TOOL_Timing
	/*********************************************- 耗时检测 -*********************************************/

	clock_t TemporaryCycleTime;//周期总耗时开始时间戳
	clock_t CycleTime = 100;//周期总耗时
	int Gap = 100;//间隔
	int CurrentCount = 0;//现在是第次轮回
	bool DetectionSwitch = false;//更新开关
	bool DetectionQuantityName = true;//第一次检测开关
	int Quantity = 0;//总检测数量
	int MomentQuantity = 1;// 0 被当作是 NULL 的索引判断 所以从 1 开始数起
	int DetectionCount = 0;//现在是检测第几个

	//嵌套堆载
	clock_t TemporaryConsumetime[100]{};//临时时间堆载
	clock_t TemporaryConsumeName[100]{};//临时嵌套索引堆载
	int TemporaryTimeQuantity = -1; //堆载指针

	//结果数据
	int ConsumeNumber;//最多检测数量
	char* Consume_name[DetectionNumber]{};//储存检测的名字
	char* MomentConsume_name[DetectionNumber]{};
	clock_t TemporaryConsume_time[DetectionNumber]{};//储存检测的周期总累加耗时
	double Consume_time[DetectionNumber]{};//储存检测的百分比
	double Consume_Second[DetectionNumber]{};//储存检测的秒
	double MomentConsume_Second[DetectionNumber]{};

	//时间记录
	bool SecondVectorBool[DetectionNumber]{}; //秒数据 最大值
	float* Consume_SecondVector[DetectionNumber]{};//储存检测的秒数组
	int SecondVectorIndex[DetectionNumber]{};//秒数组索引
	float Max_Secondvalues[SecondVectorNumber]{}; //秒数据 最大值
	float Min_Secondvalues[SecondVectorNumber]{};//秒数据 最小值

	void StartTiming(char* name, bool RecordBool)
	{
		if (DetectionQuantityName) {
			SecondVectorBool[Quantity] = RecordBool;//这个检测对象是否开启了记录
			if ((Quantity != 0) && (name == Consume_name[0])) {
				DetectionQuantityName = false;//所有检测录入一边（注意，要是一次循环没有录入就是没有录入，会有BUG，所以应用时要让他第一次全部录入进去）
				DetectionCount = 0;//新的一轮记得设为 0 ，要不然会出问题
				ConsumeNumber = Quantity;
			}
			else {
				Consume_name[Quantity] = name;
				if (SecondVectorBool[Quantity]) {
					Consume_SecondVector[Quantity] = new float[SecondVectorNumber] {};//申请记录时间数据用的空间
				}
				Quantity++;
			}
		}
		else {
			if (name == Consume_name[0]) {//判断是否是新的一轮
				DetectionCount = 0;
			}
		}
		TemporaryTimeQuantity++;//堆载指针压载
		TemporaryConsumetime[TemporaryTimeQuantity] = clock();
		TemporaryConsumeName[TemporaryTimeQuantity] = DetectionCount + TemporaryTimeQuantity;//压入索引
	}

	void StartEnd()
	{
		TemporaryConsume_time[TemporaryConsumeName[TemporaryTimeQuantity]] += (clock() - TemporaryConsumetime[TemporaryTimeQuantity]);
		TemporaryTimeQuantity--;//堆载指针出载
		DetectionCount++;
	}

	void RefreshTiming()
	{
		if (DetectionSwitch) {
			CycleTime = clock() - TemporaryCycleTime;//Interval 个轮回，结束计时，得出时间
			for (int i = 0; i < ConsumeNumber; i++) {
				Consume_time[i] = (double(TemporaryConsume_time[i] * 100) / CycleTime);//求出他在一个帧周期的耗时占比
				Consume_Second[i] = (double(TemporaryConsume_time[i]) / (CLOCKS_PER_SEC * Gap));//他所花时间
				TemporaryConsume_time[i] = 0;//清零，累计下 Interval 个轮回的时间

				if (SecondVectorBool[i]) {//时间是否记录
					Max_Secondvalues[i] = -10000.0f;
					Min_Secondvalues[i] = 10000.0f;
					for (int j = 0; j < SecondVectorNumber; j++) {
						if (SecondVectorNumber == j + 1) {
							Consume_SecondVector[i][j] = float(Consume_Second[i]);
						}
						else {
							Consume_SecondVector[i][j] = Consume_SecondVector[i][j + 1];
						}

						if (Consume_SecondVector[i][j] > Max_Secondvalues[i]) {
							Max_Secondvalues[i] = Consume_SecondVector[i][j];
						}
						if (Consume_SecondVector[i][j] < Min_Secondvalues[i]) {
							Min_Secondvalues[i] = Consume_SecondVector[i][j];
						}
					}
				}
			}
			DetectionSwitch = false;
			TemporaryCycleTime = clock();//Interval 个轮回，开始计时
		}
		else {
			CurrentCount++;//一轮结束
			if (CurrentCount >= Gap) {//判断第 Interval 就更新显示
				CurrentCount = 0;
				DetectionSwitch = true;
			}
		}
	}

	void MomentTiming(char* name, int* Index)
	{
		if (Index[0] == NULL) {
			Index[0] = MomentQuantity;
			MomentConsume_name[MomentQuantity] = name;
			MomentConsume_Second[MomentQuantity] = 0.0f;
			MomentQuantity++;
		}
		TemporaryTimeQuantity++;//堆载指针压载
		TemporaryConsumetime[TemporaryTimeQuantity] = clock();
		TemporaryConsumeName[TemporaryTimeQuantity] = Index[0];//压入索引
	}

	void MomentEnd()
	{
		MomentConsume_Second[TemporaryConsumeName[TemporaryTimeQuantity]] = double(clock() - TemporaryConsumetime[TemporaryTimeQuantity]) / CLOCKS_PER_SEC;
		TemporaryTimeQuantity--;//堆载指针出载
	}
#endif
}