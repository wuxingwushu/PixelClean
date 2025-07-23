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
    private:
        const unsigned int width;         // 板块 X 的数量
        const unsigned int height;        // 板块 Y 的数量
        Vec2_ centrality{0, 0};      // 中心位置
        MovePlate<BaseGrid *> mMovePlate; // 板块移动控制台
        BaseGrid *BaseGridBuffer;         // 所有板块网格
        GridBlock *GridBuffer;            // 每个板块网格的数据

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
        virtual GridBlock &at(int x, int y) { return (*mMovePlate.ExcursionGetPlate(x >> PixelBlockPowerMaxNum, y >> PixelBlockPowerMaxNum))->at(x & PixelBlockPowerMinNum, y & PixelBlockPowerMinNum); }
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

        /*=========MapFormwork=========*/

        /**
         * @brief 获取地图模拟场大小
         * @return 地图大小
         * @note 单位：格子 */
        virtual glm::uvec2 FMGetMapSize() { return glm::uvec2{ width * PixelBlockPowerMaxNum, height * PixelBlockPowerMaxNum }; }

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
    };

}