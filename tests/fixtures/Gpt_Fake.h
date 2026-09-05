/**
 * @file Gpt_Fake.h
 * @brief Test double for Gpt (bsw/mcal/Gpt), used only by unit tests.
 *
 * Implements the exact same public Gpt.h API (Gpt_Init, Gpt_DeInit,
 * Gpt_GetTimeElapsed) so `Os`/`Dem` code under test needs no changes, but
 * backs it with a manually-advanced counter instead of reading the host
 * clock.
 * This lets scheduler/timeout tests be fully deterministic regardless of
 * host speed (AUTOSAR_CLASSIC_PROJECT_SPEC_v1.1.md, section 16, Phase 1
 * deliverable: "Unit tests for services and scheduler with a fake Gpt").
 *
 * Per spec section 12.1, this file lives under tests/ and is never linked
 * into a production ECU target — test executables link this INSTEAD OF
 * bsw_mcal's real Gpt.c, never alongside it.
 */
#ifndef GPT_FAKE_H
#define GPT_FAKE_H

#include "Gpt.h" /* the API this fake substitutes for testing */

/** Resets the fake clock to 0ms. */
void GptFake_Reset(void);

/** Sets the fake clock to an absolute value in milliseconds. */
void GptFake_SetTimeMs(Gpt_ValueType ms);

/** Advances the fake clock by a relative amount in milliseconds. */
void GptFake_AdvanceMs(Gpt_ValueType deltaMs);

#endif /* GPT_FAKE_H */
