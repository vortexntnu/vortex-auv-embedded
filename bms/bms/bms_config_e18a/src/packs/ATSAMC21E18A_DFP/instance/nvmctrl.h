/*
 * Instance header file for ATSAMC21E18AU
 *
 * Copyright (c) 2025 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/* file generated from device description file (ATDF) version 2025-06-24T07:32:57Z */
#ifndef _SAMC21_NVMCTRL_INSTANCE_
#define _SAMC21_NVMCTRL_INSTANCE_


/* ========== Instance Parameter definitions for NVMCTRL peripheral ========== */
#define NVMCTRL_FACTORY_WORD_IMPLEMENTED_MASK    (0XC0000007FFFFFFFF)
#define NVMCTRL_FLASH_SIZE                       (262144)
#define NVMCTRL_GCLK_ID                          (39)       /* Index of Generic Clock for test */
#define NVMCTRL_INSTANCE_ID                      (34)
#define NVMCTRL_PAGES                            (4096)
#define NVMCTRL_PAGES_PR_REGION                  (256)
#define NVMCTRL_PAGE_SIZE                        (64)
#define NVMCTRL_PMSB                             (3)
#define NVMCTRL_PSM_0_FRMFW_FWS_1_MAX_FREQ       (12000000)
#define NVMCTRL_PSM_0_FRMHS_FWS_0_MAX_FREQ       (25000000)
#define NVMCTRL_PSM_0_FRMHS_FWS_1_MAX_FREQ       (50000000)
#define NVMCTRL_PSM_0_FRMLP_FWS_0_MAX_FREQ       (18000000)
#define NVMCTRL_PSM_0_FRMLP_FWS_1_MAX_FREQ       (36000000)
#define NVMCTRL_PSM_1_FRMFW_FWS_1_MAX_FREQ       (12000000)
#define NVMCTRL_PSM_1_FRMLP_FWS_0_MAX_FREQ       (8000000)
#define NVMCTRL_PSM_1_FRMLP_FWS_1_MAX_FREQ       (12000000)
#define NVMCTRL_PSZ_BITS                         (6)
#define NVMCTRL_ROW_SIZE                         (256)
#define NVMCTRL_RWWEE_PAGES                      (128)
#define NVMCTRL_RWWEE_ROW_SIZE                   (256)
#define NVMCTRL_RWW_EEPROM_ADDR                  (0x00400000) /* Start address of the WWR EEPROM area */
#define NVMCTRL_RWW_EEPROM_SIZE                  (8192)     /* Byte size of the WWR EEPROM area */
#define NVMCTRL_USER_PAGE_OFFSET                 (0x00800000)
#define NVMCTRL_USER_WORD_IMPLEMENTED_MASK       (0XC01FFFFFFFFFFFFF)

#endif /* _SAMC21_NVMCTRL_INSTANCE_ */
