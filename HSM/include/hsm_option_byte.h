#ifndef HSM_OPTION_BYTE_H
#define HSM_OPTION_BYTE_H

#include "hsm_type.h"

RetStatus setRDPLevelOne(void);
#if defined(ENABLE_RDP_LEVEL_TWO)
RetStatus setRDPLevelTwo(void);
#endif
uint32_t enableWRP(uint32_t wrp_sectors);
uint32_t disableWRP(uint32_t wrp_sectors);

#endif // HSM_OPTION_BYTE_H
