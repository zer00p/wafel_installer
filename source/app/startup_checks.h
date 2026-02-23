#pragma once

#include "common.h"

bool performStartupChecks();
void performAromaCheck();
void performStroopwafelCheck();
bool performIsfshaxCheck(bool usingUSB, bool wantsPartitionedStorage);
bool performPostSetupChecks(bool usingUSB, bool sdUsb);
