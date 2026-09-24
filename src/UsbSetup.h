#pragma once
#include <stdint.h>
void pollUsbSetup(bool configured);
bool captureCardForUsb(const uint8_t* uid, uint8_t size);
