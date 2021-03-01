# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/alexandru/Projects/EDGESec/lib/libnetlink

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/alexandru/Projects/EDGESec/lib/libnetlink/build

# Include any dependencies generated for this target.
include lib/CMakeFiles/ll_map.dir/depend.make

# Include the progress variables for this target.
include lib/CMakeFiles/ll_map.dir/progress.make

# Include the compile flags for this target's objects.
include lib/CMakeFiles/ll_map.dir/flags.make

lib/CMakeFiles/ll_map.dir/ll_map.c.o: lib/CMakeFiles/ll_map.dir/flags.make
lib/CMakeFiles/ll_map.dir/ll_map.c.o: ../lib/ll_map.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/alexandru/Projects/EDGESec/lib/libnetlink/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object lib/CMakeFiles/ll_map.dir/ll_map.c.o"
	cd /home/alexandru/Projects/EDGESec/lib/libnetlink/build/lib && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/ll_map.dir/ll_map.c.o   -c /home/alexandru/Projects/EDGESec/lib/libnetlink/lib/ll_map.c

lib/CMakeFiles/ll_map.dir/ll_map.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/ll_map.dir/ll_map.c.i"
	cd /home/alexandru/Projects/EDGESec/lib/libnetlink/build/lib && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/alexandru/Projects/EDGESec/lib/libnetlink/lib/ll_map.c > CMakeFiles/ll_map.dir/ll_map.c.i

lib/CMakeFiles/ll_map.dir/ll_map.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/ll_map.dir/ll_map.c.s"
	cd /home/alexandru/Projects/EDGESec/lib/libnetlink/build/lib && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/alexandru/Projects/EDGESec/lib/libnetlink/lib/ll_map.c -o CMakeFiles/ll_map.dir/ll_map.c.s

# Object files for target ll_map
ll_map_OBJECTS = \
"CMakeFiles/ll_map.dir/ll_map.c.o"

# External object files for target ll_map
ll_map_EXTERNAL_OBJECTS =

lib/libll_map.so: lib/CMakeFiles/ll_map.dir/ll_map.c.o
lib/libll_map.so: lib/CMakeFiles/ll_map.dir/build.make
lib/libll_map.so: lib/libutils.so
lib/libll_map.so: lib/liblibnetlink.so
lib/libll_map.so: lib/libjson_print.a
lib/libll_map.so: lib/libjson_writer.a
lib/libll_map.so: lib/libcolor.a
lib/libll_map.so: lib/libmpls_ntop.a
lib/libll_map.so: lib/libll_addr.a
lib/libll_map.so: lib/libmpls_pton.a
lib/libll_map.so: /home/alexandru/Projects/EDGESec/lib/libmnl-1.0.4/build/lib/libmnl.so
lib/libll_map.so: lib/CMakeFiles/ll_map.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/alexandru/Projects/EDGESec/lib/libnetlink/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C shared library libll_map.so"
	cd /home/alexandru/Projects/EDGESec/lib/libnetlink/build/lib && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/ll_map.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
lib/CMakeFiles/ll_map.dir/build: lib/libll_map.so

.PHONY : lib/CMakeFiles/ll_map.dir/build

lib/CMakeFiles/ll_map.dir/clean:
	cd /home/alexandru/Projects/EDGESec/lib/libnetlink/build/lib && $(CMAKE_COMMAND) -P CMakeFiles/ll_map.dir/cmake_clean.cmake
.PHONY : lib/CMakeFiles/ll_map.dir/clean

lib/CMakeFiles/ll_map.dir/depend:
	cd /home/alexandru/Projects/EDGESec/lib/libnetlink/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/alexandru/Projects/EDGESec/lib/libnetlink /home/alexandru/Projects/EDGESec/lib/libnetlink/lib /home/alexandru/Projects/EDGESec/lib/libnetlink/build /home/alexandru/Projects/EDGESec/lib/libnetlink/build/lib /home/alexandru/Projects/EDGESec/lib/libnetlink/build/lib/CMakeFiles/ll_map.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : lib/CMakeFiles/ll_map.dir/depend

