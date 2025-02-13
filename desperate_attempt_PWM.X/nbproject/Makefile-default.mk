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
FINAL_IMAGE=${DISTDIR}/desperate_attempt_PWM.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}
else
IMAGE_TYPE=production
OUTPUT_SUFFIX=hex
DEBUGGABLE_SUFFIX=elf
FINAL_IMAGE=${DISTDIR}/desperate_attempt_PWM.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}
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
	${MAKE}  -f nbproject/Makefile-default.mk ${DISTDIR}/desperate_attempt_PWM.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}

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
${OBJECTDIR}/peripheral/clock/plib_clock.o: peripheral/clock/plib_clock.c  .generated_files/flags/default/193745eeb7356f9381cf690887030e9b4714c11a .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/clock" 
	@${RM} ${OBJECTDIR}/peripheral/clock/plib_clock.o.d 
	@${RM} ${OBJECTDIR}/peripheral/clock/plib_clock.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_SIMULATOR=1  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/clock/plib_clock.o.d" -o ${OBJECTDIR}/peripheral/clock/plib_clock.o peripheral/clock/plib_clock.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o: peripheral/cmcc/plib_cmcc.c  .generated_files/flags/default/4b289b345522ea53e321e830c462d09aa7836af8 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/cmcc" 
	@${RM} ${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o.d 
	@${RM} ${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_SIMULATOR=1  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o.d" -o ${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o peripheral/cmcc/plib_cmcc.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/evsys/plib_evsys.o: peripheral/evsys/plib_evsys.c  .generated_files/flags/default/eb2a712e050607857b8fb7ca189c7d93a57df9d4 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/evsys" 
	@${RM} ${OBJECTDIR}/peripheral/evsys/plib_evsys.o.d 
	@${RM} ${OBJECTDIR}/peripheral/evsys/plib_evsys.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_SIMULATOR=1  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/evsys/plib_evsys.o.d" -o ${OBJECTDIR}/peripheral/evsys/plib_evsys.o peripheral/evsys/plib_evsys.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/nvic/plib_nvic.o: peripheral/nvic/plib_nvic.c  .generated_files/flags/default/4457f6e7716ee7e6e8942e38900ef0ce8a2a03f8 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/nvic" 
	@${RM} ${OBJECTDIR}/peripheral/nvic/plib_nvic.o.d 
	@${RM} ${OBJECTDIR}/peripheral/nvic/plib_nvic.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_SIMULATOR=1  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/nvic/plib_nvic.o.d" -o ${OBJECTDIR}/peripheral/nvic/plib_nvic.o peripheral/nvic/plib_nvic.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o: peripheral/nvmctrl/plib_nvmctrl.c  .generated_files/flags/default/4af08a423b1cffe31a1aaf7581ac998d1d230792 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/nvmctrl" 
	@${RM} ${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o.d 
	@${RM} ${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_SIMULATOR=1  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o.d" -o ${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o peripheral/nvmctrl/plib_nvmctrl.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/port/plib_port.o: peripheral/port/plib_port.c  .generated_files/flags/default/fdc4e34648d09c179b3b0b94bb0f12588a42c6c1 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/port" 
	@${RM} ${OBJECTDIR}/peripheral/port/plib_port.o.d 
	@${RM} ${OBJECTDIR}/peripheral/port/plib_port.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_SIMULATOR=1  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/port/plib_port.o.d" -o ${OBJECTDIR}/peripheral/port/plib_port.o peripheral/port/plib_port.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/initialization.o: initialization.c  .generated_files/flags/default/d1ca6a2480fc7e00caf8e93f5c018cd1f527b975 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/initialization.o.d 
	@${RM} ${OBJECTDIR}/initialization.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_SIMULATOR=1  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/initialization.o.d" -o ${OBJECTDIR}/initialization.o initialization.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/main.o: main.c  .generated_files/flags/default/5520b613f6f04574269c4bd7cb1b80f823973151 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/main.o.d 
	@${RM} ${OBJECTDIR}/main.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG -D__MPLAB_DEBUGGER_SIMULATOR=1  -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/main.o.d" -o ${OBJECTDIR}/main.o main.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
else
${OBJECTDIR}/peripheral/clock/plib_clock.o: peripheral/clock/plib_clock.c  .generated_files/flags/default/faa2697d07f912e65eee1aaf74c26a93a99ee483 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/clock" 
	@${RM} ${OBJECTDIR}/peripheral/clock/plib_clock.o.d 
	@${RM} ${OBJECTDIR}/peripheral/clock/plib_clock.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/clock/plib_clock.o.d" -o ${OBJECTDIR}/peripheral/clock/plib_clock.o peripheral/clock/plib_clock.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o: peripheral/cmcc/plib_cmcc.c  .generated_files/flags/default/2509b2beebae78aabe647ba69d4f2f5d8d3302cb .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/cmcc" 
	@${RM} ${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o.d 
	@${RM} ${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o.d" -o ${OBJECTDIR}/peripheral/cmcc/plib_cmcc.o peripheral/cmcc/plib_cmcc.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/evsys/plib_evsys.o: peripheral/evsys/plib_evsys.c  .generated_files/flags/default/babdb7dfdda5ed13f9e0146402506d8e7892d5c0 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/evsys" 
	@${RM} ${OBJECTDIR}/peripheral/evsys/plib_evsys.o.d 
	@${RM} ${OBJECTDIR}/peripheral/evsys/plib_evsys.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/evsys/plib_evsys.o.d" -o ${OBJECTDIR}/peripheral/evsys/plib_evsys.o peripheral/evsys/plib_evsys.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/nvic/plib_nvic.o: peripheral/nvic/plib_nvic.c  .generated_files/flags/default/5ddab7b62741de12467759fd31d9692394046e35 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/nvic" 
	@${RM} ${OBJECTDIR}/peripheral/nvic/plib_nvic.o.d 
	@${RM} ${OBJECTDIR}/peripheral/nvic/plib_nvic.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/nvic/plib_nvic.o.d" -o ${OBJECTDIR}/peripheral/nvic/plib_nvic.o peripheral/nvic/plib_nvic.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o: peripheral/nvmctrl/plib_nvmctrl.c  .generated_files/flags/default/ea9ebf3af359ef9caecb98eaad56c0786f660950 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/nvmctrl" 
	@${RM} ${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o.d 
	@${RM} ${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o.d" -o ${OBJECTDIR}/peripheral/nvmctrl/plib_nvmctrl.o peripheral/nvmctrl/plib_nvmctrl.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/peripheral/port/plib_port.o: peripheral/port/plib_port.c  .generated_files/flags/default/d14beba422d48f80921c1dc9e61d7b394bf0cd6f .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/peripheral/port" 
	@${RM} ${OBJECTDIR}/peripheral/port/plib_port.o.d 
	@${RM} ${OBJECTDIR}/peripheral/port/plib_port.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/peripheral/port/plib_port.o.d" -o ${OBJECTDIR}/peripheral/port/plib_port.o peripheral/port/plib_port.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/initialization.o: initialization.c  .generated_files/flags/default/5568f830ae07f7990c38da995e212e7b0d1bcd88 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/initialization.o.d 
	@${RM} ${OBJECTDIR}/initialization.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -MP -MMD -MF "${OBJECTDIR}/initialization.o.d" -o ${OBJECTDIR}/initialization.o initialization.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/main.o: main.c  .generated_files/flags/default/896207b9be8b21b5353646cb9aa7ae8e1262192 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
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
${DISTDIR}/desperate_attempt_PWM.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk    
	@${MKDIR} ${DISTDIR} 
	${MP_CC} $(MP_EXTRA_LD_PRE) -g  -D__MPLAB_DEBUGGER_SIMULATOR=1 -mprocessor=$(MP_PROCESSOR_OPTION)  -o ${DISTDIR}/desperate_attempt_PWM.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX} ${OBJECTFILES_QUOTED_IF_SPACED}          -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -Wl,--defsym=__MPLAB_BUILD=1$(MP_EXTRA_LD_POST)$(MP_LINKER_FILE_OPTION),--defsym=__MPLAB_DEBUG=1,--defsym=__DEBUG=1,-D=__DEBUG_D,--defsym=__MPLAB_DEBUGGER_SIMULATOR=1,-Map="${DISTDIR}/${PROJECTNAME}.${IMAGE_TYPE}.map",--memorysummary,${DISTDIR}/memoryfile.xml -mdfp="${DFP_DIR}"
	
else
${DISTDIR}/desperate_attempt_PWM.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk   
	@${MKDIR} ${DISTDIR} 
	${MP_CC} $(MP_EXTRA_LD_PRE)  -mprocessor=$(MP_PROCESSOR_OPTION)  -o ${DISTDIR}/desperate_attempt_PWM.X.${IMAGE_TYPE}.${DEBUGGABLE_SUFFIX} ${OBJECTFILES_QUOTED_IF_SPACED}          -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -Wl,--defsym=__MPLAB_BUILD=1$(MP_EXTRA_LD_POST)$(MP_LINKER_FILE_OPTION),-Map="${DISTDIR}/${PROJECTNAME}.${IMAGE_TYPE}.map",--memorysummary,${DISTDIR}/memoryfile.xml -mdfp="${DFP_DIR}"
	${MP_CC_DIR}/xc32-bin2hex ${DISTDIR}/desperate_attempt_PWM.X.${IMAGE_TYPE}.${DEBUGGABLE_SUFFIX} 
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
