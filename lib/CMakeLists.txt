include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../utils/ExtraWriters)

add_clang_library(clangTemplight
  TemplightAction.cpp
  TemplightDebugger.cpp
  TemplightEntryPrinter.cpp
  TemplightProtobufWriter.cpp
  TemplightTracer.cpp
  ../utils/ExtraWriters/TemplightExtraWriters.cpp

  LINK_LIBS
  clangAST
  clangBasic
  clangFrontend
  clangLex
  clangSema
  clangSerialization
  )
