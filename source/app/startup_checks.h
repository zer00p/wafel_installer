#pragma once

#include "common.h"

bool performStartupChecks();
bool performAromaCheck();
bool performStroopwafelCheck(bool& isInstalled);
bool performIsfshaxCheck(bool usingUSB, bool wantsPartitionedStorage);
bool performPostSetupChecks(bool usingUSB, bool wantsPartitionedStorage);
