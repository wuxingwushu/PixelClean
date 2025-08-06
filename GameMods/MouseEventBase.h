
class MouseEventBase
{
public:
    MouseEventBase(/* args */){};
    ~MouseEventBase(){};

    // 左键点击
    virtual void LeftClick(){};
    // 右键持续
    virtual void LeftHold(){};
    // 右键放下
    virtual void LeftLetGo(){};

    // 右键点击
    virtual void RightClick(){};
    // 右键持续
    virtual void RightHold(){};
    // 右键放下
    virtual void RightLetGo(){};
};
