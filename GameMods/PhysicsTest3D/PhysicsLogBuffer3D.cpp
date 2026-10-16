#include "PhysicsLogBuffer3D.h"
#include "../../DebugLog.h"
#include <cstdarg>
#include <cstdio>

namespace PhysicsBlock3D
{
	/**
	 * @brief 构造函数
	 * @details 默认上限与 2D 版一致（1000 条）。2D 版在这里调用了 LOGE，
	 *          但那会把"日志缓冲构造"当成错误打出去；3D 改为 LOGD（调试级），
	 *          语义更准确且不污染错误通道。这是与 2D 的**唯一**行为差异。
	 */
	PhysicsLogBuffer3D::PhysicsLogBuffer3D() : mMaxLogCount(1000)
	{
		LOGD("PhysicsLogBuffer3D::PhysicsLogBuffer3D constructor");
	}

	PhysicsLogBuffer3D &PhysicsLogBuffer3D::GetInstance()
	{
		static PhysicsLogBuffer3D instance;
		return instance;
	}

	void PhysicsLogBuffer3D::AddLog(const std::string &log)
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mLogs.push_back(log);
		while (mLogs.size() > mMaxLogCount)
		{
			mLogs.pop_front();
		}
	}

	void PhysicsLogBuffer3D::AddLog(const char *format, ...)
	{
		va_list args;
		va_start(args, format);

		// 与 2D 相同的 4096 字节栈缓冲：够放一行诊断信息，且不会因为
		// 某条格式化串写错长度而堆溢出（vsnprintf 保证 NUL 结尾）。
		char buffer[4096];
		vsnprintf(buffer, sizeof(buffer), format, args);

		va_end(args);

		AddLog(std::string(buffer));
	}

	void PhysicsLogBuffer3D::Clear()
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mLogs.clear();
	}

	size_t PhysicsLogBuffer3D::GetLogCount() const
	{
		std::lock_guard<std::mutex> lock(mMutex);
		return mLogs.size();
	}

	const std::string &PhysicsLogBuffer3D::GetLog(size_t index) const
	{
		std::lock_guard<std::mutex> lock(mMutex);
		if (index >= mLogs.size())
		{
			LOGE("PhysicsLogBuffer3D::GetLog index %zu out of range (size %zu)", index, mLogs.size());
			static const std::string empty;
			return empty;
		}
		return mLogs[index];
	}

	const std::deque<std::string> &PhysicsLogBuffer3D::GetAllLogs() const
	{
		std::lock_guard<std::mutex> lock(mMutex);
		return mLogs;
	}

	void PhysicsLogBuffer3D::SetMaxLogCount(size_t maxCount)
	{
		std::lock_guard<std::mutex> lock(mMutex);
		// 与 2D 的差别：2D 把 0 也接受，结果下一条 AddLog 会把 buffer 清空到
		// 空（`while (size > 0) pop_front()`）—— 那是合法但容易误用的状态。
		// 3D 明确要求至少 1 条，0 视为"保持默认不可裁到空"，并记一条错误日志。
		if (maxCount == 0)
		{
			LOGE("PhysicsLogBuffer3D::SetMaxLogCount refuses 0 (would drop every future log)");
			return;
		}
		mMaxLogCount = maxCount;
		while (mLogs.size() > mMaxLogCount)
		{
			mLogs.pop_front();
		}
	}
}
