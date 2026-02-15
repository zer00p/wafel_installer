#pragma once

#include "common.h"

bool performStartupChecks();
void performAromaCheck();
void performStroopwafelCheck(bool wantsPartitionedStorage);
void performIsfshaxCheck(bool usingUSB, bool wantsPartitionedStorage);
void performPostSetupChecks(bool usingUSB, bool wantsPartitionedStorage);
