#pragma once

#define AStar_MemoryPool

#ifdef AStar_MemoryPool// 开启  内存池
#include "MemoryPool.h"
#endif

struct AStarNode {
    int x;
    int y;
    float g;    // 累积代价
    float h;    // 启发式代价
    float f;    // 总代价
    AStarNode* parent = nullptr;    //父节点
    std::vector<AStarNode*> pChild; //子节点

    AStarNode(int _x, int _y, float _g, float _h, AStarNode* _parent) : x(_x), y(_y), g(_g), h(_h), parent(_parent) {
        f = g + h;
    }

    ~AStarNode()
    {
    }
};

struct AStarVec2 {
    int x;
    int y;
};

class AStar {
private:
    AStarVec2 StartingPoint = { 0,0 };              //开始位置（也是偏移值）
    const int mRange = 0;                           //范围
    const int mSteps = 0;                           //最大步数
    bool mPathfindingCompleted = true;              //寻路是否结束
    typedef bool (*_ObstaclesCallback)(int x, int y, void* ptr);
    _ObstaclesCallback mObstaclesCallback = nullptr;    //是否为障碍物回调函数
    void* mDataPointer = nullptr;                       //回调参数
    int** visited;//记录当前格子是否被走过 或者 记录路径
#ifdef AStar_MemoryPool
    MemoryPool<AStarNode, sizeof(AStarNode) * 100> mMemoryPool;    //内存池
#endif

    //位置是否合法
    bool Legitimacy(int x, int y) {
        if (x >= -mRange && x < mRange && y >= -mRange && y < mRange) { return true; }
        else { return false; }
    }
    bool Legitimacy2(int x, int y) {
        if (x >= 0 && x < mRange*2 && y >= 0 && y < mRange*2) { return true; }
        else { return false; }
    }

    //调用回调函数
    bool isValid(int x, int y) {
        if (Legitimacy(x, y)) {
            return mObstaclesCallback(x + StartingPoint.x, y + StartingPoint.y, mDataPointer);
        }
        return false;
    }

    //计算代价
    float calculateH(int x, int y, int targetX, int targetY) {
        return fabs(x - targetX) + fabs(y - targetY);
    }

    AStarNode* NewAStarNode(int _x, int _y, float _g, float _h, AStarNode* _parent) {
#ifdef AStar_MemoryPool
        return mMemoryPool.newElement(_x, _y, _g, _h, _parent);
#else
        return new AStarNode(_x, _y, _g, _h, _parent);
#endif
    }

    void DeleteAStarNode(AStarNode* _parent) {
#ifdef AStar_MemoryPool
        mMemoryPool.deleteElement(_parent);
#else
        delete _parent;
#endif
    }
public:
    AStar(int Range, int Steps):mRange(Range), mSteps(Steps){
        visited = new int*[mRange*2];
        for (size_t i = 0; i < mRange*2; ++i)
        {
            visited[i] = new int[mRange*2];
            for (size_t j = 0; j < mRange*2; ++j)
            {
                visited[i][j] = 0;
            }
        }
    }
    ~AStar() {
        for (size_t j = 0; j < mRange*2; ++j)
        {
            delete visited[j];
        }
        delete visited;
    }

    //设置障碍物回调函数
    void SetObstaclesCallback(_ObstaclesCallback ObstaclesCallback, void* DataPointer) {
        mObstaclesCallback = ObstaclesCallback;
        mDataPointer = DataPointer;
    }

    //获取寻路是否结束
    bool GetPathfindingCompleted() {
        return mPathfindingCompleted;
    }

    //开始寻路
    void FindPath(AStarVec2 start, AStarVec2 target, std::vector<AStarVec2>* PathVector, AStarVec2 deviation = { 0, 0 }) {
        //目标地点合法性
        start.x += deviation.x;
        start.y += deviation.y;
        target.x += deviation.x;
        target.y += deviation.y;
        target.x -= start.x;
        target.y -= start.y;
        StartingPoint = start;
        if (!isValid(target.x, target.y)) {
            return;
        }
        //开始寻路
        mPathfindingCompleted = false;
        start = { 0,0 };
        for (size_t i = 0; i < mRange*2; ++i)
        {
            for (size_t j = 0; j < mRange*2; ++j)
            {
                visited[i][j] = 0;
            }
        }

        AStarNode* DAStarNode;
        AStarNode* LAStarNode = NewAStarNode(start.x, start.y, 0.0f, calculateH(start.x, start.y, target.x, target.y), nullptr);
        
        int x, newX;
        int y, newY;
        int Count = 0;
        do
        {
            //是否超过最大步数
            if (Count > mSteps) {
                mPathfindingCompleted = true;
                return;
            }
            x = LAStarNode->x;
            y = LAStarNode->y;

            if (visited[x + mRange][y + mRange] == 0) {
                Count++;
                visited[x + mRange][y + mRange] = 1;
                for (int i = -1; i <= 1; ++i) {
                    for (int j = -1; j <= 1; ++j) {
                        if (i == 0 && j == 0)
                            continue;

                        newX = x + i;
                        newY = y + j;

                        if (isValid(newX, newY) && visited[newX + mRange][newY + mRange] == 0) {
                            float newG = LAStarNode->g + (i == 0 && j == 0) ? 1.0f : 1.414f;
                            float newH = calculateH(newX, newY, target.x, target.y);
                            LAStarNode->pChild.push_back(NewAStarNode(newX, newY, newG, newH, LAStarNode));
                        }
                    }
                }
                std::sort(LAStarNode->pChild.begin(), LAStarNode->pChild.end(), [](AStarNode* a, AStarNode* b) {
                    return a->f > b->f;
                    });
            }

            if (LAStarNode->pChild.size() != 0) {
                //取最小代价的子节点
                DAStarNode = LAStarNode->pChild.back();
                //删除最小代价节点
                LAStarNode->pChild.pop_back();
                //当前点移动到这个最小代价点
                LAStarNode = DAStarNode;

                //有没有到达终点
                if (LAStarNode->x == target.x && LAStarNode->y == target.y)
                {
                    //获取终点位置
                    AStarVec2 LStart = { LAStarNode->x + mRange, LAStarNode->y +mRange };
                    AStarVec2 CurrentStart = LStart;
                    Count = 2;
                    while (LAStarNode != nullptr)
                    {
                        //遍历走过的点
                        visited[LAStarNode->x + mRange][LAStarNode->y + mRange] = ++Count;
                        //销毁所有子节点
                        for (auto i : LAStarNode->pChild)
                        {
                            DeleteAStarNode(i);
                        }
                        DAStarNode = LAStarNode;
                        //获取父节点
                        LAStarNode = LAStarNode->parent;
                        //销毁自身
                        DeleteAStarNode(DAStarNode);
                    }

                    StartingPoint.x -= mRange;
                    StartingPoint.y -= mRange;
                    StartingPoint.x -= deviation.x;
                    StartingPoint.y -= deviation.y;
                    //是否到终点
                    while (visited[LStart.x][LStart.y] != Count)
                    {
                        //获取路径
                        PathVector->push_back({ LStart.x + StartingPoint.x, LStart.y + StartingPoint.y });
                        //取周围最大的点
                        for (int i = -1; i <= 1; ++i) {
                            for (int j = -1; j <= 1; ++j) {
                                if (i == 0 && j == 0)
                                    continue;

                                newX = LStart.x + i;
                                newY = LStart.y + j;
                                if (Legitimacy2(newX, newY) && visited[newX][newY] > visited[CurrentStart.x][CurrentStart.y]) {
                                    CurrentStart.x = newX;
                                    CurrentStart.y = newY;
                                }
                            }
                        }
                        //设置为最大点
                        LStart = CurrentStart;
                    }
                    //获取路径
                    PathVector->push_back({ LStart.x + StartingPoint.x, LStart.y + StartingPoint.y });
                }
            }
            else//没有子节点，返回父节点
            {
                DAStarNode = LAStarNode;
                LAStarNode = LAStarNode->parent;
                DeleteAStarNode(DAStarNode);
            }

        } while (LAStarNode != nullptr);
        //寻路结束
        mPathfindingCompleted = true;
    }
};