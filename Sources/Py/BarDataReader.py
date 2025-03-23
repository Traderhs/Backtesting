"""
Bar Data Parquet 파일을 출력하는 파일
"""
import pandas as pd

pd.set_option('display.width', None)
pd.set_option('display.max_colwidth', None)
pd.set_option('display.max_rows', None)

# 읽을 Parquet 파일 경로 설정
base_path = "D:\\Programming\\Backtesting\\Data"
data_type = "Continuous Klines"
# data_type = "Mark Price Klines"
symbol = "BTCUSDT"
timeframe = "1h"

df = pd.read_parquet(f"{base_path}\\{data_type}\\{symbol}\\{timeframe}.parquet")
head = df.head(20).copy()
tail = df.tail(20).copy()
print("원본 데이터\n", head, "\n\n", tail, "\n\n" "행: ", len(df), "\n")

head['Open Time'] = pd.to_datetime(head['Open Time'], unit='ms').dt.strftime('%Y-%m-%d %H:%M:%S')
head['Close Time'] = pd.to_datetime(head['Close Time'], unit='ms').dt.strftime('%Y-%m-%d %H:%+M:%S')

tail['Open Time'] = pd.to_datetime(tail['Open Time'], unit='ms').dt.strftime('%Y-%m-%d %H:%M:%S')
tail['Close Time'] = pd.to_datetime(tail['Close Time'], unit='ms').dt.strftime('%Y-%m-%d %H:%M:%S')

print("=" * 130)
print("시간 조정 데이터\n", head, "\n\n", tail, "\n\n" "행: ", len(df))
