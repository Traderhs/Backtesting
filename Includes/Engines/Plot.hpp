#pragma once

// 표준 라이브러리
#include <memory>
#include <optional>
#include <string>

// 내부 헤더
#include "Engines/Export.hpp"

// 전방 선언
namespace backtesting::analyzer {
class Analyzer;
}

namespace backtesting::engine {
class Engine;
}

namespace backtesting::strategy {
class Strategy;
}

namespace backtesting::logger {
class Logger;
}

// 네임 스페이스
using namespace std;
namespace backtesting {
using namespace analyzer;
using namespace engine;
using namespace strategy;
using namespace logger;
}  // namespace backtesting

namespace backtesting::plot {

/// 플롯 RGBA 값을 나타내는 색 구조체
///
/// RGBA 값으로 저장된 기본 색도 제공
struct BACKTESTING_API Rgba {
  Rgba() = delete;
  Rgba(const int rgba_red, const int rgba_green, const int rgba_blue,
       const float rgba_alpha) {
    IsValidRgb("Red", rgba_red);
    IsValidRgb("Green", rgba_green);
    IsValidRgb("Blue", rgba_blue);
    IsValidAlpha(rgba_alpha);

    this->rgba_red = static_cast<unsigned char>(rgba_red);
    this->rgba_green = static_cast<unsigned char>(rgba_green);
    this->rgba_blue = static_cast<unsigned char>(rgba_blue);
    this->rgba_alpha = rgba_alpha;
  }

  [[nodiscard]] unsigned char GetRed() const;
  [[nodiscard]] unsigned char GetGreen() const;
  [[nodiscard]] unsigned char GetBlue() const;
  [[nodiscard]] float GetAlpha() const;

  /// RGBA 설정을 16진수 문자열로 변환하여 반환하는 함수
  [[nodiscard]] string RgbaToHex() const;

  // 기본 색상
  static const Rgba red;     // 빨강
  static const Rgba orange;  // 주황
  static const Rgba yellow;  // 노랑
  static const Rgba green;   // 초록
  static const Rgba cyan;    // 청록
  static const Rgba blue;    // 파랑
  static const Rgba purple;  // 보라
  static const Rgba pink;    // 핑크
  static const Rgba gray;    // 회색
  static const Rgba black;   // 검정
  static const Rgba white;   // 흰색

 private:
  unsigned char rgba_red;
  unsigned char rgba_green;
  unsigned char rgba_blue;
  float rgba_alpha;

  static void IsValidRgb(const string& color_name, int value);
  static void IsValidAlpha(float value);
};

/// 플롯 선의 모양을 지정하는 열거형 클래스
enum class LineStyle {
  SOLID,        // 실선
  DOTTED,       // 점선
  DASHED,       // 파선
  WIDE_DOTTED,  // 넓은 점선
  WIDE_DASHED   // 넓은 파선
};
using enum LineStyle;

/// 플롯 선의 종류를 지정하는 열거형 클래스
enum class LineType {
  SIMPLE,   // 직선
  STEPPED,  // 계단선
  CURVED    // 곡선
};
using enum LineType;

/// 지표 값의 툴팁 포맷을 지정하는 열거형 클래스
enum class Format {
  NONE,     // 포맷 없음 => 100, -100
  PERCENT,  // 퍼센트 접미사 => 100%, -100%
  DOLLAR,   // 달러 접두사 => $100, -$100
  VOLUME    // 거래량 접미사 => 123, 1.23K, 1.23M, 1.23B, 1.13T
};
using enum Format;

/// 지표의 플롯 스타일을 설정하는 가상 클래스
class BACKTESTING_API Plot {
  // 차트 작성 정보 참조 시 사용
  friend class Analyzer;

 public:
  // Plot을 복사해 동적 생성한 뒤 shared_ptr<Plot>로 리턴하는 함수
  [[nodiscard]] virtual shared_ptr<Plot> MakeShared() const = 0;

 protected:
  /**
   * @brief Plot 클래스의 생성자
   *
   * @param line_width 선 굵기 (1 ~ 4)
   * @param line_style 선 모양
   * @param line_type 선 종류
   * @param plot_point_markers 선 위 값에 마커 표시 여부
   * @param point_markers_radius 마커의 반지름 (1 ~ 4)
   * @param overlay 메인 차트 위에 지표를 겹쳐서 표시할지 여부
   * @param pane_name 지표를 표시할 페인 이름. 같은 이름은 같은 페인에 플롯됨
   *                  (overlay가 true면 무시됨)
   * @param format 지표 값의 툴팁에 접두사 혹은 접미사로 붙을 포맷
   * @param precision 지표 값 툴팁의 소수점 정밀도 (0 ~ 15)
   *                  지정하지 않을 시 심볼 가격의 소수점 정밀도가 사용됨.
   *                  (VOLUME은 수량 최소 단위의 정밀도 사용)
   */
  explicit Plot(char line_width, LineStyle line_style, LineType line_type,
                bool plot_point_markers, char point_markers_radius,
                bool overlay, const string& pane_name, Format format,
                optional<int> precision);
  virtual ~Plot() = default;

  static shared_ptr<Logger>& logger_;

 private:
  unsigned char line_width_;            // 선 굵기 (1 ~ 4)
  LineStyle line_style_;                // 선 모양
  LineType line_type_;                  // 선 종류
  bool plot_point_markers_;             // 선 위 값에 마커 표시 여부
  unsigned char point_markers_radius_;  // 마커 반지름 (1 ~ 4)
  bool overlay_;      // 메인 차트 위에 지표를 겹쳐서 표시할지 여부
  string pane_name_;  /// 페인 이름이 같으면 같은 페인에 플롯됨
                      /// overlay_가 true면 이 값은 무시됨

  Format format_;            // 지표 값의 툴팁에 접두사 혹은 접미사로 붙을 포맷
  optional<int> precision_;  // 지표 값 툴팁의 소수점 정밀도
};

/// 영역으로 플롯하는 설정을 생성하는 클래스
class BACKTESTING_API Area final : public Plot {
  // 차트 작성 정보 참조 시 사용
  friend class Analyzer;

 public:
  Area() = delete;

  /**
   * @brief 영역 차트의 생성자
   *
   * @param top_gradient_color 그래프 채우기 영역의 위쪽 그라데이션 색상
   * @param bottom_gradient_color 그래프 채우기 영역의 아래쪽 그라데이션 색상
   * @param line_color 선 색상
   * @param line_width 선 굵기 (1 ~ 4)
   * @param line_style 선 모양
   * @param line_type 선 종류
   * @param plot_point_markers 선 위 값에 마커 표시 여부
   * @param point_markers_radius 마커의 반지름 (1 ~ 4)
   * @param overlay 메인 차트 위에 이 영역을 겹쳐서 그릴지 여부
   * @param pane_name 지표를 그릴 페인 이름 (overlay가 true면 무시됨)
   * @param format 지표 값의 툴팁에 접두사 혹은 접미사로 붙을 포맷
   * @param precision 지표 값 툴팁의 소수점 정밀도 (0 ~ 15)
   *                  지정하지 않을 시 심볼 가격의 소수점 정밀도가 사용됨.
   *                  (VOLUME은 수량 최소 단위의 정밀도 사용)
   */
  explicit Area(const Rgba& top_gradient_color,
                const Rgba& bottom_gradient_color, const Rgba& line_color,
                char line_width, LineStyle line_style, LineType line_type,
                bool plot_point_markers, char point_markers_radius,
                bool overlay, const string& pane_name = "",
                Format format = NONE, optional<int> precision = nullopt);

  // Area를 복사해 동적 생성한 뒤 shared_ptr<Plot>로 리턴하는 함수
  [[nodiscard]] shared_ptr<Plot> MakeShared() const override;

 private:
  Rgba top_gradient_color_;     // 위쪽 그라데이션 색상
  Rgba bottom_gradient_color_;  // 아래쪽 그라데이션 색상
  Rgba line_color_;             // 선 색상
};

/// 기준선으로 플롯하는 설정을 생성하는 클래스
class BACKTESTING_API Baseline final : public Plot {
  // 차트 작성 정보 참조 시 사용
  friend class Analyzer;

 public:
  Baseline() = delete;

  /**
   * @brief 기준선 차트의 생성자
   *
   * @param base_value 위/아래 영역을 나눌 기준값
   * @param top_line_color 기준값보다 높은 값에 대한 선 색상
   * @param top_gradient_color1 기준값보다 높은 값 영역의
   *                            위쪽 그라데이션 색상
   * @param top_gradient_color2 기준값보다 높은 값 영역의
   *                            아래쪽 그라데이션 색상
   * @param bottom_line_color 기준값보다 낮은 값에 대한 선 색상
   * @param bottom_gradient_color1 기준값보다 낮은 값 영역의 위쪽
   *                               그라데이션 색상
   * @param bottom_gradient_color2 기준값보다 낮은 값 영역의 아래쪽
   *                               그라데이션 색상
   * @param line_width 선 굵기 (1 ~ 4)
   * @param line_style 선 모양
   * @param line_type 선 종류
   * @param plot_point_markers 선 위 값에 마커 표시 여부
   * @param point_markers_radius 마커의 반지름 (1 ~ 4)
   * @param overlay 메인 차트 위에 이 영역을 겹쳐서 그릴지 여부
   * @param pane_name 지표를 그릴 페인 이름 (overlay가 true면 무시됨)
   * @param format 지표 값의 툴팁에 접두사 혹은 접미사로 붙을 포맷
   * @param precision 지표 값 툴팁의 소수점 정밀도 (0 ~ 15)
   *                  지정하지 않을 시 심볼 가격의 소수점 정밀도가 사용됨.
   *                  (VOLUME은 수량 최소 단위의 정밀도 사용)
   */
  explicit Baseline(double base_value, const Rgba& top_line_color,
                    const Rgba& top_gradient_color1,
                    const Rgba& top_gradient_color2,
                    const Rgba& bottom_line_color,
                    const Rgba& bottom_gradient_color1,
                    const Rgba& bottom_gradient_color2, char line_width,
                    LineStyle line_style, LineType line_type,
                    bool plot_point_markers, char point_markers_radius,
                    bool overlay, const string& pane_name = "",
                    Format format = NONE, optional<int> precision = nullopt);

  // Baseline을 복사해 동적 생성한 뒤 shared_ptr<Plot>로 리턴하는 함수
  [[nodiscard]] shared_ptr<Plot> MakeShared() const override;

 private:
  // 위/아래 영역을 나눌 기준값
  double base_value_;

  // 기준값보다 높은 값에 대한 선 색상
  Rgba top_line_color_;

  // 기준값보다 높은 값 영역의 위쪽 그라데이션 색상
  Rgba top_gradient_color1_;

  // 기준값보다 높은 값 영역의 아래쪽 그라데이션 색상
  Rgba top_gradient_color2_;

  // 기준값보다 낮은 값에 대한 선 색상
  Rgba bottom_line_color_;

  // 기준값보다 낮은 값 영역의 위쪽 그라데이션 색상
  Rgba bottom_gradient_color1_;

  // 기준값보다 낮은 값 영역의 아래쪽 그라데이션 색상
  Rgba bottom_gradient_color2_;
};

/// 히스토그램으로 플롯하는 설정을 생성하는 클래스
class BACKTESTING_API Histogram final : public Plot {
  // 차트 작성 정보 참조 시 사용
  friend class Analyzer;

 public:
  Histogram() = delete;

  /**
   * @brief 히스토그램 차트의 생성자
   *
   * @param base_value 기준값 (이 값을 기준으로 히스토그램을 위로 그림)
   * @param bullish_color 양봉일 때 히스토그램 색상
   * @param bearish_color 음봉일 때 히스토그램 색상
   * @param overlay 메인 차트 위에 이 히스토그램을 겹쳐서 그릴지 여부
   * @param pane_name 지표를 그릴 페인 이름 (overlay가 true면 무시됨)
   * @param format 지표 값의 툴팁에 접두사 혹은 접미사로 붙을 포맷
   * @param precision 지표 값 툴팁의 소수점 정밀도 (0 ~ 15)
   *                  지정하지 않을 시 심볼 가격의 소수점 정밀도가 사용됨.
   *                  (VOLUME은 수량 최소 단위의 정밀도 사용)
   */
  explicit Histogram(double base_value, const Rgba& bullish_color,
                     const Rgba& bearish_color, bool overlay,
                     const string& pane_name = "", Format format = NONE,
                     optional<int> precision = nullopt);

  // Histogram을 복사해 동적 생성한 뒤 shared_ptr<Plot>로 리턴하는 함수
  [[nodiscard]] shared_ptr<Plot> MakeShared() const override;

 private:
  double base_value_;   // 기준값 (이 값을 기준으로 히스토그램을 위로 그림)
  Rgba bullish_color_;  // 양봉일 때 히스토그램 색상
  Rgba bearish_color_;  // 음봉일 때 히스토그램 색상
};

/// 선으로 플롯하는 설정을 생성하는 클래스
class BACKTESTING_API Line final : public Plot {
  // 차트 작성 정보 참조 시 사용
  friend class Analyzer;

 public:
  Line() = delete;

  /**
   * @brief 선 차트의 생성자
   *
   * @param line_color 선 색상
   * @param line_width 선 굵기 (1 ~ 4)
   * @param line_style 선 모양
   * @param line_type 선 종류
   * @param plot_point_markers 선 위 값에 마커 표시 여부
   * @param point_markers_radius 마커의 반지름 (1 ~ 4)
   * @param overlay 메인 차트 위에 이 선을 겹쳐서 그릴지 여부
   * @param pane_name 지표를 그릴 페인 이름 (overlay가 true면 무시됨)
   * @param format 지표 값의 툴팁에 접두사 혹은 접미사로 붙을 포맷
   * @param precision 지표 값 툴팁의 소수점 정밀도 (0 ~ 15)
   *                  지정하지 않을 시 심볼 가격의 소수점 정밀도가 사용됨.
   *                  (VOLUME은 수량 최소 단위의 정밀도 사용)
   */
  explicit Line(const Rgba& line_color, char line_width, LineStyle line_style,
                LineType line_type, bool plot_point_markers,
                char point_markers_radius, bool overlay,
                const string& pane_name = "", Format format = NONE,
                optional<int> precision = nullopt);

  // Line을 복사해 동적 생성한 뒤 shared_ptr<Plot>로 리턴하는 함수
  [[nodiscard]] shared_ptr<Plot> MakeShared() const override;

 private:
  Rgba line_color_;  // 선 색상
};

/// 플롯하지 않을 때 사용하는 클래스
class BACKTESTING_API Null final : public Plot {
  friend class BaseAnalyzer;

 public:
  explicit Null();

  // Null을 복사해 동적 생성한 뒤 shared_ptr<Plot>로 리턴하는 함수
  [[nodiscard]] shared_ptr<Plot> MakeShared() const override;
};

}  // namespace backtesting::plot
