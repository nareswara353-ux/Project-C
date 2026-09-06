#ifndef LSM_CONFIG_H
#define LSM_CONFIG_H

#include "lsm_tree.h"

LsmOptions lsm_config_from_env(void);
LsmOptions lsm_config_from_file(const char* path);
LsmStatus lsm_config_validate(const LsmOptions* opts);

#endif
