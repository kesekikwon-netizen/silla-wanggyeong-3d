@echo off
rem DBpia 수집용 Chrome: 저장소의 research\dbpia\chrome-profile 프로필 + 디버깅 포트 9222 (docs\research\METHODS.md)
start "" "C:\Program Files\Google\Chrome\Application\chrome.exe" --remote-debugging-port=9222 --user-data-dir="%~dp0..\..\research\dbpia\chrome-profile" --no-first-run --new-window https://ai.dbpia.co.kr/ko/search
