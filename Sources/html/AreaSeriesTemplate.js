      const {{ area_series }} = chart.addSeries(LightweightCharts.AreaSeries, {
        // 공동 변수
        priceFormat: {
          type: 'price',
          precision: {{ precision }},    
          minMove: {{ tick_size }}     
        },
        lineStyle: {{ line_style }},
        lineWidth: {{ line_width }},
        lineType: {{ line_type }},
        pointMarkersVisible: {{ point_markers_visible }},
        pointMarkersRadius: {{ point_markers_radius }},

        // 영역 차트 변수
        topColor: "{{ top_color }}",
        bottomColor: "{{ bottom_color }}",
        lineColor: "{{ line_color }}"
      }, {{ pane_idx }});

      {{ area_series }}.applyOptions({
        lastValueVisible: false,       // 마지막 값 라벨 숨김
        priceLineVisible: false,       // 마지막 값 선 숨김
        crosshairMarkerVisible: false, // 마우스 따라다니는 점 숨김
      });

      {{ area_series }}.setData(JSON.parse('{{ data }}')
        .map(pt => ({
                      time: pt.time,
                      value: pt.value === null ? NaN : pt.value,
                    })
            )
      );
      
      // 지표 데이터와 이름을 전역 변수에 저장
      const {{ area_series }}_data = JSON.parse('{{ data }}').map(pt => ({
            time: pt.time,
            value: pt.value === null ? NaN : pt.value,
      }));
      
      window.indicatorData = window.indicatorData || {};
      window.indicatorData["{{ area_series }}"] = {{ area_series }}_data;
      window.indicatorSeriesInfo = window.indicatorSeriesInfo || {};
      window.indicatorSeriesInfo["{{ area_series }}"] = {
            name: "{{ indicator_name }}",
            pane: {{ pane_idx }},
            seriesType: "Area",
            lineColor: "{{ line_color }}"
      };

      