import pandas as pd

pd.set_option('display.width', None)
pd.set_option('display.max_colwidth', None)
pd.set_option('display.max_rows', None)

# 읽을 Parquet 파일 경로 설정
path = "D:\\Programming\\Backtesting\\Results\\20250905_220349\\Backboard\\Indicators\\sma1\\1567962000_1603958400.parquet"
# path = "D:\\Programming\\Backtesting\\Data\\Mark Price Klines\\APTUSDT\\1h.parquet"

df = pd.read_parquet(path)

#print("원본 데이터\n", df, "\n\n" "행: ", len(df), "\n")

# "Open Time" 열을 datetime으로 변환

print("변환된 'Open Time'\n", df.head(2000), "\n")
