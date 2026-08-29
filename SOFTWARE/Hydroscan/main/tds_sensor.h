
#ifndef TDS_H
#define TDS_H

#ifdef __cplusplus
extern "C" {
#endif

void tds_init(void);

void tds_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif