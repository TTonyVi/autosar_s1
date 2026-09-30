# ARXML boundary

This project does **not** generate or consume schema-valid AUTOSAR XML
(ARXML). Its `*_Cfg.c`/`*_Cfg.h` static configuration is a hand-written
stand-in for what a real AUTOSAR toolchain would generate from validated
XML work products. This document explains what those work products are in
a real project and why this repository doesn't attempt to produce them, so
a reader coming from a real AUTOSAR toolchain knows exactly what's missing
and why.

## What a real project exchanges

A commercial AUTOSAR Classic project moves through several XML work
products, each owned by a different role and validated against the
official AUTOSAR schema:

| Work product | Owner | Purpose |
|---|---|---|
| **System Configuration Description** | System architect | Vehicle-wide network/communication matrix: which ECUs exist, which signals/PDUs/frames exist, and how they map onto buses. |
| **ECU Extract of System Configuration** | System architect (per ECU) | The subset of the System Configuration Description relevant to one ECU — its Rx/Tx PDUs, signals, and timing, extracted for that ECU's configuration tool. |
| **ECU Configuration Description** | ECU/BSW integrator | Full, ECU-specific configuration of every BSW module (Com, PduR, CanIf, Can, MCAL drivers, OS tasks, etc.), from which the BSW/RTE generator produces `*_Cfg.c`/`*_Cfg.h` and generated RTE code. |
| **Software Component Description** | SW-C developer | Ports, interfaces, data elements, operations, and internal behavior (runnables, events) for one SW-C — the input to RTE generation on the application side. |
| **BSW Module Description** | BSW module vendor | The configuration parameters, ports, and generated-code contract a given BSW module exposes to the configuration tool. |

In a real toolchain, these are produced and validated by dedicated tools
(e.g. a system description editor, an ECU configuration editor, an
RTE/BSW generator), and the generated C code is a build artifact, not
something an engineer hand-edits.

## What this project does instead

This project has no licensed generator, so v1 uses:

- Hand-written C tables (`ecu/<Ecu>/config/*_Cfg.c` and `.h`) playing the
  role of what an ECU Configuration Description would generate — see spec
  section 11.
- Hand-written, generated-*style* RTE code under `rte/generated/<Ecu>/`
  playing the role of what a Software Component Description plus RTE
  generator would produce — see spec section 5.
- No System Configuration Description, ECU Extract, or BSW Module
  Description exists at all; the two-ECU communication matrix (spec
  section 6.2) is documented directly in the specification and mirrored by
  hand into `Com`/`PduR`/`CanIf` configuration tables.

None of this is claimed to be schema-valid ARXML, and no tool in this
repository parses or emits ARXML.

## Future extension

Per spec section 18, item 6, a possible (not started, not scheduled)
future extension is adding small illustrative XML examples explicitly
labeled `illustrative-not-schema-validated` — never presented as real,
validated ARXML. As of Phase 0, no such files exist.
