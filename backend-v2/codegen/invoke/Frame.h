#ifndef FRAME_H
#define FRAME_H

#include "../../RuntimeHeaders.h"

namespace rt {
  struct Frame {
    // Frame linking might not be needed? - we keep this comment as a possible concept
    // Frame* callerFrame;    
    
    FunctionMethod *method;
    RTValue variadicSeq;         
    
    int bailoutEntryIndex;     
    int localsCount;           
    RTValue locals[];           
  };

} // namespace rt

#endif
