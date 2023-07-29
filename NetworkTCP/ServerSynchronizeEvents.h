#pragma once
#include "Server.h"
#include "StructTCP.h"

//位置同步
void SGamePlayerSynchronize(bufferevent* be, void* Data);
//子弹同步
void SArmsSynchronize(bufferevent* be, void* Data);
//玩家损伤成度同步
void SGamePlayerBroken(bufferevent* be, void* Data);
//返回玩家的初始信息
void SPlayerInformation(bufferevent* be, void* Data);
//地图初始化同步
void SInitLabyrinth(bufferevent* be, void* Data);
//地图破坏同步
void SLabyrinthPixel(bufferevent* be, void* Data);
