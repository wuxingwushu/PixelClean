#pragma once
#include "Client.h"
#include "StructTCP.h"

void CGamePlayerSynchronize(bufferevent* be, void* Data);

void CArmsSynchronize(bufferevent* be, void* Data);

void CGamePlayerBroken(bufferevent* be, void* Data);

void CPlayerInformation(bufferevent* be, void* Data);
