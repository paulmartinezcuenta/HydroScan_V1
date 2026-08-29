
#ifndef GPS_H
#define GPS_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t gps_init(void);

esp_err_t gps_update(void);

bool gps_has_fix(void);

#endif