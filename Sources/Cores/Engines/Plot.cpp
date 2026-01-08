// 표준 라이브러리
#include <format>
#include <iomanip>
#include <sstream>

// 파일 헤더
#include "Engines/Plot.hpp"

// 내부 헤더
#include "Engines/Logger.hpp"

// 네임 스페이스
using namespace backtesting::logger;

namespace backtesting::plot {

unsigned char Rgba::GetRed() const { return rgba_red; }
unsigned char Rgba::GetGreen() const { return rgba_green; }
unsigned char Rgba::GetBlue() const { return rgba_blue; }
float Rgba::GetAlpha() const { return rgba_alpha; }

string Rgba::RgbaToHex() const {
  ostringstream oss;
  oss << '#' << uppercase << hex << setfill('0') << setw(2)
      << static_cast<unsigned>(rgba_red) << setw(2)
      << static_cast<unsigned>(rgba_green) << setw(2)
      << static_cast<unsigned>(rgba_blue)
      // alpha: 0.0 ~ 1.0 → 0 ~ 255 변환
      << setw(2) << static_cast<unsigned>(round(rgba_alpha * 255.0));
  return oss.str();
}

BACKTESTING_API const Rgba Rgba::red = Rgba(255, 82, 82, 1);       // 빨강
BACKTESTING_API const Rgba Rgba::orange = Rgba(255, 152, 0, 1);    // 주황
BACKTESTING_API const Rgba Rgba::yellow = Rgba(255, 245, 157, 1);  // 노랑
BACKTESTING_API const Rgba Rgba::green = Rgba(76, 175, 80, 1);     // 초록
BACKTESTING_API const Rgba Rgba::cyan = Rgba(0, 188, 212, 1);      // 청록
BACKTESTING_API const Rgba Rgba::blue = Rgba(41, 98, 255, 1);      // 파랑
BACKTESTING_API const Rgba Rgba::purple = Rgba(103, 58, 183, 1);   // 보라
BACKTESTING_API const Rgba Rgba::pink = Rgba(156, 39, 176, 1);     // 검정
BACKTESTING_API const Rgba Rgba::gray = Rgba(120, 123, 134, 1);    // 검정
BACKTESTING_API const Rgba Rgba::black = Rgba(0, 0, 0, 1);         // 검정
BACKTESTING_API const Rgba Rgba::white = Rgba(255, 255, 255, 1);   // 흰색

void Rgba::IsValidRgb(const string& color_name, const int value) {
  if (value < 0 || value > 255) {
    Logger::LogAndThrowError(format("지정된 {} 값 [{}]은(는) RGB 범위 "
                                    "[0 - 255] 사이로 지정해야 합니다.",
                                    color_name, to_string(value)),
                             __FILE__, __LINE__);
  }
}

void Rgba::IsValidAlpha(const float value) {
  if (value < 0 || value > 1) {
    Logger::LogAndThrowError(format("지정된 Alpha 값 [{}]은(는) "
                                    "[0 - 1] 사이로 지정해야 합니다.",
                                    to_string(value)),
                             __FILE__, __LINE__);
  }
}

Plot::Plot(const char line_width, const LineStyle line_style,
           const LineType line_type, const bool plot_point_markers,
           const char point_markers_radius, const bool overlay,
           const string& pane_name, const Format format,
           const optional<int> precision)
    : line_width_(line_width),
      line_style_(line_style),
      line_type_(line_type),
      plot_point_markers_(plot_point_markers),
      point_markers_radius_(point_markers_radius),
      overlay_(overlay),
      pane_name_(pane_name),
      format_(format),
      precision_(precision) {
  if (line_width < 1 || line_width > 4) {
    Logger::LogAndThrowError(std::format("주어진 플롯의 선 굵기 [{}]은(는) 1 "
                                         "이상, 4 이하로 설정해야 합니다.",
                                         to_string(line_width)),
                             __FILE__, __LINE__);
  }

  if (plot_point_markers &&
      (point_markers_radius < 1 || point_markers_radius > 4)) {
    Logger::LogAndThrowError(
        std::format("주어진 플롯의 포인트 마커의 픽셀 [{}]은(는) 1 이상, "
                    "4 이하로 설정해야 합니다.",
                    to_string(point_markers_radius)),
        __FILE__, __LINE__);
  }

  if (!overlay && pane_name.empty()) {
    Logger::LogAndThrowError(
        "주어진 플롯이 오버레이 하지 않는다면 반드시 페인 이름을 가져야 "
        "합니다.",
        __FILE__, __LINE__);
  }

  if (precision) {
    if (const auto precision_val = *precision;
        precision_val < 0 || precision_val > 15)
      Logger::LogAndThrowError(
          std::format("주어진 플롯의 소수점 정밀도 [{}]은(는) 0 이상, "
                      "15 이하로 설정해야 합니다.",
                      to_string(precision_val)),
          __FILE__, __LINE__);
  }
}

BACKTESTING_API shared_ptr<Logger>& Plot::logger_ = Logger::GetLogger();

Area::Area(const Rgba& top_gradient_color, const Rgba& bottom_gradient_color,
           const Rgba& line_color, const char line_width,
           const LineStyle line_style, const LineType line_type,
           const bool plot_point_markers, const char point_markers_radius,
           const bool overlay, const string& pane_name, const Format format,
           const optional<int> precision)
    : Plot(line_width, line_style, line_type, plot_point_markers,
           point_markers_radius, overlay, pane_name, format, precision),
      top_gradient_color_(top_gradient_color),
      bottom_gradient_color_(bottom_gradient_color),
      line_color_(line_color) {}

shared_ptr<Plot> Area::MakeShared() const { return make_shared<Area>(*this); }

Baseline::Baseline(const double base_value, const Rgba& top_line_color,
                   const Rgba& top_gradient_color1,
                   const Rgba& top_gradient_color2,
                   const Rgba& bottom_line_color,
                   const Rgba& bottom_gradient_color1,
                   const Rgba& bottom_gradient_color2, const char line_width,
                   const LineStyle line_style, const LineType line_type,
                   const bool plot_point_markers,
                   const char point_markers_radius, const bool overlay,
                   const string& pane_name, const Format format,
                   const optional<int> precision)
    : Plot(line_width, line_style, line_type, plot_point_markers,
           point_markers_radius, overlay, pane_name, format, precision),
      base_value_(base_value),
      top_line_color_(top_line_color),
      top_gradient_color1_(top_gradient_color1),
      top_gradient_color2_(top_gradient_color2),
      bottom_line_color_(bottom_line_color),
      bottom_gradient_color1_(bottom_gradient_color1),
      bottom_gradient_color2_(bottom_gradient_color2) {}

shared_ptr<Plot> Baseline::MakeShared() const {
  return make_shared<Baseline>(*this);
}

Histogram::Histogram(const double base_value, const Rgba& bullish_color,
                     const Rgba& bearish_color, const bool overlay,
                     const string& pane_name, const Format format,
                     const optional<int> precision)
    : Plot(1, SOLID, SIMPLE, false, 0, overlay, pane_name, format, precision),
      base_value_(base_value),
      bullish_color_(bullish_color),
      bearish_color_(bearish_color) {}
// ※ Histogram 생성자에서 Plot으로 전달하는 인수는, 직접 입력한 인수 외에는 무시

shared_ptr<Plot> Histogram::MakeShared() const {
  return make_shared<Histogram>(*this);
}

Line::Line(const Rgba& line_color, const char line_width,
           const LineStyle line_style, const LineType line_type,
           const bool plot_point_markers, const char point_markers_radius,
           const bool overlay, const string& pane_name, const Format format,
           const optional<int> precision)
    : Plot(line_width, line_style, line_type, plot_point_markers,
           point_markers_radius, overlay, pane_name, format, precision),
      line_color_(line_color) {}

shared_ptr<Plot> Line::MakeShared() const { return make_shared<Line>(*this); }

// ※ Null 생성자의 인수는 무시
Null::Null() : Plot(1, SOLID, SIMPLE, false, 0, false, "Null", NONE, nullopt) {}

shared_ptr<Plot> Null::MakeShared() const { return make_shared<Null>(*this); }

}  // namespace backtesting::plot
