      const {{ baseline_series }} = chart.addSeries(LightweightCharts.BaselineSeries, {
        // 공통 변수
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

        // 기준선 차트 변수
        baseValue: {
          type: "price",
          price: {{ base_value }},          
        },     
        topFillColor1: "{{ top_fill_color1 }}",            
        topFillColor2: "{{ top_fill_color2 }}",            
        topLineColor: "{{ top_line_color }}",             
        bottomFillColor1: "{{ bottom_fill_color1 }}",         
        bottomFillColor2: "{{ bottom_fill_color2 }}",         
        bottomLineColor: "{{ bottom_line_color }}" 
      }, {{ pane_idx }});
            
      {{ baseline_series }}.applyOptions({
        lastValueVisible: false,       // 마지막 값 라벨 숨김
        priceLineVisible: false,       // 마지막 값 선 숨김
        crosshairMarkerVisible: false, // 마우스 따라다니는 점 숨김
      });


      {{ baseline_series }}.setData(JSON.parse('{{ data }}')
        .map(pt => ({
                      time: pt.time,
                      value: pt.value === null ? NaN : pt.value,
                    })
            )
      );

      // 지표 데이터와 이름을 전역 변수에 저장
      const {{ baseline_series }}_data = JSON.parse('{{ data }}').map(pt => ({
            time: pt.time,
            value: pt.value === null ? NaN : pt.value,
      }));
      
      window.indicatorData = window.indicatorData || {};
      window.indicatorData["{{ baseline_series }}"] = {{ baseline_series }}_data;
      window.indicatorSeriesInfo = window.indicatorSeriesInfo || {};
      window.indicatorSeriesInfo["{{ baseline_series }}"] = {
            name: "{{ indicator_name }}",
            pane: {{ pane_idx }},
            seriesType: "Baseline",
            baseValue: {{ base_value }},
            topLineColor: "{{ top_line_color }}",
            bottomLineColor: "{{ bottom_line_color }}"
      };

      