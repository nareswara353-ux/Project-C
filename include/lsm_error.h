#ifndef LSM_ERROR_H
#define LSM_ERROR_H

#include "lsm_types.h"

const char *lsm_status_string(LsmStatus status);
bool lsm_status_is_ok(LsmStatus status);
bool lsm_status_is_error(LsmStatus status);
LsmStatus lsm_status_from_errno(int err);
void lsm_status_print(LsmStatus status, const char *context);

#endif
