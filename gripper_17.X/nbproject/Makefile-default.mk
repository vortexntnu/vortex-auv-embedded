#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Include project Makefile
ifeq "${IGNORE_LOCAL}" "TRUE"
# do not include local makefile. User is passing all local related variables already
else
include Makefile
# Include makefile containing local settings
ifeq "$(wildcard nbproject/Makefile-local-default.mk)" "nbproject/Makefile-local-default.mk"
include nbproject/Makefile-local-default.mk
endif
endif

# Environment
MKDIR=mkdir -p
RM=rm -f 
MV=mv 
CP=cp 

# Macros
CND_CONF=default
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
IMAGE_TYPE=debug
OUTPUT_SUFFIX=elf
DEBUGGABLE_SUFFIX=elf
FINAL_IMAGE=${DISTDIR}/gripper_17.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}
else
IMAGE_TYPE=production
OUTPUT_SUFFIX=hex
DEBUGGABLE_SUFFIX=elf
FINAL_IMAGE=${DISTDIR}/gripper_17.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}
endif

ifeq ($(COMPARE_BUILD), true)
COMPARISON_BUILD=-mafrlcsj
else
COMPARISON_BUILD=
endif

# Object Directory
OBJECTDIR=build/${CND_CONF}/${IMAGE_TYPE}

# Distribution Directory
DISTDIR=dist/${CND_CONF}/${IMAGE_TYPE}

# Source Files Quoted if spaced
SOURCEFILES_QUOTED_IF_SPACED=src/can1.c src/clock.c src/i2c.c src/main.c src/sercom0_i2c.c src/sercom1_i2c.c src/system_init.c src/tcc.c src/tcc0.c src/usart.c src/rtc.c src/adc0.c src/dmac.c src/pm.c src/wdt.c

# Object Files Quoted if spaced
OBJECTFILES_QUOTED_IF_SPACED=${OBJECTDIR}/src/can1.o ${OBJECTDIR}/src/clock.o ${OBJECTDIR}/src/i2c.o ${OBJECTDIR}/src/main.o ${OBJECTDIR}/src/sercom0_i2c.o ${OBJECTDIR}/src/sercom1_i2c.o ${OBJECTDIR}/src/system_init.o ${OBJECTDIR}/src/tcc.o ${OBJECTDIR}/src/tcc0.o ${OBJECTDIR}/src/usart.o ${OBJECTDIR}/src/rtc.o ${OBJECTDIR}/src/adc0.o ${OBJECTDIR}/src/dmac.o ${OBJECTDIR}/src/pm.o ${OBJECTDIR}/src/wdt.o
POSSIBLE_DEPFILES=${OBJECTDIR}/src/can1.o.d ${OBJECTDIR}/src/clock.o.d ${OBJECTDIR}/src/i2c.o.d ${OBJECTDIR}/src/main.o.d ${OBJECTDIR}/src/sercom0_i2c.o.d ${OBJECTDIR}/src/sercom1_i2c.o.d ${OBJECTDIR}/src/system_init.o.d ${OBJECTDIR}/src/tcc.o.d ${OBJECTDIR}/src/tcc0.o.d ${OBJECTDIR}/src/usart.o.d ${OBJECTDIR}/src/rtc.o.d ${OBJECTDIR}/src/adc0.o.d ${OBJECTDIR}/src/dmac.o.d ${OBJECTDIR}/src/pm.o.d ${OBJECTDIR}/src/wdt.o.d

# Object Files
OBJECTFILES=${OBJECTDIR}/src/can1.o ${OBJECTDIR}/src/clock.o ${OBJECTDIR}/src/i2c.o ${OBJECTDIR}/src/main.o ${OBJECTDIR}/src/sercom0_i2c.o ${OBJECTDIR}/src/sercom1_i2c.o ${OBJECTDIR}/src/system_init.o ${OBJECTDIR}/src/tcc.o ${OBJECTDIR}/src/tcc0.o ${OBJECTDIR}/src/usart.o ${OBJECTDIR}/src/rtc.o ${OBJECTDIR}/src/adc0.o ${OBJECTDIR}/src/dmac.o ${OBJECTDIR}/src/pm.o ${OBJECTDIR}/src/wdt.o

# Source Files
SOURCEFILES=src/can1.c src/clock.c src/i2c.c src/main.c src/sercom0_i2c.c src/sercom1_i2c.c src/system_init.c src/tcc.c src/tcc0.c src/usart.c src/rtc.c src/adc0.c src/dmac.c src/pm.c src/wdt.c

# Pack Options 
PACK_COMMON_OPTIONS=-I "${CMSIS_DIR}/CMSIS/Core/Include"



CFLAGS=
ASFLAGS=
LDLIBSOPTIONS=

############# Tool locations ##########################################
# If you copy a project from one host to another, the path where the  #
# compiler is installed may be different.                             #
# If you open this project with MPLAB X in the new host, this         #
# makefile will be regenerated and the paths will be corrected.       #
#######################################################################
# fixDeps replaces a bunch of sed/cat/printf statements that slow down the build
FIXDEPS=fixDeps

.build-conf:  ${BUILD_SUBPROJECTS}
ifneq ($(INFORMATION_MESSAGE), )
	@echo $(INFORMATION_MESSAGE)
endif
	${MAKE}  -f nbproject/Makefile-default.mk ${DISTDIR}/gripper_17.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}

MP_PROCESSOR_OPTION=ATSAMC21E17A
MP_LINKER_FILE_OPTION=
# ------------------------------------------------------------------------------------
# Rules for buildStep: assemble
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
else
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: assembleWithPreprocess
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
else
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: compile
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
${OBJECTDIR}/src/can1.o: src/can1.c  .generated_files/flags/default/6c0a4b1a1f4171ae419820d3a131f20108ac7e1a .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/can1.o.d 
	@${RM} ${OBJECTDIR}/src/can1.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/can1.o.d" -o ${OBJECTDIR}/src/can1.o src/can1.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/clock.o: src/clock.c  .generated_files/flags/default/440ad4041e0e318383926de7d2f2a1352f92704 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/clock.o.d 
	@${RM} ${OBJECTDIR}/src/clock.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/clock.o.d" -o ${OBJECTDIR}/src/clock.o src/clock.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/i2c.o: src/i2c.c  .generated_files/flags/default/583f0ceec3cc49328f11fbadd30acfa4b31a8a75 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/i2c.o.d 
	@${RM} ${OBJECTDIR}/src/i2c.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/i2c.o.d" -o ${OBJECTDIR}/src/i2c.o src/i2c.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/main.o: src/main.c  .generated_files/flags/default/c178bd61849531104a1a621835108c2d3dd734de .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/main.o.d 
	@${RM} ${OBJECTDIR}/src/main.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/main.o.d" -o ${OBJECTDIR}/src/main.o src/main.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/sercom0_i2c.o: src/sercom0_i2c.c  .generated_files/flags/default/b73172c93cc5a57577be886cf0e227d4add1f8bb .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/sercom0_i2c.o.d 
	@${RM} ${OBJECTDIR}/src/sercom0_i2c.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/sercom0_i2c.o.d" -o ${OBJECTDIR}/src/sercom0_i2c.o src/sercom0_i2c.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/sercom1_i2c.o: src/sercom1_i2c.c  .generated_files/flags/default/7e7ff777342dcb430da27ba5056c7b30c937836f .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/sercom1_i2c.o.d 
	@${RM} ${OBJECTDIR}/src/sercom1_i2c.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/sercom1_i2c.o.d" -o ${OBJECTDIR}/src/sercom1_i2c.o src/sercom1_i2c.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/system_init.o: src/system_init.c  .generated_files/flags/default/d806c7c647755edc28386580f5b8edacf997ab67 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/system_init.o.d 
	@${RM} ${OBJECTDIR}/src/system_init.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/system_init.o.d" -o ${OBJECTDIR}/src/system_init.o src/system_init.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/tcc.o: src/tcc.c  .generated_files/flags/default/af26b660f4eaa2bfa5455b8bc9acb7a84cad1e5a .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/tcc.o.d 
	@${RM} ${OBJECTDIR}/src/tcc.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/tcc.o.d" -o ${OBJECTDIR}/src/tcc.o src/tcc.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/tcc0.o: src/tcc0.c  .generated_files/flags/default/fe096c1966350636082319c04a94795f4a6b6f00 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/tcc0.o.d 
	@${RM} ${OBJECTDIR}/src/tcc0.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/tcc0.o.d" -o ${OBJECTDIR}/src/tcc0.o src/tcc0.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/usart.o: src/usart.c  .generated_files/flags/default/4f0b7fc56f6711c9166fa9094709df5bf059e307 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/usart.o.d 
	@${RM} ${OBJECTDIR}/src/usart.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/usart.o.d" -o ${OBJECTDIR}/src/usart.o src/usart.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/rtc.o: src/rtc.c  .generated_files/flags/default/d46ef55e1b76628c33df48ae877b4f9d70929686 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/rtc.o.d 
	@${RM} ${OBJECTDIR}/src/rtc.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/rtc.o.d" -o ${OBJECTDIR}/src/rtc.o src/rtc.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/adc0.o: src/adc0.c  .generated_files/flags/default/a80832d1ab7e3582368d31953ae8c62605931292 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/adc0.o.d 
	@${RM} ${OBJECTDIR}/src/adc0.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/adc0.o.d" -o ${OBJECTDIR}/src/adc0.o src/adc0.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/dmac.o: src/dmac.c  .generated_files/flags/default/e41366999ebee2ec57f483cd88002f7a8f6c5423 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/dmac.o.d 
	@${RM} ${OBJECTDIR}/src/dmac.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/dmac.o.d" -o ${OBJECTDIR}/src/dmac.o src/dmac.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/pm.o: src/pm.c  .generated_files/flags/default/9f6a0db6faa35dd782e587725b98b2c3516695df .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/pm.o.d 
	@${RM} ${OBJECTDIR}/src/pm.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/pm.o.d" -o ${OBJECTDIR}/src/pm.o src/pm.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/wdt.o: src/wdt.c  .generated_files/flags/default/3e77d3c0195acce84f1d1a7572fb06ade5adb52f .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/wdt.o.d 
	@${RM} ${OBJECTDIR}/src/wdt.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/wdt.o.d" -o ${OBJECTDIR}/src/wdt.o src/wdt.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
else
${OBJECTDIR}/src/can1.o: src/can1.c  .generated_files/flags/default/e2a369faef2dd8ae20878667aefb39ebc84677f1 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/can1.o.d 
	@${RM} ${OBJECTDIR}/src/can1.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/can1.o.d" -o ${OBJECTDIR}/src/can1.o src/can1.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/clock.o: src/clock.c  .generated_files/flags/default/e45633ce751cf7e6c704750eeec20009e35ca645 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/clock.o.d 
	@${RM} ${OBJECTDIR}/src/clock.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/clock.o.d" -o ${OBJECTDIR}/src/clock.o src/clock.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/i2c.o: src/i2c.c  .generated_files/flags/default/eaac7b7171ec55e14cb374b18f4fcae8646212dd .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/i2c.o.d 
	@${RM} ${OBJECTDIR}/src/i2c.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/i2c.o.d" -o ${OBJECTDIR}/src/i2c.o src/i2c.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/main.o: src/main.c  .generated_files/flags/default/fa2db9d915514d433fd1561fa62e72e3209412ed .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/main.o.d 
	@${RM} ${OBJECTDIR}/src/main.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/main.o.d" -o ${OBJECTDIR}/src/main.o src/main.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/sercom0_i2c.o: src/sercom0_i2c.c  .generated_files/flags/default/44104eb9c476fd33aeee5e2424719e2c154bf27d .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/sercom0_i2c.o.d 
	@${RM} ${OBJECTDIR}/src/sercom0_i2c.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/sercom0_i2c.o.d" -o ${OBJECTDIR}/src/sercom0_i2c.o src/sercom0_i2c.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/sercom1_i2c.o: src/sercom1_i2c.c  .generated_files/flags/default/b378a389b33cfb6915efea8532990837f62b308c .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/sercom1_i2c.o.d 
	@${RM} ${OBJECTDIR}/src/sercom1_i2c.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/sercom1_i2c.o.d" -o ${OBJECTDIR}/src/sercom1_i2c.o src/sercom1_i2c.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/system_init.o: src/system_init.c  .generated_files/flags/default/3030efe6028c5c148b66b7be7c0e5fce21b6bc67 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/system_init.o.d 
	@${RM} ${OBJECTDIR}/src/system_init.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/system_init.o.d" -o ${OBJECTDIR}/src/system_init.o src/system_init.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/tcc.o: src/tcc.c  .generated_files/flags/default/fab42d1ad58f6a4b18d1b7ff87fa9d230e329cda .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/tcc.o.d 
	@${RM} ${OBJECTDIR}/src/tcc.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/tcc.o.d" -o ${OBJECTDIR}/src/tcc.o src/tcc.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/tcc0.o: src/tcc0.c  .generated_files/flags/default/3edde6d8e054cb5ca157bd2abb7cfc090c3a32dd .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/tcc0.o.d 
	@${RM} ${OBJECTDIR}/src/tcc0.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/tcc0.o.d" -o ${OBJECTDIR}/src/tcc0.o src/tcc0.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/usart.o: src/usart.c  .generated_files/flags/default/2eff13aa687abf86202c7f31374e94e186026324 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/usart.o.d 
	@${RM} ${OBJECTDIR}/src/usart.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/usart.o.d" -o ${OBJECTDIR}/src/usart.o src/usart.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/rtc.o: src/rtc.c  .generated_files/flags/default/f0e63c981dc01628d60b9ca89b67a4d4db0b7bbe .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/rtc.o.d 
	@${RM} ${OBJECTDIR}/src/rtc.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/rtc.o.d" -o ${OBJECTDIR}/src/rtc.o src/rtc.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/adc0.o: src/adc0.c  .generated_files/flags/default/44c0461f1015faa5b5fd84e74c5fd6b4131e044d .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/adc0.o.d 
	@${RM} ${OBJECTDIR}/src/adc0.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/adc0.o.d" -o ${OBJECTDIR}/src/adc0.o src/adc0.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/dmac.o: src/dmac.c  .generated_files/flags/default/19b07e48f54a5ed71d9f8cc91f1ad72311663ab3 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/dmac.o.d 
	@${RM} ${OBJECTDIR}/src/dmac.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/dmac.o.d" -o ${OBJECTDIR}/src/dmac.o src/dmac.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/pm.o: src/pm.c  .generated_files/flags/default/7736e18ba026be8b7876c7dda3223a46c718f5a6 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/pm.o.d 
	@${RM} ${OBJECTDIR}/src/pm.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/pm.o.d" -o ${OBJECTDIR}/src/pm.o src/pm.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/wdt.o: src/wdt.c  .generated_files/flags/default/d1972a92cc0f5e397ca0fbe58d83966fc6c06259 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/wdt.o.d 
	@${RM} ${OBJECTDIR}/src/wdt.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"include" -Werror -Wall -MP -MMD -MF "${OBJECTDIR}/src/wdt.o.d" -o ${OBJECTDIR}/src/wdt.o src/wdt.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: compileCPP
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
else
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: link
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
${DISTDIR}/gripper_17.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk    
	@${MKDIR} ${DISTDIR} 
	${MP_CC} $(MP_EXTRA_LD_PRE) -g   -mprocessor=$(MP_PROCESSOR_OPTION)  -o ${DISTDIR}/gripper_17.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX} ${OBJECTFILES_QUOTED_IF_SPACED}          -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -Wl,--defsym=__MPLAB_BUILD=1$(MP_EXTRA_LD_POST)$(MP_LINKER_FILE_OPTION),--defsym=__ICD2RAM=1,--defsym=__MPLAB_DEBUG=1,--defsym=__DEBUG=1,-D=__DEBUG_D,--defsym=_min_heap_size=0,--gc-sections,-Map="${DISTDIR}/${PROJECTNAME}.${IMAGE_TYPE}.map",--memorysummary,${DISTDIR}/memoryfile.xml -mdfp="${DFP_DIR}/samc21"
	
else
${DISTDIR}/gripper_17.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk   
	@${MKDIR} ${DISTDIR} 
	${MP_CC} $(MP_EXTRA_LD_PRE)  -mprocessor=$(MP_PROCESSOR_OPTION)  -o ${DISTDIR}/gripper_17.X.${IMAGE_TYPE}.${DEBUGGABLE_SUFFIX} ${OBJECTFILES_QUOTED_IF_SPACED}          -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -Wl,--defsym=__MPLAB_BUILD=1$(MP_EXTRA_LD_POST)$(MP_LINKER_FILE_OPTION),--defsym=_min_heap_size=0,--gc-sections,-Map="${DISTDIR}/${PROJECTNAME}.${IMAGE_TYPE}.map",--memorysummary,${DISTDIR}/memoryfile.xml -mdfp="${DFP_DIR}/samc21"
	${MP_CC_DIR}/xc32-bin2hex ${DISTDIR}/gripper_17.X.${IMAGE_TYPE}.${DEBUGGABLE_SUFFIX} 
endif


# Subprojects
.build-subprojects:


# Subprojects
.clean-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${OBJECTDIR}
	${RM} -r ${DISTDIR}

# Enable dependency checking
.dep.inc: .depcheck-impl

DEPFILES=$(wildcard ${POSSIBLE_DEPFILES})
ifneq (${DEPFILES},)
include ${DEPFILES}
endif
