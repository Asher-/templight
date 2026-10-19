# A test plugin whose pragma ends the compilation in SIGILL. It links nothing:
# loaded into templight, it uses templight's own clang symbols.
set(LLVM_LINK_COMPONENTS)

if(NOT WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|x86_64|AMD64)$")
  add_llvm_library(TemplightIllegalInstruction MODULE IllegalInstruction.cpp
    PLUGIN_TOOL templight)
endif()
