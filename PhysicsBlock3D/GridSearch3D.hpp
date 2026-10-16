#pragma once
#include "PhysicsFormwork3D.hpp"
#include "BaseCalculate3D.hpp"
#include <vector>
#include <unordered_map>
#include <algorithm>

namespace PhysicsBlock3D
{

	// 是否采用莫顿编码优化网格索引（与 2D 一致）
#define Morton_define3D 1

	// 是否预计算每层网格的偏移量（与 2D 一致）
#define CurerExcursionVector3D 1

	/**
	 * @brief   八叉网格搜索类（3D）
	 * @details 由 2D `GridSearch`（四叉网格）改写。用于物理引擎的空间索引，
	 *          支持多层八叉网格与莫顿编码。
	 *
	 * ── 与 2D 的结构差异（均为维度导致，非设计偏好） ──
	 *
	 *  1. **层基数 4 → 8**：2D 每层四等分，3D 每层八等分。
	 *     层偏移公式 `(4^k-1)/3` → `(8^k-1)/7`，总格数 `(4^s-1)/3` → `(8^s-1)/7`。
	 *  2. **莫顿码 32 位 → 64 位**：2D `Morton2D` 位交错 2 路（每轴 ≤16 bit）；
	 *     3D 用 `Morton3D` 位交错 3 路（每轴 ≤21 bit），见 BaseCalculate3D。
	 *  3. **格索引存储：稠密数组 → 稀疏哈希表**（本类最关键的改动）。
	 *     2D 的 `mCellHead` 是 `std::vector<unsigned int>`，一格 4 字节。
	 *     SetMapRange(1024) 时 2D 是 (4^12-1)/3 ≈ 5.59e6 格 ≈ 21.4MB；
	 *     同样输入在 3D 是 (8^12-1)/7 ≈ 9.8e9 格 ≈ **39.3GB**，完全不可行
	 *     （即使 mStorey=8 也需 (8^8-1)/7 ≈ 2.40e6 格，但每轴分辨率只剩 256）。
	 *     故 3D 改用 `std::unordered_map<unsigned int, unsigned int>`：
	 *     内存 ∝ **实际占用的格数**（∝ 对象数），而非 ∝ 网格总格数，
	 *     从而在保持每轴分辨率的前提下把索引区压到 O(对象数)。
	 *     代价是格访问由 O(1) 数组下标变为哈希查找 —— 对宽相查询是可接受的。
	 *  4. **索引位宽上限**：为保持 `PhysicsFormwork3D::mGridIndex` 为 32 位
	 *     （与 2D 一致），层数上限取 `kMaxStorey3D = 11`：此时
	 *     `(8^11-1)/7 = 1227133513 < UINT_MAX`。对应最细层每轴 2^11 = 2048 格，
	 *     世界覆盖范围 [-1024, 1024)。请求更大的 Size 会被夹到此上限。
	 *
	 * ── 保留自 2D 的设计 ──
	 *  · 「稠密索引 + 槽位链表」的槽位复用（mSlots/mNext/mFreeSlots），
	 *    槽位下标全程稳定，删除只回收不搬移。
	 *  · 「整树重建 + 脏格定点清零」：每帧 UpData 只清上帧写入过的格
	 *    （成本 ∝ 对象数），而非清空整表；稳态零分配。
	 *  · 多线程路径（UpDaraWorkeTask / UpDaraWorkeTaskEnd）保持
	 *    「并行算索引 + 串行应用」结构，各线程写不相交槽位区间。
	 *  · 超大型对象（比整张网格还大）放入 GridExtrovert。
	 */
	class GridSearch3D
	{
	private:
		/**
		 * @brief   层数上限（受 32 位格索引约束）
		 * @details (8^11-1)/7 = 1227133513 < UINT_MAX；mStorey=12 时
		 *          索引会达到 9.8e9 溢出 32 位，故硬性夹在 11。 */
		static const int kMaxStorey3D = 11;

		/**
		 * @brief   每格链表头（稀疏）
		 * @details key = 格索引，value = 对象槽位下标（mSlots 的下标）。
		 *          不存在的 key 表示该格为空（等价于 2D 的 UINT_MAX）。
		 *          稀疏存储的原因见类注释第 3 条。
		 */
		std::unordered_map<unsigned int, unsigned int> mCellHead;

		/**
		 * @brief   对象槽位容器
		 * @details 存储网格内所有物理对象指针；槽位采用"删除回收进 mFreeSlots 栈"
		 *          的方式复用，槽位内容从不被 swap/搬移，因此链表里的槽位下标全程稳定。
		 */
		std::vector<PhysicsFormwork3D *> mSlots;

		/**
		 * @brief   槽位在格内链表中的下一槽位
		 * @details mSlots[i] 与 mNext[i] 平行；遍历一格 = 从链头顺 mNext 走到底。
		 */
		std::vector<unsigned int> mNext;

		/**
		 * @brief   每帧多线程 UpData 计算的"新格号"暂存区
		 * @details 各线程只写自己分到的槽位区间（写不相交，无数据竞争），
		 *          主线程 UpDaraWorkeTaskEnd 统一按此重建整树。
		 */
		std::vector<unsigned int> mNewIndex;

		/**
		 * @brief   空闲槽位栈
		 * @details Remove() 把槽位入栈，Add() 优先复用，对象数量波动时内存不增长。
		 */
		std::vector<unsigned int> mFreeSlots;

		/**
		 * @brief   上帧写入过的格子（脏格集合，无重复元素）
		 * @details 整树重建时只需把这些格的头清掉（成本 ∝ 对象数），
		 *          而不是遍历整张网格表（成本 ∝ 格数）。
		 */
		std::vector<unsigned int> mDirtyCells;

		/**
		 * @brief   网格外对象容器
		 * @details 存储超出网格范围的物理对象（例如超大型对象），
		 *          这些对象在碰撞检测时需要特殊处理。
		 */
		std::vector<PhysicsFormwork3D *> GridExtrovert;

		/**
		 * @brief   网格总格数（紧凑布局的虚总长）
		 * @details 等于 (8^mStorey - 1) / 7。仅用于索引合法性判断，
		 *          不再对应任何实体内存（mCellHead 是稀疏的）。
		 */
		unsigned int mGridSize = 0;

		/**
		 * @brief   八叉网格层数
		 * @details 第 mStorey 层为根节点（整个空间，1 格），每往下一层空间八等分。
		 *          第 1 层为最细层（每轴 2^(mStorey-1) 格）。
		 */
		int mStorey = 0;

		/**
		 * @brief   网格维度（最细层级网格坐标上限）
		 * @details 恒为 2 的幂（= 2^mStorey），有效网格坐标范围为 [0, mGridDim - 1]。
		 */
		unsigned int mGridDim = 0;

		/**
		 * @brief   相对世界坐标系的位置偏移量
		 * @details 将世界坐标转换为网格坐标的偏移量，使负坐标也能正确映射。
		 */
		Vec3_ mExcursion{0.0f};

		/**
		 * @brief   线程数量（0 = 单线程模式）
		 */
		unsigned int mThreadCount = 0;

#if CurerExcursionVector3D
		/**
		 * @brief   预计算的每层偏移量数组
		 * @details ExcursionVector[i] = (8^(mStorey-i) - 1) / 7。
		 */
		unsigned int *ExcursionVector = nullptr;
#endif

	public:
		GridSearch3D() {}

		~GridSearch3D()
		{
			if (ExcursionVector != nullptr)
			{
				delete[] ExcursionVector;
			}
		}

		// 不可拷贝（持有裸数组）
		GridSearch3D(const GridSearch3D &) = delete;
		GridSearch3D &operator=(const GridSearch3D &) = delete;

	private:
		/**
		 * @brief   计算八叉网格某层的起始偏移量（紧凑布局）
		 * @param   storey 网格层数（1 = 最细层，mStorey = 根层/整个空间）
		 * @return  返回该层在一维索引空间中的起始位置
		 * @details 公式：off(storey) = (8^(mStorey - storey) - 1) / 7
		 *
		 * 紧凑布局原理（层 storey 含 8^(mStorey-storey) 个格子）：
		 *   storey = mStorey   （根，1 格）    -> 偏移 0
		 *   storey = mStorey-1 （8 格）        -> 偏移 1
		 *   storey = mStorey-2 （64 格）       -> 偏移 1+8 = 9
		 *   ...
		 *   偏移 = 8^0 + 8^1 + ... + 8^(mStorey-storey-1) = (8^(mStorey-storey) - 1)/7
		 *
		 * 对应 2D 的 (4^(mStorey-storey) - 1)/3。
		 */
		inline unsigned int Excursion(int storey)
		{
			int k = mStorey - storey;
			if (k <= 0)
				return 0; // 根层（或超出范围）偏移为 0
			// 64 位中间量：8^11 = 8.6e9 已超 32 位，移位阶段必须用 64 位
			return (unsigned int)(((1ULL << (3 * k)) - 1) / 7);
		}

		/**
		 * @brief   计算对象应该放入的网格细分层
		 * @param   R 对象的碰撞半径（世界坐标系单位）
		 * @return  返回网格层数（整数，1 = 最细层，mStorey = 根层）
		 * @details 与 2D 公式相同：3D 的网格单元是**立方体**，边长 = 2^storey
		 *          世界单位，故"对象直径能塞进单个立方体"的判据与 2D 一致，
		 *          层数计算与维度无关。
		 *
		 * 算法：取对象直径 2R 上取整后的最高有效位位置 +1。
		 */
		inline int Storey(FLOAT_ R)
		{
			unsigned int r = (unsigned int)(R * 2 + 0.99f);
			if (r == 0)
				return 1; // 零半径对象默认放在第 1 层

			unsigned long index;
#if defined(_MSC_VER)
			_BitScanReverse(&index, r);
#else
			index = 31 - __builtin_clz(r);
#endif
			// 注意：**不要**在此夹到 mStorey。返回值 > mStorey 表示
			// 对象比整张网格还大，atIndex 据此把对象判为网格外
			// （放入 GridExtrovert），与 2D 行为一致。
			return (int)index + 1;
		}

		/**
		 * @brief   世界位置转换为网格坐标（加偏移，处理负坐标）
		 */
		inline glm::ivec3 Pos_ToInt(Vec3_ pos)
		{
			return glm::ivec3(pos + mExcursion);
		}

		/**
		 * @brief   计算对象所在的网格索引
		 * @param   xyz 对象位置（世界坐标）
		 * @param   R 对象碰撞半径
		 * @return  返回网格索引；≥ mGridSize 表示网格外
		 * @details 与 2D 同构，但：
		 *          ① 跨格判定对 x/y/z 三轴同时进行；
		 *          ② 莫顿编码用 3 路交错的 Morton3D。
		 */
		inline unsigned int atIndex(Vec3_ xyz, FLOAT_ R)
		{
			// 转换到网格坐标系（处理负坐标）
			xyz += mExcursion;

			// 将网格坐标 clamp 到有效范围，防止后续位运算溢出
			FLOAT_ maxCoord = (FLOAT_)(mGridDim - 1);
			if (xyz.x < 0) xyz.x = 0;
			if (xyz.y < 0) xyz.y = 0;
			if (xyz.z < 0) xyz.z = 0;
			if (xyz.x > maxCoord) xyz.x = maxCoord;
			if (xyz.y > maxCoord) xyz.y = maxCoord;
			if (xyz.z > maxCoord) xyz.z = maxCoord;

			glm::ivec3 pos = glm::ivec3(xyz);

			int _storey = Storey(R);

			// 对象比整张网格还大，直接判为网格外
			if (_storey > mStorey)
				return mGridSize;

			// 右移相当于除以 2^_storey，即计算在当前层的网格坐标
			pos >>= _storey;

			// 计算这个网格立方体的边界
			glm::ivec3 Spos = pos << _storey;
			glm::ivec3 Epos = (pos + 1) << _storey;

			// 判断对象是否跨网格边界（3D 需判三个轴）
			if ((Spos.x > (xyz.x - R)) || (Epos.x < (xyz.x + R)) ||
			    (Spos.y > (xyz.y - R)) || (Epos.y < (xyz.y + R)) ||
			    (Spos.z > (xyz.z - R)) || (Epos.z < (xyz.z + R)))
			{
				++_storey; // 升级到上一层（更大的网格）
				pos >>= 1; // 在上一层中的位置也要重新计算

				if (_storey > mStorey)
					return mGridSize;
			}

#if CurerExcursionVector3D
			return ExcursionVector[_storey] +
			       (unsigned int)Morton3D((uint_fast32_t)pos.x,
			                              (uint_fast32_t)pos.y,
			                              (uint_fast32_t)pos.z);
#else
			return Excursion(_storey) +
			       (unsigned int)Morton3D((uint_fast32_t)pos.x,
			                              (uint_fast32_t)pos.y,
			                              (uint_fast32_t)pos.z);
#endif
		}

		/**
		 * @brief   分配/复用对象槽位
		 */
		inline unsigned int NewSlot(PhysicsFormwork3D *atocr)
		{
			unsigned int id;
			if (mFreeSlots.empty())
			{
				id = (unsigned int)mSlots.size();
				mSlots.push_back(atocr);
				mNext.push_back(UINT_MAX);
				mNewIndex.push_back(UINT_MAX);
			}
			else
			{
				id = mFreeSlots.back();
				mFreeSlots.pop_back();
				mSlots[id] = atocr;
				mNext[id] = UINT_MAX;
			}
			return id;
		}

		/**
		 * @brief   把槽位头插到指定格子
		 * @details 头插 O(1)（哈希均摊）；若该格此前不存在则记录为脏格，
		 *          供下一次整树重建时定点清除。
		 */
		inline void LinkCell(unsigned int cellIdx, unsigned int slot)
		{
			auto it = mCellHead.find(cellIdx);
			if (it == mCellHead.end())
			{
				// 该格本帧首次写入：记录为脏格（供整树重建时定点清除）
				mCellHead.emplace(cellIdx, slot);
				mNext[slot] = UINT_MAX;
				mDirtyCells.push_back(cellIdx);
				return;
			}
			// 已存在：头插 O(1)
			mNext[slot] = it->second;
			it->second = slot;
		}

		/**
		 * @brief   整树重建（供单线程 UpData 与多线程 UpDaraWorkeTaskEnd 共用）
		 * @details 前提：mNewIndex[i] 已填好（每槽位的新格号）。
		 *          1. 只清上帧写入过的脏格
		 *          2. 清空并重建网格外对象列表
		 *          3. 遍历所有槽位，把对象按新格号头插回网格
		 */
		void ApplyRebuild()
		{
			// 1. 清除上帧写入过的格子
			for (size_t i = 0; i < mDirtyCells.size(); ++i)
				mCellHead.erase(mDirtyCells[i]);
			mDirtyCells.clear();

			// 2. 网格外列表整体重建
			GridExtrovert.clear();

			// 3. 重建所有链接
			for (size_t slot = 0; slot < mSlots.size(); ++slot)
			{
				PhysicsFormwork3D *Formwork = mSlots[slot];
				if (Formwork == nullptr)
					continue; // 已删除的空槽位

				unsigned int newIndex = mNewIndex[slot];
				if (newIndex < mGridSize)
				{
					LinkCell(newIndex, (unsigned int)slot);
					Formwork->mGridIndex = newIndex;
				}
				else
				{
					GridExtrovert.push_back(Formwork);
					Formwork->mGridIndex = UINT_MAX;
				}
			}
		}

	public:
		/**
		 * @brief   设置多线程 UpData 的线程数（0 = 单线程）
		 */
		void SetThreadCount(unsigned int threadCount) { mThreadCount = threadCount; }

		/**
		 * @brief   设置网格范围大小
		 * @param   Size 网格的单边大小（世界坐标单位）
		 * @details 网格实际空间是输入的 4 倍（与 2D 一致，支持负坐标区域）。
		 *          内部把 mGridDim 取整为 2 的幂（紧凑布局 + 莫顿编码的前提），
		 *          并夹到 kMaxStorey3D 对应上限（2048，覆盖 [-1024, 1024)）。
		 */
		void SetMapRange(unsigned int Size)
		{
#if CurerExcursionVector3D
			if (ExcursionVector != nullptr)
			{
				delete[] ExcursionVector;
				ExcursionVector = nullptr;
			}
#endif
			Size *= 4; // 实际空间是输入的 4 倍

			// 网格维度向上取整到 2 的幂
			unsigned int gridDim = 1;
			int storey = 0;
			while (gridDim < Size)
			{
				gridDim <<= 1;
				++storey;
			}
			// 夹到 32 位格索引可表达的上限
			while (storey > kMaxStorey3D)
			{
				gridDim >>= 1;
				--storey;
			}

			mGridDim = gridDim;
			mStorey = storey;
			mExcursion = Vec3_(gridDim / 2.0f); // 偏移量，使负坐标变为正坐标

			// 紧凑布局总格数 = (8^mStorey - 1) / 7（用 64 位中间量）
			unsigned long long total = 1;
			for (int i = 0; i < storey; ++i)
				total *= 8;
			mGridSize = (unsigned int)((total - 1) / 7);

#if CurerExcursionVector3D
			ExcursionVector = new unsigned int[(size_t)mStorey + 1];
			for (size_t i = 0; i <= (size_t)mStorey; i++)
			{
				ExcursionVector[i] = Excursion((int)i);
			}
#endif

			// 重建索引表（稀疏表直接清空；已有对象仍在槽位中，下次 UpData 重新归位）
			mCellHead.clear();
			mDirtyCells.clear();
		}

		/**
		 * @brief   移动网格中心到新位置并立即重建索引
		 */
		void MoveGridTo(Vec3_ newCenter)
		{
			FLOAT_ halfRange = mGridDim / 2.0f;
			mExcursion = Vec3_(halfRange) - newCenter;
			UpData();
		}

		/**
		 * @brief   仅更新网格偏移量（不触发索引重建）
		 */
		void UpdateGridOffset(Vec3_ newCenter)
		{
			FLOAT_ halfRange = mGridDim / 2.0f;
			mExcursion = Vec3_(halfRange) - newCenter;
		}

		/**
		 * @brief   获取当前网格中心（世界坐标）
		 */
		Vec3_ GetGridCenter() const
		{
			FLOAT_ halfRange = mGridDim / 2.0f;
			return Vec3_(halfRange) - mExcursion;
		}

		/** @brief 当前网格维度（最细层坐标上限 +1，恒为 2 的幂） */
		unsigned int GetGridDim() const { return mGridDim; }
		/** @brief 当前层数 */
		int GetStorey() const { return mStorey; }
		/** @brief 当前总格数（虚） */
		unsigned int GetGridSize() const { return mGridSize; }
		/** @brief 当前实际占用的格数（稀疏表大小，用于内存观测） */
		size_t GetOccupiedCellCount() const { return mCellHead.size(); }

		/**
		 * @brief   添加物理对象到网格
		 */
		void Add(PhysicsFormwork3D *atocr)
		{
			unsigned int index = atIndex(atocr->PFGetPos(), atocr->PFGetCollisionR());
			unsigned int slot = NewSlot(atocr);
			if (index < mGridSize)
			{
				LinkCell(index, slot);
				atocr->mGridIndex = index;
			}
			else
			{
				GridExtrovert.push_back(atocr);
				atocr->mGridIndex = UINT_MAX;
			}
		}

		/**
		 * @brief   从网格中移除物理对象
		 * @details 先利用 mGridIndex 定位到格子，沿链表找到并摘下；
		 *          不在网格中则从 GridExtrovert 用 swap-and-pop 移除。
		 */
		void Remove(PhysicsFormwork3D *atocr)
		{
			unsigned int index = atocr->mGridIndex;

			if (index != UINT_MAX && index < mGridSize)
			{
				auto it = mCellHead.find(index);
				if (it != mCellHead.end())
				{
					unsigned int prev = UINT_MAX;
					for (unsigned int slot = it->second; slot != UINT_MAX;
					     prev = slot, slot = mNext[slot])
					{
						if (mSlots[slot] == atocr)
						{
							if (prev == UINT_MAX)
								it->second = mNext[slot]; // 摘的是链头
							else
								mNext[prev] = mNext[slot]; // 摘的是中间/尾部节点

							if (it->second == UINT_MAX)
							{
								// 格子已空：从稀疏表移除（脏格记录不再需要，
								// 因为 erase 后该 key 不存在）
								mCellHead.erase(it);
							}
							mSlots[slot] = nullptr;
							mFreeSlots.push_back(slot);
							return;
						}
					}
				}
				// 未在该格找到：说明 mGridIndex 已过期，回退到网格外容器查找
			}

			for (size_t i = 0; i < GridExtrovert.size(); ++i)
			{
				if (GridExtrovert[i] == atocr)
				{
					GridExtrovert[i] = GridExtrovert.back();
					GridExtrovert.pop_back();
					return;
				}
			}
		}

		/**
		 * @brief   获取指定球形范围内的所有对象（外接立方体粗筛）
		 * @details 与 2D 的 `R *= 2`（半径转直径）一致，3D 取外接立方体。
		 *          返回的对象可能包含在球外但在立方体内的对象，调用方需二次筛选。
		 */
		void Get(Vec3_ pos, FLOAT_ R, std::vector<PhysicsFormwork3D *> &Out)
		{
			R *= 2;
			Get(pos - Vec3_(R), pos + Vec3_(R), Out);
		}

		/**
		 * @brief   获取指定轴对齐盒范围内的所有对象
		 * @param   Spos 盒最小角（世界坐标）
		 * @param   Epos 盒最大角（世界坐标）
		 * @param   Out 输出参数，接收查询到的对象列表
		 * @details 从最细层（第 1 层）向上遍历到根层（第 mStorey 层），
		 *          每上一层三个轴坐标各右移 1 位，逐格沿槽位链表收集对象。
		 *          查询范围超出网格边界时额外包含 GridExtrovert。
		 */
		void Get(Vec3_ Spos, Vec3_ Epos, std::vector<PhysicsFormwork3D *> &Out)
		{
			Out.clear();

			Vec3_ S = Spos + mExcursion;
			Vec3_ E = Epos + mExcursion;

			unsigned int w = mGridDim - 1;
			bool Extrovert = false;

			// Clamp 到有效网格坐标范围，防止负数转 unsigned 回绕
			if (S.x < 0) { Extrovert = true; S.x = 0; }
			if (S.y < 0) { Extrovert = true; S.y = 0; }
			if (S.z < 0) { Extrovert = true; S.z = 0; }
			if (E.x < 0) { Extrovert = true; E.x = 0; }
			if (E.y < 0) { Extrovert = true; E.y = 0; }
			if (E.z < 0) { Extrovert = true; E.z = 0; }
			if (S.x > (FLOAT_)w) { Extrovert = true; S.x = (FLOAT_)w; }
			if (S.y > (FLOAT_)w) { Extrovert = true; S.y = (FLOAT_)w; }
			if (S.z > (FLOAT_)w) { Extrovert = true; S.z = (FLOAT_)w; }
			if (E.x > (FLOAT_)w) { Extrovert = true; E.x = (FLOAT_)w; }
			if (E.y > (FLOAT_)w) { Extrovert = true; E.y = (FLOAT_)w; }
			if (E.z > (FLOAT_)w) { Extrovert = true; E.z = (FLOAT_)w; }

			glm::uvec3 iS = glm::uvec3(S);
			glm::uvec3 iE = glm::uvec3(E);

			if (Extrovert)
			{
				Out.insert(Out.end(), GridExtrovert.begin(), GridExtrovert.end());
			}

			// 从最细层开始向上遍历所有层
			for (int i = 1; i <= mStorey; ++i)
			{
				iS >>= 1;
				iE >>= 1;

#if CurerExcursionVector3D
				unsigned int off = ExcursionVector[i];
#else
				unsigned int off = Excursion(i);
#endif

				for (unsigned int x = iS.x; x <= iE.x; ++x)
				{
					for (unsigned int y = iS.y; y <= iE.y; ++y)
					{
						for (unsigned int z = iS.z; z <= iE.z; ++z)
						{
							unsigned int cellIdx =
							    off + (unsigned int)Morton3D(x, y, z);
							if (cellIdx >= mGridSize)
								continue;

							auto it = mCellHead.find(cellIdx);
							if (it == mCellHead.end())
								continue;
							for (unsigned int slot = it->second; slot != UINT_MAX;
							     slot = mNext[slot])
							{
								Out.push_back(mSlots[slot]);
							}
						}
					}
				}
			}
		}

		/**
		 * @brief   获取物体所在的所有细分网格范围（用于"视野"计算）
		 * @return  返回网格范围列表（每两个 Vec3_ 构成一个盒的对角点）
		 * @details 修正了 2D 版 `GetDividedVision` 的循环缺陷：2D 的
		 *          `for (int i = _storey; i <= mStorey; ++i) { ++_storey; pos >>= 1; ... }`
		 *          在循环体内又自增 _storey，与循环变量 i 双重计数，
		 *          实际只输出到约 mStorey/2 层。3D 这里改为按层号单步推进。
		 */
		std::vector<Vec3_> GetDividedVision(PhysicsFormwork3D *atocr)
		{
			std::vector<Vec3_> Vision;

			Vec3_ atocr_pos = atocr->PFGetPos();
			FLOAT_ R = atocr->PFGetCollisionR();
			atocr_pos += mExcursion;

			FLOAT_ maxCoord = (FLOAT_)(mGridDim - 1);
			if (atocr_pos.x < 0) atocr_pos.x = 0;
			if (atocr_pos.y < 0) atocr_pos.y = 0;
			if (atocr_pos.z < 0) atocr_pos.z = 0;
			if (atocr_pos.x > maxCoord) atocr_pos.x = maxCoord;
			if (atocr_pos.y > maxCoord) atocr_pos.y = maxCoord;
			if (atocr_pos.z > maxCoord) atocr_pos.z = maxCoord;

			glm::ivec3 pos = glm::ivec3(atocr_pos);
			int _storey = Storey(R);
			if (_storey > mStorey)
				_storey = mStorey; // 超大对象取根层视野
			pos >>= _storey;

			glm::ivec3 Spos = pos << _storey;
			glm::ivec3 Epos = (pos + 1) << _storey;

			// 检查是否跨边界，如果跨了则放到上一层
			if ((Spos.x > (atocr_pos.x - R)) || (Epos.x < (atocr_pos.x + R)) ||
			    (Spos.y > (atocr_pos.y - R)) || (Epos.y < (atocr_pos.y + R)) ||
			    (Spos.z > (atocr_pos.z - R)) || (Epos.z < (atocr_pos.z + R)))
			{
				++_storey;
				if (_storey > mStorey)
					_storey = mStorey;
				pos >>= 1;
				Spos = pos << _storey;
				Epos = (pos + 1) << _storey;
			}

			Vision.push_back(Vec3_(Spos) - mExcursion);
			Vision.push_back(Vec3_(Epos) - mExcursion);

			// 逐层向上输出父格范围
			glm::ivec3 p = pos;
			for (int s = _storey + 1; s <= mStorey; ++s)
			{
				p >>= 1;
				glm::ivec3 sp = p << s;
				glm::ivec3 ep = (p + 1) << s;
				Vision.push_back(Vec3_(sp) - mExcursion);
				Vision.push_back(Vec3_(ep) - mExcursion);
			}

			return Vision;
		}

		/** @brief 获取所有网格外的对象 */
		std::vector<PhysicsFormwork3D *> &GetGridExtrovert()
		{
			return GridExtrovert;
		}

		/**
		 * @brief   更新所有对象的网格位置（整树重建，O(对象数)）
		 */
		void UpData()
		{
			for (size_t slot = 0; slot < mSlots.size(); ++slot)
			{
				PhysicsFormwork3D *Formwork = mSlots[slot];
				if (Formwork == nullptr)
					continue;
				mNewIndex[slot] = atIndex(Formwork->PFGetPos(), Formwork->PFGetCollisionR());
			}
			ApplyRebuild();
		}

		/**
		 * @brief   多线程 UpData 工作函数（并行算索引，写不相交区间）
		 */
		void UpDaraWorkeTask(unsigned int ThreadSize, unsigned int ThreadID)
		{
			if (ThreadSize == 0)
				return;

			unsigned int Count = (unsigned int)mSlots.size();
			unsigned int BlockSize = Count / ThreadSize;
			unsigned int BlockRemain = Count % ThreadSize;

			unsigned int Start;
			unsigned int End;
			if (ThreadID < BlockRemain)
			{
				Start = ThreadID * (BlockSize + 1);
				End = Start + BlockSize + 1;
			}
			else
			{
				Start = BlockRemain * (BlockSize + 1) + (ThreadID - BlockRemain) * BlockSize;
				End = Start + BlockSize;
			}

			for (unsigned int i = Start; i < End; ++i)
			{
				PhysicsFormwork3D *Formwork = mSlots[i];
				if (Formwork == nullptr)
					continue;
				mNewIndex[i] = atIndex(Formwork->PFGetPos(), Formwork->PFGetCollisionR());
			}
		}

		/**
		 * @brief   多线程 UpData 收尾函数（仅主线程调用）
		 */
		void UpDaraWorkeTaskEnd()
		{
			ApplyRebuild();
		}
	};

}
