BaseStruct 数据结构体
BaseDefine 常量
BaseCalculate 基本计算函数

BaseArbiter 基础物理判决
PhysicsArbiter 物理判决
PhysicsCollide 物理碰撞

PhysicsShape 物理形状
PhysicsParticle 物理粒子
BaseGrid 基础网格
BaseOutline 基础轮廓

MapFormwork 模板地图
MapStatic 静态地图
MapDynamic 动态地图

PhysicsFormwrok 刚体物理模板

PhysicsJunction 物理连接

PhysicsWorld 物理世界

```mermaid
%% 時序圖例子,-> 直線，-->虛線，->>實線箭頭
%% graph 橫向：LR, 豎向：TD
graph TD

PhysicsParticle(PhysicsParticle\n物理粒子)
PhysicsShape(PhysicsShape\n物理形状)
BaseGrid(BaseGrid\n基础网格)
BaseOutline(BaseOutline\n基础轮廓)
MapStatic(MapStatic\n静态地图)
MapFormwork(MapFormwork\n模板地图)
MapDynamic(MapDynamic\n动态地图)
PhysicsWorld(PhysicsWorld\n物理世界)
PhysicsFormwrok(PhysicsFormwrok\n刚体物理模板)

PhysicsJunction(PhysicsJunction\n物理连接)
BaseArbiter(BaseArbiter\n基础物理判决)
PhysicsArbiter(PhysicsArbiter\n物理判决)
PhysicsCollide(PhysicsCollide\n物理碰撞)


PhysicsFormwrok --> PhysicsParticle

BaseGrid --> BaseOutline
BaseOutline --> PhysicsShape
PhysicsParticle --> PhysicsShape


MapFormwork --> MapStatic
BaseGrid --> MapStatic

MapFormwork --> MapDynamic
BaseGrid --> MapDynamic

BaseArbiter --> PhysicsArbiter
PhysicsCollide --> PhysicsArbiter

PhysicsParticle --> PhysicsJunction
PhysicsShape --> PhysicsJunction

PhysicsArbiter --> PhysicsWorld
PhysicsParticle --> PhysicsWorld
PhysicsShape --> PhysicsWorld
MapDynamic --> PhysicsWorld
MapStatic --> PhysicsWorld






```


