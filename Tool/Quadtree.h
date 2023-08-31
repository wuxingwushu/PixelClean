#pragma once

// https://github.com/pvigier/Quadtree

#include <cassert>
#include <algorithm>
#include <array>
#include <memory>
#include <type_traits>
#include <vector>

namespace quadtree
{
template<typename T>
class Vector2
{
public:
    T x;
    T y;

    constexpr Vector2<T>(T X = 0, T Y = 0) noexcept : x(X), y(Y)
    {

    }

    constexpr Vector2<T>& operator+=(const Vector2<T>& other) noexcept
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    constexpr Vector2<T>& operator/=(T t) noexcept
    {
        x /= t;
        y /= t;
        return *this;
    }
};

template<typename T>
constexpr Vector2<T> operator+(Vector2<T> lhs, const Vector2<T>& rhs) noexcept
{
    lhs += rhs;
    return lhs;
}

template<typename T>
constexpr Vector2<T> operator/(Vector2<T> vec, T t) noexcept
{
    vec /= t;
    return vec;
}




template<typename T>
class Box
{
public:
    T left;
    T top;
    T width; // Must be positive
    T height; // Must be positive

    constexpr Box(T Left = 0, T Top = 0, T Width = 0, T Height = 0) noexcept :
        left(Left), top(Top), width(Width), height(Height)
    {

    }

    constexpr Box(const Vector2<T>& position, const Vector2<T>& size) noexcept :
        left(position.x), top(position.y), width(size.x), height(size.y)
    {

    }

    constexpr T getRight() const noexcept
    {
        return left + width;
    }

    constexpr T getBottom() const noexcept
    {
        return top + height;
    }

    constexpr Vector2<T> getTopLeft() const noexcept
    {
        return Vector2<T>(left, top);
    }

    //获取中心点
    constexpr Vector2<T> getCenter() const noexcept
    {
        return Vector2<T>(left + width / 2, top + height / 2);
    }

    constexpr Vector2<T> getSize() const noexcept
    {
        return Vector2<T>(width, height);
    }

    //是否包含在内 包含: true
    constexpr bool contains(const Box<T>& box) const noexcept
    {
        return left <= box.left && box.getRight() <= getRight() &&
            top <= box.top && box.getBottom() <= getBottom();
    }

    //是否有相交点 有相交点: true
    constexpr bool intersects(const Box<T>& box) const noexcept
    {
        return !(left >= box.getRight() || getRight() <= box.left ||
            top >= box.getBottom() || getBottom() <= box.top);
    }

    //点在范围内
    constexpr bool Range(const Vector2<T>& pos) const noexcept
    {
        return left <= pos.x && pos.x <= getRight() &&
            top <= pos.y && pos.y <= getBottom();
    }
};




template<typename T, typename GetBox, typename Equal = std::equal_to<T>, typename Float = float>
class Quadtree
{
    static_assert(std::is_convertible_v<std::invoke_result_t<GetBox, const T&>, Box<Float>>,
        "GetBox must be a callable of signature Box<Float>(const T&)");
    static_assert(std::is_convertible_v<std::invoke_result_t<Equal, const T&, const T&>, bool>,
        "Equal must be a callable of signature bool(const T&, const T&)");
    static_assert(std::is_arithmetic_v<Float>);

public:
    Quadtree(const Box<Float>& box, const GetBox& getBox = GetBox(),
        const Equal& equal = Equal()) :
        mBox(box), mRoot(std::make_unique<Node>()), mGetBox(getBox), mEqual(equal)
    {}

    //添加
    void add(const T& value)
    {
        add_(mRoot.get(), 0, mBox, value);
    }

    //移除
    void remove(const T& value)
    {
        remove(mRoot.get(), mBox, value);
    }

    //查询
    std::vector<T> query(const Box<Float>& box) const
    {
        auto values = std::vector<T>();
        query(mRoot.get(), mBox, box, values);
        return values;
    }

    //获取全部两两相交
    std::vector<std::pair<T, T>> findAllIntersections() const
    {
        auto intersections = std::vector<std::pair<T, T>>();
        findAllIntersections(mRoot.get(), intersections);
        return intersections;
    }

    //更新全部数据
    void UpdateQuadtree() {
        UpdateAllDescendants(mRoot.get(), mBox);
    }

    Box<Float> getBox() const 
    {
        return mBox;
    }
    
private:
    static constexpr auto Threshold = std::size_t(16);  //门槛
    static constexpr auto MaxDepth = std::size_t(8);    //最大深度

    struct Node
    {
        std::array<std::unique_ptr<Node>, 4> children;
        std::vector<T> values;
    };

    Box<Float> mBox;
    std::unique_ptr<Node> mRoot;
    GetBox mGetBox;//获取 自定义数据结构里的Box 函数指针
    Equal mEqual;//判断 两个 是否 相等

    //判断 是否是 最小的正方形，没有子节点， 没有返回 true
    bool isLeaf(const Node* node) const
    {
        return !static_cast<bool>(node->children[0]);
    }

    //计算获取 正方形 中 均分四份之一的 小正方形
    Box<Float> computeBox(const Box<Float>& box, int i) const
    {
        auto origin = box.getTopLeft();
        auto childSize = box.getSize() / static_cast<Float>(2);
        switch (i)
        {
            // North West
            case 0:
                return Box<Float>(origin, childSize);
            // Norst East
            case 1:
                return Box<Float>(Vector2<Float>(origin.x + childSize.x, origin.y), childSize);
            // South West
            case 2:
                return Box<Float>(Vector2<Float>(origin.x, origin.y + childSize.y), childSize);
            // South East
            case 3:
                return Box<Float>(origin + childSize, childSize);
            default:
                assert(false && "Invalid child index");
                return Box<Float>();
        }
    }

    //获取 node 在 那个 小正方形 中，或者 不在
    int getQuadrant(const Box<Float>& nodeBox, const Box<Float>& valueBox) const
    {
        auto center = nodeBox.getCenter();
        // West
        if (valueBox.getRight() < center.x)
        {
            // North West
            if (valueBox.getBottom() < center.y)
                return 0;
            // South West
            else if (valueBox.top >= center.y)
                return 2;
            // Not contained in any quadrant
            else
                return -1;
        }
        // East
        else if (valueBox.left >= center.x)
        {
            // North East
            if (valueBox.getBottom() < center.y)
                return 1;
            // South East
            else if (valueBox.top >= center.y)
                return 3;
            // Not contained in any quadrant
            else
                return -1;
        }
        // Not contained in any quadrant
        else
            return -1;
    }

    void add_(Node* node, std::size_t depth, const Box<Float>& box, const T& value)
    {
        assert(node != nullptr);
        assert(box.contains(mGetBox(value)));
        if (isLeaf(node))
        {
            //没有子节点了

            // Insert the value in this node if possible
            if (depth >= MaxDepth || node->values.size() < Threshold)
                node->values.push_back(value);
            // Otherwise, we split and we try again
            else
            {
                split(node, box);//切分正方形
                add_(node, depth, box, value);
            }
        }
        else
        {
            //有子节点

            auto i = getQuadrant(box, mGetBox(value));//在那个子节点
            // Add the value in a child if the value is entirely contained in it
            if (i != -1)//没在子节点范围内
                add_(node->children[static_cast<std::size_t>(i)].get(), depth + 1, computeBox(box, i), value);
            // Otherwise, we add the value in the current node
            else
                node->values.push_back(value);//都不在就父类储存
        }
    }

    //切分正方形
    void split(Node* node, const Box<Float>& box)
    {
        assert(node != nullptr);
        assert(isLeaf(node) && "Only leaves can be split");
        // Create children
        for (auto& child : node->children)
            child = std::make_unique<Node>();//构建子节点
        // Assign values to children
        auto newValues = std::vector<T>(); // New values for this node
        for (const auto& value : node->values)
        {
            auto i = getQuadrant(box, mGetBox(value));//判断在那个小正方形内
            if (i != -1)
                node->children[static_cast<std::size_t>(i)]->values.push_back(value);
            else
                newValues.push_back(value);//都不在就父类储存
        }
        node->values = std::move(newValues);//转移数据到
    }

    //移除
    bool remove(Node* node, const Box<Float>& box, const T& value)
    {
        assert(node != nullptr);
        assert(box.contains(mGetBox(value)));
        if (isLeaf(node))
        {
            // Remove the value from node
            removeValue(node, value);
            return true;//当时在子节点时，递归函数 返回 true
        }
        else
        {
            // Remove the value in a child if the value is entirely contained in it
            auto i = getQuadrant(box, mGetBox(value));
            if (i != -1)
            {
                if (remove(node->children[static_cast<std::size_t>(i)].get(), computeBox(box, i), value))//收到子节点 返回的 true
                    return tryMerge(node);
            }
            // Otherwise, we remove the value from the current node
            else
                removeValue(node, value);
            return false;
        }
    }

    //删除对应的数据
    void removeValue(Node* node, const T& value)
    {
        // Find the value in node->values //找到对应的数据
        auto it = std::find_if(std::begin(node->values), std::end(node->values),
            [this, &value](const auto& rhs){ return mEqual(value, rhs); });
        assert(it != std::end(node->values) && "Trying to remove a value that is not present in the node");
        // Swap with the last element and pop back
        *it = std::move(node->values.back());//最后一个数据赋值给被删除的数据
        node->values.pop_back();//弹出最后一个数据
    }

    //尝试合并正方形
    bool tryMerge(Node* node)
    {
        assert(node != nullptr);
        assert(!isLeaf(node) && "Only interior nodes can be merged");
        auto nbValues = node->values.size();
        //获取正方形内，数据数量
        for (const auto& child : node->children)
        {
            if (!isLeaf(child.get()))
                return false;
            nbValues += child->values.size();
        }
        //当数据小于 Threshold 时 合并正方形
        if (nbValues <= Threshold)
        {
            node->values.reserve(nbValues);
            // Merge the values of all the children
            for (const auto& child : node->children)
            {
                for (const auto& value : child->values)
                    node->values.push_back(value);//把子节点的数据都还给父节点
            }
            // Remove the children
            for (auto& child : node->children)
                child.reset();//释放父节点
            return true;
        }
        else
            return false;
    }

    //查询
    void query(Node* node, const Box<Float>& box, const Box<Float>& queryBox, std::vector<T>& values) const
    {
        assert(node != nullptr);
        assert(queryBox.intersects(box));
        for (const auto& value : node->values)//遍历数据
        {
            if (queryBox.intersects(mGetBox(value)))//获取相交的正方形
                values.push_back(value);
        }
        if (!isLeaf(node))//当子节点不为空时，进行递归
        {
            for (auto i = std::size_t(0); i < node->children.size(); ++i)
            {
                auto childBox = computeBox(box, static_cast<int>(i));//获取小正方形
                if (queryBox.intersects(childBox))//和小正方形 是否 相交
                    query(node->children[i].get(), childBox, queryBox, values);//递归
            }
        }
    }

    //在自身正方形内查找两两相交的
    void findAllIntersections(Node* node, std::vector<std::pair<T, T>>& intersections) const
    {
        // Find intersections between values stored in this node
        // Make sure to not report the same intersection twice
        for (auto i = std::size_t(0); i < node->values.size(); ++i)
        {
            for (auto j = std::size_t(0); j < i; ++j)
            {
                if (mGetBox(node->values[i]).intersects(mGetBox(node->values[j])))
                    intersections.emplace_back(node->values[i], node->values[j]);
            }
        }
        if (!isLeaf(node))
        {
            // Values in this node can intersect values in descendants
            for (const auto& child : node->children)//编译每一个子节点
            {
                for (const auto& value : node->values)//父类每一个对象都编译一边
                    findIntersectionsInDescendants(child.get(), value, intersections);
            }
            // Find intersections in children
            for (const auto& child : node->children)
                findAllIntersections(child.get(), intersections);//递归
        }
    }

    //输入数据 和 子节点 相交判断
    void findIntersectionsInDescendants(Node* node, const T& value, std::vector<std::pair<T, T>>& intersections) const
    {
        // Test against the values stored in this node
        for (const auto& other : node->values)
        {
            if (mGetBox(value).intersects(mGetBox(other)))
                intersections.emplace_back(value, other);
        }
        // Test against values stored into descendants of this node
        if (!isLeaf(node))//还有子节点进行递归
        {
            for (const auto& child : node->children)
                findIntersectionsInDescendants(child.get(), value, intersections);
        }
    }

    //更新全部数据
    bool UpdateAllDescendants(Node* node, const Box<Float>& box)
    {
        bool MergeBool = false;//是否尝试合并
        for (auto i = std::size_t(0); i < node->values.size(); ++i)
        {
            if (!box.Range(mGetBox(node->values[i]).getCenter())) {//点不在范围内
                //重新添加
                add_(mRoot.get(), 0, mBox, node->values[i]);
                //删除久位置
                node->values[i] = std::move(node->values.back());
                node->values.pop_back();
                //如果是子节点就通知父节点尝试合并
                if (!isLeaf(node))
                    MergeBool = true;
            }
        }
        if (!isLeaf(node)) {
            for (auto i = std::size_t(0); i < node->children.size(); ++i)//遍历所有子节点
                if (UpdateAllDescendants(node->children[i].get(), computeBox(box, i)))
                    tryMerge(node);//尝试合并
        }
        return MergeBool;
    }
};

}
