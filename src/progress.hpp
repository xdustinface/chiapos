#ifndef SRC_CPP_PROGRESS_HPP_
#define SRC_CPP_PROGRESS_HPP_

#include <util.hpp>

typedef std::function<void(int,int,int)> ProgressCallbackFunc;

static ProgressCallbackFunc progressCallbackDefault{[](int phase, int n, int max_n){
    float p = (100.0 / 4) * ((phase - 1.0) + (1.0 * n / max_n));
    Util::Log("Progress: %s%%\n", static_cast<int64_t>(p));
}};
static ProgressCallbackFunc progressCallbackNone{[](int, int, int){}};

#endif  // SRC_CPP_PROGRESS_HPP
