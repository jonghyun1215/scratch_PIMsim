#!/bin/bash

# 현재 날짜를 YYYY-MM-DD 형식으로 가져옵니다.
current_date=$(date +"%Y-%m-%d")

# 변경된 파일을 스테이징합니다.
git add .

# 커밋을 현재 날짜를 메시지로 사용하여 생성합니다.
git commit -m "$current_date"

# 원격 저장소로 변경사항을 푸시합니다.
git push -u origin master

