/**
 * @file ClusterEcu_Cfg.h
 * @brief Link-time configuration of the ClusterEcu (section 11.1).
 *
 * Everything that makes this ECU different from the SensorEcu lives here:
 * the module implementations themselves are shared (`REQ-ARCH-005`).
 */
#ifndef CLUSTERECU_CFG_H
#define CLUSTERECU_CFG_H

#include "EcuM.h"

/**
 * Assembles the ECU's configuration, folding in the post-build-like
 * command-line values. Returns NULL if `postBuild` is unusable.
 */
const EcuM_ConfigType *ClusterEcu_BuildConfig(const EcuM_PostBuildType *postBuild);

#endif /* CLUSTERECU_CFG_H */
