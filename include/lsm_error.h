#ifndef LSM_ERROR_H
#define LSM_ERROR_H

#include "lsm_tree.h"

const char* lsm_status_string(LsmStatus status);
void lsm_status_assert_ok(LsmStatus status);
LsmStatus lsm_status_from_errno(int err);

#endif
