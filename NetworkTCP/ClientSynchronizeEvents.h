#pragma once
#include "Client.h"


//位置同步
void CGamePlayerSynchronize(bufferevent* be, void* Data);
//子弹同步
void CArmsSynchronize(bufferevent* be, void* Data);
//玩家损伤成度同步
void CGamePlayerBroken(bufferevent* be, void* Data);
//返回玩家的初始信息
void CPlayerInformation(bufferevent* be, void* Data);
//地图初始化同步
void CInitLabyrinth(bufferevent* be, void* Data);
//地图破坏同步
void CLabyrinthPixel(bufferevent* be, void* Data);
//NPC同步
void CNPCSSynchronize(bufferevent* be, void* Data);
//Str发送
void CStrSend(bufferevent* be, void* Data);
//Str接收
void CStrReceive(bufferevent* be, void* Data);
