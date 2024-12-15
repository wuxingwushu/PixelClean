BaseStruct 数据结构体
BaseDefine 常量
BaseCalculate 基本计算函数

PhysicsShape 物理形状
PhysicsParticle 物理粒子
BaseGrid 基础网格
BaseOutline 基础轮廓

MapFormwork 模板地图
MapStatic 静态地图
MapDynamic 动态地图

PhysicsJunction 物理连接

PhysicsWorld 物理世界

```mermaid
%% 時序圖例子,-> 直線，-->虛線，->>實線箭頭
%% graph 橫向：LR, 豎向：TD
graph TD

A(PhysicsParticle\n物理粒子)
B(PhysicsShape\n物理形状)
C(BaseGrid\n基础网格)
D(BaseOutline\n基础轮廓)
E(MapStatic\n静态地图)
F(MapFormwork\n模板地图)
G(MapDynamic\n动态地图)
W(PhysicsWorld\n物理世界)

C --> D
A --> B
D --> B

F --> E
C --> E

F --> G
C --> G


A --> W
B --> W

G --> W
E --> W


```