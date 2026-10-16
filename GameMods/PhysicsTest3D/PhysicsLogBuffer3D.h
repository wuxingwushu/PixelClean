#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <deque>

/*
 * ═══════════════════════════════════════════════════════════════════════════
 * PhysicsBlock3D::PhysicsLogBuffer3D —— 3D 沙盒外壳专用日志缓冲
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * 由 2D 版 `GameMods/PhysicsTest/PhysicsLogBuffer.{h,cpp}` 逐行改写。
 * 差异只有两处：
 *   1. 命名空间 `PhysicsBlock` → `PhysicsBlock3D`，类名加 `3D` 后缀 ——
 *      否则与 2D 版同名类在同一工程里链接冲突（两者都会被 `add_subdirectory`
 *      编进同一个可执行文件）。
 *   2. 便利宏名 `PhysicsLog` → `PhysicsLog3D`，原因同上：2D 版在
 *      `PhysicsLogBuffer.h` 里无条件 `#define PhysicsLog(...)`，两个头文件
 *      同 TU 包含时会产生**宏重定义**。3D 版取独立名字，两套日志可同时存在。
 *
 * 语义与 2D 完全一致：单例、线程安全（互斥量）、`std::deque` 环形裁剪、
 * 支持 `printf` 风格的变参重载。
 */
namespace PhysicsBlock3D
{
	class PhysicsLogBuffer3D
	{
	public:
		/** @brief 获取全局单例（C++11 魔法静态，线程安全） */
		static PhysicsLogBuffer3D &GetInstance();

		/** @brief 追加一条已经格式化好的日志 */
		void AddLog(const std::string &log);

		/** @brief 追加一条 printf 风格日志（最长 4096 字符，超出截断） */
		void AddLog(const char *format, ...);

		/** @brief 清空全部日志 */
		void Clear();

		/** @brief 当前日志条数 */
		size_t GetLogCount() const;

		/** @brief 按下标取日志（越界返回空串，与 2D 行为一致） */
		const std::string &GetLog(size_t index) const;

		/** @brief 取全部日志（供 UI 一次性快照） */
		const std::deque<std::string> &GetAllLogs() const;

		/** @brief 设置最大保留条数（超出后从头部丢弃） */
		void SetMaxLogCount(size_t maxCount);

	private:
		PhysicsLogBuffer3D();
		~PhysicsLogBuffer3D() = default;

		// 单例，禁止拷贝/赋值
		PhysicsLogBuffer3D(const PhysicsLogBuffer3D &) = delete;
		PhysicsLogBuffer3D &operator=(const PhysicsLogBuffer3D &) = delete;

		/** @brief 日志环形缓冲（deque 便于从头部裁剪） */
		std::deque<std::string> mLogs;

		/** @brief 保护 mLogs 的互斥量（物理步进线程与 UI 线程都会写） */
		mutable std::mutex mMutex;

		/** @brief 最大保留条数，默认 1000（与 2D 相同） */
		size_t mMaxLogCount;
	};
}

/**
 * @brief 3D 沙盒的日志宏
 * @note 与 2D 的 `PhysicsLog(...)` 是两个不同的宏名，可同时使用；
 *       调用点写法一致：`Log3D("物体数 %d\n", n);`
 */
#define Log3D(...) PhysicsBlock3D::PhysicsLogBuffer3D::GetInstance().AddLog(__VA_ARGS__)
