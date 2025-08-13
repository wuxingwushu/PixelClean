BaseStruct 数据结构体
BaseDefine 常量
BaseCalculate 基本计算函数

BaseArbiter 基础判决
PhysicsArbiter 物理判决
PhysicsBaseArbiter 基础物理判决
PhysicsBaseCollide 基础物理碰撞

PhysicsParticle 物理粒子
PhysicsAngle 物理角度
PhysicsCircle 物理圆
PhysicsLine 物理线段
PhysicsShape 物理形状
BaseGrid 基础网格
BaseOutline 基础轮廓

MapFormwork 模板地图
MapStatic 静态地图
MapDynamic 动态地图

PhysicsFormwrok 刚体物理模板

PhysicsJoint 物理关节
PhysicsJunction 物理连接

PhysicsWorld 物理世界
GridSearch 网格搜索


```mermaid
%% 時序圖例子,-> 直線，-->虛線，->>實線箭頭
%% graph 橫向：LR, 豎向：TD
graph TD

BaseGrid(BaseGrid \n 基础网格)
BaseOutline(BaseOutline \n 基础轮廓)

PhysicsFormwrok(PhysicsFormwrok \n 刚体物理模板)
PhysicsParticle(PhysicsParticle \n 物理粒子)
PhysicsAngle(PhysicsAngle \n 物理角度)
PhysicsCircle(PhysicsCircle \n 物理圆)
PhysicsLine(PhysicsLine \n 物理线段)
PhysicsShape(PhysicsShape \n 物理形状)

MapStatic(MapStatic \n 静态地图)
MapFormwork(MapFormwork \n 模板地图)
MapDynamic(MapDynamic \n 动态地图)

PhysicsWorld(PhysicsWorld \n 物理世界)
GridSearch(GridSearch \n 网格搜索)

PhysicsJoint(PhysicsJoint \n 物理关节)
PhysicsJunction(PhysicsJunction \n 物理连接)

BaseArbiter(BaseArbiter \n 基础判决)
PhysicsArbiter(PhysicsArbiter \n 物理判决)
PhysicsBaseArbiter(PhysicsBaseArbiter \n 基础物理判决)
PhysicsBaseCollide(PhysicsBaseCollide \n 基础物理碰撞)


PhysicsFormwrok --> PhysicsParticle
PhysicsParticle --> PhysicsAngle

BaseGrid --> BaseOutline
BaseOutline --> PhysicsShape
PhysicsAngle --> PhysicsShape
PhysicsAngle --> PhysicsCircle
PhysicsAngle --> PhysicsLine


MapFormwork --> MapStatic
BaseGrid --> MapStatic

MapFormwork --> MapDynamic
BaseGrid --> MapDynamic

BaseArbiter --> PhysicsBaseArbiter
PhysicsBaseArbiter --> PhysicsArbiter
PhysicsBaseCollide --> PhysicsArbiter

PhysicsParticle --> PhysicsJunction
PhysicsAngle --> PhysicsJunction
PhysicsAngle --> PhysicsJoint

MapFormwork --> PhysicsWorld
GridSearch --> PhysicsWorld

PhysicsShape --> GridSearch
PhysicsCircle --> GridSearch
PhysicsParticle --> GridSearch


```


