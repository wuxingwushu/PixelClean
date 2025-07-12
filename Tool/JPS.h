#pragma once
#include <algorithm>
#define JPS_MemoryPool
//#include <set>
#ifdef JPS_MemoryPool// 开启  内存池
#include "MemoryPool.h"
#endif
#include <unordered_set>


enum class JPSDirection {
    Upper = 0,      //上
    Lower,          //下
    Left,           //左
    Right,          //右
    UpperLeft,      //左上
    LowerLeft,      //左下
    UpperRight,     //右上
    LowerRight      //右下
};

struct JPSVec2 {
    int x;
    int y;

    inline constexpr JPSVec2& operator+=(const JPSVec2& other) noexcept
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    inline constexpr JPSVec2 operator+(const JPSVec2& other) noexcept
    {
        return { x += other.x, y += other.y };
    }

    inline constexpr JPSVec2& operator-=(const JPSVec2& other) noexcept
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    inline constexpr JPSVec2 operator-(const JPSVec2& other) noexcept
    {
        return { x - other.x, y - other.y };
    }

    inline constexpr bool operator!=(const JPSVec2& other) noexcept
    {
        return (x != other.x) || (y != other.y);
    }

    inline constexpr bool operator==(const JPSVec2& other) noexcept
    {
        return (x == other.x) && (y == other.y);
    }
};

struct JPSNode : public JPSVec2 {
    int g;    // 累积代价
    int h;    // 启发式代价
    int f;    // 总代价
    JPSNode* parent = nullptr;      //父节点
    std::vector<JPSNode*> pChild{}; //子节点
    JPSDirection Direction;
    inline JPSNode(int _x, int _y, int _g, int _h, JPSNode* _parent, JPSDirection direction) : JPSVec2{_x, _y}, g(_g), h(_h), parent(_parent), Direction(direction) {
        f = g + h;
    }

    ~JPSNode()
    {
    }
};



struct NodeData : public JPSVec2
{
    JPSDirection Direction;

    NodeData(int X, int Y, JPSDirection direction):JPSVec2{ X, Y }, Direction(direction){}
    NodeData(JPSVec2 pos, JPSDirection direction) :JPSVec2{ pos }, Direction(direction) {}

    inline bool operator<(const NodeData& other) const {
        return x < other.x || (x == other.x && y < other.y) || (x == other.x && y == other.y && Direction < other.Direction);
    }

    inline bool operator==(const NodeData& other) const {
        return x == other.x && y == other.y && Direction == other.Direction;
    }
};

struct NodeDataHash {
    inline std::size_t operator()(const NodeData& node) const {
        std::size_t h1 = std::hash<int>{}(node.x);
        std::size_t h2 = std::hash<int>{}(node.y);
        std::size_t h3 = std::hash<int>{}(static_cast<int>(node.Direction));
        return h3 + (h2 << 1) + (h1 << 2);
    }
};



class JPS {
private:
    //std::set<NodeData> RepeatJudge;     //有过的跳点
    std::unordered_set<NodeData, NodeDataHash> RepeatJudge;     //有过的跳点
private:
    JPSVec2 TargetPosition{};           //目标位置
    JPSVec2 CurrentPosition{};          //当前位置
    JPSNode* ParentJPSNode = nullptr;   //当前节点
    bool Finish = false;                //寻路是否结束
    bool Scanning = true;               //是否是初次跳点
public:
    JPSVec2 StartingPoint = { 0,0 };                //开始位置（也是偏移值）
private:
    const int mRange = 0;                           //范围
    const int mSteps = 0;                           //最大步数
    bool mPathfindingCompleted = true;              //寻路是否结束
    typedef bool (*_ObstaclesCallback)(int x, int y, void* ptr);
    _ObstaclesCallback mObstaclesCallback = nullptr;    //是否为障碍物回调函数
    void* mDataPointer = nullptr;                       //回调参数

    std::vector<JPSNode*>* mJPSDirectionVec2{};       //子集指针
#ifdef JPS_MemoryPool
    MemoryPool<JPSNode, sizeof(JPSNode) * 100> mMemoryPool;    //内存池
#endif
public:
    //位置是否合法
    inline bool Legitimacy(int x, int y) {
        if (x >= -mRange && x < mRange && y >= -mRange && y < mRange) { return true; }
        else { return false; }
    }
private:
    //调用回调函数
    virtual inline bool isValid(int x, int y) {
        if (Legitimacy(x, y)) {
            return mObstaclesCallback(x + StartingPoint.x, y + StartingPoint.y, mDataPointer);
        }
        return false;
    }

    //计算代价
    inline int calculateH(int x, int y) {
        return fabs(x - TargetPosition.x) + fabs(y - TargetPosition.y);
    }
    inline int calculateH(JPSVec2 pos) {
        return fabs(pos.x - TargetPosition.x) + fabs(pos.y - TargetPosition.x);
    }

    //创建JPSNode
    inline JPSNode* NewJPSNode(JPSVec2 pos, int _g, int _h, JPSNode* _parent, JPSDirection direction) {
#ifdef JPS_MemoryPool
        return mMemoryPool.newElement(pos.x, pos.y, _g, _h, _parent, direction);
#else
        return new JPSNode(pos.x, pos.y, _g, _h, _parent, direction);
#endif
    }

    //销毁JPSNode
    inline void DeleteJPSNode(JPSNode* _parent) {
#ifdef JPS_MemoryPool
        mMemoryPool.deleteElement(_parent);
#else
        delete _parent;
#endif
    }

    //判断是否到终点
    inline bool EndPointJudge(JPSVec2 pos) {
        bool BJ = pos.x == TargetPosition.x && pos.y == TargetPosition.y;
        if (BJ) {
            Finish = true;
        }
        return BJ;
    }

    //方向的偏移
    JPSVec2 GetProgramme(JPSDirection Direction) {
        switch (Direction)
        {
        case JPSDirection::Upper:
            return JPSVec2{ 0, 1 };
            break;
        case JPSDirection::Lower:
            return JPSVec2{ 0, -1 };
            break;
        case JPSDirection::Left:
            return JPSVec2{ -1, 0 };
            break;
        case JPSDirection::Right:
            return JPSVec2{ 1, 0 };
            break;
        case JPSDirection::UpperLeft:
            return JPSVec2{ -1, 1 };
            break;
        case JPSDirection::LowerLeft:
            return JPSVec2{ -1, -1 };
            break;
        case JPSDirection::UpperRight:
            return JPSVec2{ 1, 1 };
            break;
        case JPSDirection::LowerRight:
            return JPSVec2{ 1, -1 };
            break;
        default:
            break;
        }
        return JPSVec2{ 0, 0 };
    }

    //获取合法位置
    bool GetDirectionLegitimate(JPSVec2 pos, JPSDirection Direction) {
        switch (Direction)
        {
        case JPSDirection::Upper:
            return isValid(pos.x, pos.y + 1);
            break;
        case JPSDirection::Lower:
            return isValid(pos.x, pos.y - 1);
            break;
        case JPSDirection::Left:
            return isValid(pos.x - 1, pos.y);
            break;
        case JPSDirection::Right:
            return isValid(pos.x + 1, pos.y);
            break;
        case JPSDirection::UpperLeft:
            return (isValid(pos.x - 1, pos.y) || isValid(pos.x, pos.y + 1)) && isValid(pos.x - 1, pos.y + 1);
            break;
        case JPSDirection::LowerLeft:
            return (isValid(pos.x - 1, pos.y) || isValid(pos.x, pos.y - 1)) && isValid(pos.x - 1, pos.y - 1);
            break;
        case JPSDirection::UpperRight:
            return (isValid(pos.x + 1, pos.y) || isValid(pos.x, pos.y + 1)) && isValid(pos.x + 1, pos.y + 1);
            break;
        case JPSDirection::LowerRight:
            return (isValid(pos.x + 1, pos.y) || isValid(pos.x, pos.y - 1)) && isValid(pos.x + 1, pos.y - 1);
            break;
        default:
            break;
        }
        return false;
    }

    //邻近点的检测
    void RadialEvent(JPSVec2 pos, JPSNode* Parent, JPSDirection Direction) {
        Scanning = true;
        JPSNode* wParent = Parent;
        switch (Direction)
        {
        case JPSDirection::Upper:
            if(isValid(pos.x - 1, pos.y))RadialNearLeft({ pos.x - 1, pos.y }, wParent);
            Scanning = true;
            if (isValid(pos.x + 1, pos.y))RadialNearRight({ pos.x + 1, pos.y }, wParent);
            break;
        case JPSDirection::Lower:
            if (isValid(pos.x - 1, pos.y))RadialNearLeft({ pos.x - 1, pos.y }, wParent);
            Scanning = true;
            if (isValid(pos.x + 1, pos.y))RadialNearRight({ pos.x + 1, pos.y }, wParent);
            break;
        case JPSDirection::Left:
            if (isValid(pos.x, pos.y + 1))RadialNearUpper({ pos.x, pos.y + 1 }, wParent);
            Scanning = true;
            if (isValid(pos.x, pos.y - 1))RadialNearLower({ pos.x, pos.y - 1 }, wParent);
            break;
        case JPSDirection::Right:
            if (isValid(pos.x, pos.y + 1))RadialNearUpper({ pos.x, pos.y + 1 }, wParent);
            Scanning = true;
            if (isValid(pos.x, pos.y - 1))RadialNearLower({ pos.x, pos.y - 1 }, wParent);
            break;
        case JPSDirection::UpperLeft:
            if (isValid(pos.x, pos.y + 1))RadialNearUpper({ pos.x, pos.y + 1 }, wParent);
            Scanning = true;
            if (isValid(pos.x - 1, pos.y))RadialNearLeft({ pos.x - 1, pos.y }, wParent);
            break;
        case JPSDirection::LowerLeft:
            if (isValid(pos.x, pos.y - 1))RadialNearLower({ pos.x, pos.y - 1 }, wParent);
            Scanning = true;
            if (isValid(pos.x - 1, pos.y))RadialNearLeft({ pos.x - 1, pos.y }, wParent);
            break;
        case JPSDirection::UpperRight:
            if (isValid(pos.x, pos.y + 1))RadialNearUpper({ pos.x, pos.y + 1 }, wParent);
            Scanning = true;
            if (isValid(pos.x + 1, pos.y))RadialNearRight({ pos.x + 1, pos.y }, wParent);
            break;
        case JPSDirection::LowerRight:
            if (isValid(pos.x, pos.y - 1))RadialNearLower({ pos.x, pos.y - 1 }, wParent);
            Scanning = true;
            if (isValid(pos.x + 1, pos.y))RadialNearRight({ pos.x + 1, pos.y }, wParent);
            break;
        default:
            break;
        }
    }

    //移动事件和邻近点的检测
    void MoveEvent(JPSVec2 pos, JPSNode* Parent, JPSDirection Direction) {
        Scanning = true;
        JPSNode* wParent = Parent;
        switch (Direction)
        {
        case JPSDirection::Upper:
            CurrentPosition = { pos.x, pos.y + 1 };
            RadialNearLeft(CurrentPosition, wParent);
            Scanning = true;
            RadialNearRight(CurrentPosition, wParent);
            break;
        case JPSDirection::Lower:
            CurrentPosition = { pos.x, pos.y - 1 };
            RadialNearLeft(CurrentPosition, wParent);
            Scanning = true;
            RadialNearRight(CurrentPosition, wParent);
            break;
        case JPSDirection::Left:
            CurrentPosition = { pos.x - 1, pos.y };
            RadialNearUpper(CurrentPosition, wParent);
            Scanning = true;
            RadialNearLower(CurrentPosition, wParent);
            break;
        case JPSDirection::Right:
            CurrentPosition = { pos.x + 1, pos.y };
            RadialNearUpper(CurrentPosition, wParent);
            Scanning = true;
            RadialNearLower(CurrentPosition, wParent);
            break;
        case JPSDirection::UpperLeft:
            CurrentPosition = { pos.x - 1, pos.y + 1 };
            RadialNearUpper(CurrentPosition, wParent);
            Scanning = true;
            RadialNearLeft(CurrentPosition, wParent);
            break;
        case JPSDirection::LowerLeft:
            CurrentPosition = { pos.x - 1, pos.y - 1 };
            RadialNearLower(CurrentPosition, wParent);
            Scanning = true;
            RadialNearLeft(CurrentPosition, wParent);
            break;
        case JPSDirection::UpperRight:
            CurrentPosition = { pos.x + 1, pos.y + 1 };
            RadialNearUpper(CurrentPosition, wParent);
            Scanning = true;
            RadialNearRight(CurrentPosition, wParent);
            break;
        case JPSDirection::LowerRight:
            CurrentPosition = { pos.x + 1, pos.y - 1 };
            RadialNearLower(CurrentPosition, wParent);
            Scanning = true;
            RadialNearRight(CurrentPosition, wParent);
            break;
        default:
            break;
        }
        if (EndPointJudge(CurrentPosition)) {
            ParentJPSNode = NewJPSNode(CurrentPosition, 0, 0, wParent, wParent->Direction);
        }
    }

    //添加邻近点
    JPSNode* AddSkipPoint(JPSVec2 posd, JPSNode* ParentD, std::vector<JPSNode*> Skip, JPSDirection Direction) {
        if (Skip.size() != 0 && Scanning) {
            Scanning = false;
            JPSVec2 LSpos = CurrentPosition - GetProgramme(ParentD->Direction);
            JPSNode* jiao = NewJPSNode(LSpos, 0, calculateH(posd), ParentD, ParentD->Direction);
            ParentD->pChild.push_back(jiao);
            JPSVec2 LSpos2 = LSpos - posd;
            if (fabs(LSpos2.x) > 1 || fabs(LSpos2.y) > 1) {
                if (fabs(LSpos2.x) > 1) {
                    ParentD = NewJPSNode({ LSpos.x + (LSpos2.x < 0 ? 1 : -1), posd.y }, 1, calculateH(LSpos.x + (LSpos2.x < 0 ? 1 : -1), posd.y), jiao, Direction);
                }
                else {
                    ParentD = NewJPSNode({ posd.x, LSpos.y + (LSpos2.y < 0 ? 1 : -1) }, 1, calculateH(posd.x, LSpos.y + (LSpos2.y < 0 ? 1 : -1)), jiao, Direction);
                }
            }
            else {
                ParentD = NewJPSNode(posd, 1, calculateH(posd), jiao, Direction);
            }
            jiao->pChild.push_back(ParentD);
            mJPSDirectionVec2 = &ParentD->pChild;
            for (auto i : Skip)
            {
                i->parent = ParentD;
                mJPSDirectionVec2->push_back(i);
            }
        }
        else {
            for (auto i : Skip)
            {
                mJPSDirectionVec2->push_back(i);
            }
        }
        return ParentD;
    }


    //上
    void RadialNearUpper(JPSVec2 pos, JPSNode* Parent) {
        if (EndPointJudge(pos)) {
            JPSNode* jiao = NewJPSNode(CurrentPosition, 0, 0, Parent, Parent->Direction);
            ParentJPSNode = NewJPSNode(pos, 0, 0, jiao, JPSDirection::UpperLeft);
            return;
        }
        std::vector<JPSNode*> LSJPSDirectionVec2;
        if (!isValid(pos.x - 1, pos.y) && isValid(pos.x - 1, pos.y + 1))//左上强制邻近点
        {
            if (RepeatJudge.find({ pos, JPSDirection::UpperLeft }) == RepeatJudge.end()) {
                float newH = calculateH(pos.x - 1, pos.y + 1);
                LSJPSDirectionVec2.push_back(NewJPSNode(pos, 2, newH, Parent, JPSDirection::UpperLeft));
                RepeatJudge.insert({ pos, JPSDirection::UpperLeft });
            }
        }
        if (!isValid(pos.x + 1, pos.y) && isValid(pos.x + 1, pos.y + 1))//右上强制邻近点
        {
            if (RepeatJudge.find({ pos, JPSDirection::UpperRight }) == RepeatJudge.end()) {
                float newH = calculateH(pos.x + 1, pos.y + 1);
                LSJPSDirectionVec2.push_back(NewJPSNode(pos, 2, newH, Parent, JPSDirection::UpperRight));
                RepeatJudge.insert({ pos, JPSDirection::UpperRight });
            }
        }
        Parent = AddSkipPoint(pos, Parent, LSJPSDirectionVec2, JPSDirection::Upper);
        if (isValid(pos.x, pos.y + 1)) {
            RadialNearUpper({ pos.x, pos.y + 1 }, Parent);
        }
        /*
        int shu = isValid(pos.x, pos.y);
        if (isValid(pos.x, pos.y + shu)) {
            RadialNearUpper({ pos.x, pos.y + shu }, Parent);
        }
        */
    }

    //下
    void RadialNearLower(JPSVec2 pos, JPSNode* Parent) {
        if (EndPointJudge(pos)) {
            JPSNode* jiao = NewJPSNode(CurrentPosition, 0, 0, Parent, Parent->Direction);
            ParentJPSNode = NewJPSNode(pos, 0, 0, jiao, JPSDirection::UpperLeft);
            return;
        }
        std::vector<JPSNode*> LSJPSDirectionVec2;
        if (!isValid(pos.x - 1, pos.y) && isValid(pos.x - 1, pos.y - 1))//左下强制邻近点
        {
            if (RepeatJudge.find({ pos, JPSDirection::LowerLeft }) == RepeatJudge.end()) {
                float newH = calculateH(pos.x - 1, pos.y - 1);
                LSJPSDirectionVec2.push_back(NewJPSNode(pos, 2, newH, Parent, JPSDirection::LowerLeft));
                RepeatJudge.insert({ pos, JPSDirection::LowerLeft });
            }
        }
        if (!isValid(pos.x + 1, pos.y) && isValid(pos.x + 1, pos.y - 1))//右下强制邻近点
        {
            if (RepeatJudge.find({ pos, JPSDirection::LowerRight }) == RepeatJudge.end()) {
                float newH = calculateH(pos.x + 1, pos.y - 1);
                LSJPSDirectionVec2.push_back(NewJPSNode(pos, 2, newH, Parent, JPSDirection::LowerRight));
                RepeatJudge.insert({ pos, JPSDirection::LowerRight });
            }
        }
        Parent = AddSkipPoint(pos, Parent, LSJPSDirectionVec2, JPSDirection::Lower);
        if (isValid(pos.x, pos.y - 1)) {
            RadialNearLower({ pos.x, pos.y - 1 }, Parent);
        }
    }

    //左
    void RadialNearLeft(JPSVec2 pos, JPSNode* Parent) {
        if (EndPointJudge(pos)) {
            JPSNode* jiao = NewJPSNode(CurrentPosition, 0, 0, Parent, Parent->Direction);
            ParentJPSNode = NewJPSNode(pos, 0, 0, jiao, JPSDirection::UpperLeft);
            return;
        }
        std::vector<JPSNode*> LSJPSDirectionVec2;
        if (!isValid(pos.x, pos.y - 1) && isValid(pos.x - 1, pos.y - 1))//左下强制邻近点
        {
            if (RepeatJudge.find({ pos, JPSDirection::LowerLeft }) == RepeatJudge.end()) {
                float newH = calculateH(pos.x - 1, pos.y - 1);
                LSJPSDirectionVec2.push_back(NewJPSNode(pos, 2, newH, Parent, JPSDirection::LowerLeft));
                RepeatJudge.insert({ pos, JPSDirection::LowerLeft });
            }
        }
        if (!isValid(pos.x, pos.y + 1) && isValid(pos.x - 1, pos.y + 1))//左上强制邻近点
        {
            if (RepeatJudge.find({ pos, JPSDirection::UpperLeft }) == RepeatJudge.end()) {
                float newH = calculateH(pos.x - 1, pos.y + 1);
                LSJPSDirectionVec2.push_back(NewJPSNode(pos, 2, newH, Parent, JPSDirection::UpperLeft));
                RepeatJudge.insert({ pos, JPSDirection::UpperLeft });
            }
        }
        Parent = AddSkipPoint(pos, Parent, LSJPSDirectionVec2, JPSDirection::Left);
        if (isValid(pos.x - 1, pos.y)) {
            RadialNearLeft({ pos.x - 1, pos.y }, Parent);
        }
    }

    //右
    void RadialNearRight(JPSVec2 pos, JPSNode* Parent) {
        if (EndPointJudge(pos)) {
            JPSNode* jiao = NewJPSNode(CurrentPosition, 0, 0, Parent, Parent->Direction);
            ParentJPSNode = NewJPSNode(pos, 0, 0, jiao, JPSDirection::UpperLeft);
            return;
        }
        std::vector<JPSNode*> LSJPSDirectionVec2;
        if (!isValid(pos.x, pos.y - 1) && isValid(pos.x + 1, pos.y - 1))//右下强制邻近点
        {
            if (RepeatJudge.find({ pos, JPSDirection::LowerRight }) == RepeatJudge.end()) {
                float newH = calculateH(pos.x + 1, pos.y - 1);
                LSJPSDirectionVec2.push_back(NewJPSNode(pos, 2, newH, Parent, JPSDirection::LowerRight));
                RepeatJudge.insert({ pos, JPSDirection::LowerRight });
            }
        }
        if (!isValid(pos.x, pos.y + 1) && isValid(pos.x + 1, pos.y + 1))//右上强制邻近点
        {
            if (RepeatJudge.find({ pos, JPSDirection::UpperRight }) == RepeatJudge.end()) {
                float newH = calculateH(pos.x + 1, pos.y + 1);
                LSJPSDirectionVec2.push_back(NewJPSNode(pos, 2, newH, Parent, JPSDirection::UpperRight));
                RepeatJudge.insert({ pos, JPSDirection::UpperRight });
            }
        }
        Parent = AddSkipPoint(pos, Parent, LSJPSDirectionVec2, JPSDirection::Right);
        if (isValid(pos.x + 1, pos.y)) {
            RadialNearRight({ pos.x + 1, pos.y }, Parent);
        }
    }

    //销毁树
    void DeleteNodeS(JPSNode* jPSNode) {
        for (auto i : jPSNode->pChild)
        {
            if (i->pChild.size() != 0)DeleteNodeS(i);
            DeleteJPSNode(i);
        }
    }

    //节点排序
    inline void NodeSort(JPSNode* jPSNode) {
        std::sort(jPSNode->pChild.begin(), jPSNode->pChild.end(),
            [](JPSNode* a, JPSNode* b) {
                return a->f > b->f;
            }
        );
        for (auto i : jPSNode->pChild)
        {
            NodeSort(i);
        }
    }

public:
    JPS(int Range, int Steps) :mRange(Range), mSteps(Steps) {
    }
    ~JPS() {
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
    void FindPath(JPSVec2 start, JPSVec2 target, std::vector<JPSVec2>* PathVector, JPSVec2 deviation = { 0, 0 }) {
        //目标地点合法性
        target += deviation;
        start += deviation;
        TargetPosition = target - start;
        StartingPoint = start;
        if (!isValid(TargetPosition.x, TargetPosition.y)) {
            return;
        }
        //开始寻路
        mPathfindingCompleted = false;
        Finish = false;
        CurrentPosition = { 0, 0 };
        RepeatJudge.clear();
        
        JPSNode* LSDEJPSNode = NewJPSNode(CurrentPosition, 0, 0, nullptr, JPSDirection::Upper);
        if (GetDirectionLegitimate(CurrentPosition, JPSDirection::UpperLeft ))LSDEJPSNode->pChild.push_back(NewJPSNode(CurrentPosition, 0, calculateH(CurrentPosition.x - 1, CurrentPosition.y + 1), LSDEJPSNode, JPSDirection::UpperLeft));
        if (GetDirectionLegitimate(CurrentPosition, JPSDirection::LowerLeft ))LSDEJPSNode->pChild.push_back(NewJPSNode(CurrentPosition, 0, calculateH(CurrentPosition.x - 1, CurrentPosition.y - 1), LSDEJPSNode, JPSDirection::LowerLeft));
        if (GetDirectionLegitimate(CurrentPosition, JPSDirection::UpperRight))LSDEJPSNode->pChild.push_back(NewJPSNode(CurrentPosition, 0, calculateH(CurrentPosition.x + 1, CurrentPosition.y + 1), LSDEJPSNode, JPSDirection::UpperRight));
        if (GetDirectionLegitimate(CurrentPosition, JPSDirection::LowerRight))LSDEJPSNode->pChild.push_back(NewJPSNode(CurrentPosition, 0, calculateH(CurrentPosition.x + 1, CurrentPosition.y - 1), LSDEJPSNode, JPSDirection::LowerRight));
        if (GetDirectionLegitimate(CurrentPosition, JPSDirection::Upper))LSDEJPSNode->pChild.push_back(NewJPSNode(CurrentPosition, 0, calculateH(CurrentPosition.x, CurrentPosition.y + 1), LSDEJPSNode, JPSDirection::Upper));
        if (GetDirectionLegitimate(CurrentPosition, JPSDirection::Lower))LSDEJPSNode->pChild.push_back(NewJPSNode(CurrentPosition, 0, calculateH(CurrentPosition.x, CurrentPosition.y - 1), LSDEJPSNode, JPSDirection::Lower));
        if (GetDirectionLegitimate(CurrentPosition, JPSDirection::Left ))LSDEJPSNode->pChild.push_back(NewJPSNode(CurrentPosition, 0, calculateH(CurrentPosition.x - 1, CurrentPosition.y), LSDEJPSNode, JPSDirection::Left ));
        if (GetDirectionLegitimate(CurrentPosition, JPSDirection::Right))LSDEJPSNode->pChild.push_back(NewJPSNode(CurrentPosition, 0, calculateH(CurrentPosition.x + 1, CurrentPosition.y), LSDEJPSNode, JPSDirection::Right));
        
        NodeSort(LSDEJPSNode);

        JPSNode* DeleteJPSNodeX;
        
        do
        {
            if (LSDEJPSNode->pChild.size() != 0) {
                DeleteJPSNodeX = LSDEJPSNode->pChild.back();
                LSDEJPSNode->pChild.pop_back();
                LSDEJPSNode = DeleteJPSNodeX;
                
                if (LSDEJPSNode->pChild.size() == 0) {
                    ParentJPSNode = LSDEJPSNode;//绑定父类
                    mJPSDirectionVec2 = &LSDEJPSNode->pChild;
                    CurrentPosition = { LSDEJPSNode->x, LSDEJPSNode->y };
                    RadialEvent(CurrentPosition, LSDEJPSNode, LSDEJPSNode->Direction);
                    while (GetDirectionLegitimate(CurrentPosition, LSDEJPSNode->Direction)) {
                        MoveEvent(CurrentPosition, LSDEJPSNode, LSDEJPSNode->Direction);
                    }
                    if (Finish) {
                        break;
                    }
                    if (LSDEJPSNode->pChild.size() == 0) {
                        DeleteJPSNodeX = LSDEJPSNode;
                        LSDEJPSNode = LSDEJPSNode->parent;
                        DeleteJPSNode(DeleteJPSNodeX);
                    }
                    else {
                        NodeSort(LSDEJPSNode);
                    }
                }
            }
            else {
                DeleteJPSNodeX = LSDEJPSNode;
                LSDEJPSNode = LSDEJPSNode->parent;
                DeleteJPSNode(DeleteJPSNodeX);
            }
        } while (LSDEJPSNode != nullptr);
        if (LSDEJPSNode != nullptr) {
            StartingPoint -= deviation;
            PathVector->push_back({ ParentJPSNode->x + StartingPoint.x, ParentJPSNode->y + StartingPoint.y });
            if (ParentJPSNode->parent->pChild.size() != 0) {
                bool cf = true;
                for (auto i : ParentJPSNode->parent->pChild)
                {
                    if (i == ParentJPSNode) {//元素是否重复
                        cf = false;
                        break;
                    }
                }
                if (cf) {
                    ParentJPSNode->parent->pChild.push_back(ParentJPSNode);//添加到多叉树
                }
            }
            else {
                ParentJPSNode->parent->pChild.push_back(ParentJPSNode);//添加到多叉树
            }
            ParentJPSNode = ParentJPSNode->parent;
            JPSVec2 CFpos;
            do
            {
                CFpos = JPSVec2{ ParentJPSNode->x + StartingPoint.x, ParentJPSNode->y + StartingPoint.y };
                if (PathVector->back() != CFpos) {
                    PathVector->push_back(CFpos);
                }
                if (ParentJPSNode->parent == nullptr) {
                    DeleteNodeS(ParentJPSNode);//销毁树
                    DeleteJPSNode(ParentJPSNode);
                    mPathfindingCompleted = true;
                    return;
                }
                else {
                    if (ParentJPSNode->parent->pChild.size() != 0) {
                        bool cf = true;
                        for (auto i : ParentJPSNode->parent->pChild)
                        {
                            if (i == ParentJPSNode) {//元素是否重复
                                cf = false;
                                break;
                            }
                        }
                        if (cf) {
                            ParentJPSNode->parent->pChild.push_back(ParentJPSNode);//添加到多叉树
                        }
                    }
                    else {
                        ParentJPSNode->parent->pChild.push_back(ParentJPSNode);//添加到多叉树
                    }
                }
                ParentJPSNode = ParentJPSNode->parent;
            } while (true);
        }

        mPathfindingCompleted = true;
        return;
    }
};