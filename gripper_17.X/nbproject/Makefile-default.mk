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
SOURCEFILES_QUOTED_IF_SPACED=src/can1.c src/clock.c src/i2c.c src/main.c src/sercom0_i2c.c src/sercom1_i2c.c src/system_init.c src/tcc.c src/tcc0.c src/usart.c

# Object Files Quoted if spaced
OBJECTFILES_QUOTED_IF_SPACED=${OBJECTDIR}/src/can1.o ${OBJECTDIR}/src/clock.o ${OBJECTDIR}/src/i2c.o ${OBJECTDIR}/src/main.o ${OBJECTDIR}/src/sercom0_i2c.o ${OBJECTDIR}/src/sercom1_i2c.o ${OBJECTDIR}/src/system_init.o ${OBJECTDIR}/src/tcc.o ${OBJECTDIR}/src/tcc0.o ${OBJECTDIR}/src/usart.o
POSSIBLE_DEPFILES=${OBJECTDIR}/src/can1.o.d ${OBJECTDIR}/src/clock.o.d ${OBJECTDIR}/src/i2c.o.d ${OBJECTDIR}/src/main.o.d ${OBJECTDIR}/src/sercom0_i2c.o.d ${OBJECTDIR}/src/sercom1_i2c.o.d ${OBJECTDIR}/src/system_init.o.d ${OBJECTDIR}/src/tcc.o.d ${OBJECTDIR}/src/tcc0.o.d ${OBJECTDIR}/src/usart.o.d

# Object Files
OBJECTFILES=${OBJECTDIR}/src/can1.o ${OBJECTDIR}/src/clock.o ${OBJECTDIR}/src/i2c.o ${OBJECTDIR}/src/main.o ${OBJECTDIR}/src/sercom0_i2c.o ${OBJECTDIR}/src/sercom1_i2c.o ${OBJECTDIR}/src/system_init.o ${OBJECTDIR}/src/tcc.o ${OBJECTDIR}/src/tcc0.o ${OBJECTDIR}/src/usart.o

# Source Files
SOURCEFILES=src/can1.c src/clock.c src/i2c.c src/main.c src/sercom0_i2c.c src/sercom1_i2c.c src/system_init.c src/tcc.c src/tcc0.c src/usart.c

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
