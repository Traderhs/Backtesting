import pandas as pd

pd.set_option('display.width', None)
pd.set_option('display.max_colwidth', None)
pd.set_option('display.max_rows', None)

# 읽을 Parquet 파일 경로 설정
path = "D:\\Programming\\Backtesting\\Data\\Continuous Klines\\BTCUSDT\\1w\\1w.parquet"
# path = "D:\\Programming\\Backtesting\\Data\\Mark Price Klines\\APTUSDT\\1h.parquet"

df = pd.read_parquet(path)

#print("원본 데이터\n", df, "\n\n" "행: ", len(df), "\n")

# "Open Time" 열을 datetime으로 변환
df["Open Time"] = pd.to_datetime(df["Open Time"], unit='ms')  # 단위가 밀리초인 경우

print("변환된 'Open Time'\n", df.head(20000), "\n")
