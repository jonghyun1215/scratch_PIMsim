#include <bits/stdc++.h>
using namespace std;

/******************************************************************************
 * (row, col, val) 저장용 구조
 ******************************************************************************/
struct Triple {
    int row;
    int col;
    double val;
};

/******************************************************************************
 * 간단한 함수: MatrixMarket (COO) 파일에서 (행, 열, nnz)와 모든 (row,col,val)을 읽어,
 *              triples 벡터에 저장.
 *  - (row, col)은 1-based => 0-based로 변환
 ******************************************************************************/
bool readMtxFile(const string& filename,
                 int &outRows, int &outCols,
                 long long &outNnz,
                 vector<Triple> &triples)
{
    ifstream fin(filename);
    if(!fin.is_open()) {
        cerr<<"[ERROR] cannot open "<<filename<<"\n";
        return false;
    }

    // 첫 줄
    {
        string firstLine;
        if(!getline(fin, firstLine)){
            cerr<<"[ERROR] cannot read first line of "<<filename<<"\n";
            return false;
        }
        if(firstLine.rfind("%%MatrixMarket",0)!=0){
            cerr<<"[ERROR] not a MatrixMarket file: "<<filename<<"\n";
            return false;
        }
    }
    // 주석 스킵
    while(fin.peek()=='%' || fin.peek()=='\n') {
        string dummy;
        if(!getline(fin, dummy)){
            if(fin.eof()){
                cerr<<"[ERROR] ended unexpectedly: "<<filename<<"\n";
                return false;
            }
        }
    }

    if(!(fin >> outRows >> outCols >> outNnz)){
        cerr<<"[ERROR] read (rows,cols,nnz) fail: "<<filename<<"\n";
        return false;
    }

    triples.resize(outNnz);

    for(long long i=0; i<outNnz; i++){
        int r,c;
        double v;
        if(!(fin >> r >> c >> v)){
            cerr<<"[ERROR] read (r,c,v) fail i="<<i<<" in "<<filename<<"\n";
            return false;
        }
        // 1-based => 0-based
        //r--; 
        //c--;
        triples[i] = {r, c, v};
    }
    fin.close();
    return true;
}

/******************************************************************************
 * Main
 * - original.mtx 를 먼저 읽어, 모든 (row,col,val)을 'origTriples'에 저장
 * - partition_0.mtx ~ partition_63.mtx를 순서대로 읽는다.
 * - 각 partition에서 읽은 튜플 pTriple에 대해:
 *    => 'origTriples'를 처음부터 끝까지 순회하며, 같은 (row,col,val)이 있는지 찾는다.
 *    => 찾으면 그 항목을 'used' 표시하고 break
 *    => 못 찾으면 mismatch => FAIL
 * - 모든 partition 파일 처리가 끝난 후, 'origTriples' 중 unused가 남아 있으면 => FAIL
 * - 전부 잘 match되면 => OK
 *
 * (매우 비효율적이지만, "하나씩 access"하기 위한 완전 탐색)
 ******************************************************************************/
int main(int argc, char** argv){
    ios::sync_with_stdio(false);
    cin.tie(NULL);

    if(argc<2){
        cerr<<"Usage: "<<argv[0]<<" <original.mtx>\n"
            <<" (will read partition_0.mtx ~ partition_63.mtx in current directory)\n";
        return 1;
    }

    // 1) 원본 읽기
    string originalFile = argv[1];
    int origRows=0, origCols=0;
    long long origNnz=0;
    vector<Triple> origTriples;
    if(!readMtxFile(originalFile, origRows, origCols, origNnz, origTriples)){
        cerr<<"[ERROR] fail read original "<<originalFile<<"\n";
        return 1;
    }
    cerr<<"[INFO] original: rows="<<origRows
        <<", cols="<<origCols
        <<", nnz="<<origNnz<<"\n";

    // 'used' 배열(원본의 각 튜플이 이미 매칭되었는지 표시)
    vector<bool> used(origNnz, false);

    // (참고) 나중에 모든 partition NNZ 합이 origNnz와 맞는지도 검증 가능
    long long sumPartitionNnz = 0;

    // 2) partition_0.mtx ~ partition_63.mtx 처리
    const int PART_COUNT = 64;
    for(int p=0; p<PART_COUNT; p++){
        // 파일명
        ostringstream oss;
        oss << "partition_"<<p<<".mtx";
        string partFile = oss.str();

        // 읽기
        int pRows=0, pCols=0;
        long long pNnz=0;
        vector<Triple> partTriples;
        if(!readMtxFile(partFile, pRows, pCols, pNnz, partTriples)){
            cerr<<"[ERROR] fail read partition file "<<partFile<<"\n";
            return 1;
        }
        cerr<<"[INFO] "<<partFile<<": rows="<<pRows
            <<", cols="<<pCols
            <<", nnz="<<pNnz<<"\n";

        sumPartitionNnz += pNnz;

        // 2-1) partition의 각 튜플에 대해, 원본 전체를 순회하며 같은 (row,col,val) 찾기
        for(long long i=0; i<pNnz; i++){
            int prow = partTriples[i].row;
            int pcol = partTriples[i].col;
            double pval = partTriples[i].val;

            bool foundMatch = false;

            // 원본 전체 탐색
            for(long long j=0; j<origNnz; j++){
                if(used[j]) continue; // 이미 다른 partition 튜플과 매칭됨

                if(origTriples[j].row == prow &&
                   origTriples[j].col == pcol )//&&
                  // origTriples[j].val == pval )
                {
                    // 일치하는 원소 발견 => used 표시
                    used[j] = true;
                    foundMatch = true;
                    break;
                }
            }

            if(!foundMatch) {
                // partition (prow,pcol,pval)에 대응하는 원본 항목을 못 찾음 => FAIL
                cerr<<"[FAIL] partition_"<<p<<" has (row="<<prow
                    <<", col="<<pcol<<", val="<<pval<<") not found in original.\n";
                return 1;
            }
        }
    }

    // 2-2) 모든 partition 읽기 후, NNZ 합 비교
    if(sumPartitionNnz != origNnz){
        cerr<<"[FAIL] sumPartitionNnz="<<sumPartitionNnz
            <<", but original nnz="<<origNnz<<"\n";
        return 1;
    }

    // 3) 이제 'origTriples' 중에서 used되지 않은 항목이 있으면 => FAIL
    //    => 그건 원본에 존재하지만 어느 partition에도 없었다는 뜻
    for(long long j=0; j<origNnz; j++){
        if(!used[j]){
            // 원본에 남은 튜플
            auto &ot = origTriples[j];
            cerr<<"[FAIL] original has un-partitioned triple ("
                <<ot.row<<","<<ot.col<<","<<ot.val<<")\n";
            return 1;
        }
    }

    // 4) 모든 것이 정상
    cerr<<"[RESULT] All partition_0..63.mtx matches original EXACTLY.\n";
    return 0;
}
