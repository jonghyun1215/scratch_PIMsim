#include <bits/stdc++.h>
using namespace std;

/******************************************************************************
 * 간단 함수: partition_X.mtx를 읽어 (nRows, nCols, nnz) + (row[], col[]).
 * val은 무시 (row,col,val 중 row,col만)
 * - row,col은 1-based든 0-based든 그대로 처리 (여기서는 그대로 저장)
 ******************************************************************************/
bool readPartitionMtx(const string& filepath,
                      int &nRows, int &nCols, long long &nnz,
                      vector<int> &rowVec,
                      vector<int> &colVec)
{
    ifstream fin(filepath);
    if(!fin.is_open()){
        cerr<<"[WARN] cannot open "<<filepath<<"\n";
        return false;
    }

    // 첫 줄
    {
        string firstLine;
        if(!getline(fin, firstLine)){
            cerr<<"[ERROR] cannot read first line of "<<filepath<<"\n";
            return false;
        }
        if(firstLine.rfind("%%MatrixMarket",0)!=0){
            cerr<<"[ERROR] not a MatrixMarket file: "<<filepath<<"\n";
            return false;
        }
    }
    // 주석 등 스킵
    while(fin.peek()=='%' || fin.peek()=='\n'){
        string dummy;
        if(!getline(fin,dummy)){
            if(fin.eof()){
                cerr<<"[ERROR] ended unexpectedly in "<<filepath<<"\n";
                return false;
            }
        }
    }

    // (nRows, nCols, nnz)
    if(!(fin >> nRows >> nCols >> nnz)){
        cerr<<"[ERROR] read (nRows,nCols,nnz) fail: "<<filepath<<"\n";
        return false;
    }

    rowVec.resize(nnz);
    colVec.resize(nnz);

    for(long long i=0; i<nnz; i++){
        int r,c; double v;
        if(!(fin >> r >> c >> v)){
            cerr<<"[ERROR] read (r,c,v) fail i="<<i<<" in "<<filepath<<"\n";
            return false;
        }
        rowVec[i]=r;
        colVec[i]=c;
    }
    fin.close();
    return true;
}

/******************************************************************************
 * Jaccard(A,B) = |A∩B| / |A∪B|.
 * 집합은 unordered_set<int>로 예시
 ******************************************************************************/
double jaccard(const unordered_set<int> &A, const unordered_set<int> &B){
    // intersection
    int interCount=0;
    for(auto &x : A){
        if(B.find(x)!=B.end()) interCount++;
    }
    int unionCount= A.size() + B.size() - interCount;
    if(unionCount==0){
        // 둘 다 empty => jaccard=1.0 으로 처리
        return 1.0;
    }
    return double(interCount)/double(unionCount);
}

/******************************************************************************
 * 하나의 폴더 dirName 안에 partition_0..63.mtx 를 읽어,
 * "row index similarity" = 파티션별 평균 Jaccard 의 평균을 계산
 * => 반환
 ******************************************************************************/
double calcFolderJaccard(const string& dirName, int partCount=64){
    double sumPartitionJacc=0.0;
    int validCount=0;

    for(int p=0; p<partCount; p++){
        // 파일 경로
        ostringstream oss;
        oss<<dirName<<"/partition_"<<p<<".mtx";
        string filepath = oss.str();

        // 읽기
        int nR=0,nC=0; long long nnz=0;
        vector<int> rowVec, colVec;
        if(!readPartitionMtx(filepath, nR, nC, nnz, rowVec, colVec)){
            // 만약 파일이 없거나 잘못되었으면 skip
            //cerr<<"[WARN] skip partition "<<p<<" in "<<dirName<<"\n";
            continue;
        }
        if(nnz==0){
            // 비어있으면 => jacc=1.0? or 0?
            // 여기선 "열이 없다" => meanJacc=1.0
            sumPartitionJacc += 1.0;
            validCount++;
            continue;
        }

        // col->rowSet
        unordered_map<int, unordered_set<int>> colMap;
        colMap.reserve(nC+10);

        for(long long i=0; i<nnz; i++){
            int r = rowVec[i];
            int c = colVec[i];
            colMap[c].insert(r);
        }

        // 파티션 안에 있는 column들
        vector<int> columnsInPart;
        columnsInPart.reserve(colMap.size());
        for(auto &kv : colMap){
            columnsInPart.push_back(kv.first);
        }

        int colCountInPart = columnsInPart.size();
        // 열이 1개 이하 => mean=1.0
        if(colCountInPart<=1){
            sumPartitionJacc += 1.0;
            validCount++;
            continue;
        }

        // 모든 열 쌍 Jaccard
        long long pairCount=0; 
        double sumJ=0.0;
        for(int i=0; i<colCountInPart; i++){
            for(int j=i+1; j<colCountInPart; j++){
                pairCount++;
                int c1= columnsInPart[i];
                int c2= columnsInPart[j];
                double jv = jaccard(colMap[c1], colMap[c2]);
                sumJ += jv;
            }
        }
        double meanJ=1.0;
        if(pairCount>0){
            meanJ= sumJ/double(pairCount);
        }
        sumPartitionJacc += meanJ;
        validCount++;
    }

    double finalScore=0.0;
    if(validCount>0){
        finalScore = sumPartitionJacc / double(validCount);
    }
    return finalScore;
}

/******************************************************************************
 * main
 * => "ASIC_100k_tiled", "bcsstk13_tiled", ... 등 폴더 목록 하드코딩
 * => 각 폴더마다 calcFolderJaccard() -> 결과 출력
 ******************************************************************************/
int main(){
    ios::sync_with_stdio(false);
    cin.tie(NULL);

    // 원하는 폴더 목록 (사용자 질문에서 예시)
    vector<string> folders = {
        "ASIC_100k_new",
        "bcsstk32_new",
        "cant_new",
        "consph_new",
        "crankseg_2_new",
        "ct20stif_new",
        "G2_circuit_new",
        "lhr71_new",
        "ohne2_new",
        "pdb1HYS_new",
        "pwtk_new",
        "rma10_new",
        "shipsec1_new",
        "soc-sign-epinions_new",
        "Stanford_new",
        "webbase-1M_new",
        "xenon2_new"
    };

    int partCount=64; // partition_0..63.mtx

    cout<<"=== Jaccard Similarity per suite (row index) ===\n";
    for(auto &dirName : folders){
        double score = calcFolderJaccard(dirName, partCount);
        cout<< dirName <<" => mean Jaccard: "<< score <<"\n";
    }
    cout<<"=== Done ===\n";
    return 0;
}

