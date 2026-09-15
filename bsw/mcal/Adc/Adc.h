/**
 * @file Adc.h
 * @brief ADC driver (simplified) — raw wheel-speed counts, no
 * interpretation.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 4.1.2, 7.4, 11.2. Reads
 * the last value received on its virtual hardware link (via VirtualHw) and
 * returns it exactly as received, with its age — REQ-MCAL-001: "no
 * scaling, filtering, or interpretation." Converting counts to km/h and
 * judging plausibility is IoHwAb's job (Phase 5), not this module's.
 *
 * Deviation: real AUTOSAR ADC groups can span multiple channels, so
 * `Adc_ValueGroupType *data_buffer` points at an array. Every group this
 * project configures has exactly one channel, so `data_buffer` points at a
 * single value.
 */
#ifndef ADC_H
#define ADC_H

#include "Gpt.h"
#include "Platform_Types.h"
#include "Std_Types.h"

typedef uint8 Adc_GroupType;
typedef uint16 Adc_ValueGroupType; /* one raw count, 0..4095 for a 12-bit channel */

typedef uint8 Adc_StatusType;
#define ADC_IDLE ((Adc_StatusType)0U) /* no conversion result has ever arrived */
#define ADC_STREAM_CONVERTED                                                                       \
    ((Adc_StatusType)1U) /* at least one result is available (check age for freshness) */

typedef struct {
    Adc_GroupType groupId;
    uint8 channelId; /* selects the wire key "ADC<channelId>"; this project uses only channel 0 */
    uint8 resolutionBits;
    uint8 virtualHwLinkId;
    uint16 plausibilityMinCounts; /* consumed by IoHwAb starting Phase 5; stored here, not enforced
                                     yet */
    uint16 plausibilityMaxCounts;
} Adc_GroupConfigType;

typedef struct {
    const Adc_GroupConfigType *groups;
    uint8 numGroups;
} Adc_ConfigType;

void Adc_Init(const Adc_ConfigType *config);
void Adc_DeInit(void);
Std_ReturnType Adc_ReadGroup(Adc_GroupType group, Adc_ValueGroupType *data_buffer);
Adc_StatusType Adc_GetGroupStatus(Adc_GroupType group);

/**
 * Project-local name for draining the virtual sensor line (spec section
 * 7.4). Deviation: on real hardware this value would arrive via a
 * conversion-complete interrupt or DMA, not by polling a socket.
 */
void Adc_MainFunction_Read(void);

/**
 * Extension beyond spec section 7.4's minimum API: milliseconds since
 * `group`'s last valid conversion result, needed to satisfy REQ-MCAL-001
 * ("...together with its age"). Returns UINT32_MAX if no result has ever
 * arrived.
 */
Gpt_ValueType Adc_GetGroupAge(Adc_GroupType group);

/* Observability for tests and the status log; never read by Adc's own
 * logic (section 14.2 IT-04: "Counters are test instrumentation, not an
 * alternate data path"). */
uint32 Adc_GetGroupReadCount(Adc_GroupType group);

#endif /* ADC_H */
