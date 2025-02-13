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
FINAL_IMAGE=${DISTDIR}/Desperate_attempt_PWM.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}
else
IMAGE_TYPE=production
OUTPUT_SUFFIX=hex
DEBUGGABLE_SUFFIX=elf
FINAL_IMAGE=${DISTDIR}/Desperate_attempt_PWM.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}
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
SOURCEFILES_QUOTED_IF_SPACED=peripheral/clock/plib_clock.c peripheral/cmcc/plib_cmcc.c peripheral/evsys/plib_evsys.c peripheral/nvic/plib_nvic.c peripheral/nvmctrl/plib_nvmctrl.c peripheral/port/plib_port.c initialization.c main.c

# Object Files Quoted if spaced
OBJECTFILES_QUOTED_IF_SPACED=${OBJECTDIR}/peripheral/clock/plib_clock.o ${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o ${OBJECTDIR}/peripheral/evsys/plib_evsys.o ${OBJECTDIR}/peripheral/nvic/plib_nvic.o ${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o ${OBJECTDIR}/peripheral/port/plib_port.o ${OBJECTDIR}/initialization.o ${OBJECTDIR}/main.o
POSSIBLE_DEPFILES=${OBJECTDIR}/peripheral/clock/plib_clock.o.d ${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o.d ${OBJECTDIR}/peripheral/evsys/plib_evsys.o.d ${OBJECTDIR}/peripheral/nvic/plib_nvic.o.d ${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o.d ${OBJECTDIR}/peripheral/port/plib_port.o.d ${OBJECTDIR}/initialization.o.d ${OBJECTDIR}/main.o.d

# Object Files
OBJECTFILES=${OBJECTDIR}/peripheral/clock/plib_clock.o ${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o ${OBJECTDIR}/peripheral/evsys/plib_evsys.o ${OBJECTDIR}/peripheral/nvic/plib_nvic.o ${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o ${OBJECTDIR}/peripheral/port/plib_port.o ${OBJECTDIR}/initialization.o ${OBJECTDIR}/main.o

# Source Files
SOURCEFILES=peripheral/clock/plib_clock.c peripheral/cmcc/plib_cmcc.c peripheral/evsys/plib_evsys.c peripheral/nvic/plib_nvic.c peripheral/nvmctrl/plib_nvmctrl.c peripheral/port/plib_port.c initialization.c main.c

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
	${MAKE}  -f nbproject/Makefile-default.mk ${DISTDIR}/Desperate_attempt_PWM.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}

MP_PROCESSOR_OPTION=ATSAME51J20A
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
${OBJECTDIR}/peripheral/clock/plib_clock.o: peripheral/clock/plib_clock.c  .generated_files/flags/default/2c246e92fa592cddf258b6be3d456b221e42eda .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/clock" 
	@${RM} ${OBJECTDIR}/peripheral/clock/plib_clock.o.d 
	@${RM} ${OBJECTDIR}/peripheral/clock/plib_clock.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_SIMULATOR=1  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/clock/plib_clock.o.d" -o ${OBJECTDIR}/peripheral/clock/plib_clock.o peripheral/clock/plib_clock.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o: peripheral/cmcc/plib_cmcc.c  .generated_files/flags/default/7f21b457e2284a1f1b65c22040a09db0a310eb29 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/cmcc" 
	@${RM} ${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o.d 
	@${RM} ${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_SIMULATOR=1  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o.d" -o ${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o peripheral/cmcc/plib_cmcc.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/evsys/plib_evsys.o: peripheral/evsys/plib_evsys.c  .generated_files/flags/default/872d9e43be0cc98fccc3f2de19aa4dc0880ee971 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/evsys" 
	@${RM} ${OBJECTDIR}/peripheral/evsys/plib_evsys.o.d 
	@${RM} ${OBJECTDIR}/peripheral/evsys/plib_evsys.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_SIMULATOR=1  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/evsys/plib_evsys.o.d" -o ${OBJECTDIR}/peripheral/evsys/plib_evsys.o peripheral/evsys/plib_evsys.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/nvic/plib_nvic.o: peripheral/nvic/plib_nvic.c  .generated_files/flags/default/e8848306e3d2acdfb43d5b9e3ef22633d3c39d95 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/nvic" 
	@${RM} ${OBJECTDIR}/peripheral/nvic/plib_nvic.o.d 
	@${RM} ${OBJECTDIR}/peripheral/nvic/plib_nvic.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_SIMULATOR=1  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/nvic/plib_nvic.o.d" -o ${OBJECTDIR}/peripheral/nvic/plib_nvic.o peripheral/nvic/plib_nvic.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o: peripheral/nvmctrl/plib_nvmctrl.c  .generated_files/flags/default/cb7da4d5a7c2469d4a617b39dc9e9b99817c08a0 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/nvmctrl" 
	@${RM} ${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o.d 
	@${RM} ${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_SIMULATOR=1  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o.d" -o ${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o peripheral/nvmctrl/plib_nvmctrl.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/port/plib_port.o: peripheral/port/plib_port.c  .generated_files/flags/default/4cc64e6b6a843e021ba7dcf3bd91daacd2f314e7 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/port" 
	@${RM} ${OBJECTDIR}/peripheral/port/plib_port.o.d 
	@${RM} ${OBJECTDIR}/peripheral/port/plib_port.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_SIMULATOR=1  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/port/plib_port.o.d" -o ${OBJECTDIR}/peripheral/port/plib_port.o peripheral/port/plib_port.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/initialization.o: initialization.c  .generated_files/flags/default/85400209277b3eefd0310b60f84c174f9ed8db5f .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/initialization.o.d 
	@${RM} ${OBJECTDIR}/initialization.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_SIMULATOR=1  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/initialization.o.d" -o ${OBJECTDIR}/initialization.o initialization.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/main.o: main.c  .generated_files/flags/default/c95059c2df643d3d0c2b5eabf2104bf11664a40d .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/main.o.d 
	@${RM} ${OBJECTDIR}/main.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_SIMULATOR=1  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/main.o.d" -o ${OBJECTDIR}/main.o main.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
else
${OBJECTDIR}/peripheral/clock/plib_clock.o: peripheral/clock/plib_clock.c  .generated_files/flags/default/f4e297ebd1f700f9acd2316c9b071ab9277071d5 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/clock" 
	@${RM} ${OBJECTDIR}/peripheral/clock/plib_clock.o.d 
	@${RM} ${OBJECTDIR}/peripheral/clock/plib_clock.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/clock/plib_clock.o.d" -o ${OBJECTDIR}/peripheral/clock/plib_clock.o peripheral/clock/plib_clock.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o: peripheral/cmcc/plib_cmcc.c  .generated_files/flags/default/8f10d0d46a4e73eebf64c0044fcc355e1a10b779 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/cmcc" 
	@${RM} ${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o.d 
	@${RM} ${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o.d" -o ${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o peripheral/cmcc/plib_cmcc.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/evsys/plib_evsys.o: peripheral/evsys/plib_evsys.c  .generated_files/flags/default/352b5f73b540468556974e2f528868be9dd3b33 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/evsys" 
	@${RM} ${OBJECTDIR}/peripheral/evsys/plib_evsys.o.d 
	@${RM} ${OBJECTDIR}/peripheral/evsys/plib_evsys.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/evsys/plib_evsys.o.d" -o ${OBJECTDIR}/peripheral/evsys/plib_evsys.o peripheral/evsys/plib_evsys.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/nvic/plib_nvic.o: peripheral/nvic/plib_nvic.c  .generated_files/flags/default/bbc54ec2c4661bf14a97afefe731e2377bb7fe17 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/nvic" 
	@${RM} ${OBJECTDIR}/peripheral/nvic/plib_nvic.o.d 
	@${RM} ${OBJECTDIR}/peripheral/nvic/plib_nvic.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/nvic/plib_nvic.o.d" -o ${OBJECTDIR}/peripheral/nvic/plib_nvic.o peripheral/nvic/plib_nvic.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o: peripheral/nvmctrl/plib_nvmctrl.c  .generated_files/flags/default/513e5ea115bbb4ac49b3d61b6c3480825c6fb6e3 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/nvmctrl" 
	@${RM} ${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o.d 
	@${RM} ${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o.d" -o ${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o peripheral/nvmctrl/plib_nvmctrl.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/port/plib_port.o: peripheral/port/plib_port.c  .generated_files/flags/default/4c1566d868411ed6888d7c7232373630ba9df2f6 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/port" 
	@${RM} ${OBJECTDIR}/peripheral/port/plib_port.o.d 
	@${RM} ${OBJECTDIR}/peripheral/port/plib_port.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/port/plib_port.o.d" -o ${OBJECTDIR}/peripheral/port/plib_port.o peripheral/port/plib_port.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/initialization.o: initialization.c  .generated_files/flags/default/cbc3d732a7f05098ab6d731e89d24d927cfed083 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/initialization.o.d 
	@${RM} ${OBJECTDIR}/initialization.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/initialization.o.d" -o ${OBJECTDIR}/initialization.o initialization.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/main.o: main.c  .generated_files/flags/default/c93dcef51a040f36a45f2e8c5dbc37a0493c30d5 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/main.o.d 
	@${RM} ${OBJECTDIR}/main.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/main.o.d" -o ${OBJECTDIR}/main.o main.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: compileCPP
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
else
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: link
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
${DISTDIR}/Desperate_attempt_PWM.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk    
	@${MKDIR} ${DISTDIR} 
	${MP_CC} $(MP_EXTRA_LD_PRE) -g  -D__MPLAB_DEBUGGER_SIMULATOR=1 -mprocessor=$(MP_PROCESSOR_OPTION)  -o ${DISTDIR}/Desperate_attempt_PWM.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX} ${OBJECTFILES_QUOTED_IF_SPACED}          -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -Wl,--defsym=__MPLAB_BUILD=1$(MP_EXTRA_LD_POST)$(MP_LINKER_FILE_OPTION),--defsym=__MPLAB_DEBUG=1,--defsym=__DEBUG=1,-D=__DEBUG_D,--defsym=__MPLAB_DEBUGGER_SIMULATOR=1,-Map="${DISTDIR}/${PROJECTNAME}.${IMAGE_TYPE}.map",--memorysummary,${DISTDIR}/memoryfile.xml -mdfp="${DFP_DIR}"
	
else
${DISTDIR}/Desperate_attempt_PWM.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk   
	@${MKDIR} ${DISTDIR} 
	${MP_CC} $(MP_EXTRA_LD_PRE)  -mprocessor=$(MP_PROCESSOR_OPTION)  -o ${DISTDIR}/Desperate_attempt_PWM.X.${IMAGE_TYPE}.${DEBUGGABLE_SUFFIX} ${OBJECTFILES_QUOTED_IF_SPACED}          -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -Wl,--defsym=__MPLAB_BUILD=1$(MP_EXTRA_LD_POST)$(MP_LINKER_FILE_OPTION),-Map="${DISTDIR}/${PROJECTNAME}.${IMAGE_TYPE}.map",--memorysummary,${DISTDIR}/memoryfile.xml -mdfp="${DFP_DIR}"
	${MP_CC_DIR}/xc32-bin2hex ${DISTDIR}/Desperate_attempt_PWM.X.${IMAGE_TYPE}.${DEBUGGABLE_SUFFIX} 
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
