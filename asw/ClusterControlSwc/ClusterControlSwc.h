/**
 * @file ClusterControlSwc.h
 * @brief Runnable entry points of the ClusterEcu application component.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 4.2.1 and 4.4.
 */
#ifndef CLUSTERCONTROLSWC_H
#define CLUSTERCONTROLSWC_H

#include "Rte_Type.h"

/** Activated by `Rte_Start`; state starts at COMM_FAULT until valid remote
 *  data is received (section 4.2.1). */
void ClusterControlSwc_Init(void);

/** Timing event, 10 ms: read vehicle state, run the state machine, write
 *  cluster status. */
void ClusterControlSwc_10ms(void);

/** Timing event, 100 ms: refresh the instrument display through the RTE
 *  display port, then produce a status log line. */
void ClusterControlSwc_100ms(void);

/* --- current state, read by the status log and by tests --- */
ClusterStateType ClusterControlSwc_GetState(void);
DisplayedSpeedRawType ClusterControlSwc_GetDisplayedSpeedRaw(void);
ClusterStatusFlagsType ClusterControlSwc_GetStatusFlags(void);
uint8 ClusterControlSwc_GetConsecutiveValidCount(void);

#endif /* CLUSTERCONTROLSWC_H */
