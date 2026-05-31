#
#  Project     	Chameleon
#
#  Copyright 	Dynniq UK Ltd 2018
#
#  All rights reserved
#
#  The information contained herein is the property of Peek Traffic Limited
#  and is supplied without any liability for errors or omissions. No part
#  may be reproduced or copied except as authorised by contract or other
#  written permission. The copyright and foregoing restriction on
#  reproduction and use extend to all media in which the information may be
#  embodied.
#
#   \file		108MOVA.mk
#
#	\brief		Make file to build MOVA M8.0 Kernel
#
#	\author		Andrew Leigh
#
#	\date		19-May-2018
#
MODULE = MOVA
MODNUM = 108
MODVER = M8.0

HDR_DIR = $(BASE)/Include

# Definitions
# -----------

OBJ = $(BASE)/$(MODULE)/obj/$(MODVER)
BIN = $(BASE)/$(MODULE)/bin/$(MODVER)
SRC = $(BASE)/$(MODULE)/source/$(MODVER)
TST = $(BASE)/$(MODULE)/test/$(MODVER)
ARC = $(BASE)/lib

# Executable
EXE = $(BIN)/$(MODNUM)MOVA
MOVALIB = $(ARC)/lib$(MODNUM)MovaLib.a
EXE_LIC = $(BIN)/.$(MODNUM)NewL

# Header list
HDR_LST = \
	$(HDR_DIR)/000Common.h \
	$(HDR_DIR)/501DiagLib.h \
	$(HDR_DIR)/502FaultLib.h \
	$(HDR_DIR)/503SktLib.h \
	$(HDR_DIR)/504CmnLib.h \
	$(HDR_DIR)/505EventLib.h \
	$(SRC)/Messages.h \
	$(SRC)/MsgTypes.h \
	$(SRC)/MVUtils/MVMath.h \
	$(SRC)/MVUtils/CmdLine.h \
	$(SRC)/MVUtils/MVTypes.h \
	$(SRC)/MVUtils/MVError.h \
	$(SRC)/MVUtils/MVDebug.h \
	$(SRC)/MVUtils/resource.h \
	$(SRC)/MVUtils/MVLog.h \
	$(SRC)/MVUtils/MVTrace.h \
	$(SRC)/MVComms/TCPServer.h \
	$(SRC)/MVComms/MVComms.h \
	$(SRC)/MVComms/stdafx.h \
	$(SRC)/MVComms/CommsErr.h \
	$(SRC)/MVComms/TCPCommon.h \
	$(SRC)/MVComms/Serial.h \
	$(SRC)/MVComms/cmstypes.h \
	$(SRC)/MVComms/resource.h \
	$(SRC)/MVComms/clicomms.h \
	$(SRC)/MVComms/TCPClient.h \
	$(SRC)/Messages.h \
	$(SRC)/stdafx.h \
	$(SRC)/MsgTypes.h \
	$(SRC)/pcclock.h \
	$(SRC)/aml_handler.h \
	$(SRC)/aml_messages_decoder_encoder.h \
	$(SRC)/pckernel.h \
	$(SRC)/aml_messages_handler.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/chvalid.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/parserInternals.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xpointer.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/debugXML.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/schematron.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/uri.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/parser.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/encoding.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/tree.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/threads.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xmlversion.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/globals.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/catalog.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/pattern.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xmlreader.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/HTMLtree.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/SAX2.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xmlexports.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xlink.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/SAX.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/entities.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xmlregexp.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/list.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xinclude.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xmlerror.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/schemasInternals.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/dict.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xmlIO.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/HTMLparser.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xmlautomata.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xmlsave.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xmlstring.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xpath.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/valid.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xmlschemastypes.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/c14n.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/DOCBparser.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xmlmodule.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/relaxng.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/nanohttp.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xmlunicode.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xmlwriter.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xmlmemory.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xpathInternals.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/nanoftp.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/hash.h \
	$(SRC)/Externals/headers/libxml2-2.9.2/libxml/xmlschemas.h \
	$(SRC)/Externals/headers/iconv-1.9.2/iconv.h \
	$(SRC)/aml_generic_files_handler.h \
	$(SRC)/pccontio.h \
	$(SRC)/jsmn-master/jsmn.h \
	$(SRC)/pctasks.h \
	$(SRC)/SiemensDummyFiles/error.h \
	$(SRC)/SiemensDummyFiles/uart_int.h \
	$(SRC)/SiemensDummyFiles/proj_def.h \
	$(SRC)/SiemensDummyFiles/base_types.h \
	$(SRC)/SiemensDummyFiles/inpoup.h \
	$(SRC)/SiemensDummyFiles/nu_omu.h \
	$(SRC)/SiemensDummyFiles/keptdata.h \
	$(SRC)/SiemensDummyFiles/nucleus.h \
	$(SRC)/SiemensDummyFiles/licence.h \
	$(SRC)/SiemensDummyFiles/hardware.h \
	$(SRC)/SiemensDummyFiles/diff_bss.h \
	$(SRC)/mova_api.h \
	$(SRC)/resource.h \
	$(SRC)/TestingMOVA/stdafx.h \
	$(SRC)/TestingMOVA/DataManagerForTesting.h \
	$(SRC)/TestingMOVA/targetver.h \
	$(SRC)/TestingMOVA/common_data.h \
	$(SRC)/aml_dataset_file_handler.h \
	$(SRC)/pcerror.h \
	$(SRC)/pctypes.h \
	$(SRC)/kernel/detectors_handler.h \
	$(SRC)/kernel/stages_codes_handler.h \
	$(SRC)/kernel/errhand.h \
	$(SRC)/kernel/datahand.h \
	$(SRC)/kernel/links_ep_handler.h \
	$(SRC)/kernel/messages_manager.h \
	$(SRC)/kernel/estimate_right_turners_count_during_opposed_stage.h \
	$(SRC)/kernel/dynamic_data.h \
	$(SRC)/kernel/tma_mem.h \
	$(SRC)/kernel/sftst.h \
	$(SRC)/kernel/sftstprv.h \
	$(SRC)/kernel/sf.h \
	$(SRC)/kernel/spec_cond.h \
	$(SRC)/kernel/gbltypes.h \
	$(SRC)/kernel/program_controller.h \
	$(SRC)/kernel/display_msg.h \
	$(SRC)/kernel/repository.h \
	$(SRC)/kernel/xml_dataset.h \
	$(SRC)/kernel/mova_op.h \
	$(SRC)/kernel/algorithms_manager.h \
	$(SRC)/kernel/sftypes.h \
	$(SRC)/kernel/timers_manager.h \
	$(SRC)/kernel/tma_alerts.h \
	$(SRC)/kernel/mova_dataset.h \
	$(SRC)/kernel/sfstatsprv.h \
	$(SRC)/kernel/stages_handler.h \
	$(SRC)/kernel/estimate_queues_size.h \
	$(SRC)/kernel/sfdebug.h \
	$(SRC)/kernel/lanes_endsat_handler.h \
	$(SRC)/kernel/kdebugsettings.h \
	$(SRC)/kernel/write.h \
	$(SRC)/kernel/defines.h \
	$(SRC)/kernel/tma_strc.h \
	$(SRC)/kernel/ui_dataaccesstest.h \
	$(SRC)/kernel/sfdepen.h \
	$(SRC)/kernel/links_endsat_handler.h \
	$(SRC)/kernel/kdebug.h \
	$(SRC)/kernel/stages_ep_handler.h \
	$(SRC)/kernel/lanes_handler.h \
	$(SRC)/kernel/generalfunc.h \
	$(SRC)/kernel/sfstats.h \
	$(SRC)/kernel/calculate_reduced_lost_time.h \
	$(SRC)/kernel/capacity_maximisation.h \
	$(SRC)/kernel/tma_preprint.h \
	$(SRC)/kernel/junction_handler.h \
	$(SRC)/kernel/mova_constants.h \
	$(SRC)/kernel/alerts.h \
	$(SRC)/kernel/sfprv.h \
	$(SRC)/kernel/delay_and_stops_optimiser.h \
	$(SRC)/kernel/calculate_short_cycle_control_params.h \
	$(SRC)/kernel/core_interface.h \
	$(SRC)/kernel/mova.h \
	$(SRC)/kernel/mova_types.h \
	$(SRC)/kernel/calculate_bonus_green_time.h \
	$(SRC)/kernel/dataset_interface.h \
	$(SRC)/kernel/obclock.h \
	$(SRC)/kernel/ui_dataaccess.h \
	$(SRC)/kernel/common_macros.h \
	$(SRC)/kernel/getentry.h \
	$(SRC)/kernel/sfmath.h \
	$(SRC)/kernel/links_handler.h \
	$(SRC)/kernel/dataset_handler.h \
	$(SRC)/kernel/select.h \
	$(SRC)/pcioscan.h \
	$(SRC)/cradle/$(MODNUM)Ioscan.h \
	$(SRC)/cradle/$(MODNUM)MovaCradle.h \
	$(SRC)/cradle/$(MODNUM)MovaFaults.h \
	$(SRC)/cradle/$(MODNUM)MovaLib.h


# Source files
SRC_LST = \
	$(SRC)/cradle/$(MODNUM)MovaMain.c \
	$(SRC)/cradle/$(MODNUM)MovaFaults.c \
	$(SRC)/cradle/$(MODNUM)Ioscan.c \
	$(SRC)/cradle/$(MODNUM)MovaCradle.c \
	$(SRC)/cradle/$(MODNUM)MovaLib.c \
	$(SRC)/cradle/$(MODNUM)NewL.c \
	$(SRC)/pctasks.c \
	$(SRC)/MVUtils/MVTrace.c \
	$(SRC)/MVUtils/MVLog.c \
	$(SRC)/MVUtils/MVError.c \
	$(SRC)/MVComms/Serial.c \
	$(SRC)/MVComms/clicomms.c \
	$(SRC)/MVComms/TCPClient.c \
	$(SRC)/MVComms/MVComms.c \
	$(SRC)/MVComms/TCPServer.c \
	$(SRC)/MVComms/stdafx.c \
	$(SRC)/MVComms/CommsErr.c \
	$(SRC)/MVComms/TCPCommon.c \
	$(SRC)/mova_api.c \
	$(SRC)/pcerror.c \
	$(SRC)/aml_dataset_file_handler.c \
	$(SRC)/pcioscan.c \
	$(SRC)/jsmn-master/jsmn.c \
	$(SRC)/SiemensDummyFiles/dummydata.c \
	$(SRC)/pcclock.c \
	$(SRC)/Messages.c \
	$(SRC)/aml_messages_handler.c \
	$(SRC)/aml_messages_decoder_encoder.c \
	$(SRC)/aml_handler.c \
	$(SRC)/pckernel.c \
	$(SRC)/pcdatset.c \
	$(SRC)/pccontio.c \
	$(SRC)/aml_generic_files_handler.c \
	$(SRC)/pcuserio.c \
	$(SRC)/kernel/calculate_reduced_lost_time.c \
	$(SRC)/kernel/sfstats.c \
	$(SRC)/kernel/stages_ep_handler.c \
	$(SRC)/kernel/generalfunc.c \
	$(SRC)/kernel/lanes_handler.c \
	$(SRC)/kernel/sfdepen.c \
	$(SRC)/kernel/kdebug.c \
	$(SRC)/kernel/links_endsat_handler.c \
	$(SRC)/kernel/ui_dataaccesstest.c \
	$(SRC)/kernel/term.c \
	$(SRC)/kernel/alerts.c \
	$(SRC)/kernel/genstg.c \
	$(SRC)/kernel/mova_if.c \
	$(SRC)/kernel/monitr.c \
	$(SRC)/kernel/blocka.c \
	$(SRC)/kernel/junction_handler.c \
	$(SRC)/kernel/tma_preprint.c \
	$(SRC)/kernel/capacity_maximisation.c \
	$(SRC)/kernel/userbig.c \
	$(SRC)/kernel/dataset_interface.c \
	$(SRC)/kernel/calculate_bonus_green_time.c \
	$(SRC)/kernel/detscn.c \
	$(SRC)/kernel/blockp.c \
	$(SRC)/kernel/core_interface.c \
	$(SRC)/kernel/mova.c \
	$(SRC)/kernel/calculate_short_cycle_control_params.c \
	$(SRC)/kernel/delay_and_stops_optimiser.c \
	$(SRC)/kernel/dataset_handler.c \
	$(SRC)/kernel/links_handler.c \
	$(SRC)/kernel/getentry.c \
	$(SRC)/kernel/blockb.c \
	$(SRC)/kernel/obclock.c \
	$(SRC)/kernel/ui_dataaccess.c \
	$(SRC)/kernel/phone.c \
	$(SRC)/kernel/estimate_right_turners_count_during_opposed_stage.c \
	$(SRC)/kernel/messages_manager.c \
	$(SRC)/kernel/datahand.c \
	$(SRC)/kernel/links_ep_handler.c \
	$(SRC)/kernel/answer.c \
	$(SRC)/kernel/detectors_handler.c \
	$(SRC)/kernel/ioscan.c \
	$(SRC)/kernel/movadata.c \
	$(SRC)/kernel/stages_codes_handler.c \
	$(SRC)/kernel/errhand.c \
	$(SRC)/kernel/display_msg.c \
	$(SRC)/kernel/program_controller.c \
	$(SRC)/kernel/sf.c \
	$(SRC)/kernel/spec_cond.c \
	$(SRC)/kernel/tma_mem.c \
	$(SRC)/kernel/blockc.c \
	$(SRC)/kernel/tma_alerts.c \
	$(SRC)/kernel/timers_manager.c \
	$(SRC)/kernel/algorithms_manager.c \
	$(SRC)/kernel/xml_dataset.c \
	$(SRC)/kernel/write.c \
	$(SRC)/kernel/comms.c \
	$(SRC)/kernel/lanes_endsat_handler.c \
	$(SRC)/kernel/blockd.c \
	$(SRC)/kernel/output.c \
	$(SRC)/kernel/estimate_queues_size.c \
	$(SRC)/kernel/stages_handler.c

# Object files
OBJ_MAIN = $(OBJ)/$(MODNUM)mova.o
OBJ_LST = \
	$(OBJ)/cradle/$(MODNUM)MovaMain.o \
	$(OBJ)/cradle/$(MODNUM)MovaFaults.o \
	$(OBJ)/cradle/$(MODNUM)Ioscan.o \
	$(OBJ)/cradle/$(MODNUM)MovaCradle.o \
	$(OBJ)/cradle/$(MODNUM)MovaLib.o \
	$(OBJ)/cradle/$(MODNUM)NewL.o \
	$(OBJ)/pctasks.o \
	$(OBJ)/MVUtils/MVTrace.o \
	$(OBJ)/MVUtils/MVLog.o \
	$(OBJ)/MVUtils/MVError.o \
	$(OBJ)/MVComms/Serial.o \
	$(OBJ)/MVComms/clicomms.o \
	$(OBJ)/MVComms/TCPClient.o \
	$(OBJ)/MVComms/MVComms.o \
	$(OBJ)/MVComms/TCPServer.o \
	$(OBJ)/MVComms/stdafx.o \
	$(OBJ)/MVComms/CommsErr.o \
	$(OBJ)/MVComms/TCPCommon.o \
	$(OBJ)/mova_api.o \
	$(OBJ)/pcerror.o \
	$(OBJ)/aml_dataset_file_handler.o \
	$(OBJ)/pcioscan.o \
	$(OBJ)/jsmn-master/jsmn.o \
	$(OBJ)/SiemensDummyFiles/dummydata.o \
	$(OBJ)/pcclock.o \
	$(OBJ)/Messages.o \
	$(OBJ)/aml_messages_handler.o \
	$(OBJ)/aml_messages_decoder_encoder.o \
	$(OBJ)/aml_handler.o \
	$(OBJ)/pckernel.o \
	$(OBJ)/pcdatset.o \
	$(OBJ)/pccontio.o \
	$(OBJ)/aml_generic_files_handler.o \
	$(OBJ)/pcuserio.o \
	$(OBJ)/kernel/calculate_reduced_lost_time.o \
	$(OBJ)/kernel/sfstats.o \
	$(OBJ)/kernel/stages_ep_handler.o \
	$(OBJ)/kernel/generalfunc.o \
	$(OBJ)/kernel/lanes_handler.o \
	$(OBJ)/kernel/sfdepen.o \
	$(OBJ)/kernel/kdebug.o \
	$(OBJ)/kernel/links_endsat_handler.o \
	$(OBJ)/kernel/ui_dataaccesstest.o \
	$(OBJ)/kernel/term.o \
	$(OBJ)/kernel/alerts.o \
	$(OBJ)/kernel/genstg.o \
	$(OBJ)/kernel/mova_if.o \
	$(OBJ)/kernel/monitr.o \
	$(OBJ)/kernel/blocka.o \
	$(OBJ)/kernel/junction_handler.o \
	$(OBJ)/kernel/tma_preprint.o \
	$(OBJ)/kernel/capacity_maximisation.o \
	$(OBJ)/kernel/userbig.o \
	$(OBJ)/kernel/dataset_interface.o \
	$(OBJ)/kernel/calculate_bonus_green_time.o \
	$(OBJ)/kernel/detscn.o \
	$(OBJ)/kernel/blockp.o \
	$(OBJ)/kernel/core_interface.o \
	$(OBJ)/kernel/mova.o \
	$(OBJ)/kernel/calculate_short_cycle_control_params.o \
	$(OBJ)/kernel/delay_and_stops_optimiser.o \
	$(OBJ)/kernel/dataset_handler.o \
	$(OBJ)/kernel/links_handler.o \
	$(OBJ)/kernel/getentry.o \
	$(OBJ)/kernel/blockb.o \
	$(OBJ)/kernel/obclock.o \
	$(OBJ)/kernel/ui_dataaccess.o \
	$(OBJ)/kernel/phone.o \
	$(OBJ)/kernel/estimate_right_turners_count_during_opposed_stage.o \
	$(OBJ)/kernel/messages_manager.o \
	$(OBJ)/kernel/datahand.o \
	$(OBJ)/kernel/links_ep_handler.o \
	$(OBJ)/kernel/answer.o \
	$(OBJ)/kernel/detectors_handler.o \
	$(OBJ)/kernel/ioscan.o \
	$(OBJ)/kernel/movadata.o \
	$(OBJ)/kernel/stages_codes_handler.o \
	$(OBJ)/kernel/errhand.o \
	$(OBJ)/kernel/display_msg.o \
	$(OBJ)/kernel/program_controller.o \
	$(OBJ)/kernel/sf.o \
	$(OBJ)/kernel/spec_cond.o \
	$(OBJ)/kernel/tma_mem.o \
	$(OBJ)/kernel/blockc.o \
	$(OBJ)/kernel/tma_alerts.o \
	$(OBJ)/kernel/timers_manager.o \
	$(OBJ)/kernel/algorithms_manager.o \
	$(OBJ)/kernel/xml_dataset.o \
	$(OBJ)/kernel/write.o \
	$(OBJ)/kernel/comms.o \
	$(OBJ)/kernel/lanes_endsat_handler.o \
	$(OBJ)/kernel/blockd.o \
	$(OBJ)/kernel/output.o \
	$(OBJ)/kernel/estimate_queues_size.o \
	$(OBJ)/kernel/stages_handler.o

SRC_LIC = $(SRC)/$(MODNUM)NewL.c

OBJ_LIB = $(SRC)/$(MODNUM)MovaLib.o

OBJ_LIC = $(SRC)/$(MODNUM)NewL.o

# Library list (nb do not include 'lib')
LIBS = -lc -lpthread -l501DiagLib -l503SktLib -l504CmnLib -l505EventLib -l502FaultLib

#Debug_Preprocessor_Definitions
#CFLAGS = $(DBG_FLAG) -I$(HDR_DIR) -I$(SRC) -DPEEK -D GCC_BUILD -D _DEBUG -D _LIB -D _TRACE -D _CRT_SECURE_NO_DEPRECATE -D M8_ENABLED
#Release_Preprocessor_Definitions
CFLAGS = $(DBG_FLAG) -I$(HDR_DIR) -I$(SRC) -DPEEK -D GCC_BUILD -D NDEBUG -D _LIB -D _TRACE -D _CRT_SECURE_NO_DEPRECATE -D M8_ENABLED
LDFLAGS += -L$(ARC) -Wl,-rpath=$(RUNTIME_LINKPATH)

# Library dependencies
LIB_LIC = $(ARC)/lib$(MODNUM)MovaLib.a

LIBLIC = -l$(MODNUM)MovaLib

# Make Auto Variable
# $@ - target
# $% - target member
# $< - first dependency
# $^ - all dependencies
# $? - newer dependencies
# $^ - all dependencies
# $(@F) - dir/file file only
# $(*F) - dir/file dir only

# Targets
# -------
$(MODULE): $(EXE) #$(EXE_LIC)

# Builds MOVA, MOVALIB and its licence tool
$(EXE): $(OBJ_LST) $(OBJ_LIC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@ $<
	$(AR) cru $(MOVALIB) $(OBJ_LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(LIBLIC) -o $(EXE_LIC) $(OBJ_LIC)

TIDY:
	rm -f $(OBJ_LST) $(EXE) $(MOVALIB) $(EXE_LIC)

$(OBJ_MAIN): $(SRC_LST) $(HDR_LST)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_LIC): $(SRC_LIC) $(HDR_LST)
	$(CC) $(CFLAGS) -c $< -o $@
