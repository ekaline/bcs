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
include unittests/CMakeFiles/test_portable_binary_archive.dir/depend.make

# Include the progress variables for this target.
include unittests/CMakeFiles/test_portable_binary_archive.dir/progress.make

# Include the compile flags for this target's objects.
include unittests/CMakeFiles/test_portable_binary_archive.dir/flags.make

unittests/CMakeFiles/test_portable_binary_archive.dir/portable_binary_archive.cpp.o: unittests/CMakeFiles/test_portable_binary_archive.dir/flags.make
unittests/CMakeFiles/test_portable_binary_archive.dir/portable_binary_archive.cpp.o: unittests/portable_binary_archive.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/vitaly/libeka_git/libeka/cereal/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object unittests/CMakeFiles/test_portable_binary_archive.dir/portable_binary_archive.cpp.o"
	cd /home/vitaly/libeka_git/libeka/cereal/unittests && /usr/lib64/ccache/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/test_portable_binary_archive.dir/portable_binary_archive.cpp.o -c /home/vitaly/libeka_git/libeka/cereal/unittests/portable_binary_archive.cpp

unittests/CMakeFiles/test_portable_binary_archive.dir/portable_binary_archive.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/test_portable_binary_archive.dir/portable_binary_archive.cpp.i"
	cd /home/vitaly/libeka_git/libeka/cereal/unittests && /usr/lib64/ccache/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/vitaly/libeka_git/libeka/cereal/unittests/portable_binary_archive.cpp > CMakeFiles/test_portable_binary_archive.dir/portable_binary_archive.cpp.i

unittests/CMakeFiles/test_portable_binary_archive.dir/portable_binary_archive.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/test_portable_binary_archive.dir/portable_binary_archive.cpp.s"
	cd /home/vitaly/libeka_git/libeka/cereal/unittests && /usr/lib64/ccache/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/vitaly/libeka_git/libeka/cereal/unittests/portable_binary_archive.cpp -o CMakeFiles/test_portable_binary_archive.dir/portable_binary_archive.cpp.s

# Object files for target test_portable_binary_archive
test_portable_binary_archive_OBJECTS = \
"CMakeFiles/test_portable_binary_archive.dir/portable_binary_archive.cpp.o"

# External object files for target test_portable_binary_archive
test_portable_binary_archive_EXTERNAL_OBJECTS =

unittests/test_portable_binary_archive: unittests/CMakeFiles/test_portable_binary_archive.dir/portable_binary_archive.cpp.o
unittests/test_portable_binary_archive: unittests/CMakeFiles/test_portable_binary_archive.dir/build.make
unittests/test_portable_binary_archive: unittests/CMakeFiles/test_portable_binary_archive.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/vitaly/libeka_git/libeka/cereal/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable test_portable_binary_archive"
	cd /home/vitaly/libeka_git/libeka/cereal/unittests && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/test_portable_binary_archive.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
unittests/CMakeFiles/test_portable_binary_archive.dir/build: unittests/test_portable_binary_archive

.PHONY : unittests/CMakeFiles/test_portable_binary_archive.dir/build

unittests/CMakeFiles/test_portable_binary_archive.dir/clean:
	cd /home/vitaly/libeka_git/libeka/cereal/unittests && $(CMAKE_COMMAND) -P CMakeFiles/test_portable_binary_archive.dir/cmake_clean.cmake
.PHONY : unittests/CMakeFiles/test_portable_binary_archive.dir/clean

unittests/CMakeFiles/test_portable_binary_archive.dir/depend:
	cd /home/vitaly/libeka_git/libeka/cereal && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vitaly/libeka_git/libeka/cereal /home/vitaly/libeka_git/libeka/cereal/unittests /home/vitaly/libeka_git/libeka/cereal /home/vitaly/libeka_git/libeka/cereal/unittests /home/vitaly/libeka_git/libeka/cereal/unittests/CMakeFiles/test_portable_binary_archive.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : unittests/CMakeFiles/test_portable_binary_archive.dir/depend

