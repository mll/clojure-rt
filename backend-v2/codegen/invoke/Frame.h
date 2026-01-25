#ifndef FRAME_H
#define FRAME_H

#include "../../RuntimeHeaders.h"

namespace rt {
  struct Frame {
    struct Frame* leafFrame;    
    
    FunctionMethod *method;
    RTValue variadicSeq;         
    
    int bailoutEntryIndex;     
    int localsCount;           
    RTValue locals[];           
  };

} // namespace rt

#endif
