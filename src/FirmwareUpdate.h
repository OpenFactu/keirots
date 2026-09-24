#pragma once
#include "DeviceConfig.h"
// Downloads only from the configured platform; never forwards credentials to GitHub.
bool installFirmware(const DeviceConfig& config, const cJSON* update);
