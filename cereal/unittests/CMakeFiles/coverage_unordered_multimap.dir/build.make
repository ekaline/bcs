# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.17

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
CMAKE_COMMAND = /usr/bin/cmake3

# The command to remove a file.
RM = /usr/bin/cmake3 -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/vitaly/libeka_git/libeka/cereal

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/vitaly/libeka_git/libeka/cereal

# Include any dependencies generated for this target.
include unittests/CMakeFiles/coverage_unordered_multimap.dir/depend.make

# Include the progress variables for this target.
include unittests/CMakeFiles/coverage_unordered_multimap.dir/progress.make

# Include the compile flags for this target's objects.
include unittests/CMakeFiles/coverage_unordered_multimap.dir/flags.make

unittests/CMakeFiles/coverage_unordered_multimap.dir/unordered_multimap.cpp.o: unittests/CMakeFiles/coverage_unordered_multimap.dir/flags.make
unittests/CMakeFiles/coverage_unordered_multimap.dir/unordered_multimap.cpp.o: unittests/unordered_multimap.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/vitaly/libeka_git/libeka/cereal/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object unittests/CMakeFiles/coverage_unordered_multimap.dir/unordered_multimap.cpp.o"
	cd /home/vitaly/libeka_git/libeka/cereal/unittests && /usr/lib64/ccache/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/coverage_unordered_multimap.dir/unordered_multimap.cpp.o -c /home/vitaly/libeka_git/libeka/cereal/unittests/unordered_multimap.cpp

unittests/CMakeFiles/coverage_unordered_multimap.dir/unordered_multimap.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/coverage_unordered_multimap.dir/unordered_multimap.cpp.i"
	cd /home/vitaly/libeka_git/libeka/cereal/unittests && /usr/lib64/ccache/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/vitaly/libeka_git/libeka/cereal/unittests/unordered_multimap.cpp > CMakeFiles/coverage_unordered_multimap.dir/unordered_multimap.cpp.i

unittests/CMakeFiles/coverage_unordered_multimap.dir/unordered_multimap.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/coverage_unordered_multimap.dir/unordered_multimap.cpp.s"
	cd /home/vitaly/libeka_git/libeka/cereal/unittests && /usr/lib64/ccache/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/vitaly/libeka_git/libeka/cereal/unittests/unordered_multimap.cpp -o CMakeFiles/coverage_unordered_multimap.dir/unordered_multimap.cpp.s

# Object files for target coverage_unordered_multimap
coverage_unordered_multimap_OBJECTS = \
"CMakeFiles/coverage_unordered_multimap.dir/unordered_multimap.cpp.o"

# External object files for target coverage_unordered_multimap
coverage_unordered_multimap_EXTERNAL_OBJECTS =

coverage/coverage_unordered_multimap: unittests/CMakeFiles/coverage_unordered_multimap.dir/unordered_multimap.cpp.o
coverage/coverage_unordered_multimap: unittests/CMakeFiles/coverage_unordered_multimap.dir/build.make
coverage/coverage_unordered_multimap: unittests/CMakeFiles/coverage_unordered_multimap.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/vitaly/libeka_git/libeka/cereal/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable ../coverage/coverage_unordered_multimap"
	cd /home/vitaly/libeka_git/libeka/cereal/unittests && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/coverage_unordered_multimap.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
unittests/CMakeFiles/coverage_unordered_multimap.dir/build: coverage/coverage_unordered_multimap

.PHONY : unittests/CMakeFiles/coverage_unordered_multimap.dir/build

unittests/CMakeFiles/coverage_unordered_multimap.dir/clean:
	cd /home/vitaly/libeka_git/libeka/cereal/unittests && $(CMAKE_COMMAND) -P CMakeFiles/coverage_unordered_multimap.dir/cmake_clean.cmake
.PHONY : unittests/CMakeFiles/coverage_unordered_multimap.dir/clean

unittests/CMakeFiles/coverage_unordered_multimap.dir/depend:
	cd /home/vitaly/libeka_git/libeka/cereal && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vitaly/libeka_git/libeka/cereal /home/vitaly/libeka_git/libeka/cereal/unittests /home/vitaly/libeka_git/libeka/cereal /home/vitaly/libeka_git/libeka/cereal/unittests /home/vitaly/libeka_git/libeka/cereal/unittests/CMakeFiles/coverage_unordered_multimap.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : unittests/CMakeFiles/coverage_unordered_multimap.dir/depend

