// 파일 헤더
#include "Numeric.hpp"

namespace backtesting::numeric {

template class Numeric<float>;
template class Numeric<double>;
template class Numeric<long double>;
template class Numeric<int>;
template class Numeric<long>;
template class Numeric<long long>;

}  // namespace backtesting::numeric