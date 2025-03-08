"""
Parquet 파일을 출력하는 파일
"""
import pandas as pd

pd.set_option('display.width', None)
pd.set_option('display.max_colwidth', None)

# 읽을 Parquet 파일 경로 설정
path = "D:\\Programming\\Backtesting\\Data\\Mark Price Klines\\XRPUSDT\\1h.parquet"

df = pd.read_parquet(path)
head = df.head(20).copy()
tail = df.tail(20).copy()
print("원본 데이터\n", head, "\n\n", tail, "\n\n" "행: ", len(df), "\n")

head['Open Time'] = pd.to_datetime(head['Open Time'], unit='ms').dt.strftime('%Y-%m-%d %H:%M:%S')
head['Close Time'] = pd.to_datetime(head['Close Time'], unit='ms').dt.strftime('%Y-%m-%d %H:%+M:%S')

tail['Open Time'] = pd.to_datetime(tail['Open Time'], unit='ms').dt.strftime('%Y-%m-%d %H:%M:%S')
tail['Close Time'] = pd.to_datetime(tail['Close Time'], unit='ms').dt.strftime('%Y-%m-%d %H:%M:%S')

print("=" * 130)
print("시간 조정 데이터\n", head, "\n\n", tail, "\n\n" "행: ", len(df))


# 누락된 데이터 검사용
'''
plus = df['Open Time'][1] - df['Open Time'][0]
li = list()

for i in range(1, len(df)):
    if (df['Open Time'][i] - df['Open Time'][i-1]) != plus:
        li.append(df['Open Time'][i])

li = pd.to_datetime(li, unit='ms')

print(li)
'''




