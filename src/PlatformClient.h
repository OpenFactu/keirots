#pragma once
#include "DeviceConfig.h"
bool startPlatform(const DeviceConfig& config);
bool submitCard(const uint8_t* uid, uint8_t size);
const char* platformState();
