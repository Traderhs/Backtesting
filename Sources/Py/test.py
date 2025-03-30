import pandas as pd
import whisper

model=whisper.load_model("large")
result=model.transcribe("korean_audio.mp3",language="ko")
print(result["text"])