#!/bin/bash

python3 run_all_data.py
./extract_log.sh
python3 total_summary.py