#include "lsm_error.h"

#include "lsm_types.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

const char *lsm_status_string(LsmStatus status) {
    switch (status) {
    case LSM_OK:
        return "ok";
    case LSM_ERR_IO:
        return "io error";
    case LSM_ERR_CORRUPTION:
        return "corruption detected";
    case LSM_ERR_NOT_FOUND:
        return "key not found";
    case LSM_ERR_FULL:
        return "storage full";
    case LSM_ERR_INVALID_ARG:
        return "invalid argument";
    case LSM_ERR_MEMORY:
        return "out of memory";
    case LSM_ERR_BUSY:
        return "resource busy";
    default:
        return "unknown error";
    }
}

bool lsm_status_is_ok(LsmStatus status) {
    return status == LSM_OK;
}

bool lsm_status_is_error(LsmStatus status) {
    return status != LSM_OK;
}

LsmStatus lsm_status_from_errno(int err) {
    switch (err) {
    case 0:
        return LSM_OK;
    case ENOMEM:
        return LSM_ERR_MEMORY;
    case EINVAL:
        return LSM_ERR_INVALID_ARG;
    case ENOENT:
        return LSM_ERR_NOT_FOUND;
    case ENOSPC:
        return LSM_ERR_FULL;
    case EBUSY:
        return LSM_ERR_BUSY;
    case EIO:
        return LSM_ERR_IO;
    default:
        return LSM_ERR_IO;
    }
}

void lsm_status_print(LsmStatus status, const char *context) {
    if (!context)
        context = "lsm";
    fprintf(stderr, "[%s] %s (code=%d)\n", context, lsm_status_string(status), (int)status);
}
