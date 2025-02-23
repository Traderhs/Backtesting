import pandas as pd
import plotly.graph_objects as go

# Parquet 파일 읽기 (파일 경로에 맞게 수정)
path = "D:\\Programming\\Backtesting\\Data\\Klines\\BTCUSDT\\1d.parquet"
df = pd.read_parquet(path)

# 시간 열을 사람이 읽을 수 있는 날짜/시간으로 변환
df['Open Time'] = pd.to_datetime(df['Open Time'], unit='ms')

indicator_df1 = pd.read_csv("C:\\Users\\0908r\\Desktop\\sma2.csv", header=None)
indicator_df2 = pd.read_csv("C:\\Users\\0908r\\Desktop\\sma1.csv", header=None)

# 인터랙티브 차트 생성
fig = go.Figure(data=[go.Candlestick(
    x=df['Open Time'],
    open=df['Open'],
    high=df['High'],
    low=df['Low'],
    close=df['Close'],
    name="Candlestick Chart",
    hoverinfo='x+y'
)])

# 라인 차트 추가
fig.add_trace(go.Scatter(
    x=df['Open Time'],  # 동일한 시간 값 사용
    y=indicator_df1[0],  # CSV에서 읽은 지표값
    mode='lines',
    name='Indicator Line 1',
    line=dict(color='red'),
    hoverinfo='x+y'
))

fig.add_trace(go.Scatter(
    x=df['Open Time'],  # 동일한 시간 값 사용
    y=indicator_df2[0],  # CSV에서 읽은 지표값
    mode='lines',
    name='Indicator Line 2',
    line=dict(color='yellow'),
    hoverinfo='x+y'
))

# 차트 레이아웃 설정
fig.update_layout(
    title="Candlestick Chart",
    xaxis_title="Date",
    yaxis_title="Price",
    xaxis_rangeslider_visible=False,

    # 툴팁 스타일 조정
    hovermode='x unified',  # 값 전부를 호버
    hoverlabel=dict(
        bgcolor="rgba(255,255,255,0.2)",  # 투명한 흰색 배경
        font_size=12,
        font_family="Arial"
    ),

    # 가로선 설정 추가
    spikedistance=1000,
    xaxis=dict(
        showspikes=True,
        spikemode="across",
        spikesnap="cursor",
        showline=True,
        showgrid=True
    ),
    yaxis=dict(
        showspikes=True,
        spikemode="across",
        spikethickness=1,
        spikedash="solid",
        showline=True,        showgrid=True,
        tickformat=".6~g"  # 기존 설정 유지
    )
)

# 차트 출력
fig.show()
