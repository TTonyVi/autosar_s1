/**
 * @file IoHwAb.h
 * @brief ECU Abstraction layer: raw hardware quantities to physical values.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 3.1, 4.1.2 and 7.4.
 *
 * This is the layer that turns "counts and levels" into "km/h and
 * ignition, plus a validity". It is the only place in the project where
 * the ADC scaling lives, and it is deliberately unable to decide anything
 * about application state (`REQ-IOHWAB-004`) — it reports health through
 * DEM and hands the SW-C a value plus a validity, nothing more.
 *
 * Callers: the RTE only. A SW-C calling into this header is a
 * `REQ-ARCH-008` violation and is caught by a static check.
 */
#ifndef IOHWAB_H
#define IOHWAB_H

#include "IoHwAb_Cfg.h"
#include "Platform_Types.h"
#include "Rte_Type.h"
#include "Std_Types.h"

/** Mirrors `Rte_InputValidityType` one to one (section 5.2). */
typedef uint8 IoHwAb_ValidityType;

#define IOHWAB_VALID ((IoHwAb_ValidityType)0U)
#define IOHWAB_INVALID ((IoHwAb_ValidityType)1U)

/** Which MCAL driver an abstracted signal is wired to. */
typedef uint8 IoHwAb_SourceDriverType;

#define IOHWAB_SOURCE_ADC ((IoHwAb_SourceDriverType)0U)
#define IOHWAB_SOURCE_DIO ((IoHwAb_SourceDriverType)1U)

typedef uint8 IoHwAb_SignalIdType;

#define IOHWAB_SIGNAL_VEHICLE_SPEED ((IoHwAb_SignalIdType)0U)
#define IOHWAB_SIGNAL_IGNITION ((IoHwAb_SignalIdType)1U)
#define IOHWAB_NUM_SIGNALS (2U)

/** Section 11.2: source driver and channel, scaling numerator and
 *  denominator, offset, output minimum and maximum, input timeout. */
typedef struct {
    IoHwAb_SignalIdType signalId; /* must equal its index in the table */
    IoHwAb_SourceDriverType sourceDriver;
    uint8 sourceChannel; /* Adc group id or Dio channel id */
    uint32 scalingNumerator;
    uint32 scalingDenominator; /* must be non-zero */
    sint32 offset;
    uint32 outputMin;
    uint32 outputMax; /* must be >= outputMin */
    uint32 plausibilityMinCounts;
    uint32 plausibilityMaxCounts; /* must be >= plausibilityMinCounts */
    uint32 inputTimeoutMs;        /* must be non-zero */
} IoHwAb_SignalConfigType;

typedef struct {
    const IoHwAb_SignalConfigType *signals;
    uint8 numSignals;
    /** TRUE on the ECU that owns a display panel (ClusterEcu). */
    boolean hasDisplay;
    uint32 displayTimeoutMs;
} IoHwAb_ConfigType;

/** The display value this layer assembles for the panel (section 15.5.2). */
typedef struct {
    ClusterStateType state;
    DisplayedSpeedRawType displayedSpeedRaw;
    ClusterStatusFlagsType flags;
} IoHwAb_ClusterDisplayType;

/** Static configuration instances (IoHwAb_Cfg.c), one per ECU role. */
extern const IoHwAb_ConfigType IoHwAb_ConfigSensor;
extern const IoHwAb_ConfigType IoHwAb_ConfigCluster;

void IoHwAb_Init(const IoHwAb_ConfigType *config);
void IoHwAb_DeInit(void);

/**
 * Latest vehicle speed in 0.01 km/h per bit plus its validity.
 * REQ-IOHWAB-001: never returns a stale value marked valid.
 * Returns E_NOT_OK (and reports to Det) only on a caller contract error —
 * a null pointer or a call before init. An invalid *input* is not a
 * caller error: it returns E_OK with validity IOHWAB_INVALID.
 */
Std_ReturnType IoHwAb_GetVehicleSpeedRaw(VehicleSpeedRawType *speed_raw,
                                         IoHwAb_ValidityType *validity);

/** Latest ignition state plus its validity; same contract as above. */
Std_ReturnType IoHwAb_GetIgnitionState(IgnitionOnType *ignition_on, IoHwAb_ValidityType *validity);

/**
 * Assembles the display frame and hands it to the Display driver.
 * A write failure is counted and reported through DEM but is never fatal
 * and never changes application behavior (REQ-VHW-005).
 */
Std_ReturnType IoHwAb_SetClusterDisplay(const IoHwAb_ClusterDisplayType *value);

/**
 * Samples the input drivers once and refreshes the cached value/validity
 * pair for each abstracted signal, reporting health through DEM. Runs in
 * Task_Io_5ms after Adc_MainFunction_Read/Dio_MainFunction_Read
 * (section 8.2).
 */
void IoHwAb_MainFunction(void);

/* --- observability for tests and the status log; no control flow --- */
uint32 IoHwAb_GetInvalidSampleCount(void);
uint32 IoHwAb_GetValidSampleCount(void);
uint32 IoHwAb_GetDisplayWriteFailureCount(void);

#endif /* IOHWAB_H */
