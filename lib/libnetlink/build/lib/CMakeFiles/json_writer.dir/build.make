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
include lib/CMakeFiles/json_writer.dir/depend.make

# Include the progress variables for this target.
include lib/CMakeFiles/json_writer.dir/progress.make

# Include the compile flags for this target's objects.
include lib/CMakeFiles/json_writer.dir/flags.make

lib/CMakeFiles/json_writer.dir/json_writer.c.o: lib/CMakeFiles/json_writer.dir/flags.make
lib/CMakeFiles/json_writer.dir/json_writer.c.o: ../lib/json_writer.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/alexandru/Projects/EDGESec/lib/libnetlink/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object lib/CMakeFiles/json_writer.dir/json_writer.c.o"
	cd /home/alexandru/Projects/EDGESec/lib/libnetlink/build/lib && /usr/bin/gcc-9 $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/json_writer.dir/json_writer.c.o   -c /home/alexandru/Projects/EDGESec/lib/libnetlink/lib/json_writer.c

lib/CMakeFiles/json_writer.dir/json_writer.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/json_writer.dir/json_writer.c.i"
	cd /home/alexandru/Projects/EDGESec/lib/libnetlink/build/lib && /usr/bin/gcc-9 $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/alexandru/Projects/EDGESec/lib/libnetlink/lib/json_writer.c > CMakeFiles/json_writer.dir/json_writer.c.i

lib/CMakeFiles/json_writer.dir/json_writer.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/json_writer.dir/json_writer.c.s"
	cd /home/alexandru/Projects/EDGESec/lib/libnetlink/build/lib && /usr/bin/gcc-9 $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/alexandru/Projects/EDGESec/lib/libnetlink/lib/json_writer.c -o CMakeFiles/json_writer.dir/json_writer.c.s

# Object files for target json_writer
json_writer_OBJECTS = \
"CMakeFiles/json_writer.dir/json_writer.c.o"

# External object files for target json_writer
json_writer_EXTERNAL_OBJECTS =

lib/libjson_writer.a: lib/CMakeFiles/json_writer.dir/json_writer.c.o
lib/libjson_writer.a: lib/CMakeFiles/json_writer.dir/build.make
lib/libjson_writer.a: lib/CMakeFiles/json_writer.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/alexandru/Projects/EDGESec/lib/libnetlink/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C static library libjson_writer.a"
	cd /home/alexandru/Projects/EDGESec/lib/libnetlink/build/lib && $(CMAKE_COMMAND) -P CMakeFiles/json_writer.dir/cmake_clean_target.cmake
	cd /home/alexandru/Projects/EDGESec/lib/libnetlink/build/lib && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/json_writer.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
lib/CMakeFiles/json_writer.dir/build: lib/libjson_writer.a

.PHONY : lib/CMakeFiles/json_writer.dir/build

lib/CMakeFiles/json_writer.dir/clean:
	cd /home/alexandru/Projects/EDGESec/lib/libnetlink/build/lib && $(CMAKE_COMMAND) -P CMakeFiles/json_writer.dir/cmake_clean.cmake
.PHONY : lib/CMakeFiles/json_writer.dir/clean

lib/CMakeFiles/json_writer.dir/depend:
	cd /home/alexandru/Projects/EDGESec/lib/libnetlink/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/alexandru/Projects/EDGESec/lib/libnetlink /home/alexandru/Projects/EDGESec/lib/libnetlink/lib /home/alexandru/Projects/EDGESec/lib/libnetlink/build /home/alexandru/Projects/EDGESec/lib/libnetlink/build/lib /home/alexandru/Projects/EDGESec/lib/libnetlink/build/lib/CMakeFiles/json_writer.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : lib/CMakeFiles/json_writer.dir/depend

