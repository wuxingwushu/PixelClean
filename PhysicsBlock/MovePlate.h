#pragma once
#include <assert.h>

/**
 * @brief   板块移动信息结构体
 * @details 用于存储板块移动的结果信息
 */
struct MovePlateInfo
{
    bool UpData = false;  ///< 是否发生了更新
    int X = 0;            ///< X 方向的移动量
    int Y = 0;            ///< Y 方向的移动量
};

/**
 * @brief   可移动板块管理器（模板类）
 * @tparam  PlateT 板块类型
 * @details 用于管理一系列可动态移动的板块，支持视口移动时的板块更新
 *          通过回调机制实现板块的生成和销毁
 */
template<typename PlateT>
class MovePlate
{
    /**
     * @brief   板块生成回调函数类型
     * @param   mT 板块指针
     * @param   x 板块的 X 坐标
     * @param   y 板块的 Y 坐标
     * @param   Data 用户自定义数据
     */
    typedef void (*_GenerateCallback)(PlateT* mT, int x, int y, void* Data);
    
    /**
     * @brief   板块销毁回调函数类型
     * @param   mT 板块指针
     * @param   Data 用户自定义数据
     */
    typedef void (*_DeleteCallback)(PlateT* mT, void* Data);

private:
    _GenerateCallback mGenerateCallback = nullptr;  ///< 生成回调函数指针
    void* mGenerateData = nullptr;                  ///< 生成回调的用户数据
    _DeleteCallback mDeleteCallback = nullptr;      ///< 销毁回调函数指针
    void* mDeleteData = nullptr;                    ///< 销毁回调的用户数据

    const unsigned int mEdge;                       ///< 单个板块的边长（像素）
    const unsigned int mNumberX, mNumberY;          ///< X 和 Y 方向的板块数量
    const unsigned int mOriginX, mOriginY;          ///< 中心板块的索引
    PlateT* mPlate = nullptr;                       ///< 板块数组（二维展开为一维）

    int mPosX = 0, mPosY = 0;                       ///< 当前监测位置（板块坐标）

private:
    PlateT* LPlate = nullptr;                       ///< 临时缓冲区，用于板块移动时的数据暂存

public:
    /**
     * @brief   获取当前监测位置的 X 坐标
     * @return  返回当前 X 位置（板块坐标）
     */
    [[nodiscard]] inline int GetPosX() { return mPosX; }
    
    /**
     * @brief   获取当前监测位置的 Y 坐标
     * @return  返回当前 Y 位置（板块坐标）
     */
    [[nodiscard]] inline int GetPosY() { return mPosY; }

    /**
     * @brief   获取当前中心板块的世界 X 坐标
     * @return  返回中心板块左上角的世界 X 坐标
     */
    [[nodiscard]] inline int GetPlateX() { return (mOriginX - mPosX) * mEdge; }
    
    /**
     * @brief   获取当前中心板块的世界 Y 坐标
     * @return  返回中心板块左上角的世界 Y 坐标
     */
    [[nodiscard]] inline int GetPlateY() { return (mOriginY - mPosY) * mEdge; }

    /**
     * @brief   将世界 X 坐标转换为板块数组索引
     * @param   x 世界 X 坐标（像素）
     * @return  返回对应的板块 X 索引
     */
    [[nodiscard]] inline int GetCalculatePosX(int x) { return (x / mEdge) - mPosX + mOriginX; }
    
    /**
     * @brief   将世界 Y 坐标转换为板块数组索引
     * @param   y 世界 Y 坐标（像素）
     * @return  返回对应的板块 Y 索引
     */
    [[nodiscard]] inline int GetCalculatePosY(int y) { return (y / mEdge) - mPosY + mOriginY; }

    /**
     * @brief   构造函数
     * @param   NumberX X 方向的板块数量
     * @param   NumberY Y 方向的板块数量
     * @param   Edge 单个板块的边长（像素）
     * @param   OriginX 中心板块的 X 索引（默认为 0）
     * @param   OriginY 中心板块的 Y 索引（默认为 0）
     * @details 初始化板块数组和临时缓冲区
     */
    constexpr MovePlate(const unsigned int NumberX, const unsigned int NumberY, const unsigned int Edge,
        const unsigned int OriginX = 0, const unsigned int OriginY = 0) :
        mNumberX(NumberX), mNumberY(NumberY), mEdge(Edge),
        mOriginX(OriginX), mOriginY(OriginY)
    {
        assert(mEdge != 0 && "Error : mEdge = 0");
        mPlate = (PlateT*)new char[mNumberX * mNumberY * sizeof(PlateT)];
        LPlate = (PlateT*)new char[(mNumberX > mNumberY ? mNumberX : mNumberY) * sizeof(PlateT)];
    }

    /**
     * @brief   析构函数
     * @details 释放板块数组和临时缓冲区的内存
     */
    ~MovePlate()
    {
        delete (char*)mPlate;
        delete (char*)LPlate;
    }

    /**
     * @brief   获取指定索引的板块指针
     * @param   x 板块 X 索引
     * @param   y 板块 Y 索引
     * @return  返回板块指针
     * @warning 不进行边界检查，请确保索引有效
     */
    [[nodiscard]] inline PlateT* PlateAt(unsigned int x, unsigned int y) {
        return &mPlate[x * mNumberY + y];
    }

    /**
     * @brief   设置回调函数
     * @param   G 生成回调函数
     * @param   GData 生成回调的用户数据
     * @param   D 销毁回调函数
     * @param   DData 销毁回调的用户数据
     */
    inline void SetCallback(_GenerateCallback G, void* GData, _DeleteCallback D, void* DData) noexcept {
        mGenerateCallback = G;
        mGenerateData = GData;
        mDeleteCallback = D;
        mDeleteData = DData;
    }

    /**
     * @brief   设置监测位置
     * @param   x 世界 X 坐标（像素）
     * @param   y 世界 Y 坐标（像素）
     * @details 将世界坐标转换为板块坐标并设置当前位置
     */
    inline void SetPos(float x, float y) noexcept {
        mPosX = x / mEdge;
        mPosY = y / mEdge;
    }

    /**
     * @brief   获取指定索引的板块（带边界检查）
     * @param   x 板块 X 索引
     * @param   y 板块 Y 索引
     * @return  返回板块指针，如果索引越界返回 nullptr
     */
    inline PlateT* GetPlate(unsigned int x, unsigned int y) noexcept {
        if (x >= mNumberX || y >= mNumberY) {
            return nullptr;
        }
        return PlateAt(x, y);
    }
    
    /**
     * @brief   通过偏移量获取板块
     * @param   x 偏移后的 X 坐标
     * @param   y 偏移后的 Y 坐标
     * @return  返回板块指针，如果索引越界返回 nullptr
     * @details 根据当前位置和偏移量计算板块索引
     */
    inline PlateT* ExcursionGetPlate(int x, int y) noexcept {
        unsigned int xx = x - mPosX + mOriginX;
        unsigned int yy = y - mPosY + mOriginY;
        return GetPlate(xx, yy);
    }

    /**
     * @brief   将世界坐标转换为板块指针（double 版本）
     * @param   x 世界 X 坐标（像素）
     * @param   y 世界 Y 坐标（像素）
     * @return  返回板块指针，如果索引越界返回 nullptr
     */
    inline PlateT* CalculateGetPlate(double x, double y) noexcept {
        unsigned int xx = (x / mEdge) - mPosX + mOriginX;
        unsigned int yy = (y / mEdge) - mPosY + mOriginY;
        return GetPlate(xx, yy);
    }

    /**
     * @brief   将世界坐标转换为板块指针（float 版本）
     * @param   x 世界 X 坐标（像素）
     * @param   y 世界 Y 坐标（像素）
     * @return  返回板块指针，如果索引越界返回 nullptr
     */
    inline PlateT* CalculateGetPlate(float x, float y) noexcept {
        unsigned int xx = (x / mEdge) - mPosX + mOriginX;
        unsigned int yy = (y / mEdge) - mPosY + mOriginY;
        return GetPlate(xx, yy);
    }

    /**
     * @brief   将世界坐标转换为板块指针（int 版本）
     * @param   x 世界 X 坐标（像素）
     * @param   y 世界 Y 坐标（像素）
     * @return  返回板块指针，如果索引越界返回 nullptr
     */
    inline PlateT* CalculateGetPlate(int x, int y) noexcept {
        unsigned int xx = (x / mEdge) - mPosX + mOriginX;
        unsigned int yy = (y / mEdge) - mPosY + mOriginY;
        return GetPlate(xx, yy);
    }

    /**
     * @brief   更新监测位置并判断是否需要移动板块
     * @param   x 新的监测位置 X 坐标（像素）
     * @param   y 新的监测位置 Y 坐标（像素）
     * @return  返回板块移动信息
     * @details 根据新位置计算需要移动的板块数量，调用回调函数更新板块内容
     *          如果移动量超过板块数量，则全部重新生成
     */
    MovePlateInfo UpData(double x, double y) {
        MovePlateInfo Info;
        Info.X = (x / mEdge) - mPosX;
        Info.Y = (y / mEdge) - mPosY;
        
        // 检查移动量是否在可接受范围内
        if ((fabs(Info.X) < mNumberX) && (fabs(Info.Y) < mNumberY)) {
            // 优先处理 Y 方向移动
            if (Info.Y != 0) {
                mPosY += Info.Y;
                MovePlateY(Info.Y, Info.X);
                Info.UpData = true;
                return Info;
            }
            // 处理 X 方向移动
            if (Info.X != 0) {
                mPosX += Info.X;
                MovePlateX(Info.X);
                Info.UpData = true;
                return Info;
            }
        }
        else {
            // 移动量过大，全部重新生成
            mPosX += Info.X;
            mPosY += Info.Y;
            for (size_t Nx = 0; Nx < mNumberX; ++Nx)
            {
                for (size_t Ny = 0; Ny < mNumberY; ++Ny)
                {
                    mDeleteCallback(PlateAt(Nx, Ny), mDeleteData);
                    mGenerateCallback(PlateAt(Nx, Ny), (Nx + mPosX - mOriginX), (Ny + mPosY - mOriginY), mGenerateData);
                }
            }
            Info.UpData = true;
        }
        return Info;
    }

private:

    /**
     * @brief   沿 X 方向移动板块
     * @param   uX 移动量（正数向右，负数向左）
     * @details 将板块数组沿 X 方向滚动，对移出视野的板块调用销毁回调，
     *          对新进入视野的板块调用生成回调
     */
    void MovePlateX(int uX) {
        if (uX > 0) {
            // 向右移动
            for (size_t iy = 0; iy < mNumberY; ++iy)
            {
                // 保存要移出的板块
                for (size_t ix = 0; ix < uX; ++ix)
                {
                    LPlate[ix] = *PlateAt(ix, iy);
                }
                // 向左滚动板块
                for (size_t ix = 0; ix < (mNumberX - uX); ++ix)
                {
                    *PlateAt(ix, iy) = *PlateAt(ix + uX, iy);
                }
                // 将保存的板块放到右侧，并重新生成
                for (size_t ix = (mNumberX - uX); ix < mNumberX; ++ix)
                {
                    *PlateAt(ix, iy) = LPlate[ix - (mNumberX - uX)];
                    mDeleteCallback(PlateAt(ix, iy), mDeleteData);
                    mGenerateCallback(PlateAt(ix, iy), (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), mGenerateData);
                }
            }
        }
        else {
            // 向左移动
            for (size_t iy = 0; iy < mNumberY; ++iy)
            {
                // 保存要移出的板块
                for (size_t ix = 0; ix < -uX; ++ix)
                {
                    LPlate[ix] = *PlateAt(mNumberX - ix - 1, iy);
                }
                // 向右滚动板块
                for (int ix = (mNumberX + uX - 1); ix >= 0; --ix)
                {
                    *PlateAt(ix - uX, iy) = *PlateAt(ix, iy);
                }
                // 将保存的板块放到左侧，并重新生成
                for (size_t ix = 0; ix < -uX; ++ix)
                {
                    *PlateAt(ix, iy) = LPlate[ix];
                    mDeleteCallback(PlateAt(ix, iy), mDeleteData);
                    mGenerateCallback(PlateAt(ix, iy), (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), mGenerateData);
                }
            }
        }
    }

    /**
     * @brief   沿 Y 方向移动板块
     * @param   uY Y 方向移动量（正数向下，负数向上）
     * @param   uX X 方向的预测移动量（用于优化双重移动时的回调调用）
     * @details 将板块数组沿 Y 方向滚动，对移出视野的板块调用销毁回调，
     *          对新进入视野的板块调用生成回调
     *          如果同时有 X 方向移动，某些板块会在 MovePlateX 中重新生成，
     *          这里会跳过这些板块以避免重复调用回调
     */
    void MovePlateY(int uY, int uX) {
        int tX, wX;
        if (uX > 0) {
            tX = uX;
            wX = 0;
        }
        else {
            tX = 0;
            wX = uX;
        }

        if (uY > 0) {
            // 向下移动
            for (size_t ix = 0; ix < mNumberX; ++ix)
            {
                // 保存要移出的板块
                for (size_t iy = 0; iy < uY; ++iy)
                {
                    LPlate[iy] = *PlateAt(ix, iy);
                }
                // 向上滚动板块
                for (size_t iy = 0; iy < (mNumberY - uY); ++iy)
                {
                    *PlateAt(ix, iy) = *PlateAt(ix, iy + uY);
                }
                // 将保存的板块放到下方，并重新生成（跳过会在 X 移动中生成的板块）
                for (size_t iy = (mNumberY - uY); iy < mNumberY; ++iy)
                {
                    *PlateAt(ix, iy) = LPlate[iy - (mNumberY - uY)];
                    if ((ix >= tX) && (ix < (mNumberX + wX))) {
                        mDeleteCallback(PlateAt(ix, iy), mDeleteData);
                        mGenerateCallback(PlateAt(ix, iy), (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), mGenerateData);
                    }
                }
            }
        }
        else {
            // 向上移动
            for (size_t ix = 0; ix < mNumberX; ++ix)
            {
                // 保存要移出的板块
                for (size_t iy = 0; iy < -uY; ++iy)
                {
                    LPlate[iy] = *PlateAt(ix, mNumberY - iy - 1);
                }
                // 向下滚动板块
                for (int iy = (mNumberY + uY - 1); iy >= 0; --iy)
                {
                    *PlateAt(ix, iy - uY) = *PlateAt(ix, iy);
                }
                // 将保存的板块放到上方，并重新生成（跳过会在 X 移动中生成的板块）
                for (size_t iy = 0; iy < -uY; ++iy)
                {
                    *PlateAt(ix, iy) = LPlate[iy];
                    if ((ix >= tX) && (ix < (mNumberX + wX))) {
                        mDeleteCallback(PlateAt(ix, iy), mDeleteData);
                        mGenerateCallback(PlateAt(ix, iy), (ix + mPosX - mOriginX), (iy + mPosY - mOriginY), mGenerateData);
                    }
                }
            }
        }
    }
};
