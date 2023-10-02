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

# Utility rule file for valgrind.

# Include the progress variables for this target.
include unittests/CMakeFiles/valgrind.dir/progress.make

unittests/CMakeFiles/valgrind:
	cd /home/vitaly/libeka_git/libeka/cereal/unittests && ./run_valgrind.sh

valgrind: unittests/CMakeFiles/valgrind
valgrind: unittests/CMakeFiles/valgrind.dir/build.make

.PHONY : valgrind

# Rule to build all files generated by this target.
unittests/CMakeFiles/valgrind.dir/build: valgrind

.PHONY : unittests/CMakeFiles/valgrind.dir/build

unittests/CMakeFiles/valgrind.dir/clean:
	cd /home/vitaly/libeka_git/libeka/cereal/unittests && $(CMAKE_COMMAND) -P CMakeFiles/valgrind.dir/cmake_clean.cmake
.PHONY : unittests/CMakeFiles/valgrind.dir/clean

unittests/CMakeFiles/valgrind.dir/depend:
	cd /home/vitaly/libeka_git/libeka/cereal && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vitaly/libeka_git/libeka/cereal /home/vitaly/libeka_git/libeka/cereal/unittests /home/vitaly/libeka_git/libeka/cereal /home/vitaly/libeka_git/libeka/cereal/unittests /home/vitaly/libeka_git/libeka/cereal/unittests/CMakeFiles/valgrind.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : unittests/CMakeFiles/valgrind.dir/depend

