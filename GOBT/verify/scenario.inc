// gobot 行为回归场景：new_test.cpp include 后编译运行，输出需与 baseline.txt 逐行一致。
// 只使用公开 API；输出格式固定，禁止打印地址等非确定性内容。
// ⚠ 修改本文件后必须重新生成 baseline.txt（verify/README 见 GOBT/CMakeLists.txt 的 GOBOT_BUILD_VERIFY）。

#include <any>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using Map = std::unordered_map<gobot::WorldKey, gobot::WorldValue>;

std::vector<std::string> g_events;

int g_fail_left = 0;     // >0 时动作强制失败，用于驱动战术失败路径
int g_wander_ticks = 0;  // 巡逻第二步的 Running/Success 交替计数

const char* st(gobot::Status s) {
    switch (s) {
        case gobot::Status::Success: return "Success";
        case gobot::Status::Failure: return "Failure";
        case gobot::Status::Running: return "Running";
    }
    return "?";
}

std::string join_events() {
    if (g_events.empty()) return "-";
    std::string out;
    for (std::size_t i = 0; i < g_events.size(); ++i) {
        if (i) out += "|";
        out += g_events[i];
    }
    return out;
}

// 永远返回 Running 的动作：驱动"子树被重置后才会重新实例化"的观测
gobot::BTNodePtr make_running_subtree(int& created, const char* action_name) {
    ++created;
    auto act = std::make_shared<gobot::Action>(action_name, Map{}, Map{},
        [](gobot::Context&) { return gobot::Status::Running; }, 1.0f);
    return std::make_shared<gobot::OperationalActionNode>(act);
}

} // namespace

int main() {
    auto bb = std::make_shared<gobot::SharedBlackboard>();
    auto bus = std::make_shared<gobot::EventBus>();
    bus->set_deferred(true);
    bus->subscribe(gobot::events::kSubgoalCompleted,
        [](const std::any&) { g_events.push_back("SUBGOAL_COMPLETED"); });
    bus->subscribe(gobot::events::kTacticalFailure,
        [](const std::any&) { g_events.push_back("TACTICAL_FAILURE"); });
    bus->subscribe(gobot::events::kGoalCompleted,
        [](const std::any&) { g_events.push_back("GOAL_COMPLETED"); });
    bus->subscribe(gobot::events::kStrategicInterrupt,
        [](const std::any&) { g_events.push_back("STRATEGIC_INTERRUPT"); });

    auto ws = bb->world_state();
    ws->set("injured", false);
    ws->set("kills", 0);
    ws->set("player_visible", false);
    ws->set("hp", 100);        // int
    ws->set("distance", 5.0f); // float

    // ---- 战略层：目标注册与优先级 ----
    auto gm = std::make_shared<gobot::GoalManager>();
    gm->add_goal(std::make_shared<gobot::Goal>("wander", Map{{"kills", 9}}, 10));
    gm->add_goal(std::make_shared<gobot::Goal>("hunt", Map{{"kills", 3}}, 50));
    auto survive = std::make_shared<gobot::Goal>("survive", Map{{"injured", false}}, 100);
    survive->set_suspendible(false);
    gm->add_goal(survive);

    // ---- 战术层：分解策略 ----
    auto td = std::make_shared<gobot::TacticalDecomposer>();
    td->register_strategy("hunt", [](const gobot::Goal&) {
        std::vector<gobot::SubgoalPtr> out;
        out.push_back(std::make_shared<gobot::Subgoal>(gobot::SubgoalType::MoveTo,
            "hunt.scout", Map{{"player_visible", true}}, 1));
        out.push_back(std::make_shared<gobot::Subgoal>(gobot::SubgoalType::AcquireItem,
            "hunt.strike", Map{{"kills", 3}}, 2));
        return out;
    });
    td->register_strategy("wander", [](const gobot::Goal&) {
        std::vector<gobot::SubgoalPtr> out;
        out.push_back(std::make_shared<gobot::Subgoal>(gobot::SubgoalType::Patrol,
            "wander.step", Map{{"kills", 9}}, 1));
        return out;
    });

    // 未注册策略 → 退化为单个 Custom 子目标
    auto fallback = td->decompose(gobot::Goal("lonely", Map{{"z", 1}}, 0));
    std::printf("fallback count=%zu type=%d name=%s\n", fallback.size(),
        static_cast<int>(fallback[0]->type()), fallback[0]->name().c_str());

    // ---- 战术层：子树库 ----
    auto sl = std::make_shared<gobot::SubtreeLibrary>();
    sl->register_subtree(gobot::SubgoalType::MoveTo, []() -> gobot::BTNodePtr {
        auto act = std::make_shared<gobot::Action>("scout", Map{{"player_visible", false}},
            Map{{"player_visible", true}},
            [](gobot::Context&) { return gobot::Status::Success; }, 1.0f);
        return std::make_shared<gobot::OperationalActionNode>(act);
    });
    sl->register_subtree(gobot::SubgoalType::AcquireItem, []() -> gobot::BTNodePtr {
        auto act = std::make_shared<gobot::Action>("strike", Map{}, Map{},
            [](gobot::Context& ctx) {
                if (g_fail_left > 0) {
                    --g_fail_left;
                    return gobot::Status::Failure;
                }
                auto world = ctx.blackboard().world_state();
                world->set("kills", world->get<int>("kills").value_or(0) + 1);
                return gobot::Status::Success;
            }, 1.0f);
        return std::make_shared<gobot::OperationalActionNode>(act);
    });
    // 巡逻子树：单个步进动作交替 Running/Success（原先是 SequenceNode + 两个动作的序列）
    sl->register_subtree(gobot::SubgoalType::Patrol, []() -> gobot::BTNodePtr {
        auto act = std::make_shared<gobot::Action>("wander.step", Map{}, Map{},
            [](gobot::Context&) {
                if (g_fail_left > 0) {
                    --g_fail_left;
                    return gobot::Status::Failure;
                }
                ++g_wander_ticks;
                return (g_wander_ticks % 2 == 0) ? gobot::Status::Success : gobot::Status::Running;
            }, 1.0f);
        return std::make_shared<gobot::OperationalActionNode>(act);
    });

    auto built = gobot::TreeBuilder()
                     .with_blackboard(bb)
                     .with_event_bus(bus)
                     .with_goal_manager(gm)
                     .with_decomposer(td)
                     .with_subtree_library(sl)
                     .build();
    gobot::BTExecutor executor(built.root, built.context);

    // ---- 主时间线 ----
    auto step = [&](const char* label) {
        g_events.clear();
        const gobot::Status s = executor.tick_once();
        const gobot::GoalPtr goal = bb->current_goal();
        const gobot::SubgoalPtr sg = bb->current_subgoal();
        std::printf("%-5s status=%-7s goal=%-7s subgoal=%-12s queue=%zu kills=%d pv=%d ev=%s\n",
            label, st(s), goal ? goal->name().c_str() : "(none)",
            sg ? sg->name().c_str() : "(none)", bb->subgoal_queue().size(),
            ws->get<int>("kills").value_or(-1),
            ws->get<bool>("player_visible").value_or(false) ? 1 : 0, join_events().c_str());
    };

    step("t1");   // survive 已满足被跳过；hunt 选中并分解 → scout 成功
    g_fail_left = 1;
    step("t2");   // strike 失败一次 → 子树重试 → Running
    step("t3");   // strike 成功 → kills=1 → 重新分解
    step("t4");   // scout 前置已不满足 → 战术失败
    step("t5");   // strike 成功 → kills=2
    step("t6");   // scout 再次失败
    step("t7");   // strike 成功 → kills=3 → hunt 达成（GOAL_COMPLETED）
    step("t8");   // 切到 wander，巡逻步进动作返回 Running
    step("t9");   // 步进动作返回 Success → 巡逻子目标完成
    step("t10");  // 新一轮巡逻
    g_fail_left = 3;
    step("t11");  // 巡逻失败 #1
    step("t12");  // 巡逻失败 #2
    step("t13");  // 巡逻失败 #3 → 达到阈值 → suspend("wander", 180)
    step("t14");  // 全部目标被跳过/挂起 → 无目标 → Success
    step("t15");  // 仍无目标
    std::printf("suspended_wander=%d goals=%zu\n", gm->is_suspended("wander") ? 1 : 0,
        gm->goals().size());

    gm->unsuspend("wander");
    step("t16");  // 恢复调度 → 重新选中 wander
    std::printf("suspended_wander=%d\n", gm->is_suspended("wander") ? 1 : 0);

    gm->suspend("wander", 5);
    step("t17");  // 全部目标被跳过或挂起 → 无目标 → 清空目标与子目标
    step("t18");  // 持续无目标
    gm->unsuspend("wander");
    step("t19");  // 从"无目标"切回 wander（current_goal 由 nullptr 变更）

    // ---- 世界状态与保留 API ----
    std::printf("ws cross_int_float=%d cross_str=%d getraw=%d getraw_missing=%d raw_count=%d\n",
        ws->matches(Map{{"kills", 3.0f}}) ? 1 : 0,
        ws->matches(Map{{"kills", std::string("3")}}) ? 1 : 0,
        ws->get_raw("kills").has_value() ? 1 : 0,
        ws->get_raw("nope").has_value() ? 1 : 0,
        static_cast<int>(ws->raw().count("kills")));

    auto cloned = ws->clone();
    cloned.set("kills", 99);
    std::printf("clone original=%d copy=%d\n", ws->get<int>("kills").value_or(-1),
        cloned.get<int>("kills").value_or(-1));

    auto root_composite = std::static_pointer_cast<gobot::CompositeNode>(built.root);
    std::printf("tree root=%s children=%zu has_move=%d has_combat=%d\n",
        built.root->name().c_str(), root_composite->children().size(),
        built.subtree_library->has_subtree(gobot::SubgoalType::MoveTo) ? 1 : 0,
        built.subtree_library->has_subtree(gobot::SubgoalType::Combat) ? 1 : 0);

    auto probe = std::make_shared<gobot::Action>("probe", Map{{"hp", 100}}, Map{},
        [](gobot::Context&) { return gobot::Status::Success; }, 2.5f);
    std::printf("action cost=%.1f precond=%zu effects=%zu\n", probe->cost(),
        probe->precondition().size(), probe->effects().size());

    // ---- LayerBridgeNode：战略中断必须重置子树并把本 tick 转为 Running ----
    int created_lb = 0;
    sl->register_subtree(gobot::SubgoalType::Flee, [&created_lb]() -> gobot::BTNodePtr {
        return make_running_subtree(created_lb, "flee");
    });
    auto ref_lb = std::make_shared<gobot::SubtreeReferenceNode>(sl, bus);
    auto bridge = std::make_shared<gobot::LayerBridgeNode>(ref_lb, bus);
    bb->clear_subgoals();
    bb->push_subgoal(std::make_shared<gobot::Subgoal>(gobot::SubgoalType::Flee, "lb.flee", Map{}, 5));
    g_events.clear();
    const gobot::Status l1 = bridge->tick(*built.context);
    const gobot::Status l2 = bridge->tick(*built.context);
    bus->publish(gobot::events::kStrategicInterrupt);
    bus->flush();
    const std::string interrupt_events = join_events();
    const gobot::Status l3 = bridge->tick(*built.context);
    const gobot::Status l4 = bridge->tick(*built.context);
    std::printf("layerbridge %s %s %s %s created=%d ev=%s\n", st(l1), st(l2), st(l3), st(l4),
        created_lb, interrupt_events.c_str());

    return 0;
}
