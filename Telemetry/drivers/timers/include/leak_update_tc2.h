#ifdef LEAK_UPDATE__TC2_H
#define LEAK_UPDATE_TC2_H

#ifdef __cplusplus
extern "C" {
#endif

volatile bool leakdet_tick;
void timing_tc2_init_5hz(void);

#ifdef __cplusplus
}
#endif

#endif