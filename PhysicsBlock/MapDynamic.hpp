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
    class MapDynamic : public MapFormwork
    {
    private:
        const unsigned int width;         // 板块 X 的数量
        const unsigned int height;        // 板块 Y 的数量
        glm::dvec2 centrality{0, 0};      // 中心位置
        MovePlate<BaseGrid *> mMovePlate; // 板块移动控制台
        BaseGrid *BaseGridBuffer;         // 所有板块网格
        GridBlock *GridBuffer;            // 每个板块网格的数据

        /**
         * @brief 获取网格
         * @param x 坐标x
         * @param y 坐标y
         * @return 格子 */
        BaseGrid **atGrid(int x, int y) { return mMovePlate.CalculateGetPlate(x, y); }
        BaseGrid **atGrid(double x, double y) { return mMovePlate.CalculateGetPlate(x, y); }
        BaseGrid **atGrid(glm::ivec2 pos) { return atGrid(pos.x, pos.y); }
        BaseGrid **atGrid(glm::dvec2 pos) { return atGrid(pos.x, pos.y); }

    public:
        /**
         * @brief 构建函数
         * @param Width 板块 X 的数量
         * @param Height 板块 Y 的数量
         * @note 单位：网格 */
        MapDynamic(const unsigned int Width, const unsigned int Height);
        ~MapDynamic();

        /**
         * @brief 获取格子
         * @param x 坐标x
         * @param y 坐标y
         * @return 格子
         * @warning 只可以输入范围内的 0 ~ width, 0 ~ height */
        GridBlock &at(int x, int y) { return (*mMovePlate.ExcursionGetPlate(x >> PixelBlockPowerMaxNum, y >> PixelBlockPowerMaxNum))->at(x & PixelBlockPowerMinNum, y & PixelBlockPowerMinNum); }
        GridBlock &at(double x, double y) { return at(ToInt(x), ToInt(y)); }
        GridBlock &at(glm::ivec2 pos) { return at(pos.x, pos.y); }
        GridBlock &at(glm::dvec2 pos) { return at(ToInt(pos)); }

        // 更新玩家位置
        void Updata(glm::dvec2 pos){
            mMovePlate.UpData(pos.x, pos.y);
        }

        /**
         * @brief 获取地图模拟场大小
         * @return 地图大小
         * @note 单位：格子 */
        virtual glm::uvec2 FMGetMapSize() { return glm::uvec2{ width * PixelBlockPowerMaxNum, height * PixelBlockPowerMaxNum }; }

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
        virtual CollisionInfoD FMBresenhamDetection(glm::dvec2 start, glm::dvec2 end);
    };

}