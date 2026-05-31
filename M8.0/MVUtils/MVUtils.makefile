# Compiler flags...
CPP_COMPILER = g++
C_COMPILER = gcc

# Include paths...
Debug_Include_Path=
Release_Include_Path=

# Library paths...
Debug_Library_Path=
Release_Library_Path=

# Additional libraries...
Debug_Libraries=
Release_Libraries=

# Preprocessor definitions...
Debug_Preprocessor_Definitions=-D GCC_BUILD -D _DEBUG -D _LIB -D _TRACE -D _CRT_SECURE_NO_DEPRECATE 
Release_Preprocessor_Definitions=-D GCC_BUILD -D NDEBUG -D _LIB -D _TRACE -D _CRT_SECURE_NO_DEPRECATE 

# Implictly linked object files...
Debug_Implicitly_Linked_Objects=
Release_Implicitly_Linked_Objects=

# Compiler flags...
Debug_Compiler_Flags=-O0 
Release_Compiler_Flags=-O2 

# Builds all configurations for this project...
.PHONY: build_all_configurations
build_all_configurations: Debug Release 

# Builds the Debug configuration...
.PHONY: Debug
Debug: create_folders gccDebug/CmdLine.o gccDebug/MVError.o gccDebug/MVLog.o gccDebug/MVTrace.o 
	ar rcs ../gccDebug/libMVUtils.a gccDebug/CmdLine.o gccDebug/MVError.o gccDebug/MVLog.o gccDebug/MVTrace.o  $(Debug_Implicitly_Linked_Objects)

# Compiles file CmdLine.cpp for the Debug configuration...
-include gccDebug/CmdLine.d
gccDebug/CmdLine.o: CmdLine.cpp
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c CmdLine.cpp $(Debug_Include_Path) -o gccDebug/CmdLine.o
	$(CPP_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM CmdLine.cpp $(Debug_Include_Path) > gccDebug/CmdLine.d

# Compiles file MVError.c for the Debug configuration...
-include gccDebug/MVError.d
gccDebug/MVError.o: MVError.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c MVError.c $(Debug_Include_Path) -o gccDebug/MVError.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM MVError.c $(Debug_Include_Path) > gccDebug/MVError.d

# Compiles file MVLog.c for the Debug configuration...
-include gccDebug/MVLog.d
gccDebug/MVLog.o: MVLog.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c MVLog.c $(Debug_Include_Path) -o gccDebug/MVLog.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM MVLog.c $(Debug_Include_Path) > gccDebug/MVLog.d

# Compiles file MVTrace.c for the Debug configuration...
-include gccDebug/MVTrace.d
gccDebug/MVTrace.o: MVTrace.c
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -c MVTrace.c $(Debug_Include_Path) -o gccDebug/MVTrace.o
	$(C_COMPILER) $(Debug_Preprocessor_Definitions) $(Debug_Compiler_Flags) -MM MVTrace.c $(Debug_Include_Path) > gccDebug/MVTrace.d

# Builds the Release configuration...
.PHONY: Release
Release: create_folders gccRelease/CmdLine.o gccRelease/MVError.o gccRelease/MVLog.o gccRelease/MVTrace.o 
	ar rcs ../gccRelease/libMVUtils.a gccRelease/CmdLine.o gccRelease/MVError.o gccRelease/MVLog.o gccRelease/MVTrace.o  $(Release_Implicitly_Linked_Objects)

# Compiles file CmdLine.cpp for the Release configuration...
-include gccRelease/CmdLine.d
gccRelease/CmdLine.o: CmdLine.cpp
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c CmdLine.cpp $(Release_Include_Path) -o gccRelease/CmdLine.o
	$(CPP_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM CmdLine.cpp $(Release_Include_Path) > gccRelease/CmdLine.d

# Compiles file MVError.c for the Release configuration...
-include gccRelease/MVError.d
gccRelease/MVError.o: MVError.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c MVError.c $(Release_Include_Path) -o gccRelease/MVError.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM MVError.c $(Release_Include_Path) > gccRelease/MVError.d

# Compiles file MVLog.c for the Release configuration...
-include gccRelease/MVLog.d
gccRelease/MVLog.o: MVLog.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c MVLog.c $(Release_Include_Path) -o gccRelease/MVLog.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM MVLog.c $(Release_Include_Path) > gccRelease/MVLog.d

# Compiles file MVTrace.c for the Release configuration...
-include gccRelease/MVTrace.d
gccRelease/MVTrace.o: MVTrace.c
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -c MVTrace.c $(Release_Include_Path) -o gccRelease/MVTrace.o
	$(C_COMPILER) $(Release_Preprocessor_Definitions) $(Release_Compiler_Flags) -MM MVTrace.c $(Release_Include_Path) > gccRelease/MVTrace.d

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

