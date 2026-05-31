# Compiler flags...
CPP_COMPILER = g++
C_COMPILER = gcc

# Include paths...
Debug_Include_Path=-I"../MVUtils" 
Release_Include_Path=-I"../MVUtils" 

# Library paths...
Debug_Library_Path=
Release_Library_Path=

# Additional libraries...
Debug_Libraries=-Wl,--start-group -lws2_32 -lMVUtils  -Wl,--end-group
Release_Libraries=-Wl,--start-group -lws2_32 -lMVUtils  -Wl,--end-group

# Preprocessor definitions...
Debug_Preprocessor_Definitions=-D GCC_BUILD -D _DEBUG -D _USRDLL -D MVCOMMS_EXPORTS -D _TRACE -D _CRT_SECURE_NO_DEPRECATE 
Release_Preprocessor_Definitions=-D GCC_BUILD -D NDEBUG -D _USRDLL -D MVCOMMS_EXPORTS -D _TRACE -D _CRT_SECURE_NO_DEPRECATE 

# Implictly linked object files...
Debug_Implicitly_Linked_Objects=
Release_Implicitly_Linked_Objects=

# Compiler flags...
Debug_Compiler_Flags=-fPIC -O0 -g 
Release_Compiler_Flags=-fPIC -O2 

# Builds all configurations for this project...
.PHONY: build_all_configurations
build_all_configurations: Debug Release 

# Builds the Debug configuration...
.PHONY: Debug
Debug: create_folders gccDebug/clicomms.o gccDebug/CommsErr.o gccDebug/MVComms.o gccDebug/Serial.o gccDebug/stdafx.o gccDebug/TCPClient.o gccDebug/TCPCommon.o gccDebug/TCPServer.o 
	g++ -fPIC -shared -Wl,-soname,libMVComms.so -o ../gccDebug/libMVComms.so gccDebug/clicomms.o gccDebug/CommsErr.o gccDebug/MVComms.o gccDebug/Serial.o gccDebug/stdafx.o gccDebug/TCPClient.o gccDebug/TCPCommon.o gccDebug/TCPServer.o  $(Debug_Implicitly_Linked_Objects)

# Compiles file clicomms.c for the Debug configuration...
-include gccDebug/clicomms.d
gccDebug/clicomms.o: clicomms.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c clicomms.c $(Debug_Include_Path) -o gccDebug/clicomms.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM clicomms.c $(Debug_Include_Path) > gccDebug/clicomms.d

# Compiles file CommsErr.c for the Debug configuration...
-include gccDebug/CommsErr.d
gccDebug/CommsErr.o: CommsErr.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c CommsErr.c $(Debug_Include_Path) -o gccDebug/CommsErr.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM CommsErr.c $(Debug_Include_Path) > gccDebug/CommsErr.d

# Compiles file MVComms.c for the Debug configuration...
-include gccDebug/MVComms.d
gccDebug/MVComms.o: MVComms.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c MVComms.c $(Debug_Include_Path) -o gccDebug/MVComms.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM MVComms.c $(Debug_Include_Path) > gccDebug/MVComms.d

# Compiles file Serial.c for the Debug configuration...
-include gccDebug/Serial.d
gccDebug/Serial.o: Serial.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c Serial.c $(Debug_Include_Path) -o gccDebug/Serial.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM Serial.c $(Debug_Include_Path) > gccDebug/Serial.d

# Compiles file stdafx.c for the Debug configuration...
-include gccDebug/stdafx.d
gccDebug/stdafx.o: stdafx.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c stdafx.c $(Debug_Include_Path) -o gccDebug/stdafx.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM stdafx.c $(Debug_Include_Path) > gccDebug/stdafx.d

# Compiles file TCPClient.c for the Debug configuration...
-include gccDebug/TCPClient.d
gccDebug/TCPClient.o: TCPClient.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c TCPClient.c $(Debug_Include_Path) -o gccDebug/TCPClient.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM TCPClient.c $(Debug_Include_Path) > gccDebug/TCPClient.d

# Compiles file TCPCommon.c for the Debug configuration...
-include gccDebug/TCPCommon.d
gccDebug/TCPCommon.o: TCPCommon.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c TCPCommon.c $(Debug_Include_Path) -o gccDebug/TCPCommon.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM TCPCommon.c $(Debug_Include_Path) > gccDebug/TCPCommon.d

# Compiles file TCPServer.c for the Debug configuration...
-include gccDebug/TCPServer.d
gccDebug/TCPServer.o: TCPServer.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c TCPServer.c $(Debug_Include_Path) -o gccDebug/TCPServer.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM TCPServer.c $(Debug_Include_Path) > gccDebug/TCPServer.d

# Builds the Release configuration...
.PHONY: Release
Release: create_folders gccRelease/clicomms.o gccRelease/CommsErr.o gccRelease/MVComms.o gccRelease/Serial.o gccRelease/stdafx.o gccRelease/TCPClient.o gccRelease/TCPCommon.o gccRelease/TCPServer.o 
	g++ -fPIC -shared -Wl,-soname,libMVComms.so -o ../gccRelease/libMVComms.so gccRelease/clicomms.o gccRelease/CommsErr.o gccRelease/MVComms.o gccRelease/Serial.o gccRelease/stdafx.o gccRelease/TCPClient.o gccRelease/TCPCommon.o gccRelease/TCPServer.o  $(Release_Implicitly_Linked_Objects)

# Compiles file clicomms.c for the Release configuration...
-include gccRelease/clicomms.d
gccRelease/clicomms.o: clicomms.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c clicomms.c $(Release_Include_Path) -o gccRelease/clicomms.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM clicomms.c $(Release_Include_Path) > gccRelease/clicomms.d

# Compiles file CommsErr.c for the Release configuration...
-include gccRelease/CommsErr.d
gccRelease/CommsErr.o: CommsErr.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c CommsErr.c $(Release_Include_Path) -o gccRelease/CommsErr.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM CommsErr.c $(Release_Include_Path) > gccRelease/CommsErr.d

# Compiles file MVComms.c for the Release configuration...
-include gccRelease/MVComms.d
gccRelease/MVComms.o: MVComms.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c MVComms.c $(Release_Include_Path) -o gccRelease/MVComms.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM MVComms.c $(Release_Include_Path) > gccRelease/MVComms.d

# Compiles file Serial.c for the Release configuration...
-include gccRelease/Serial.d
gccRelease/Serial.o: Serial.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c Serial.c $(Release_Include_Path) -o gccRelease/Serial.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM Serial.c $(Release_Include_Path) > gccRelease/Serial.d

# Compiles file stdafx.c for the Release configuration...
-include gccRelease/stdafx.d
gccRelease/stdafx.o: stdafx.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c stdafx.c $(Release_Include_Path) -o gccRelease/stdafx.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM stdafx.c $(Release_Include_Path) > gccRelease/stdafx.d

# Compiles file TCPClient.c for the Release configuration...
-include gccRelease/TCPClient.d
gccRelease/TCPClient.o: TCPClient.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c TCPClient.c $(Release_Include_Path) -o gccRelease/TCPClient.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM TCPClient.c $(Release_Include_Path) > gccRelease/TCPClient.d

# Compiles file TCPCommon.c for the Release configuration...
-include gccRelease/TCPCommon.d
gccRelease/TCPCommon.o: TCPCommon.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c TCPCommon.c $(Release_Include_Path) -o gccRelease/TCPCommon.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM TCPCommon.c $(Release_Include_Path) > gccRelease/TCPCommon.d

# Compiles file TCPServer.c for the Release configuration...
-include gccRelease/TCPServer.d
gccRelease/TCPServer.o: TCPServer.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c TCPServer.c $(Release_Include_Path) -o gccRelease/TCPServer.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM TCPServer.c $(Release_Include_Path) > gccRelease/TCPServer.d

# Creates the intermediate and output folders for each configuration...
.PHONY: create_folders
create_folders:
	mkdir -p gccDebug
	mkdir -p ../gccDebug
	mkdir -p gccRelease
	mkdir -p ../gccRelease

# Cleans intermediate and output files (objects, libraries, executables)...
.PHONY: clean
clean:
	rm -f gccDebug/*.o
	rm -f gccDebug/*.d
	rm -f ../gccDebug/*.a
	rm -f ../gccDebug/*.so
	rm -f ../gccDebug/*.dll
	rm -f ../gccDebug/*.exe
	rm -f gccRelease/*.o
	rm -f gccRelease/*.d
	rm -f ../gccRelease/*.a
	rm -f ../gccRelease/*.so
	rm -f ../gccRelease/*.dll
	rm -f ../gccRelease/*.exe

