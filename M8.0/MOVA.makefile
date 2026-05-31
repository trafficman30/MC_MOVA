# Compiler flags...
CPP_COMPILER = g++
C_COMPILER = gcc

# Include paths...
Debug_Include_Path=-I"./kernel" -I"./MVComms" -I"./MVUtils" -I"./SiemensDummyFiles" -I"./Externals/headers/libxml2-2.9.2"
Release_Include_Path=-I"./kernel" -I"./MVComms" -I"./MVUtils" -I"./SiemensDummyFiles" -I"./Externals/headers/libxml2-2.9.2"

# Library paths...
Debug_Library_Path=-L"gccDebug" 
Release_Library_Path=-L"gccRelease" 

# Additional libraries...
Debug_Libraries=-Wl,--start-group -lMVComms -lMVUtils  -Wl,--end-group
Release_Libraries=-Wl,--start-group -lMVComms -lMVUtils  -Wl,--end-group

# Preprocessor definitions...
Debug_Preprocessor_Definitions=-D GCC_BUILD -D _DEBUG -D _LIB -D _TRACE -D _CRT_SECURE_NO_DEPRECATE -D M8_ENABLED
Release_Preprocessor_Definitions=-D GCC_BUILD -D NDEBUG -D _LIB -D _TRACE -D _CRT_SECURE_NO_DEPRECATE -D M8_ENABLED
# SLM FIXME - make lib for now but keep CONSOLE option for later
#Debug_Preprocessor_Definitions=-D GCC_BUILD -D _DEBUG -D _CONSOLE -D _TRACE -D PC_WRAPPER -D PCMOVA -D _CRT_SECURE_NO_DEPRECATE 
#Release_Preprocessor_Definitions=-D GCC_BUILD -D NDEBUG -D _CONSOLE -D _TRACE -D PC_WRAPPER -D PCMOVA -D _CRT_SECURE_NO_DEPRECATE 

# Implictly linked object files...
Debug_Implicitly_Linked_Objects=
Release_Implicitly_Linked_Objects=

# Compiler flags...
Debug_Compiler_Flags=-Wall -O0 -g
Release_Compiler_Flags=-O2 

# Builds all configurations for this project...
.PHONY: build_all_configurations
build_all_configurations: Debug Release 

# Builds the Debug configuration...
.PHONY: Debug
Debug: create_folders \
Debug/gccobj/aml_dataset_file_handler.o Debug/gccobj/aml_handler.o Debug/gccobj/aml_messages_decoder_encoder.o \
Debug/gccobj/aml_messages_handler.o Debug/gccobj/jsmn.o \
Debug/gccobj/kernel/algorithms_manager.o Debug/gccobj/kernel/calculate_bonus_green_time.o \
Debug/gccobj/kernel/calculate_reduced_lost_time.o Debug/gccobj/kernel/calculate_short_cycle_control_params.o \
Debug/gccobj/kernel/capacity_maximisation.o Debug/gccobj/kernel/delay_and_stops_optimiser.o \
Debug/gccobj/kernel/estimate_queues_size.o Debug/gccobj/kernel/estimate_right_turners_count_during_opposed_stage.o \
Debug/gccobj/kernel/detectors_handler.o Debug/gccobj/kernel/junction_handler.o \
Debug/gccobj/kernel/lanes_endsat_handler.o Debug/gccobj/kernel/lanes_handler.o \
Debug/gccobj/kernel/links_endsat_handler.o Debug/gccobj/kernel/links_ep_handler.o \
Debug/gccobj/kernel/links_handler.o Debug/gccobj/kernel/stages_codes_handler.o \
Debug/gccobj/kernel/stages_ep_handler.o Debug/gccobj/kernel/stages_handler.o \
Debug/gccobj/kernel/messages_manager.o \
Debug/gccobj/kernel/sf.o Debug/gccobj/kernel/sfdepen.o Debug/gccobj/kernel/sfstats.o \
Debug/gccobj/kernel/timers_manager.o \
Debug/gccobj/kernel/core_interface.o Debug/gccobj/kernel/program_controller.o \
Debug/gccobj/kernel/dataset_handler.o Debug/gccobj/kernel/dataset_interface.o Debug/gccobj/kernel/xml_dataset.o \
Debug/gccobj/kernel/kdebug.o \
Debug/gccobj/kernel/tma_alerts.o Debug/gccobj/kernel/tma_mem.o Debug/gccobj/kernel/tma_preprint.o \
Debug/gccobj/kernel/alerts.o Debug/gccobj/kernel/comms.o Debug/gccobj/kernel/detscn.o \
Debug/gccobj/kernel/display_msg.o Debug/gccobj/kernel/errhand.o Debug/gccobj/kernel/generalfunc.o \
Debug/gccobj/kernel/genstg.o Debug/gccobj/kernel/getentry.o Debug/gccobj/kernel/monitr.o \
Debug/gccobj/kernel/mova.o Debug/gccobj/kernel/output.o Debug/gccobj/kernel/spec_cond.o \
Debug/gccobj/kernel/write.o \
Debug/gccobj/SiemensDummyFiles/dummydata.o \
Debug/gccobj/kernel/ui_dataaccess.o
	ar rcs ./gccDebug/libMOVA.a \
    Debug/gccobj/aml_dataset_file_handler.o Debug/gccobj/aml_handler.o Debug/gccobj/aml_messages_decoder_encoder.o \
    Debug/gccobj/aml_messages_handler.o Debug/gccobj/jsmn.o \
    Debug/gccobj/kernel/algorithms_manager.o Debug/gccobj/kernel/calculate_bonus_green_time.o \
    Debug/gccobj/kernel/calculate_reduced_lost_time.o Debug/gccobj/kernel/calculate_short_cycle_control_params.o \
    Debug/gccobj/kernel/capacity_maximisation.o Debug/gccobj/kernel/delay_and_stops_optimiser.o \
    Debug/gccobj/kernel/estimate_queues_size.o Debug/gccobj/kernel/estimate_right_turners_count_during_opposed_stage.o \
    Debug/gccobj/kernel/detectors_handler.o Debug/gccobj/kernel/junction_handler.o \
    Debug/gccobj/kernel/lanes_endsat_handler.o Debug/gccobj/kernel/lanes_handler.o \
    Debug/gccobj/kernel/links_endsat_handler.o Debug/gccobj/kernel/links_ep_handler.o \
    Debug/gccobj/kernel/links_handler.o Debug/gccobj/kernel/stages_codes_handler.o \
    Debug/gccobj/kernel/stages_ep_handler.o Debug/gccobj/kernel/stages_handler.o \
    Debug/gccobj/kernel/messages_manager.o \
    Debug/gccobj/kernel/sf.o Debug/gccobj/kernel/sfdepen.o Debug/gccobj/kernel/sfstats.o \
    Debug/gccobj/kernel/timers_manager.o \
    Debug/gccobj/kernel/core_interface.o Debug/gccobj/kernel/program_controller.o \
    Debug/gccobj/kernel/dataset_handler.o Debug/gccobj/kernel/dataset_interface.o Debug/gccobj/kernel/xml_dataset.o \
    Debug/gccobj/kernel/kdebug.o \
    Debug/gccobj/kernel/tma_alerts.o Debug/gccobj/kernel/tma_mem.o Debug/gccobj/kernel/tma_preprint.o \
    Debug/gccobj/kernel/alerts.o Debug/gccobj/kernel/comms.o Debug/gccobj/kernel/detscn.o \
    Debug/gccobj/kernel/display_msg.o Debug/gccobj/kernel/errhand.o Debug/gccobj/kernel/generalfunc.o \
    Debug/gccobj/kernel/genstg.o Debug/gccobj/kernel/getentry.o Debug/gccobj/kernel/monitr.o \
    Debug/gccobj/kernel/mova.o Debug/gccobj/kernel/output.o Debug/gccobj/kernel/spec_cond.o \
    Debug/gccobj/kernel/write.o \
    Debug/gccobj/SiemensDummyFiles/dummydata.o \
    Debug/gccobj/kernel/ui_dataaccess.o $(Debug_Implicitly_Linked_Objects)
# SLM FIXME - make cut down version for now but keep option for later
#Debug: create_folders Debug/gccobj/Messages.o Debug/gccobj/pcclock.o Debug/gccobj/pccontio.o \
#Debug/gccobj/pcdatset.o Debug/gccobj/pcerror.o Debug/gccobj/pcioscan.o Debug/gccobj/pckernel.o \
#Debug/gccobj/PCMOVA.o Debug/gccobj/pctasks.o Debug/gccobj/pcuserio.o Debug/gccobj/stdafx.o \
#Debug/gccobj/kernel/alerts.o \
#Debug/gccobj/kernel/comms.o Debug/gccobj/kernel/detscn.o \
#Debug/gccobj/kernel/display_msg.o Debug/gccobj/kernel/errhand.o Debug/gccobj/kernel/genstg.o \
#Debug/gccobj/kernel/getentry.o Debug/gccobj/kernel/monitr.o Debug/gccobj/kernel/mova.o \
#Debug/gccobj/kernel/output.o Debug/gccobj/kernel/sf.o \
#Debug/gccobj/kernel/sfdepen.o Debug/gccobj/kernel/sfstats.o \
#Debug/gccobj/kernel/write.o Debug/gccobj/kernel/generalfunc.o \
#Debug/gccobj/kernel/tma_alerts.o Debug/gccobj/kernel/tma_mem.o Debug/gccobj/kernel/tma_preprint.o \
#Debug/gccobj/SiemensDummyFiles/dummydata.o 
#	g++ Debug/gccobj/Messages.o Debug/gccobj/pcclock.o Debug/gccobj/pccontio.o Debug/gccobj/pcdatset.o \
#    Debug/gccobj/pcerror.o Debug/gccobj/pcioscan.o Debug/gccobj/pckernel.o Debug/gccobj/PCMOVA.o \
#    Debug/gccobj/pctasks.o Debug/gccobj/pcuserio.o Debug/gccobj/stdafx.o Debug/gccobj/kernel/alerts.o \
#    Debug/gccobj/kernel/comms.o \
#    Debug/gccobj/kernel/detscn.o Debug/gccobj/kernel/display_msg.o \
#    Debug/gccobj/kernel/errhand.o Debug/gccobj/kernel/genstg.o Debug/gccobj/kernel/getentry.o \
#    Debug/gccobj/kernel/monitr.o Debug/gccobj/kernel/mova.o \
#    Debug/gccobj/kernel/output.o Debug/gccobj/kernel/sf.o Debug/gccobj/kernel/sfdepen.o \
#    Debug/gccobj/kernel/sfstats.o \
#    Debug/gccobj/kernel/write.o Debug/gccobj/kernel/generalfunc.o Debug/gccobj/kernel/tma_alerts.o \
#    Debug/gccobj/kernel/tma_mem.o Debug/gccobj/kernel/tma_preprint.o \
#    Debug/gccobj/SiemensDummyFiles/dummydata.o  $(Debug_Library_Path) \
#    $(Debug_Libraries) -Wl,-rpath,./ -o gccDebug/MOVA.exe


#######################################################################################
### Start of AML folder ###
###########################

# Compiles file aml_dataset_file_handler.c for the Debug configuration...
-include Debug/gccobj/aml_dataset_file_handler.d
Debug/gccobj/aml_dataset_file_handler.o: aml_dataset_file_handler.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c aml_dataset_file_handler.c $(Debug_Include_Path) -o Debug/gccobj/aml_dataset_file_handler.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM aml_dataset_file_handler.c $(Debug_Include_Path) > Debug/gccobj/aml_dataset_file_handler.d

# Compiles file aml_handler.c for the Debug configuration...
-include Debug/gccobj/aml_handler.d
Debug/gccobj/aml_handler.o: aml_handler.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c aml_handler.c $(Debug_Include_Path) -o Debug/gccobj/aml_handler.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM aml_handler.c $(Debug_Include_Path) > Debug/gccobj/aml_handler.d

# Compiles file aml_messages_decoder_encoder.c for the Debug configuration...
-include Debug/gccobj/aml_messages_decoder_encoder.d
Debug/gccobj/aml_messages_decoder_encoder.o: aml_messages_decoder_encoder.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c aml_messages_decoder_encoder.c $(Debug_Include_Path) -o Debug/gccobj/aml_messages_decoder_encoder.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM aml_messages_decoder_encoder.c $(Debug_Include_Path) > Debug/gccobj/aml_messages_decoder_encoder.d

# Compiles file aml_messages_handler.c for the Debug configuration...
-include Debug/gccobj/aml_messages_handler.d
Debug/gccobj/aml_messages_handler.o: aml_messages_handler.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c aml_messages_handler.c $(Debug_Include_Path) -o Debug/gccobj/aml_messages_handler.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM aml_messages_handler.c $(Debug_Include_Path) > Debug/gccobj/aml_messages_handler.d

# Compiles file jsmn-master/jsmn.c for the Debug configuration...
-include Debug/gccobj/jsmn.d
Debug/gccobj/jsmn.o: jsmn-master/jsmn.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c jsmn-master/jsmn.c $(Debug_Include_Path) -o Debug/gccobj/jsmn.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM jsmn-master/jsmn.c $(Debug_Include_Path) > Debug/gccobj/jsmn.d

#########################
### End of AML folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel/Core/Algorithms folder ###
###################################################

# Compiles file kernel/algorithms_manager.c for the Debug configuration...
-include Debug/gccobj/kernel/algorithms_manager.d
Debug/gccobj/kernel/algorithms_manager.o: kernel/algorithms_manager.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/algorithms_manager.c $(Debug_Include_Path) -o Debug/gccobj/kernel/algorithms_manager.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/algorithms_manager.c $(Debug_Include_Path) > Debug/gccobj/kernel/algorithms_manager.d

# Compiles file kernel/calculate_bonus_green_time.c for the Debug configuration...
-include Debug/gccobj/kernel/calculate_bonus_green_time.d
Debug/gccobj/kernel/calculate_bonus_green_time.o: kernel/calculate_bonus_green_time.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/calculate_bonus_green_time.c $(Debug_Include_Path) -o Debug/gccobj/kernel/calculate_bonus_green_time.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/calculate_bonus_green_time.c $(Debug_Include_Path) > Debug/gccobj/kernel/calculate_bonus_green_time.d

# Compiles file kernel/calculate_reduced_lost_time.c for the Debug configuration...
-include Debug/gccobj/kernel/calculate_reduced_lost_time.d
Debug/gccobj/kernel/calculate_reduced_lost_time.o: kernel/calculate_reduced_lost_time.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/calculate_reduced_lost_time.c $(Debug_Include_Path) -o Debug/gccobj/kernel/calculate_reduced_lost_time.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/calculate_reduced_lost_time.c $(Debug_Include_Path) > Debug/gccobj/kernel/calculate_reduced_lost_time.d

# Compiles file kernel/calculate_short_cycle_control_params.c for the Debug configuration...
-include Debug/gccobj/kernel/calculate_short_cycle_control_params.d
Debug/gccobj/kernel/calculate_short_cycle_control_params.o: kernel/calculate_short_cycle_control_params.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/calculate_short_cycle_control_params.c $(Debug_Include_Path) -o Debug/gccobj/kernel/calculate_short_cycle_control_params.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/calculate_short_cycle_control_params.c $(Debug_Include_Path) > Debug/gccobj/kernel/calculate_short_cycle_control_params.d

# Compiles file kernel/capacity_maximisation.c for the Debug configuration...
-include Debug/gccobj/kernel/capacity_maximisation.d
Debug/gccobj/kernel/capacity_maximisation.o: kernel/capacity_maximisation.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/capacity_maximisation.c $(Debug_Include_Path) -o Debug/gccobj/kernel/capacity_maximisation.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/capacity_maximisation.c $(Debug_Include_Path) > Debug/gccobj/kernel/capacity_maximisation.d

# Compiles file kernel/delay_and_stops_optimiser.c for the Debug configuration...
-include Debug/gccobj/kernel/delay_and_stops_optimiser.d
Debug/gccobj/kernel/delay_and_stops_optimiser.o: kernel/delay_and_stops_optimiser.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/delay_and_stops_optimiser.c $(Debug_Include_Path) -o Debug/gccobj/kernel/delay_and_stops_optimiser.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/delay_and_stops_optimiser.c $(Debug_Include_Path) > Debug/gccobj/kernel/delay_and_stops_optimiser.d

# Compiles file kernel/estimate_queues_size.c for the Debug configuration...
-include Debug/gccobj/kernel/estimate_queues_size.d
Debug/gccobj/kernel/estimate_queues_size.o: kernel/estimate_queues_size.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/estimate_queues_size.c $(Debug_Include_Path) -o Debug/gccobj/kernel/estimate_queues_size.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/estimate_queues_size.c $(Debug_Include_Path) > Debug/gccobj/kernel/estimate_queues_size.d

# Compiles file kernel/estimate_right_turners_count_during_opposed_stage.c for the Debug configuration...
-include Debug/gccobj/kernel/estimate_right_turners_count_during_opposed_stage.d
Debug/gccobj/kernel/estimate_right_turners_count_during_opposed_stage.o: kernel/estimate_right_turners_count_during_opposed_stage.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/estimate_right_turners_count_during_opposed_stage.c $(Debug_Include_Path) -o Debug/gccobj/kernel/estimate_right_turners_count_during_opposed_stage.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/estimate_right_turners_count_during_opposed_stage.c $(Debug_Include_Path) > Debug/gccobj/kernel/estimate_right_turners_count_during_opposed_stage.d

#################################################
### End of MOVA Kernel/Core/Algorithms folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel/Core/Entities Handler folder ###
#########################################################

# Compiles file kernel/detectors_handler.c for the Debug configuration...
-include Debug/gccobj/kernel/detectors_handler.d
Debug/gccobj/kernel/detectors_handler.o: kernel/detectors_handler.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/detectors_handler.c $(Debug_Include_Path) -o Debug/gccobj/kernel/detectors_handler.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/detectors_handler.c $(Debug_Include_Path) > Debug/gccobj/kernel/detectors_handler.d

# Compiles file kernel/junction_handler.c for the Debug configuration...
-include Debug/gccobj/kernel/junction_handler.d
Debug/gccobj/kernel/junction_handler.o: kernel/junction_handler.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/junction_handler.c $(Debug_Include_Path) -o Debug/gccobj/kernel/junction_handler.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/junction_handler.c $(Debug_Include_Path) > Debug/gccobj/kernel/junction_handler.d

# Compiles file kernel/lanes_endsat_handler.c for the Debug configuration...
-include Debug/gccobj/kernel/lanes_endsat_handler.d
Debug/gccobj/kernel/lanes_endsat_handler.o: kernel/lanes_endsat_handler.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/lanes_endsat_handler.c $(Debug_Include_Path) -o Debug/gccobj/kernel/lanes_endsat_handler.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/lanes_endsat_handler.c $(Debug_Include_Path) > Debug/gccobj/kernel/lanes_endsat_handler.d

# Compiles file kernel/lanes_handler.c for the Debug configuration...
-include Debug/gccobj/kernel/lanes_handler.d
Debug/gccobj/kernel/lanes_handler.o: kernel/lanes_handler.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/lanes_handler.c $(Debug_Include_Path) -o Debug/gccobj/kernel/lanes_handler.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/lanes_handler.c $(Debug_Include_Path) > Debug/gccobj/kernel/lanes_handler.d

# Compiles file kernel/links_endsat_handler.c for the Debug configuration...
-include Debug/gccobj/kernel/links_endsat_handler.d
Debug/gccobj/kernel/links_endsat_handler.o: kernel/links_endsat_handler.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/links_endsat_handler.c $(Debug_Include_Path) -o Debug/gccobj/kernel/links_endsat_handler.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/links_endsat_handler.c $(Debug_Include_Path) > Debug/gccobj/kernel/links_endsat_handler.d

# Compiles file kernel/links_ep_handler.c for the Debug configuration...
-include Debug/gccobj/kernel/links_ep_handler.d
Debug/gccobj/kernel/links_ep_handler.o: kernel/links_ep_handler.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/links_ep_handler.c $(Debug_Include_Path) -o Debug/gccobj/kernel/links_ep_handler.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/links_ep_handler.c $(Debug_Include_Path) > Debug/gccobj/kernel/links_ep_handler.d

# Compiles file kernel/links_handler.c for the Debug configuration...
-include Debug/gccobj/kernel/links_handler.d
Debug/gccobj/kernel/links_handler.o: kernel/links_handler.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/links_handler.c $(Debug_Include_Path) -o Debug/gccobj/kernel/links_handler.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/links_handler.c $(Debug_Include_Path) > Debug/gccobj/kernel/links_handler.d

# Compiles file kernel/stages_codes_handler.c for the Debug configuration...
-include Debug/gccobj/kernel/stages_codes_handler.d
Debug/gccobj/kernel/stages_codes_handler.o: kernel/stages_codes_handler.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/stages_codes_handler.c $(Debug_Include_Path) -o Debug/gccobj/kernel/stages_codes_handler.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/stages_codes_handler.c $(Debug_Include_Path) > Debug/gccobj/kernel/stages_codes_handler.d

# Compiles file kernel/stages_ep_handler.c for the Debug configuration...
-include Debug/gccobj/kernel/stages_ep_handler.d
Debug/gccobj/kernel/stages_ep_handler.o: kernel/stages_ep_handler.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/stages_ep_handler.c $(Debug_Include_Path) -o Debug/gccobj/kernel/stages_ep_handler.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/stages_ep_handler.c $(Debug_Include_Path) > Debug/gccobj/kernel/stages_ep_handler.d

# Compiles file kernel/stages_handler.c for the Debug configuration...
-include Debug/gccobj/kernel/stages_handler.d
Debug/gccobj/kernel/stages_handler.o: kernel/stages_handler.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/stages_handler.c $(Debug_Include_Path) -o Debug/gccobj/kernel/stages_handler.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/stages_handler.c $(Debug_Include_Path) > Debug/gccobj/kernel/stages_handler.d

#######################################################
### End of MOVA Kernel/Core/Entities Handler folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel/Core/Messages folder ###
#################################################

# Compiles file kernel/messages_manager.c for the Debug configuration...
-include Debug/gccobj/kernel/messages_manager.d
Debug/gccobj/kernel/messages_manager.o: kernel/messages_manager.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/messages_manager.c $(Debug_Include_Path) -o Debug/gccobj/kernel/messages_manager.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/messages_manager.c $(Debug_Include_Path) > Debug/gccobj/kernel/messages_manager.d

###############################################
### End of MOVA Kernel/Core/Messages folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel/Core/SF folder ###
###########################################

# Compiles file kernel/sf.c for the Debug configuration...
-include Debug/gccobj/kernel/sf.d
Debug/gccobj/kernel/sf.o: kernel/sf.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/sf.c $(Debug_Include_Path) -o Debug/gccobj/kernel/sf.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/sf.c $(Debug_Include_Path) > Debug/gccobj/kernel/sf.d

# Compiles file kernel/sfdepen.c for the Debug configuration...
-include Debug/gccobj/kernel/sfdepen.d
Debug/gccobj/kernel/sfdepen.o: kernel/sfdepen.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/sfdepen.c $(Debug_Include_Path) -o Debug/gccobj/kernel/sfdepen.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/sfdepen.c $(Debug_Include_Path) > Debug/gccobj/kernel/sfdepen.d

# Compiles file kernel/sfstats.c for the Debug configuration...
-include Debug/gccobj/kernel/sfstats.d
Debug/gccobj/kernel/sfstats.o: kernel/sfstats.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/sfstats.c $(Debug_Include_Path) -o Debug/gccobj/kernel/sfstats.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/sfstats.c $(Debug_Include_Path) > Debug/gccobj/kernel/sfstats.d

#########################################
### End of MOVA Kernel/Core/SF folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel/Core/Timers Manager folder ###
#######################################################

# Compiles file kernel/timers_manager.c for the Debug configuration...
-include Debug/gccobj/kernel/timers_manager.d
Debug/gccobj/kernel/timers_manager.o: kernel/timers_manager.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/timers_manager.c $(Debug_Include_Path) -o Debug/gccobj/kernel/timers_manager.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/timers_manager.c $(Debug_Include_Path) > Debug/gccobj/kernel/timers_manager.d

#####################################################
### End of MOVA Kernel/Core/Timers Manager folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel/Core top level folder ###
##################################################

# Compiles file kernel/core_interface.c for the Debug configuration...
-include Debug/gccobj/kernel/core_interface.d
Debug/gccobj/kernel/core_interface.o: kernel/core_interface.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/core_interface.c $(Debug_Include_Path) -o Debug/gccobj/kernel/core_interface.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/core_interface.c $(Debug_Include_Path) > Debug/gccobj/kernel/core_interface.d

# Compiles file kernel/program_controller.c for the Debug configuration...
-include Debug/gccobj/kernel/program_controller.d
Debug/gccobj/kernel/program_controller.o: kernel/program_controller.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/program_controller.c $(Debug_Include_Path) -o Debug/gccobj/kernel/program_controller.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/program_controller.c $(Debug_Include_Path) > Debug/gccobj/kernel/program_controller.d

################################################
### End of MOVA Kernel/Core top level folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel/Data Handler folder ###
################################################

# Compiles file kernel/dataset_handler.c for the Debug configuration...
-include Debug/gccobj/kernel/dataset_handler.d
Debug/gccobj/kernel/dataset_handler.o: kernel/dataset_handler.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/dataset_handler.c $(Debug_Include_Path) -o Debug/gccobj/kernel/dataset_handler.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/dataset_handler.c $(Debug_Include_Path) > Debug/gccobj/kernel/dataset_handler.d

# Compiles file kernel/dataset_interface.c for the Debug configuration...
-include Debug/gccobj/kernel/dataset_interface.d
Debug/gccobj/kernel/dataset_interface.o: kernel/dataset_interface.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/dataset_interface.c $(Debug_Include_Path) -o Debug/gccobj/kernel/dataset_interface.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/dataset_interface.c $(Debug_Include_Path) > Debug/gccobj/kernel/dataset_interface.d

# Compiles file kernel/xml_dataset.c for the Debug configuration...
-include Debug/gccobj/kernel/xml_dataset.d
Debug/gccobj/kernel/xml_dataset.o: kernel/xml_dataset.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/xml_dataset.c $(Debug_Include_Path) -o Debug/gccobj/kernel/xml_dataset.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/xml_dataset.c $(Debug_Include_Path) > Debug/gccobj/kernel/xml_dataset.d

##############################################
### End of MOVA Kernel/Data Handler folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel/Debugging folder ###
#############################################

# Compiles file kernel/kdebug.c for the Debug configuration...
-include Debug/gccobj/kernel/kdebug.d
Debug/gccobj/kernel/kdebug.o: kernel/kdebug.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/kdebug.c $(Debug_Include_Path) -o Debug/gccobj/kernel/kdebug.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/kdebug.c $(Debug_Include_Path) > Debug/gccobj/kernel/kdebug.d

###########################################
### End of MOVA Kernel/Debugging folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel/TMA folder ###
#######################################

# Compiles file kernel/tma_alerts.c for the Debug configuration...
-include Debug/gccobj/kernel/tma_alerts.d
Debug/gccobj/kernel/tma_alerts.o: kernel/tma_alerts.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/tma_alerts.c $(Debug_Include_Path) -o Debug/gccobj/kernel/tma_alerts.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/tma_alerts.c $(Debug_Include_Path) > Debug/gccobj/kernel/tma_alerts.d

# Compiles file kernel/tma_mem.c for the Debug configuration...
-include Debug/gccobj/kernel/tma_mem.d
Debug/gccobj/kernel/tma_mem.o: kernel/tma_mem.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/tma_mem.c $(Debug_Include_Path) -o Debug/gccobj/kernel/tma_mem.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/tma_mem.c $(Debug_Include_Path) > Debug/gccobj/kernel/tma_mem.d

# Compiles file kernel/tma_preprint.c for the Debug configuration...
-include Debug/gccobj/kernel/tma_preprint.d
Debug/gccobj/kernel/tma_preprint.o: kernel/tma_preprint.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/tma_preprint.c $(Debug_Include_Path) -o Debug/gccobj/kernel/tma_preprint.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/tma_preprint.c $(Debug_Include_Path) > Debug/gccobj/kernel/tma_preprint.d

#####################################
### End of MOVA Kernel/TMA folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel top level folder ###
#############################################

# Compiles file kernel/alerts.c for the Debug configuration...
-include Debug/gccobj/kernel/alerts.d
Debug/gccobj/kernel/alerts.o: kernel/alerts.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/alerts.c $(Debug_Include_Path) -o Debug/gccobj/kernel/alerts.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/alerts.c $(Debug_Include_Path) > Debug/gccobj/kernel/alerts.d

# Compiles file kernel/comms.c for the Debug configuration...
-include Debug/gccobj/kernel/comms.d
Debug/gccobj/kernel/comms.o: kernel/comms.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/comms.c $(Debug_Include_Path) -o Debug/gccobj/kernel/comms.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/comms.c $(Debug_Include_Path) > Debug/gccobj/kernel/comms.d

# Compiles file kernel/detscn.c for the Debug configuration...
-include Debug/gccobj/kernel/detscn.d
Debug/gccobj/kernel/detscn.o: kernel/detscn.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/detscn.c $(Debug_Include_Path) -o Debug/gccobj/kernel/detscn.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/detscn.c $(Debug_Include_Path) > Debug/gccobj/kernel/detscn.d

# Compiles file kernel/display_msg.c for the Debug configuration...
-include Debug/gccobj/kernel/display_msg.d
Debug/gccobj/kernel/display_msg.o: kernel/display_msg.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/display_msg.c $(Debug_Include_Path) -o Debug/gccobj/kernel/display_msg.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/display_msg.c $(Debug_Include_Path) > Debug/gccobj/kernel/display_msg.d

# Compiles file kernel/errhand.c for the Debug configuration...
-include Debug/gccobj/kernel/errhand.d
Debug/gccobj/kernel/errhand.o: kernel/errhand.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/errhand.c $(Debug_Include_Path) -o Debug/gccobj/kernel/errhand.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/errhand.c $(Debug_Include_Path) > Debug/gccobj/kernel/errhand.d

# Compiles file kernel/generalfunc.c for the Debug configuration...
-include Debug/gccobj/kernel/generalfunc.d
Debug/gccobj/kernel/generalfunc.o: kernel/generalfunc.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/generalfunc.c $(Debug_Include_Path) -o Debug/gccobj/kernel/generalfunc.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/generalfunc.c $(Debug_Include_Path) > Debug/gccobj/kernel/generalfunc.d

# Compiles file kernel/genstg.c for the Debug configuration...
-include Debug/gccobj/kernel/genstg.d
Debug/gccobj/kernel/genstg.o: kernel/genstg.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/genstg.c $(Debug_Include_Path) -o Debug/gccobj/kernel/genstg.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/genstg.c $(Debug_Include_Path) > Debug/gccobj/kernel/genstg.d

# Compiles file kernel/getentry.c for the Debug configuration...
-include Debug/gccobj/kernel/getentry.d
Debug/gccobj/kernel/getentry.o: kernel/getentry.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/getentry.c $(Debug_Include_Path) -o Debug/gccobj/kernel/getentry.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/getentry.c $(Debug_Include_Path) > Debug/gccobj/kernel/getentry.d

# Compiles file kernel/monitr.c for the Debug configuration...
-include Debug/gccobj/kernel/monitr.d
Debug/gccobj/kernel/monitr.o: kernel/monitr.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/monitr.c $(Debug_Include_Path) -o Debug/gccobj/kernel/monitr.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/monitr.c $(Debug_Include_Path) > Debug/gccobj/kernel/monitr.d

# Compiles file kernel/mova.c for the Debug configuration...
-include Debug/gccobj/kernel/mova.d
Debug/gccobj/kernel/mova.o: kernel/mova.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/mova.c $(Debug_Include_Path) -o Debug/gccobj/kernel/mova.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/mova.c $(Debug_Include_Path) > Debug/gccobj/kernel/mova.d

# Compiles file kernel/output.c for the Debug configuration...
-include Debug/gccobj/kernel/output.d
Debug/gccobj/kernel/output.o: kernel/output.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/output.c $(Debug_Include_Path) -o Debug/gccobj/kernel/output.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/output.c $(Debug_Include_Path) > Debug/gccobj/kernel/output.d

# Compiles file kernel/spec_cond.c for the Debug configuration...
-include Debug/gccobj/kernel/spec_cond.d
Debug/gccobj/kernel/spec_cond.o: kernel/spec_cond.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/spec_cond.c $(Debug_Include_Path) -o Debug/gccobj/kernel/spec_cond.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/spec_cond.c $(Debug_Include_Path) > Debug/gccobj/kernel/spec_cond.d

# Compiles file kernel/write.c for the Debug configuration...
-include Debug/gccobj/kernel/write.d
Debug/gccobj/kernel/write.o: kernel/write.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/write.c $(Debug_Include_Path) -o Debug/gccobj/kernel/write.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/write.c $(Debug_Include_Path) > Debug/gccobj/kernel/write.d

###########################################
### End of MOVA Kernel top level folder ###
#######################################################################################


#######################################################################################
### Start of Siemens Dummy Files folder ###
###########################################

# Compiles file SiemensDummyFiles/dummydata.c for the Debug configuration...
-include Debug/gccobj/SiemensDummyFiles/dummydata.d
Debug/gccobj/SiemensDummyFiles/dummydata.o: SiemensDummyFiles/dummydata.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c SiemensDummyFiles/dummydata.c $(Debug_Include_Path) -o Debug/gccobj/SiemensDummyFiles/dummydata.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM SiemensDummyFiles/dummydata.c $(Debug_Include_Path) > Debug/gccobj/SiemensDummyFiles/dummydata.d

#########################################
### End of Siemens Dummy Files folder ###
#######################################################################################


#######################################################################################
### Start of MOVA top level folder ###
######################################

# Compiles file Messages.c for the Debug configuration...
-include Debug/gccobj/Messages.d
Debug/gccobj/Messages.o: Messages.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c Messages.c $(Debug_Include_Path) -o Debug/gccobj/Messages.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM Messages.c $(Debug_Include_Path) > Debug/gccobj/Messages.d

# Compiles file pcclock.c for the Debug configuration...
-include Debug/gccobj/pcclock.d
Debug/gccobj/pcclock.o: pcclock.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c pcclock.c $(Debug_Include_Path) -o Debug/gccobj/pcclock.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM pcclock.c $(Debug_Include_Path) > Debug/gccobj/pcclock.d

# Compiles file pccontio.c for the Debug configuration...
-include Debug/gccobj/pccontio.d
Debug/gccobj/pccontio.o: pccontio.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c pccontio.c $(Debug_Include_Path) -o Debug/gccobj/pccontio.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM pccontio.c $(Debug_Include_Path) > Debug/gccobj/pccontio.d

# Compiles file pcdatset.c for the Debug configuration...
-include Debug/gccobj/pcdatset.d
Debug/gccobj/pcdatset.o: pcdatset.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c pcdatset.c $(Debug_Include_Path) -o Debug/gccobj/pcdatset.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM pcdatset.c $(Debug_Include_Path) > Debug/gccobj/pcdatset.d

# Compiles file pcerror.c for the Debug configuration...
-include Debug/gccobj/pcerror.d
Debug/gccobj/pcerror.o: pcerror.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c pcerror.c $(Debug_Include_Path) -o Debug/gccobj/pcerror.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM pcerror.c $(Debug_Include_Path) > Debug/gccobj/pcerror.d

# Compiles file pcioscan.c for the Debug configuration...
-include Debug/gccobj/pcioscan.d
Debug/gccobj/pcioscan.o: pcioscan.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c pcioscan.c $(Debug_Include_Path) -o Debug/gccobj/pcioscan.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM pcioscan.c $(Debug_Include_Path) > Debug/gccobj/pcioscan.d

# Compiles file pckernel.c for the Debug configuration...
-include Debug/gccobj/pckernel.d
Debug/gccobj/pckernel.o: pckernel.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c pckernel.c $(Debug_Include_Path) -o Debug/gccobj/pckernel.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM pckernel.c $(Debug_Include_Path) > Debug/gccobj/pckernel.d

# Compiles file PCMOVA.cpp for the Debug configuration...
-include Debug/gccobj/PCMOVA.d
Debug/gccobj/PCMOVA.o: PCMOVA.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c PCMOVA.cpp $(Debug_Include_Path) -o Debug/gccobj/PCMOVA.o
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM PCMOVA.cpp $(Debug_Include_Path) > Debug/gccobj/PCMOVA.d

# Compiles file pctasks.c for the Debug configuration...
-include Debug/gccobj/pctasks.d
Debug/gccobj/pctasks.o: pctasks.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c pctasks.c $(Debug_Include_Path) -o Debug/gccobj/pctasks.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM pctasks.c $(Debug_Include_Path) > Debug/gccobj/pctasks.d

# Compiles file pcuserio.c for the Debug configuration...
-include Debug/gccobj/pcuserio.d
Debug/gccobj/pcuserio.o: pcuserio.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c pcuserio.c $(Debug_Include_Path) -o Debug/gccobj/pcuserio.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM pcuserio.c $(Debug_Include_Path) > Debug/gccobj/pcuserio.d

# Compiles file pcxmldataset.c for the Debug configuration...
-include Debug/gccobj/pcxmldataset.d
Debug/gccobj/pcxmldataset.o: pcxmldataset.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c pcxmldataset.c $(Debug_Include_Path) -o Debug/gccobj/pcxmldataset.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM pcxmldataset.c $(Debug_Include_Path) > Debug/gccobj/pcxmldataset.d

# Compiles file stdafx.cpp for the Debug configuration...
-include Debug/gccobj/stdafx.d
Debug/gccobj/stdafx.o: stdafx.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c stdafx.cpp $(Debug_Include_Path) -o Debug/gccobj/stdafx.o
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM stdafx.cpp $(Debug_Include_Path) > Debug/gccobj/stdafx.d

# Compiles file kernel/ui_dataaccess.c for the Debug configuration...
-include Debug/gccobj/kernel/ui_dataaccess.d
Debug/gccobj/kernel/ui_dataaccess.o: kernel/ui_dataaccess.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c kernel/ui_dataaccess.c $(Debug_Include_Path) -o Debug/gccobj/kernel/ui_dataaccess.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM kernel/ui_dataaccess.c $(Debug_Include_Path) > Debug/gccobj/kernel/ui_dataaccess.d

####################################
### End of MOVA top level folder ###
#######################################################################################




# Builds the Release configuration...
.PHONY: Release
Release: create_folders \
Release/gccobj/aml_dataset_file_handler.o Release/gccobj/aml_handler.o Release/gccobj/aml_messages_decoder_encoder.o \
Release/gccobj/aml_messages_handler.o Release/gccobj/jsmn.o \
Release/gccobj/kernel/algorithms_manager.o Release/gccobj/kernel/calculate_bonus_green_time.o \
Release/gccobj/kernel/calculate_reduced_lost_time.o Release/gccobj/kernel/calculate_short_cycle_control_params.o \
Release/gccobj/kernel/capacity_maximisation.o Release/gccobj/kernel/delay_and_stops_optimiser.o \
Release/gccobj/kernel/estimate_queues_size.o Release/gccobj/kernel/estimate_right_turners_count_during_opposed_stage.o \
Release/gccobj/kernel/detectors_handler.o Release/gccobj/kernel/junction_handler.o \
Release/gccobj/kernel/lanes_endsat_handler.o Release/gccobj/kernel/lanes_handler.o \
Release/gccobj/kernel/links_endsat_handler.o Release/gccobj/kernel/links_ep_handler.o \
Release/gccobj/kernel/links_handler.o Release/gccobj/kernel/stages_codes_handler.o \
Release/gccobj/kernel/stages_ep_handler.o Release/gccobj/kernel/stages_handler.o \
Release/gccobj/kernel/messages_manager.o \
Release/gccobj/kernel/sf.o Release/gccobj/kernel/sfdepen.o Release/gccobj/kernel/sfstats.o \
Release/gccobj/kernel/timers_manager.o \
Release/gccobj/kernel/core_interface.o Release/gccobj/kernel/program_controller.o \
Release/gccobj/kernel/dataset_handler.o Release/gccobj/kernel/dataset_interface.o Release/gccobj/kernel/xml_dataset.o \
Release/gccobj/kernel/kdebug.o \
Release/gccobj/kernel/tma_alerts.o Release/gccobj/kernel/tma_mem.o Release/gccobj/kernel/tma_preprint.o \
Release/gccobj/kernel/alerts.o Release/gccobj/kernel/comms.o Release/gccobj/kernel/detscn.o \
Release/gccobj/kernel/display_msg.o Release/gccobj/kernel/errhand.o Release/gccobj/kernel/generalfunc.o \
Release/gccobj/kernel/genstg.o Release/gccobj/kernel/getentry.o Release/gccobj/kernel/monitr.o \
Release/gccobj/kernel/mova.o Release/gccobj/kernel/output.o Release/gccobj/kernel/spec_cond.o \
Release/gccobj/kernel/write.o \
Release/gccobj/SiemensDummyFiles/dummydata.o \
Release/gccobj/kernel/ui_dataaccess.o
	ar rcs ./gccRelease/libMOVA.a  \
    Release/gccobj/aml_dataset_file_handler.o Release/gccobj/aml_handler.o Release/gccobj/aml_messages_decoder_encoder.o \
    Release/gccobj/aml_messages_handler.o Release/gccobj/jsmn.o \
    Release/gccobj/kernel/algorithms_manager.o Release/gccobj/kernel/calculate_bonus_green_time.o \
    Release/gccobj/kernel/calculate_reduced_lost_time.o Release/gccobj/kernel/calculate_short_cycle_control_params.o \
    Release/gccobj/kernel/capacity_maximisation.o Release/gccobj/kernel/delay_and_stops_optimiser.o \
    Release/gccobj/kernel/estimate_queues_size.o Release/gccobj/kernel/estimate_right_turners_count_during_opposed_stage.o \
    Release/gccobj/kernel/detectors_handler.o Release/gccobj/kernel/junction_handler.o \
    Release/gccobj/kernel/lanes_endsat_handler.o Release/gccobj/kernel/lanes_handler.o \
    Release/gccobj/kernel/links_endsat_handler.o Release/gccobj/kernel/links_ep_handler.o \
    Release/gccobj/kernel/links_handler.o Release/gccobj/kernel/stages_codes_handler.o \
    Release/gccobj/kernel/stages_ep_handler.o Release/gccobj/kernel/stages_handler.o \
    Release/gccobj/kernel/messages_manager.o \
    Release/gccobj/kernel/sf.o Release/gccobj/kernel/sfdepen.o Release/gccobj/kernel/sfstats.o \
    Release/gccobj/kernel/timers_manager.o \
    Release/gccobj/kernel/core_interface.o Release/gccobj/kernel/program_controller.o \
    Release/gccobj/kernel/dataset_handler.o Release/gccobj/kernel/dataset_interface.o Release/gccobj/kernel/xml_dataset.o \
    Release/gccobj/kernel/kdebug.o \
    Release/gccobj/kernel/tma_alerts.o Release/gccobj/kernel/tma_mem.o Release/gccobj/kernel/tma_preprint.o \
    Release/gccobj/kernel/alerts.o Release/gccobj/kernel/comms.o Release/gccobj/kernel/detscn.o \
    Release/gccobj/kernel/display_msg.o Release/gccobj/kernel/errhand.o Release/gccobj/kernel/generalfunc.o \
    Release/gccobj/kernel/genstg.o Release/gccobj/kernel/getentry.o Release/gccobj/kernel/monitr.o \
    Release/gccobj/kernel/mova.o Release/gccobj/kernel/output.o Release/gccobj/kernel/spec_cond.o \
    Release/gccobj/kernel/write.o \
    Release/gccobj/SiemensDummyFiles/dummydata.o   \
    Release/gccobj/kernel/ui_dataaccess.o $(Release_Implicitly_Linked_Objects)
# SLM FIXME - make cut down version for now but keep option for later
#Release: create_folders Release/gccobj/Messages.o Release/gccobj/pcclock.o Release/gccobj/pccontio.o \
#Release/gccobj/pcdatset.o Release/gccobj/pcerror.o Release/gccobj/pcioscan.o Release/gccobj/pckernel.o \
#Release/gccobj/PCMOVA.o Release/gccobj/pctasks.o Release/gccobj/pcuserio.o Release/gccobj/stdafx.o \
#Release/gccobj/kernel/alerts.o \
#Release/gccobj/kernel/comms.o Release/gccobj/kernel/detscn.o \
#Release/gccobj/kernel/display_msg.o Release/gccobj/kernel/errhand.o Release/gccobj/kernel/genstg.o \
#Release/gccobj/kernel/getentry.o Release/gccobj/kernel/monitr.o Release/gccobj/kernel/mova.o \
#Release/gccobj/kernel/output.o Release/gccobj/kernel/sf.o \
#Release/gccobj/kernel/sfdepen.o Release/gccobj/kernel/sfstats.o \
#Release/gccobj/kernel/write.o Release/gccobj/kernel/generalfunc.o \
#Release/gccobj/kernel/tma_alerts.o Release/gccobj/kernel/tma_mem.o Release/gccobj/kernel/tma_preprint.o \
#Release/gccobj/SiemensDummyFiles/dummydata.o 
#	g++ Release/gccobj/Messages.o Release/gccobj/pcclock.o Release/gccobj/pccontio.o \
#    Release/gccobj/pcdatset.o Release/gccobj/pcerror.o Release/gccobj/pcioscan.o \
#    Release/gccobj/pckernel.o Release/gccobj/PCMOVA.o Release/gccobj/pctasks.o \
#    Release/gccobj/pcuserio.o Release/gccobj/stdafx.o Release/gccobj/kernel/alerts.o \
#    Release/gccobj/kernel/comms.o \
#    Release/gccobj/kernel/detscn.o Release/gccobj/kernel/display_msg.o \
#    Release/gccobj/kernel/errhand.o Release/gccobj/kernel/genstg.o Release/gccobj/kernel/getentry.o \
#    Release/gccobj/kernel/monitr.o Release/gccobj/kernel/mova.o \
#    Release/gccobj/kernel/output.o Release/gccobj/kernel/sf.o Release/gccobj/kernel/sfdepen.o \
#    Release/gccobj/kernel/sfstats.o \
#    Release/gccobj/kernel/write.o Release/gccobj/kernel/generalfunc.o Release/gccobj/kernel/tma_alerts.o \
#    Release/gccobj/kernel/tma_mem.o Release/gccobj/kernel/tma_preprint.o \
#    Release/gccobj/SiemensDummyFiles/dummydata.o  $(Release_Library_Path) \
#    $(Release_Libraries) -Wl,-rpath,./ -o gccRelease/MOVA.exe


#######################################################################################
### Start of AML folder ###
###########################

# Compiles file aml_dataset_file_handler.c for the Release configuration...
-include Release/gccobj/aml_dataset_file_handler.d
Release/gccobj/aml_dataset_file_handler.o: aml_dataset_file_handler.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c aml_dataset_file_handler.c $(Release_Include_Path) -o Release/gccobj/aml_dataset_file_handler.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM aml_dataset_file_handler.c $(Release_Include_Path) > Release/gccobj/aml_dataset_file_handler.d

# Compiles file aml_handler.c for the Release configuration...
-include Release/gccobj/aml_handler.d
Release/gccobj/aml_handler.o: aml_handler.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c aml_handler.c $(Release_Include_Path) -o Release/gccobj/aml_handler.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM aml_handler.c $(Release_Include_Path) > Release/gccobj/aml_handler.d

# Compiles file aml_messages_decoder_encoder.c for the Release configuration...
-include Release/gccobj/aml_messages_decoder_encoder.d
Release/gccobj/aml_messages_decoder_encoder.o: aml_messages_decoder_encoder.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c aml_messages_decoder_encoder.c $(Release_Include_Path) -o Release/gccobj/aml_messages_decoder_encoder.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM aml_messages_decoder_encoder.c $(Release_Include_Path) > Release/gccobj/aml_messages_decoder_encoder.d

# Compiles file aml_messages_handler.c for the Release configuration...
-include Release/gccobj/aml_messages_handler.d
Release/gccobj/aml_messages_handler.o: aml_messages_handler.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c aml_messages_handler.c $(Release_Include_Path) -o Release/gccobj/aml_messages_handler.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM aml_messages_handler.c $(Release_Include_Path) > Release/gccobj/aml_messages_handler.d

# Compiles file jsmn-master/jsmn.c for the Release configuration...
-include Release/gccobj/lsmn.d
Release/gccobj/jsmn.o: jsmn-master/jsmn.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c jsmn-master/jsmn.c $(Release_Include_Path) -o Release/gccobj/jsmn.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM jsmn-master/jsmn.c $(Release_Include_Path) > Release/gccobj/jsmn.d

#########################
### End of AML folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel/Core/Algorithms folder ###
###################################################

# Compiles file kernel/algorithms_manager.c for the Release configuration...
-include Release/gccobj/kernel/algorithms_manager.d
Release/gccobj/kernel/algorithms_manager.o: kernel/algorithms_manager.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/algorithms_manager.c $(Release_Include_Path) -o Release/gccobj/kernel/algorithms_manager.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/algorithms_manager.c $(Release_Include_Path) > Release/gccobj/kernel/algorithms_manager.d

# Compiles file kernel/calculate_bonus_green_time.c for the Release configuration...
-include Release/gccobj/kernel/calculate_bonus_green_time.d
Release/gccobj/kernel/calculate_bonus_green_time.o: kernel/calculate_bonus_green_time.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/calculate_bonus_green_time.c $(Release_Include_Path) -o Release/gccobj/kernel/calculate_bonus_green_time.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/calculate_bonus_green_time.c $(Release_Include_Path) > Release/gccobj/kernel/calculate_bonus_green_time.d

# Compiles file kernel/calculate_reduced_lost_time.c for the Release configuration...
-include Release/gccobj/kernel/calculate_reduced_lost_time.d
Release/gccobj/kernel/calculate_reduced_lost_time.o: kernel/calculate_reduced_lost_time.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/calculate_reduced_lost_time.c $(Release_Include_Path) -o Release/gccobj/kernel/calculate_reduced_lost_time.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/calculate_reduced_lost_time.c $(Release_Include_Path) > Release/gccobj/kernel/calculate_reduced_lost_time.d

# Compiles file kernel/calculate_short_cycle_control_params.c for the Release configuration...
-include Release/gccobj/kernel/calculate_short_cycle_control_params.d
Release/gccobj/kernel/calculate_short_cycle_control_params.o: kernel/calculate_short_cycle_control_params.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/calculate_short_cycle_control_params.c $(Release_Include_Path) -o Release/gccobj/kernel/calculate_short_cycle_control_params.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/calculate_short_cycle_control_params.c $(Release_Include_Path) > Release/gccobj/kernel/calculate_short_cycle_control_params.d

# Compiles file kernel/capacity_maximisation.c for the Release configuration...
-include Release/gccobj/kernel/capacity_maximisation.d
Release/gccobj/kernel/capacity_maximisation.o: kernel/capacity_maximisation.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/capacity_maximisation.c $(Release_Include_Path) -o Release/gccobj/kernel/capacity_maximisation.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/capacity_maximisation.c $(Release_Include_Path) > Release/gccobj/kernel/capacity_maximisation.d

# Compiles file kernel/delay_and_stops_optimiser.c for the Release configuration...
-include Release/gccobj/kernel/delay_and_stops_optimiser.d
Release/gccobj/kernel/delay_and_stops_optimiser.o: kernel/delay_and_stops_optimiser.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/delay_and_stops_optimiser.c $(Release_Include_Path) -o Release/gccobj/kernel/delay_and_stops_optimiser.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/delay_and_stops_optimiser.c $(Release_Include_Path) > Release/gccobj/kernel/delay_and_stops_optimiser.d

# Compiles file kernel/estimate_queues_size.c for the Release configuration...
-include Release/gccobj/kernel/estimate_queues_size.d
Release/gccobj/kernel/estimate_queues_size.o: kernel/estimate_queues_size.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/estimate_queues_size.c $(Release_Include_Path) -o Release/gccobj/kernel/estimate_queues_size.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/estimate_queues_size.c $(Release_Include_Path) > Release/gccobj/kernel/estimate_queues_size.d

# Compiles file kernel/estimate_right_turners_count_during_opposed_stage.c for the Release configuration...
-include Release/gccobj/kernel/estimate_right_turners_count_during_opposed_stage.d
Release/gccobj/kernel/estimate_right_turners_count_during_opposed_stage.o: kernel/estimate_right_turners_count_during_opposed_stage.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/estimate_right_turners_count_during_opposed_stage.c $(Release_Include_Path) -o Release/gccobj/kernel/estimate_right_turners_count_during_opposed_stage.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/estimate_right_turners_count_during_opposed_stage.c $(Release_Include_Path) > Release/gccobj/kernel/estimate_right_turners_count_during_opposed_stage.d

#################################################
### End of MOVA Kernel/Core/Algorithms folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel/Core/Entities Handler folder ###
#########################################################

# Compiles file kernel/detectors_handler.c for the Release configuration...
-include Release/gccobj/kernel/detectors_handler.d
Release/gccobj/kernel/detectors_handler.o: kernel/detectors_handler.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/detectors_handler.c $(Release_Include_Path) -o Release/gccobj/kernel/detectors_handler.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/detectors_handler.c $(Release_Include_Path) > Release/gccobj/kernel/detectors_handler.d

# Compiles file kernel/junction_handler.c for the Release configuration...
-include Release/gccobj/kernel/junction_handler.d
Release/gccobj/kernel/junction_handler.o: kernel/junction_handler.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/junction_handler.c $(Release_Include_Path) -o Release/gccobj/kernel/junction_handler.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/junction_handler.c $(Release_Include_Path) > Release/gccobj/kernel/junction_handler.d

# Compiles file kernel/lanes_endsat_handler.c for the Release configuration...
-include Release/gccobj/kernel/lanes_endsat_handler.d
Release/gccobj/kernel/lanes_endsat_handler.o: kernel/lanes_endsat_handler.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/lanes_endsat_handler.c $(Release_Include_Path) -o Release/gccobj/kernel/lanes_endsat_handler.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/lanes_endsat_handler.c $(Release_Include_Path) > Release/gccobj/kernel/lanes_endsat_handler.d

# Compiles file kernel/lanes_handler.c for the Release configuration...
-include Release/gccobj/kernel/lanes_handler.d
Release/gccobj/kernel/lanes_handler.o: kernel/lanes_handler.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/lanes_handler.c $(Release_Include_Path) -o Release/gccobj/kernel/lanes_handler.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/lanes_handler.c $(Release_Include_Path) > Release/gccobj/kernel/lanes_handler.d

# Compiles file kernel/links_endsat_handler.c for the Release configuration...
-include Release/gccobj/kernel/links_endsat_handler.d
Release/gccobj/kernel/links_endsat_handler.o: kernel/links_endsat_handler.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/links_endsat_handler.c $(Release_Include_Path) -o Release/gccobj/kernel/links_endsat_handler.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/links_endsat_handler.c $(Release_Include_Path) > Release/gccobj/kernel/links_endsat_handler.d

# Compiles file kernel/links_ep_handler.c for the Release configuration...
-include Release/gccobj/kernel/links_ep_handler.d
Release/gccobj/kernel/links_ep_handler.o: kernel/links_ep_handler.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/links_ep_handler.c $(Release_Include_Path) -o Release/gccobj/kernel/links_ep_handler.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/links_ep_handler.c $(Release_Include_Path) > Release/gccobj/kernel/links_ep_handler.d

# Compiles file kernel/links_handler.c for the Release configuration...
-include Release/gccobj/kernel/links_handler.d
Release/gccobj/kernel/links_handler.o: kernel/links_handler.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/links_handler.c $(Release_Include_Path) -o Release/gccobj/kernel/links_handler.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/links_handler.c $(Release_Include_Path) > Release/gccobj/kernel/links_handler.d

# Compiles file kernel/stages_codes_handler.c for the Release configuration...
-include Release/gccobj/kernel/stages_codes_handler.d
Release/gccobj/kernel/stages_codes_handler.o: kernel/stages_codes_handler.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/stages_codes_handler.c $(Release_Include_Path) -o Release/gccobj/kernel/stages_codes_handler.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/stages_codes_handler.c $(Release_Include_Path) > Release/gccobj/kernel/stages_codes_handler.d

# Compiles file kernel/stages_ep_handler.c for the Release configuration...
-include Release/gccobj/kernel/stages_ep_handler.d
Release/gccobj/kernel/stages_ep_handler.o: kernel/stages_ep_handler.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/stages_ep_handler.c $(Release_Include_Path) -o Release/gccobj/kernel/stages_ep_handler.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/stages_ep_handler.c $(Release_Include_Path) > Release/gccobj/kernel/stages_ep_handler.d

# Compiles file kernel/stages_handler.c for the Release configuration...
-include Release/gccobj/kernel/stages_handler.d
Release/gccobj/kernel/stages_handler.o: kernel/stages_handler.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/stages_handler.c $(Release_Include_Path) -o Release/gccobj/kernel/stages_handler.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/stages_handler.c $(Release_Include_Path) > Release/gccobj/kernel/stages_handler.d

#######################################################
### End of MOVA Kernel/Core/Entities Handler folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel/Core/Messages folder ###
#################################################

# Compiles file kernel/messages_manager.c for the Release configuration...
-include Release/gccobj/kernel/messages_manager.d
Release/gccobj/kernel/messages_manager.o: kernel/messages_manager.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/messages_manager.c $(Release_Include_Path) -o Release/gccobj/kernel/messages_manager.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/messages_manager.c $(Release_Include_Path) > Release/gccobj/kernel/messages_manager.d

###############################################
### End of MOVA Kernel/Core/Messages folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel/Core/SF folder ###
###########################################

# Compiles file kernel/sf.c for the Release configuration...
-include Release/gccobj/kernel/sf.d
Release/gccobj/kernel/sf.o: kernel/sf.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/sf.c $(Release_Include_Path) -o Release/gccobj/kernel/sf.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/sf.c $(Release_Include_Path) > Release/gccobj/kernel/sf.d

# Compiles file kernel/sfdepen.c for the Release configuration...
-include Release/gccobj/kernel/sfdepen.d
Release/gccobj/kernel/sfdepen.o: kernel/sfdepen.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/sfdepen.c $(Release_Include_Path) -o Release/gccobj/kernel/sfdepen.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/sfdepen.c $(Release_Include_Path) > Release/gccobj/kernel/sfdepen.d

# Compiles file kernel/sfstats.c for the Release configuration...
-include Release/gccobj/kernel/sfstats.d
Release/gccobj/kernel/sfstats.o: kernel/sfstats.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/sfstats.c $(Release_Include_Path) -o Release/gccobj/kernel/sfstats.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/sfstats.c $(Release_Include_Path) > Release/gccobj/kernel/sfstats.d

#########################################
### End of MOVA Kernel/Core/SF folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel/Core/Timers Manager folder ###
#######################################################

# Compiles file kernel/timers_manager.c for the Release configuration...
-include Release/gccobj/kernel/timers_manager.d
Release/gccobj/kernel/timers_manager.o: kernel/timers_manager.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/timers_manager.c $(Release_Include_Path) -o Release/gccobj/kernel/timers_manager.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/timers_manager.c $(Release_Include_Path) > Release/gccobj/kernel/timers_manager.d

#####################################################
### End of MOVA Kernel/Core/Timers Manager folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel/Core top level folder ###
##################################################

# Compiles file kernel/core_interface.c for the Release configuration...
-include Release/gccobj/kernel/core_interface.d
Release/gccobj/kernel/core_interface.o: kernel/core_interface.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/core_interface.c $(Release_Include_Path) -o Release/gccobj/kernel/core_interface.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/core_interface.c $(Release_Include_Path) > Release/gccobj/kernel/core_interface.d

# Compiles file kernel/program_controller.c for the Release configuration...
-include Release/gccobj/kernel/program_controller.d
Release/gccobj/kernel/program_controller.o: kernel/program_controller.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/program_controller.c $(Release_Include_Path) -o Release/gccobj/kernel/program_controller.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/program_controller.c $(Release_Include_Path) > Release/gccobj/kernel/program_controller.d

################################################
### End of MOVA Kernel/Core top level folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel/Data Handler folder ###
################################################

# Compiles file kernel/dataset_handler.c for the Release configuration...
-include Release/gccobj/kernel/dataset_handler.d
Release/gccobj/kernel/dataset_handler.o: kernel/dataset_handler.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/dataset_handler.c $(Release_Include_Path) -o Release/gccobj/kernel/dataset_handler.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/dataset_handler.c $(Release_Include_Path) > Release/gccobj/kernel/dataset_handler.d

# Compiles file kernel/dataset_interface.c for the Release configuration...
-include Release/gccobj/kernel/dataset_interface.d
Release/gccobj/kernel/dataset_interface.o: kernel/dataset_interface.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/dataset_interface.c $(Release_Include_Path) -o Release/gccobj/kernel/dataset_interface.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/dataset_interface.c $(Release_Include_Path) > Release/gccobj/kernel/dataset_interface.d

# Compiles file kernel/xml_dataset.c for the Release configuration...
-include Release/gccobj/kernel/xml_dataset.d
Release/gccobj/kernel/xml_dataset.o: kernel/xml_dataset.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/xml_dataset.c $(Release_Include_Path) -o Release/gccobj/kernel/xml_dataset.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/xml_dataset.c $(Release_Include_Path) > Release/gccobj/kernel/xml_dataset.d

##############################################
### End of MOVA Kernel/Data Handler folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel/Debugging folder ###
#############################################

# Compiles file kernel/kdebug.c for the Release configuration...
-include Release/gccobj/kernel/kdebug.d
Release/gccobj/kernel/kdebug.o: kernel/kdebug.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/kdebug.c $(Release_Include_Path) -o Release/gccobj/kernel/kdebug.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/kdebug.c $(Release_Include_Path) > Release/gccobj/kernel/kdebug.d

###########################################
### End of MOVA Kernel/Debugging folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel/TMA folder ###
#######################################

# Compiles file kernel/tma_alerts.c for the Release configuration...
-include Release/gccobj/kernel/tma_alerts.d
Release/gccobj/kernel/tma_alerts.o: kernel/tma_alerts.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/tma_alerts.c $(Release_Include_Path) -o Release/gccobj/kernel/tma_alerts.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/tma_alerts.c $(Release_Include_Path) > Release/gccobj/kernel/tma_alerts.d

# Compiles file kernel/tma_mem.c for the Release configuration...
-include Release/gccobj/kernel/tma_mem.d
Release/gccobj/kernel/tma_mem.o: kernel/tma_mem.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/tma_mem.c $(Release_Include_Path) -o Release/gccobj/kernel/tma_mem.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/tma_mem.c $(Release_Include_Path) > Release/gccobj/kernel/tma_mem.d

# Compiles file kernel/tma_preprint.c for the Release configuration...
-include Release/gccobj/kernel/tma_preprint.d
Release/gccobj/kernel/tma_preprint.o: kernel/tma_preprint.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/tma_preprint.c $(Release_Include_Path) -o Release/gccobj/kernel/tma_preprint.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/tma_preprint.c $(Release_Include_Path) > Release/gccobj/kernel/tma_preprint.d

#####################################
### End of MOVA Kernel/TMA folder ###
#######################################################################################


#######################################################################################
### Start of MOVA Kernel top level folder ###
#############################################

# Compiles file kernel/alerts.c for the Release configuration...
-include Release/gccobj/kernel/alerts.d
Release/gccobj/kernel/alerts.o: kernel/alerts.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/alerts.c $(Release_Include_Path) -o Release/gccobj/kernel/alerts.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/alerts.c $(Release_Include_Path) > Release/gccobj/kernel/alerts.d

# Compiles file kernel/comms.c for the Release configuration...
-include Release/gccobj/kernel/comms.d
Release/gccobj/kernel/comms.o: kernel/comms.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/comms.c $(Release_Include_Path) -o Release/gccobj/kernel/comms.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/comms.c $(Release_Include_Path) > Release/gccobj/kernel/comms.d

# Compiles file kernel/detscn.c for the Release configuration...
-include Release/gccobj/kernel/detscn.d
Release/gccobj/kernel/detscn.o: kernel/detscn.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/detscn.c $(Release_Include_Path) -o Release/gccobj/kernel/detscn.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/detscn.c $(Release_Include_Path) > Release/gccobj/kernel/detscn.d

# Compiles file kernel/display_msg.c for the Release configuration...
-include Release/gccobj/kernel/display_msg.d
Release/gccobj/kernel/display_msg.o: kernel/display_msg.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/display_msg.c $(Release_Include_Path) -o Release/gccobj/kernel/display_msg.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/display_msg.c $(Release_Include_Path) > Release/gccobj/kernel/display_msg.d

# Compiles file kernel/errhand.c for the Release configuration...
-include Release/gccobj/kernel/errhand.d
Release/gccobj/kernel/errhand.o: kernel/errhand.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/errhand.c $(Release_Include_Path) -o Release/gccobj/kernel/errhand.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/errhand.c $(Release_Include_Path) > Release/gccobj/kernel/errhand.d

# Compiles file kernel/generalfunc.c for the Release configuration...
-include Release/gccobj/kernel/generalfunc.d
Release/gccobj/kernel/generalfunc.o: kernel/generalfunc.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/generalfunc.c $(Release_Include_Path) -o Release/gccobj/kernel/generalfunc.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/generalfunc.c $(Release_Include_Path) > Release/gccobj/kernel/generalfunc.d

# Compiles file kernel/genstg.c for the Release configuration...
-include Release/gccobj/kernel/genstg.d
Release/gccobj/kernel/genstg.o: kernel/genstg.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/genstg.c $(Release_Include_Path) -o Release/gccobj/kernel/genstg.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/genstg.c $(Release_Include_Path) > Release/gccobj/kernel/genstg.d

# Compiles file kernel/getentry.c for the Release configuration...
-include Release/gccobj/kernel/getentry.d
Release/gccobj/kernel/getentry.o: kernel/getentry.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/getentry.c $(Release_Include_Path) -o Release/gccobj/kernel/getentry.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/getentry.c $(Release_Include_Path) > Release/gccobj/kernel/getentry.d

# Compiles file kernel/monitr.c for the Release configuration...
-include Release/gccobj/kernel/monitr.d
Release/gccobj/kernel/monitr.o: kernel/monitr.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/monitr.c $(Release_Include_Path) -o Release/gccobj/kernel/monitr.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/monitr.c $(Release_Include_Path) > Release/gccobj/kernel/monitr.d

# Compiles file kernel/mova.c for the Release configuration...
-include Release/gccobj/kernel/mova.d
Release/gccobj/kernel/mova.o: kernel/mova.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/mova.c $(Release_Include_Path) -o Release/gccobj/kernel/mova.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/mova.c $(Release_Include_Path) > Release/gccobj/kernel/mova.d

# Compiles file kernel/output.c for the Release configuration...
-include Release/gccobj/kernel/output.d
Release/gccobj/kernel/output.o: kernel/output.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/output.c $(Release_Include_Path) -o Release/gccobj/kernel/output.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/output.c $(Release_Include_Path) > Release/gccobj/kernel/output.d

# Compiles file kernel/spec_cond.c for the Release configuration...
-include Release/gccobj/kernel/spec_cond.d
Release/gccobj/kernel/spec_cond.o: kernel/spec_cond.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/spec_cond.c $(Release_Include_Path) -o Release/gccobj/kernel/spec_cond.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/spec_cond.c $(Release_Include_Path) > Release/gccobj/kernel/spec_cond.d

# Compiles file kernel/write.c for the Release configuration...
-include Release/gccobj/kernel/write.d
Release/gccobj/kernel/write.o: kernel/write.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/write.c $(Release_Include_Path) -o Release/gccobj/kernel/write.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/write.c $(Release_Include_Path) > Release/gccobj/kernel/write.d

###########################################
### End of MOVA Kernel top level folder ###
#######################################################################################


#######################################################################################
### Start of Siemens Dummy Files folder ###
###########################################

# Compiles file SiemensDummyFiles/dummydata.c for the Release configuration...
-include Release/gccobj/SiemensDummyFiles/dummydata.d
Release/gccobj/SiemensDummyFiles/dummydata.o: SiemensDummyFiles/dummydata.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c SiemensDummyFiles/dummydata.c $(Release_Include_Path) -o Release/gccobj/SiemensDummyFiles/dummydata.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM SiemensDummyFiles/dummydata.c $(Release_Include_Path) > Release/gccobj/SiemensDummyFiles/dummydata.d

#########################################
### End of Siemens Dummy Files folder ###
#######################################################################################


#######################################################################################
### Start of MOVA top level folder ###
######################################

# Compiles file Messages.c for the Release configuration...
-include Release/gccobj/Messages.d
Release/gccobj/Messages.o: Messages.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c Messages.c $(Release_Include_Path) -o Release/gccobj/Messages.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM Messages.c $(Release_Include_Path) > Release/gccobj/Messages.d

# Compiles file pcclock.c for the Release configuration...
-include Release/gccobj/pcclock.d
Release/gccobj/pcclock.o: pcclock.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c pcclock.c $(Release_Include_Path) -o Release/gccobj/pcclock.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM pcclock.c $(Release_Include_Path) > Release/gccobj/pcclock.d

# Compiles file pccontio.c for the Release configuration...
-include Release/gccobj/pccontio.d
Release/gccobj/pccontio.o: pccontio.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c pccontio.c $(Release_Include_Path) -o Release/gccobj/pccontio.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM pccontio.c $(Release_Include_Path) > Release/gccobj/pccontio.d

# Compiles file pcdatset.c for the Release configuration...
-include Release/gccobj/pcdatset.d
Release/gccobj/pcdatset.o: pcdatset.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c pcdatset.c $(Release_Include_Path) -o Release/gccobj/pcdatset.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM pcdatset.c $(Release_Include_Path) > Release/gccobj/pcdatset.d

# Compiles file pcerror.c for the Release configuration...
-include Release/gccobj/pcerror.d
Release/gccobj/pcerror.o: pcerror.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c pcerror.c $(Release_Include_Path) -o Release/gccobj/pcerror.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM pcerror.c $(Release_Include_Path) > Release/gccobj/pcerror.d

# Compiles file pcioscan.c for the Release configuration...
-include Release/gccobj/pcioscan.d
Release/gccobj/pcioscan.o: pcioscan.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c pcioscan.c $(Release_Include_Path) -o Release/gccobj/pcioscan.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM pcioscan.c $(Release_Include_Path) > Release/gccobj/pcioscan.d

# Compiles file pckernel.c for the Release configuration...
-include Release/gccobj/pckernel.d
Release/gccobj/pckernel.o: pckernel.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c pckernel.c $(Release_Include_Path) -o Release/gccobj/pckernel.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM pckernel.c $(Release_Include_Path) > Release/gccobj/pckernel.d

# Compiles file PCMOVA.cpp for the Release configuration...
-include Release/gccobj/PCMOVA.d
Release/gccobj/PCMOVA.o: PCMOVA.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c PCMOVA.cpp $(Release_Include_Path) -o Release/gccobj/PCMOVA.o
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM PCMOVA.cpp $(Release_Include_Path) > Release/gccobj/PCMOVA.d

# Compiles file pctasks.c for the Release configuration...
-include Release/gccobj/pctasks.d
Release/gccobj/pctasks.o: pctasks.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c pctasks.c $(Release_Include_Path) -o Release/gccobj/pctasks.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM pctasks.c $(Release_Include_Path) > Release/gccobj/pctasks.d

# Compiles file pcuserio.c for the Release configuration...
-include Release/gccobj/pcuserio.d
Release/gccobj/pcuserio.o: pcuserio.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c pcuserio.c $(Release_Include_Path) -o Release/gccobj/pcuserio.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM pcuserio.c $(Release_Include_Path) > Release/gccobj/pcuserio.d

# Compiles file pcxmldataset.c for the Release configuration...
-include Release/gccobj/pcxmldataset.d
Release/gccobj/pcxmldataset.o: pcxmldataset.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c pcxmldataset.c $(Release_Include_Path) -o Release/gccobj/pcxmldataset.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM pcxmldataset.c $(Release_Include_Path) > Release/gccobj/pcxmldataset.d

# Compiles file stdafx.cpp for the Release configuration...
-include Release/gccobj/stdafx.d
Release/gccobj/stdafx.o: stdafx.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c stdafx.cpp $(Release_Include_Path) -o Release/gccobj/stdafx.o
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM stdafx.cpp $(Release_Include_Path) > Release/gccobj/stdafx.d

# Compiles file kernel/ui_dataaccess.c for the Release configuration...
-include Release/gccobj/kernel/ui_dataaccess.d
Release/gccobj/kernel/ui_dataaccess.o: kernel/ui_dataaccess.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c kernel/ui_dataaccess.c $(Release_Include_Path) -o Release/gccobj/kernel/ui_dataaccess.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM kernel/ui_dataaccess.c $(Release_Include_Path) > Release/gccobj/kernel/ui_dataaccess.d

####################################
### End of MOVA top level folder ###
#######################################################################################
    
    
# Creates the intermediate and output folders for each configuration...
.PHONY: create_folders
create_folders:
	mkdir -p Debug/gccobj
	mkdir -p Debug/gccobj/kernel
	mkdir -p Debug/gccobj/SiemensDummyFiles
	mkdir -p gccDebug
	mkdir -p Release/gccobj
	mkdir -p Release/gccobj/kernel
	mkdir -p Release/gccobj/SiemensDummyFiles
	mkdir -p gccRelease

# Cleans intermediate and output files (objects, libraries, executables)...
.PHONY: clean
clean:
	rm -f Debug/gccobj/*.o
	rm -f Debug/gccobj/*.d
	rm -f Debug/gccobj/kernel/*.o
	rm -f Debug/gccobj/kernel/*.d
	rm -f Debug/gccobj/SiemensDummyFiles/*.o
	rm -f Debug/gccobj/SiemensDummyFiles/*.d
	rm -f gccDebug/*.a
	rm -f gccDebug/*.so
	rm -f gccDebug/*.dll
	rm -f gccDebug/*.exe
	rm -f Release/gccobj/*.o
	rm -f Release/gccobj/*.d
	rm -f Release/gccobj/kernel/*.o
	rm -f Release/gccobj/kernel/*.d
	rm -f Release/gccobj/SiemensDummyFiles/*.o
	rm -f Release/gccobj/SiemensDummyFiles/*.d
	rm -f gccRelease/*.a
	rm -f gccRelease/*.so
	rm -f gccRelease/*.dll
	rm -f gccRelease/*.exe

