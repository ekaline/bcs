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

# Utility rule file for doc.

# Include the progress variables for this target.
include doc/CMakeFiles/doc.dir/progress.make

doc/CMakeFiles/doc:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/vitaly/libeka_git/libeka/cereal/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Generating API documentation with Doxygen"
	/usr/bin/doxygen /home/vitaly/libeka_git/libeka/cereal/doc/doxygen.cfg

doc: doc/CMakeFiles/doc
doc: doc/CMakeFiles/doc.dir/build.make

.PHONY : doc

# Rule to build all files generated by this target.
doc/CMakeFiles/doc.dir/build: doc

.PHONY : doc/CMakeFiles/doc.dir/build

doc/CMakeFiles/doc.dir/clean:
	cd /home/vitaly/libeka_git/libeka/cereal/doc && $(CMAKE_COMMAND) -P CMakeFiles/doc.dir/cmake_clean.cmake
.PHONY : doc/CMakeFiles/doc.dir/clean

doc/CMakeFiles/doc.dir/depend:
	cd /home/vitaly/libeka_git/libeka/cereal && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vitaly/libeka_git/libeka/cereal /home/vitaly/libeka_git/libeka/cereal/doc /home/vitaly/libeka_git/libeka/cereal /home/vitaly/libeka_git/libeka/cereal/doc /home/vitaly/libeka_git/libeka/cereal/doc/CMakeFiles/doc.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : doc/CMakeFiles/doc.dir/depend

