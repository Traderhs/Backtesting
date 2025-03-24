      const {{ line_series }} = chart.addSeries(LightweightCharts.LineSeries, {
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
        
        // 라인 차트 변수
        color: "{{ color }}"
      }, {{ pane_idx }});
            
      {{ line_series }}.applyOptions({
        lastValueVisible: false,       // 마지막 값 라벨 숨김
        priceLineVisible: false,       // 마지막 값 선 숨김
        crosshairMarkerVisible: false, // 마우스 따라다니는 점 숨김
      });
            
      {{ line_series }}.setData(JSON.parse('{{ data }}')
        .map(pt => ({
                      time: pt.time,
                      value: pt.value === null ? NaN : pt.value,
                    })
            )
      );

      // 지표 데이터와 이름을 전역 변수에 저장
      const {{ line_series }}_data = JSON.parse('{{ data }}').map(pt => ({
            time: pt.time,
            value: pt.value === null ? NaN : pt.value,
      }));
      
      window.indicatorData = window.indicatorData || {};
      window.indicatorData["{{ line_series }}"] = {{ line_series }}_data;
      window.indicatorSeriesInfo = window.indicatorSeriesInfo || {};
      window.indicatorSeriesInfo["{{ line_series }}"] = {
            name: "{{ indicator_name }}",
            pane: {{ pane_idx }},
            seriesType: "Line",
            lineColor: "{{ color }}"
      };

