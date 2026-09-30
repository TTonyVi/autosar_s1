/**
 * @file EcuM.h
 * @brief ECU State Manager: initialization and shutdown in a fixed order.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 9.
 *
 * One generic implementation serves both ECUs (`REQ-ARCH-005`): everything
 * that differs between `SensorEcu` and `ClusterEcu` is a pointer in
 * `EcuM_ConfigType`, and a NULL pointer means "this ECU does not have that
 * module". `EcuM` is the only module allowed to know the whole init order.
 */
#ifndef ECUM_H
#define ECUM_H

#include "Adc.h"
#include "BswM.h"
#include "Can.h"
#include "CanIf.h"
#include "Com.h"
#include "Dio.h"
#include "Display.h"
#include "Gpt.h"
#include "IoHwAb.h"
#include "Logger.h"
#include "Mcu.h"
#include "Os.h"
#include "PduR.h"
#include "Platform_Types.h"
#include "Port.h"
#include "Std_Types.h"
#include "SysTimer.h"
#include "VirtualHw.h"

/** Section 9.1. */
typedef uint8 EcuM_StateType;

#define ECUM_STATE_OFF ((EcuM_StateType)0U)
#define ECUM_STATE_STARTUP ((EcuM_StateType)1U)
#define ECUM_STATE_RUN ((EcuM_StateType)2U)
#define ECUM_STATE_POST_RUN ((EcuM_StateType)3U)
#define ECUM_STATE_SHUTDOWN ((EcuM_StateType)4U)

/**
 * The post-build-like values of section 11.1: read-only command-line
 * configuration applied at startup. Section 15.4 requires a link path to be
 * passed into MCAL configuration and never read by any module above MCAL,
 * which is why it stops here and inside the ECU's own configuration
 * translation unit.
 */
typedef struct {
    const char *canInterface; /* --can-if, default "vcan0" */
    const char *hwLinkPath;   /* --sensor-link or --display-link */
    boolean hwLinkEnabled;    /* FALSE when --display-link "" disables the panel */
    Logger_LevelType logLevel;
    uint32 durationSeconds; /* --duration; 0 means run until a signal */
} EcuM_PostBuildType;

typedef struct {
    const char *ecuName;
    Logger_LevelType logLevel;

    const Mcu_ConfigType *mcuConfig;
    const Port_ConfigType *portConfig;
    const SysTimer_ConfigType *sysTimerConfig;
    const Gpt_ConfigType *gptConfig;
    const VirtualHw_ConfigType *virtualHwConfig;

    /** SensorEcu only; NULL on ClusterEcu. */
    const Adc_ConfigType *adcConfig;
    const Dio_ConfigType *dioConfig;
    /** ClusterEcu only; NULL on SensorEcu, and NULL when `--display-link ""`
     *  disables the panel (section 15.4). */
    const Display_ConfigType *displayConfig;

    const Can_ConfigType *canConfig;
    const IoHwAb_ConfigType *ioHwAbConfig;
    const CanIf_ConfigType *canIfConfig;
    const PduR_ConfigType *pduRConfig;
    const Com_ConfigType *comConfig;
    const Os_ConfigType *osConfig;
    const BswM_ConfigType *bswMConfig;

    /** Controller taken to CAN_CS_STARTED at init step 17. */
    uint8 canControllerId;

    /** Optional duration limit in seconds (0 = run indefinitely). */
    uint32 durationSeconds;
} EcuM_ConfigType;

/**
 * Runs Phase 1 (Pre-OS) initialization up to StartOS.
 */
Std_ReturnType EcuM_Init(const EcuM_ConfigType *config);

/**
 * Runs Phase 2 transition to RUN (called from OS context / StartupHook).
 */
void EcuM_StartupTwo(void);

/** Runs the section 9.3 order; safe to call more than once (`REQ-ECUM-004`). */
void EcuM_Shutdown(void);

/** One scheduling pass; the ECU's main loop calls this repeatedly. */
void EcuM_MainFunction(void);

/** Asks for an orderly stop. Safe to call from a signal handler context in
 *  the sense that it only sets state — no I/O, no allocation
 *  (`REQ-ECUM-003`). */
void EcuM_RequestShutdown(void);

EcuM_StateType EcuM_GetState(void);
boolean EcuM_ShutdownRequested(void);

/** Returns TRUE if the configured durationSeconds has elapsed. */
boolean EcuM_IsDurationExpired(void);

#endif /* ECUM_H */
