set(LLVM_LINK_COMPONENTS
  Support
  )

# The lit tests read the protobuf traces templight writes with this reader.
add_llvm_utility(templight-read-trace
  templight-read-trace.cpp
  TemplightProtobufReader.cpp
  )
