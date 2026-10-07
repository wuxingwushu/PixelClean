#pragma once

// gobot 统一入口：按依赖顺序包含全部头文件
// 依赖链 core → infra → bt → layers → runtime

#include "gobot/core.hpp"
#include "gobot/infra.hpp"
#include "gobot/bt.hpp"
#include "gobot/layers.hpp"
#include "gobot/runtime.hpp"
