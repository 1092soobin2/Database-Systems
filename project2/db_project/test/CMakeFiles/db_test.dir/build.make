# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.21

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/bin/cmake

# The command to remove a file.
RM = /usr/local/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project"

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project"

# Include any dependencies generated for this target.
include test/CMakeFiles/db_test.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include test/CMakeFiles/db_test.dir/compiler_depend.make

# Include the progress variables for this target.
include test/CMakeFiles/db_test.dir/progress.make

# Include the compile flags for this target's objects.
include test/CMakeFiles/db_test.dir/flags.make

test/CMakeFiles/db_test.dir/file_test.cc.o: test/CMakeFiles/db_test.dir/flags.make
test/CMakeFiles/db_test.dir/file_test.cc.o: test/file_test.cc
test/CMakeFiles/db_test.dir/file_test.cc.o: test/CMakeFiles/db_test.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir="/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object test/CMakeFiles/db_test.dir/file_test.cc.o"
	cd "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test" && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT test/CMakeFiles/db_test.dir/file_test.cc.o -MF CMakeFiles/db_test.dir/file_test.cc.o.d -o CMakeFiles/db_test.dir/file_test.cc.o -c "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test/file_test.cc"

test/CMakeFiles/db_test.dir/file_test.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/db_test.dir/file_test.cc.i"
	cd "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test" && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test/file_test.cc" > CMakeFiles/db_test.dir/file_test.cc.i

test/CMakeFiles/db_test.dir/file_test.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/db_test.dir/file_test.cc.s"
	cd "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test" && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test/file_test.cc" -o CMakeFiles/db_test.dir/file_test.cc.s

test/CMakeFiles/db_test.dir/basic_test.cc.o: test/CMakeFiles/db_test.dir/flags.make
test/CMakeFiles/db_test.dir/basic_test.cc.o: test/basic_test.cc
test/CMakeFiles/db_test.dir/basic_test.cc.o: test/CMakeFiles/db_test.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir="/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object test/CMakeFiles/db_test.dir/basic_test.cc.o"
	cd "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test" && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT test/CMakeFiles/db_test.dir/basic_test.cc.o -MF CMakeFiles/db_test.dir/basic_test.cc.o.d -o CMakeFiles/db_test.dir/basic_test.cc.o -c "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test/basic_test.cc"

test/CMakeFiles/db_test.dir/basic_test.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/db_test.dir/basic_test.cc.i"
	cd "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test" && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test/basic_test.cc" > CMakeFiles/db_test.dir/basic_test.cc.i

test/CMakeFiles/db_test.dir/basic_test.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/db_test.dir/basic_test.cc.s"
	cd "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test" && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test/basic_test.cc" -o CMakeFiles/db_test.dir/basic_test.cc.s

# Object files for target db_test
db_test_OBJECTS = \
"CMakeFiles/db_test.dir/file_test.cc.o" \
"CMakeFiles/db_test.dir/basic_test.cc.o"

# External object files for target db_test
db_test_EXTERNAL_OBJECTS =

bin/db_test: test/CMakeFiles/db_test.dir/file_test.cc.o
bin/db_test: test/CMakeFiles/db_test.dir/basic_test.cc.o
bin/db_test: test/CMakeFiles/db_test.dir/build.make
bin/db_test: lib/libdb.a
bin/db_test: lib/libgtest_main.a
bin/db_test: lib/libgtest.a
bin/db_test: test/CMakeFiles/db_test.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir="/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX executable ../bin/db_test"
	cd "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test" && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/db_test.dir/link.txt --verbose=$(VERBOSE)
	cd "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test" && /usr/local/bin/cmake -D TEST_TARGET=db_test -D "TEST_EXECUTABLE=/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/bin/db_test" -D TEST_EXECUTOR= -D "TEST_WORKING_DIR=/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test" -D TEST_EXTRA_ARGS= -D TEST_PROPERTIES= -D TEST_PREFIX= -D TEST_SUFFIX= -D NO_PRETTY_TYPES=FALSE -D NO_PRETTY_VALUES=FALSE -D TEST_LIST=db_test_TESTS -D "CTEST_FILE=/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test/db_test[1]_tests.cmake" -D TEST_DISCOVERY_TIMEOUT=5 -D TEST_XML_OUTPUT_DIR= -P /usr/local/share/cmake-3.21/Modules/GoogleTestAddTests.cmake

# Rule to build all files generated by this target.
test/CMakeFiles/db_test.dir/build: bin/db_test
.PHONY : test/CMakeFiles/db_test.dir/build

test/CMakeFiles/db_test.dir/clean:
	cd "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test" && $(CMAKE_COMMAND) -P CMakeFiles/db_test.dir/cmake_clean.cmake
.PHONY : test/CMakeFiles/db_test.dir/clean

test/CMakeFiles/db_test.dir/depend:
	cd "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project" && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project" "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test" "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project" "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test" "/media/psf/Home/Desktop/2021-2/Database_Systems(2)/Project/Project2_DiskSpaceManager/db_project/test/CMakeFiles/db_test.dir/DependInfo.cmake" --color=$(COLOR)
.PHONY : test/CMakeFiles/db_test.dir/depend

