import subprocess
import os

data_names = [
"cora",
"citeseer",
"amazon-photo",
"amazon-com",
"Pubmed",
"corafull",
"coauthor-phy",
"coauthor-cs",

"cornell",
# "film",
"chameleon",
"squirrel"

# "github",
# "flickr",
# "ogbn-arxiv",
# "mycielski18",
# "elliptic",
# "reddit",
# "kron_g18",
# "mycielski19",
# "kron_g19",
# "Yelp",
# "kron_g20",
# "roadNet-PA",
# "com-Youtube",
# "amazon-products",
# "kron_g500",
# "com-LiveJournal"
]
# options = ['mean', 'max', 'sum']
output_folder = "./log"
for data_name in data_names:
    # 실행할 커맨드를 생성합니다. f-string을 사용하여 data_name을 동적으로 삽입합니다.
    command = f"./pimdramsim3main ../configs/HBM2_4Gb_test.ini --pim-api=spmm -m {data_name} -w > {output_folder}/{data_name}.log"

    # 생성된 커맨드를 화면에 출력합니다 (확인용).
    print(f"Executing command: {command}")
    
    # 커맨드를 실행합니다.
    # subprocess.run을 사용하는 것이 더 현대적이고 안전한 방법입니다.
    try:
        subprocess.run(command, shell=True, check=True)
        print(f"Successfully executed for {data_name}.")
    except subprocess.CalledProcessError as e:
        print(f"Error executing command for {data_name}: {e}")
    except FileNotFoundError:
        print(f"Error: pimdramsim3main executable not found. Please check the path.")
        break # 실행 파일이 없으면 반복을 중단합니다.

print("All commands have been executed.")