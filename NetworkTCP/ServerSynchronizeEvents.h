#pragma once
#include "Server.h"
#include "StructTCP.h"

void SGamePlayerSynchronize(bufferevent* be, void* Data);

void SArmsSynchronize(bufferevent* be, void* Data);

void SGamePlayerBroken(bufferevent* be, void* Data);

void SPlayerInformation(bufferevent* be, void* Data);
