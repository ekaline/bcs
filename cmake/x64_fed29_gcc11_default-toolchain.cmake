# Use gcc11/g++11
set(CMAKE_C_COMPILER gcc11)
set(CMAKE_CXX_COMPILER g++11)

# Add the runtime support for g++11 to DT_RUNPATH, it's not part of the
# normal system.
set(AVT_RUNTIME_CXXRT_RPATH "/srv/opt/gcc/gcc-11.2.0/lib64" CACHE PATH
    "Location of the gcc11 runtime, to be included in the DT_RUNPATH")

# Create an interface library called `static_cxx_runtime_noexcept`. Linking
# with this library will cause a C++ library to statically link the C++
# runtime and disable exceptions, thereby "hermetically sealing" it's C++
# nature from the dynmaic linker. This is behind an if statement because
# CMake tries to create it multiple times when present in the toolchain file.
if (NOT TARGET static_cxx_runtime_noexcept)
  add_library(static_cxx_runtime_noexcept INTERFACE)
  target_link_options(static_cxx_runtime_noexcept INTERFACE -static-libstdc++ -static-libgcc)
  target_compile_options(static_cxx_runtime_noexcept INTERFACE -fno-exceptions)
endif()
