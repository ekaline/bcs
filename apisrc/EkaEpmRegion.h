#ifndef _EKA_EPM_REGION_H_
#define _EKA_EPM_REGION_H_



class EkaEpmRegion {
 public:
  EkaEpmRegion(uint _id, uint _baseActionIdx) {
    id            = _id;
    baseActionIdx = _baseActionIdx;
    currActionIdx = 0;
  }

  uint getNextLocalActionIdx() {
    return currActionIdx++;
  }

  uint id = -1;
  
  uint baseActionIdx = -1;
  uint currActionIdx = 0;
};

#endif
