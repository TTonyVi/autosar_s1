/**
 * @file IoHwAb_Cfg.h
 * @brief Static configuration constants for the ECU Abstraction layer.
 *
 * AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, sections 4.1.2 and 11.2.
 *
 * The scaling constants live HERE and nowhere else: "Scaling constants
 * SHALL live in `IoHwAb_Cfg`, never in a SW-C or in MCAL" (section 4.1.2).
 */
#ifndef IOHWAB_CFG_H
#define IOHWAB_CFG_H

/* --- conversion contract, section 4.1.2 ---
 * Sensor full scale is 250.00 km/h at 4095 counts, 0.00 km/h at 0 counts,
 * linear: speed_raw = (adc_counts * 25000) / 4095, integer division with
 * 32-bit intermediates. */
#define IOHWAB_SPEED_SCALING_NUMERATOR (25000U)
#define IOHWAB_SPEED_SCALING_DENOMINATOR (4095U)
#define IOHWAB_SPEED_OFFSET (0)
#define IOHWAB_SPEED_OUTPUT_MIN (0U)
#define IOHWAB_SPEED_OUTPUT_MAX (25000U)

/** Plausibility window in ADC counts; counts outside it are invalid and
 *  reported through DEM (section 4.1.2, REQ-IOHWAB-004). The full 12-bit
 *  span is plausible for this sensor, so the window is the span itself —
 *  what it rejects is a driver reporting counts a 12-bit channel cannot
 *  physically produce. */
#define IOHWAB_SPEED_PLAUSIBILITY_MIN_COUNTS (0U)
#define IOHWAB_SPEED_PLAUSIBILITY_MAX_COUNTS (4095U)

/** REQ-IOHWAB-002: no fresh hardware value within this window means the
 *  input is INVALID. */
#define IOHWAB_INPUT_TIMEOUT_MS (500U)

/** Section 10.2: the display link is considered lost after writes have
 *  been failing continuously for this long. */
#define IOHWAB_DISPLAY_TIMEOUT_MS (500U)

/* --- driver wiring: which MCAL channel each abstracted signal reads --- */
#define IOHWAB_ADC_GROUP_WHEEL_SPEED (0U)
#define IOHWAB_DIO_CHANNEL_IGNITION (0U)

/** Longest display frame this layer assembles (section 15.5.2 caps the
 *  link datagram at 96 bytes). */
#define IOHWAB_DISPLAY_FRAME_MAX (96U)

#endif /* IOHWAB_CFG_H */
