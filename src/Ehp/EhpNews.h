#ifndef _EHP_NEWS_H_
#define _EHP_NEWS_H_

#include "EhpProtocol.h"

enum class EhpNewsMsg : int { News = 0 };

class EhpNews : public EhpProtocol {
public:
  EhpNews(EkaStrategy *strat);
  virtual ~EhpNews() {}

  int init();

  int createNews();

public:
  static const int NewsMsg = 0;
};

#endif
