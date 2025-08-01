import requests

url = "https://leeseobtv.co.kr/api/v1/app/board/mainClassReview"

headers = {
    "Accept": "application/json, text/plain, */*",
    "Accept-Language": "ko,en-US;q=0.9,en;q=0.8",
    "Access-Control-Allow-Origin": "*",  # 서버가 보내는 응답 헤더임 → 생략해도 무방
    "Connection": "keep-alive",
    "Content-Type": "application/json",
    "Origin": "https://leeseobtv.co.kr",
    "Referer": "https://leeseobtv.co.kr/",
    "Sec-Fetch-Dest": "empty",
    "Sec-Fetch-Mode": "cors",
    "Sec-Fetch-Site": "same-origin",
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/138.0.0.0 Safari/537.36",
    "sec-ch-ua": '"Not)A;Brand";v="8", "Chromium";v="138", "Google Chrome";v="138"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"Windows"'
}

cookies = {
    "_gcl_gs": "2.1.k2$i1753406115$u190590390",
    "_gcl_gb": "GCL.1753406116.CloKCQjws4fEBhCKARJJAMWrns3s1wziY54iRlQIIFgJ20Xp_tSTOiPgQhesNxiK7mUFVYE_HGGDQqPyW_FcWw_RvFJNBgFA76Zkyu-S2yJhth-uphxx0RoCbMg.f-fKCJ6Q_90ZEJy0xqUC",
    "_gcl_ag": "2.1.k0AAAAACSxmhx_icvzNIExfdRM4i3HDnnog$i1753406116$bf-fKCJ6Q_90ZEJy0xqUC",
    "_gcl_au": "1.1.559058967.1753406116.2085314071.1753415435.1753415435"
}

payload = {
    "lectureId": 13
}

response = requests.post(url, headers=headers, cookies=cookies, json=payload)

# 결과 확인
print("Status Code:", response.status_code)
try:
    print(response.json())
except Exception:
    print(response.text)
