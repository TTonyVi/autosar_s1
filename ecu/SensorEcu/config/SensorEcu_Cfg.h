/**
 * @file SensorEcu_Cfg.h
 * @brief Link-time configuration of the SensorEcu (section 11.1).
 *
 * Everything that makes this ECU different from the ClusterEcu lives here:
 * the module implementations themselves are shared (`REQ-ARCH-005`).
 */
#ifndef SENSORECU_CFG_H
#define SENSORECU_CFG_H

#include "EcuM.h"

/**
 * Assembles the ECU's configuration, folding in the post-build-like
 * command-line values. Returns NULL if `postBuild` is unusable.
 */
const EcuM_ConfigType *SensorEcu_BuildConfig(const EcuM_PostBuildType *postBuild);

#endif /* SENSORECU_CFG_H */
