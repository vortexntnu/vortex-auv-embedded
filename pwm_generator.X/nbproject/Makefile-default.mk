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
FINAL_IMAGE=${DISTDIR}/pwm_generator.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}
else
IMAGE_TYPE=production
OUTPUT_SUFFIX=hex
DEBUGGABLE_SUFFIX=elf
FINAL_IMAGE=${DISTDIR}/pwm_generator.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}
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
SOURCEFILES_QUOTED_IF_SPACED=src/dmac.c src/i2c.c src/pm.c src/rtc.c src/tcc.c src/tcc0.c src/usart.c src/wdt.c src/can1.c src/clock.c src/main.c src/system_init.c src/tc4.c

# Object Files Quoted if spaced
OBJECTFILES_QUOTED_IF_SPACED=${OBJECTDIR}/src/dmac.o ${OBJECTDIR}/src/i2c.o ${OBJECTDIR}/src/pm.o ${OBJECTDIR}/src/rtc.o ${OBJECTDIR}/src/tcc.o ${OBJECTDIR}/src/tcc0.o ${OBJECTDIR}/src/usart.o ${OBJECTDIR}/src/wdt.o ${OBJECTDIR}/src/can1.o ${OBJECTDIR}/src/clock.o ${OBJECTDIR}/src/main.o ${OBJECTDIR}/src/system_init.o ${OBJECTDIR}/src/tc4.o
POSSIBLE_DEPFILES=${OBJECTDIR}/src/dmac.o.d ${OBJECTDIR}/src/i2c.o.d ${OBJECTDIR}/src/pm.o.d ${OBJECTDIR}/src/rtc.o.d ${OBJECTDIR}/src/tcc.o.d ${OBJECTDIR}/src/tcc0.o.d ${OBJECTDIR}/src/usart.o.d ${OBJECTDIR}/src/wdt.o.d ${OBJECTDIR}/src/can1.o.d ${OBJECTDIR}/src/clock.o.d ${OBJECTDIR}/src/main.o.d ${OBJECTDIR}/src/system_init.o.d ${OBJECTDIR}/src/tc4.o.d

# Object Files
OBJECTFILES=${OBJECTDIR}/src/dmac.o ${OBJECTDIR}/src/i2c.o ${OBJECTDIR}/src/pm.o ${OBJECTDIR}/src/rtc.o ${OBJECTDIR}/src/tcc.o ${OBJECTDIR}/src/tcc0.o ${OBJECTDIR}/src/usart.o ${OBJECTDIR}/src/wdt.o ${OBJECTDIR}/src/can1.o ${OBJECTDIR}/src/clock.o ${OBJECTDIR}/src/main.o ${OBJECTDIR}/src/system_init.o ${OBJECTDIR}/src/tc4.o

# Source Files
SOURCEFILES=src/dmac.c src/i2c.c src/pm.c src/rtc.c src/tcc.c src/tcc0.c src/usart.c src/wdt.c src/can1.c src/clock.c src/main.c src/system_init.c src/tc4.c

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
	${MAKE}  -f nbproject/Makefile-default.mk ${DISTDIR}/pwm_generator.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}

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
${OBJECTDIR}/src/dmac.o: src/dmac.c  .generated_files/flags/default/a94e3f2b835233016035591c84b9cc8c4311dc77 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/dmac.o.d 
	@${RM} ${OBJECTDIR}/src/dmac.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/dmac.o.d" -o ${OBJECTDIR}/src/dmac.o src/dmac.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/i2c.o: src/i2c.c  .generated_files/flags/default/ebe825ea352a838922c8a49b184a023cd50eedf1 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/i2c.o.d 
	@${RM} ${OBJECTDIR}/src/i2c.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/i2c.o.d" -o ${OBJECTDIR}/src/i2c.o src/i2c.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/pm.o: src/pm.c  .generated_files/flags/default/3604a2782b8d6853727c25b535ddce0e035f0fbf .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/pm.o.d 
	@${RM} ${OBJECTDIR}/src/pm.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/pm.o.d" -o ${OBJECTDIR}/src/pm.o src/pm.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/rtc.o: src/rtc.c  .generated_files/flags/default/4da875be5304564af0f60c1847a81e204485e2a2 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/rtc.o.d 
	@${RM} ${OBJECTDIR}/src/rtc.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/rtc.o.d" -o ${OBJECTDIR}/src/rtc.o src/rtc.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/tcc.o: src/tcc.c  .generated_files/flags/default/35f13be9b864f89cfea214573fb043cfcb464607 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/tcc.o.d 
	@${RM} ${OBJECTDIR}/src/tcc.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/tcc.o.d" -o ${OBJECTDIR}/src/tcc.o src/tcc.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/tcc0.o: src/tcc0.c  .generated_files/flags/default/82cd05868c61573251de541dc62382aadfcf839 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/tcc0.o.d 
	@${RM} ${OBJECTDIR}/src/tcc0.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/tcc0.o.d" -o ${OBJECTDIR}/src/tcc0.o src/tcc0.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/usart.o: src/usart.c  .generated_files/flags/default/8fbfd4f9997f79977db2bd1cc99627dc5adce540 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/usart.o.d 
	@${RM} ${OBJECTDIR}/src/usart.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/usart.o.d" -o ${OBJECTDIR}/src/usart.o src/usart.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/wdt.o: src/wdt.c  .generated_files/flags/default/de0b81d53dc73feba36ed210249b19d59ae53aa0 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/wdt.o.d 
	@${RM} ${OBJECTDIR}/src/wdt.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/wdt.o.d" -o ${OBJECTDIR}/src/wdt.o src/wdt.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/can1.o: src/can1.c  .generated_files/flags/default/a228731136b51cf842b92404023eb5d1845979ba .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/can1.o.d 
	@${RM} ${OBJECTDIR}/src/can1.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/can1.o.d" -o ${OBJECTDIR}/src/can1.o src/can1.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/clock.o: src/clock.c  .generated_files/flags/default/f421fd8e0079cd6984cbde2613b9d586f500dbd3 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/clock.o.d 
	@${RM} ${OBJECTDIR}/src/clock.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/clock.o.d" -o ${OBJECTDIR}/src/clock.o src/clock.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/main.o: src/main.c  .generated_files/flags/default/af1465eb47eadc410ee733f355b7f02eb36a09fe .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/main.o.d 
	@${RM} ${OBJECTDIR}/src/main.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/main.o.d" -o ${OBJECTDIR}/src/main.o src/main.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/system_init.o: src/system_init.c  .generated_files/flags/default/41b1a1d628c3a1213f86b7668a601a5d30349111 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/system_init.o.d 
	@${RM} ${OBJECTDIR}/src/system_init.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/system_init.o.d" -o ${OBJECTDIR}/src/system_init.o src/system_init.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/tc4.o: src/tc4.c  .generated_files/flags/default/6d479e8e758f308f8d0ffc65366f82276b18e1fe .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/tc4.o.d 
	@${RM} ${OBJECTDIR}/src/tc4.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/tc4.o.d" -o ${OBJECTDIR}/src/tc4.o src/tc4.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
else
${OBJECTDIR}/src/dmac.o: src/dmac.c  .generated_files/flags/default/6eaa01eb30b37f294a33297bc2d0f9db9a84261 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/dmac.o.d 
	@${RM} ${OBJECTDIR}/src/dmac.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/dmac.o.d" -o ${OBJECTDIR}/src/dmac.o src/dmac.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/i2c.o: src/i2c.c  .generated_files/flags/default/9e78f0318b9b5e4dae20f524408bf2a3dfe25125 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/i2c.o.d 
	@${RM} ${OBJECTDIR}/src/i2c.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/i2c.o.d" -o ${OBJECTDIR}/src/i2c.o src/i2c.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/pm.o: src/pm.c  .generated_files/flags/default/dbba79f2faca9f0f1573e25b145426d713a0c5a6 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/pm.o.d 
	@${RM} ${OBJECTDIR}/src/pm.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/pm.o.d" -o ${OBJECTDIR}/src/pm.o src/pm.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/rtc.o: src/rtc.c  .generated_files/flags/default/ce053343f8a3c606b369ab95db981d2aa67419e .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/rtc.o.d 
	@${RM} ${OBJECTDIR}/src/rtc.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/rtc.o.d" -o ${OBJECTDIR}/src/rtc.o src/rtc.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/tcc.o: src/tcc.c  .generated_files/flags/default/72357a924a00d4b6cb9543169cece86d31cbf864 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/tcc.o.d 
	@${RM} ${OBJECTDIR}/src/tcc.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/tcc.o.d" -o ${OBJECTDIR}/src/tcc.o src/tcc.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/tcc0.o: src/tcc0.c  .generated_files/flags/default/3214c11419c574b860ef30667317c1375d51391f .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/tcc0.o.d 
	@${RM} ${OBJECTDIR}/src/tcc0.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/tcc0.o.d" -o ${OBJECTDIR}/src/tcc0.o src/tcc0.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/usart.o: src/usart.c  .generated_files/flags/default/2f341a466808cf5d4f45871dd2b1fd9ad386f327 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/usart.o.d 
	@${RM} ${OBJECTDIR}/src/usart.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/usart.o.d" -o ${OBJECTDIR}/src/usart.o src/usart.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/wdt.o: src/wdt.c  .generated_files/flags/default/39887ceb5e58488751cc31d8e024f266795b5dd1 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/wdt.o.d 
	@${RM} ${OBJECTDIR}/src/wdt.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/wdt.o.d" -o ${OBJECTDIR}/src/wdt.o src/wdt.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/can1.o: src/can1.c  .generated_files/flags/default/8c0b03951963e89b418a84a9c54c596285cc169a .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/can1.o.d 
	@${RM} ${OBJECTDIR}/src/can1.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/can1.o.d" -o ${OBJECTDIR}/src/can1.o src/can1.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/clock.o: src/clock.c  .generated_files/flags/default/950482db2a6b797e022cbdb1f967c6ee9e5ec8f0 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/clock.o.d 
	@${RM} ${OBJECTDIR}/src/clock.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/clock.o.d" -o ${OBJECTDIR}/src/clock.o src/clock.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/main.o: src/main.c  .generated_files/flags/default/99f4e9f7b8d6b9667dcac2af054492049ac732ec .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/main.o.d 
	@${RM} ${OBJECTDIR}/src/main.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/main.o.d" -o ${OBJECTDIR}/src/main.o src/main.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/system_init.o: src/system_init.c  .generated_files/flags/default/864fcbce78ac530ce0980b6b43c6c835a8416f82 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/system_init.o.d 
	@${RM} ${OBJECTDIR}/src/system_init.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/system_init.o.d" -o ${OBJECTDIR}/src/system_init.o src/system_init.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/src/tc4.o: src/tc4.c  .generated_files/flags/default/e9274ceb6866fc5f8a58f9199ca6c04364a0da77 .generated_files/flags/default/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/src" 
	@${RM} ${OBJECTDIR}/src/tc4.o.d 
	@${RM} ${OBJECTDIR}/src/tc4.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -fno-common -I"include" -MP -MMD -MF "${OBJECTDIR}/src/tc4.o.d" -o ${OBJECTDIR}/src/tc4.o src/tc4.c    -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -mdfp="${DFP_DIR}/samc21" ${PACK_COMMON_OPTIONS} 
	
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: compileCPP
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
else
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: link
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
${DISTDIR}/pwm_generator.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk    
	@${MKDIR} ${DISTDIR} 
	${MP_CC} $(MP_EXTRA_LD_PRE) -g   -mprocessor=$(MP_PROCESSOR_OPTION)  -o ${DISTDIR}/pwm_generator.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX} ${OBJECTFILES_QUOTED_IF_SPACED}          -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -Wl,--defsym=__MPLAB_BUILD=1$(MP_EXTRA_LD_POST)$(MP_LINKER_FILE_OPTION),--defsym=__ICD2RAM=1,--defsym=__MPLAB_DEBUG=1,--defsym=__DEBUG=1,-D=__DEBUG_D,-Map="${DISTDIR}/${PROJECTNAME}.${IMAGE_TYPE}.map",--memorysummary,${DISTDIR}/memoryfile.xml -mdfp="${DFP_DIR}/samc21"
	
else
${DISTDIR}/pwm_generator.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk   
	@${MKDIR} ${DISTDIR} 
	${MP_CC} $(MP_EXTRA_LD_PRE)  -mprocessor=$(MP_PROCESSOR_OPTION)  -o ${DISTDIR}/pwm_generator.X.${IMAGE_TYPE}.${DEBUGGABLE_SUFFIX} ${OBJECTFILES_QUOTED_IF_SPACED}          -DXPRJ_default=$(CND_CONF)    $(COMPARISON_BUILD)  -Wl,--defsym=__MPLAB_BUILD=1$(MP_EXTRA_LD_POST)$(MP_LINKER_FILE_OPTION),-Map="${DISTDIR}/${PROJECTNAME}.${IMAGE_TYPE}.map",--memorysummary,${DISTDIR}/memoryfile.xml -mdfp="${DFP_DIR}/samc21"
	${MP_CC_DIR}/xc32-bin2hex ${DISTDIR}/pwm_generator.X.${IMAGE_TYPE}.${DEBUGGABLE_SUFFIX} 
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
