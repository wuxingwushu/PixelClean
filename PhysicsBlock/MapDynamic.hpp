#pragma once
#include "MapFormwork.hpp"
#include "BaseGrid.hpp"
#include "MovePlate.h"
#include "BaseDefine.h"
#include <assert.h>
#include "BaseCalculate.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 动态地图 */
    class MapDynamic : public MapFormwork, public BaseGrid
    {
    public:
        typedef void (*_MapDynamicGenerateCallback)(BaseGrid** mT, int x, int y, void* Data);
        typedef void (*_MapDynamicDeleteCallback)(BaseGrid** mT, void* Data);

    private:
        /**
         * @brief 板块 X 的数量 */
        const unsigned int width;
        /**
         * @brief 板块 Y 的数量 */
        const unsigned int height;
        /**
         * @brief 中心位置 */
        Vec2_ centrality{0, 0};
        /**
         * @brief 板块移动控制台 */
        MovePlate<BaseGrid *> mMovePlate;
        /**
         * @brief 所有板块网格 */
        BaseGrid *BaseGridBuffer;
        /**
         * @brief 每个板块网格的数据 */
        GridBlock *GridBuffer;

        /**
         * @brief 获取网格
         * @param x 坐标x
         * @param y 坐标y
         * @return 格子 */
        BaseGrid **atGrid(int x, int y) { return mMovePlate.CalculateGetPlate(x, y); }
        BaseGrid **atGrid(FLOAT_ x, FLOAT_ y) { return mMovePlate.CalculateGetPlate(x, y); }
        BaseGrid **atGrid(glm::ivec2 pos) { return atGrid(pos.x, pos.y); }
        BaseGrid **atGrid(Vec2_ pos) { return atGrid(pos.x, pos.y); }

    public:
        /**
         * @brief 构建函数
         * @param Width 板块 X 的数量
         * @param Height 板块 Y 的数量
         * @note 单位：网格 */
        MapDynamic(const unsigned int Width, const unsigned int Height);
        ~MapDynamic();

        /*=========BaseGrid=========*/

        /**
         * @brief 获取格子
         * @param x 坐标x
         * @param y 坐标y
         * @return 格子 */
        virtual GridBlock &at(int x, int y) {
            BaseGrid **platePtr = mMovePlate.ExcursionGetPlate(x >> PixelBlockPowerMaxNum, y >> PixelBlockPowerMaxNum);
            if (platePtr) return (*platePtr)->at(x & PixelBlockPowerMinNum, y & PixelBlockPowerMinNum);
            static GridBlock emptyBlock{};
            return emptyBlock;
        }
        /**
         * @brief 获取格子
         * @param pos 坐标
         * @return 格子 */
        virtual GridBlock &at(glm::ivec2 pos) { return at(pos.x, pos.y); }


        /**
         * @brief 获取格子
         * @param x 坐标x
         * @param y 坐标y
         * @return 格子 */
        GridBlock &at(FLOAT_ x, FLOAT_ y) { return at(ToInt(x), ToInt(y)); }
        /**
         * @brief 获取格子
         * @param pos 坐标
         * @return 格子 */
        GridBlock &at(Vec2_ pos) { return at(ToInt(pos)); }

        /**
         * @brief 更新玩家位置
         * @param pos 新位置 */
        MovePlateInfo Updata(Vec2_ pos){
            return mMovePlate.UpData(pos.x, pos.y);
        }

        /**
         * @brief 设置板块生成和销毁回调
         * @param G 生成回调函数
         * @param GData 生成回调的用户数据
         * @param D 销毁回调函数
         * @param DData 销毁回调的用户数据 */
        inline void SetCallback(_MapDynamicGenerateCallback G, void* GData, _MapDynamicDeleteCallback D, void* DData) noexcept {
            mMovePlate.SetCallback(G, GData, D, DData);
        }

        /**
         * @brief 设置初始监测位置
         * @param x 世界 X 坐标（像素）
         * @param y 世界 Y 坐标（像素） */
        inline void SetPos(float x, float y) noexcept {
            mMovePlate.SetPos(x, y);
        }

        /**
         * @brief 全局刷新所有板块数据
         * @param x 世界 X 坐标（像素）
         * @param y 世界 Y 坐标（像素） */
        inline void ALLUpData(float x, float y) noexcept {
            mMovePlate.ALLUpData(x, y);
        }

        /**
         * @brief 获取当前所在板块坐标
         * @return 板块坐标 */
        inline glm::ivec2 GetPlatePos() const noexcept { return glm::ivec2{ mMovePlate.GetPlateX(), mMovePlate.GetPlateY()}; }
        /**
         * @brief 获取板块边长大小
         * @return 板块边长（像素） */
        inline unsigned int GetPlateEdgeSize() const noexcept { return PixelBlockEdgeSize; }
        /**
         * @brief 获取板块 X 方向数量
         * @return 板块数量 */
        inline unsigned int GetPlateCountX() const noexcept { return width; }
        /**
         * @brief 获取板块 Y 方向数量
         * @return 板块数量 */
        inline unsigned int GetPlateCountY() const noexcept { return height; }

        /**
         * @brief 获取轻量级轮廓
         * @param x_ 起始X坐标（世界坐标）
         * @param y_ 起始Y坐标（世界坐标）
         * @param w_ 结束X坐标（世界坐标）
         * @param h_ 结束Y坐标（世界坐标）
         * @return 轮廓点向量 */
        std::vector<MapOutline> GetLightweightOutline(int x_, int y_, int w_, int h_);

        /**
         * @brief 获取完整轮廓
         * @param x_ 起始X坐标（世界坐标）
         * @param y_ 起始Y坐标（世界坐标）
         * @param w_ 结束X坐标（世界坐标）
         * @param h_ 结束Y坐标（世界坐标）
         * @return 轮廓点向量 */
        std::vector<MapOutline> GetOutline(int x_, int y_, int w_, int h_);

        /*=========MapFormwork=========*/

        /**
         * @brief 获取地图模拟场大小
         * @return 地图大小
         * @note 单位：格子 */
        virtual glm::uvec2 FMGetMapSize() { return glm::uvec2{ width * PixelBlockEdgeSize, height * PixelBlockEdgeSize }; }

        /**
         * @brief 获取指定位置的碰撞状态
         * @param start 网格坐标（整数）
         * @return 是否碰撞 */
        virtual bool FMGetCollide(glm::ivec2 start) override {
            return at(start.x, start.y).Collision;
        }

        /**
         * @brief 获取指定位置的碰撞状态
         * @param start 世界坐标（浮点数）
         * @return 是否碰撞 */
        virtual bool FMGetCollide(Vec2_ start) override {
            return at(start).Collision;
        }

        /**
         * @brief 获取网格(不安全)
         * @param start 网格位置
         * @warning 范围不安全，超出会报错 */
        virtual GridBlock& FMGetGridBlock(glm::ivec2 start) {
            return at(start);
        }

        /**
         * @brief 地图 线段(Bresenham) 检测
         * @param start 起始位置
         * @param end 结束位置
         * @return 第一个碰撞的位置(网格位置) */
        virtual CollisionInfoI FMBresenhamDetection(glm::ivec2 start, glm::ivec2 end);

        /**
         * @brief 地图 线段(Bresenham) 检测
         * @param start 起始位置
         * @param end 结束位置
         * @return 第一个碰撞的位置(准确位置) */
        virtual CollisionInfoD FMBresenhamDetection(Vec2_ start, Vec2_ end);

        /**
         * @brief 地图 线段(Bresenham) 检测（安全边界裁剪版本）
         * @param start 起始位置
         * @param end 结束位置
         * @return 第一个碰撞的位置(网格位置)
         * @note 坐标会被裁剪到地图范围内，避免越界 */
        virtual CollisionInfoI FMSafeBresenhamDetection(glm::ivec2 start, glm::ivec2 end);

        /**
         * @brief 获取地图模拟场中心位置
         * @return 中心位置坐标 */
        virtual Vec2_ FMGetCentrality() override { return {0, 0}; }
        virtual std::vector<MapOutline> FMGetLightweightOutline(int x_, int y_, int w_, int h_) override { return GetLightweightOutline(x_, y_, w_, h_); }
        virtual std::vector<MapOutline> FMGetOutline(int x_, int y_, int w_, int h_) override { return GetOutline(x_, y_, w_, h_); }

        /**
         * @brief 安全设置碰撞状态（带边界检查）
         * @param pos 网格坐标
         * @param state 碰撞状态
         * @return 是否成功设置 */
        virtual bool SafeSetCollision(glm::ivec2 pos, bool state) override;
        /**
         * @brief 设置摩擦力系数
         * @param x 网格 X 坐标
         * @param y 网格 Y 坐标
         * @param friction 摩擦力系数 */
        virtual void SetFriction(int x, int y, FLOAT_ friction) override;
        /**
         * @brief 获取摩擦力系数
         * @param x 网格 X 坐标
         * @param y 网格 Y 坐标
         * @return 摩擦力系数 */
        virtual FLOAT_ GetFriction(int x, int y) override;

    private:
    };

}