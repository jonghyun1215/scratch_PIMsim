#include <bits/stdc++.h>
using namespace std;

/******************************************************************************
 * 1) .mtx (Matrix Market) 파일을 1-based 인덱스로 읽기 (그대로 보존)
 ******************************************************************************/
bool readMtxFile(const string& filename,
                 int& numRows,  // 원본 행 개수 (1-based 최대치)
                 int& numCols,  // 원본 열 개수 (1-based 최대치)
                 long long& nnz,
                 vector<int>& rows,  // 1-based row
                 vector<int>& cols,  // 1-based col
                 vector<double>& vals)// 값
{
    ifstream fin(filename);
    if (!fin.is_open()) {
        cerr << "[ERROR] cannot open file " << filename << endl;
        return false;
    }

    // 첫 줄: "%%MatrixMarket ..."
    {
        string firstLine;
        if (!getline(fin, firstLine)) {
            cerr << "[ERROR] cannot read first line\n";
            return false;
        }
        if (firstLine.rfind("%%MatrixMarket", 0) != 0) {
            cerr << "[ERROR] not a valid MatrixMarket file\n";
            return false;
        }
    }

    // 주석/빈 줄 스킵
    while (fin.peek()=='%' || fin.peek()=='\n') {
        string dummy;
        if(!getline(fin, dummy)) {
            if(fin.eof()){
                cerr<<"[ERROR] file ended unexpectedly\n";
                return false;
            }
        }
    }

    // (numRows, numCols, nnz) 읽기 (1-based 활용)
    fin >> numRows >> numCols >> nnz;
    rows.reserve(nnz);
    cols.reserve(nnz);
    vals.reserve(nnz);

    // (row, col, val) 그대로 (1-based) 저장
    for (long long i=0; i<nnz; i++){
        int r,c; 
        double v;
        if (!(fin >> r >> c >> v)) {
            cerr<<"[ERROR] reading (row,col,val) failed at index "<< i <<"\n";
            return false;
        }
        // 여기서 r, c를 줄이지 않음(그대로 1-based)
        rows.push_back(r);
        cols.push_back(c);
        vals.push_back(v);
    }
    fin.close();
    return true;
}

/******************************************************************************
 * 2) 열(Column) 구조체 (rowIndices: 이 열에 등장하는 row(1-based) 목록)
 ******************************************************************************/
struct Column {
    int colId;          // 1-based 열 번호
    vector<int> rowIndices; 
    int nnz;            // rowIndices.size()
    // features: K-means 클러스터링에 사용할 간단한 특성 벡터 (optional)
    vector<float> features;
};

/******************************************************************************
 * 3) row 분포 -> feature 벡터 (row가 1-based이지만 내부 계산은 편의상 0-based화 가능)
 *    => 여기서는 rowMax 등 고려해 간단히 "seg = ceil(maxRow / dim)"
 ******************************************************************************/
vector<float> computeFeatures(const vector<int>& rIdx, int maxRow, int dim){
    // row가 1-based이므로, segSize = ceil(maxRow/dim)
    // r-1을 써서 구간 인덱스 잡아도 되고, 단순히 (r-1)/segSize 사용 가능
    // 여기서는 일단 (r-1)/segSize 방식
    vector<float> feats(dim, 0.0f);
    if(rIdx.empty()) return feats;

    int segSize = (maxRow + dim - 1) / dim; // 단순 등분
    for(auto r : rIdx){
        // 1-based row -> 0-based로 변환시 (r-1)
        int idx = (r-1) / segSize;
        if(idx >= dim) idx = dim-1;
        feats[idx] += 1.0f;
    }
    // L2정규화
    float norm = 0.0f;
    for(auto f : feats) norm += f*f;
    norm = sqrt(norm);
    if(norm > 1e-9f){
        for(auto &f : feats) f /= norm;
    }
    return feats;
}

/******************************************************************************
 * 4) 유클리드 거리
 ******************************************************************************/
float distanceEuclid(const vector<float>& a, const vector<float>& b){
    float dist=0.0f;
    for(size_t i=0; i<a.size(); i++){
        float d= a[i] - b[i];
        dist += d*d;
    }
    return sqrt(dist);
}

/******************************************************************************
 * 5) "Min/Max Capacity" K-means (BoundedCapKMeans) - column 단위
 *    => columns[c] 전체 rowIndices가 쪼개지지 않음
 ******************************************************************************/
struct BoundedCapKMeans {
    vector<Column>& cols; // 열들 (각 열이 rowIndices를 갖음), 1-based colId
    int k;
    int maxIter;
    long long minCap;  // NNZ 최소 (열의 비제로 개수 합)
    long long maxCap;  // NNZ 최대
    vector<int> assignments;      // 열 c -> 어느 클러스터?
    vector<long long> clusterNnz; // 각 클러스터 NNZ
    vector<vector<float>> centroids; // 센트로이드(특징 벡터)

    BoundedCapKMeans(vector<Column>& c, int K, int iters,
                     long long minC, long long maxC)
      : cols(c), k(K), maxIter(iters), minCap(minC), maxCap(maxC)
    {
        assignments.resize(cols.size(), -1);
        clusterNnz.resize(k, 0LL);
        centroids.resize(k);
    }

    // 임의 초기화
    void initCentroids() {
        srand((unsigned)time(NULL));
        for(int i=0; i<k; i++){
            int r = rand() % cols.size();
            centroids[i] = cols[r].features;
        }
        fill(clusterNnz.begin(), clusterNnz.end(), 0LL);
    }

    void run(){
        initCentroids();
        bool changed=true;
        for(int iter=0; iter<maxIter; iter++){
            changed = assignmentStep();
            if(!changed && iter>0) break;
            updateCentroids();
        }
    }

    bool assignmentStep(){
        vector<int> newA(cols.size(), -1);
        vector<long long> newN(k, 0LL);
        bool changed=false;

        for(size_t c=0; c<cols.size(); c++){
            float bestCost = FLT_MAX;
            int bestCluster = -1;
            long long colNnz = cols[c].nnz;
            auto &feat = cols[c].features;

            // (1) minCap, maxCap
            for(int cl=0; cl<k; cl++){
                long long future = newN[cl] + colNnz;
                if(future <= maxCap){
                    float dist = distanceEuclid(feat, centroids[cl]);
                    // minCap 미만이면 페널티 ↓
                    if(newN[cl] < minCap){
                        dist *= 0.5f;
                    }
                    if(dist < bestCost){
                        bestCost = dist;
                        bestCluster = cl;
                    }
                }
            }
            // (2) 모두 maxCap 초과 => "가장 NNZ 적은" 클러스터 강제
            if(bestCluster<0){
                int minIdx=0;
                long long mn = newN[0];
                for(int cl=1; cl<k; cl++){
                    if(newN[cl] < mn){
                        mn = newN[cl];
                        minIdx= cl;
                    }
                }
                bestCluster= minIdx;
            }
            newA[c] = bestCluster;
            newN[bestCluster]+= colNnz;

            if(bestCluster != assignments[c]) changed=true;
        }

        assignments = newA;
        clusterNnz = newN;
        return changed;
    }

    void updateCentroids(){
        // (cluster별) 특징벡터 합산
        vector<vector<float>> newC(k, vector<float>(centroids[0].size(), 0.0f));
        vector<int> count(k, 0);

        for(size_t c=0; c<cols.size(); c++){
            int cl = assignments[c];
            if(cl>=0 && cl<k){
                // features 합산
                auto &f = cols[c].features;
                for(size_t d=0; d<f.size(); d++){
                    newC[cl][d]+= f[d];
                }
                count[cl]++;
            }
        }
        // 평균
        for(int cl=0; cl<k; cl++){
            if(count[cl]>0){
                for(size_t d=0; d<newC[cl].size(); d++){
                    newC[cl][d]/= float(count[cl]);
                }
            } else {
                // 비어있으면 임의 초기화
                int r= rand()%cols.size();
                newC[cl] = cols[r].features;
            }
        }
        centroids= newC;
    }
};

/******************************************************************************
 * 6) Greedy 후처리: NNZ 균등화
 ******************************************************************************/
void balanceNNZRefinement(vector<Column>& cols,
                          vector<int>& assignments,
                          int k,
                          int maxIter,
                          float distThreshold=0.2f)
{
    // 클러스터별 NNZ
    auto getClusterNnz=[&](int cl){
        long long s=0;
        for(size_t c=0; c<cols.size(); c++){
            if(assignments[c]==cl) s+= cols[c].nnz;
        }
        return s;
    };
    // 클러스터별 centroid
    auto getClusterCentroid=[&](int cl)->vector<float>{
        vector<float> cent(cols[0].features.size(), 0.0f);
        int count=0;
        for(size_t c=0; c<cols.size(); c++){
            if(assignments[c]==cl){
                auto &f = cols[c].features;
                for(size_t d=0; d<f.size(); d++){
                    cent[d]+= f[d];
                }
                count++;
            }
        }
        if(count>0){
            for(auto &v: cent){
                v/= float(count);
            }
        }
        return cent;
    };

    // totalNnz
    long long totalNnz=0;
    for(auto &co: cols) totalNnz += co.nnz;
    long long ideal= (k>0)? (totalNnz/k) : 0;

    for(int iter=0; iter<maxIter; iter++){
        vector<long long> cN(k);
        vector<vector<float>> cCent(k);
        for(int cl=0; cl<k; cl++){
            cN[cl] = getClusterNnz(cl);
            cCent[cl] = getClusterCentroid(cl);
        }
        bool improved=false;

        // "큰 -> 작은" 클러스터로 열 이동
        for(int big=0; big<k; big++){
            if(cN[big]<=ideal) continue;
            for(int small=0; small<k; small++){
                if(cN[small]>=ideal) continue;
                if(big==small) continue;

                // 후보 열
                for(size_t c=0; c<cols.size(); c++){
                    if(assignments[c]!=big) continue; // big 클러스터 열만
                    // 거리 차
                    float dCurr= distanceEuclid(cols[c].features, cCent[big]);
                    float dNew= distanceEuclid(cols[c].features, cCent[small]);
                    if(dNew - dCurr < distThreshold){
                        // NNZ 분산 개선?
                        long long nb= cN[big] - cols[c].nnz;
                        long long ns= cN[small]+ cols[c].nnz;

                        auto sq=[](long long x){return x*x;};
                        long long oldErr= sq(cN[big]-ideal)+sq(cN[small]-ideal);
                        long long newErr= sq(nb-ideal)+sq(ns-ideal);
                        if(newErr<oldErr){
                            assignments[c]= small;
                            cN[big]= nb;
                            cN[small]= ns;
                            improved= true;
                        }
                    }
                }
            }
        }

        if(!improved) break;
    }
}

/******************************************************************************
 * 7) 파티션별 .mtx 저장
 *    -> 원본의 (row, col, val)을 그대로 사용(1-based), 열 재매핑 없음
 *    -> 하나의 column은 하나의 파티션에만 기록
 ******************************************************************************/
void writePartitionsToMtx(const string& baseName,
                          int k,
                          const vector<int>& assignments,
                          int numRows, // 1-based 최대 행
                          int numCols, // 1-based 최대 열
                          const vector<int>& rowIdx, // 1-based
                          const vector<int>& colIdx, // 1-based
                          const vector<double>& vals)
{
    // 파티션별로 .mtx 파일을 만들기
    // part p => partition_p.mtx
    // 이 때, row, col은 원본 그대로(1-based).
    // "하나의 column"이 쪼개지지 않도록 => 이미 assignment에서 col단위로 결정.

    // 파티션별 (row, col, val)을 수집하기 위해, 
    //   rowIdx[i], colIdx[i], vals[i]를 보고 assignments[colIdx[i]-1]로 확인
    //   (주의) colIdx는 1-based, assignments[]는 0-based index => colIdx[i]-1
    // -> 만약 partition == p 이면, 해당 튜플을 p파일에 기록

    // 임시로 파티션별 (row, col, val) 저장
    vector<vector<int>> partRows(k), partCols(k);
    vector<vector<double>> partVals(k);

    // 또한, row, col의 최대값도 추적(헤더에 기록)
    vector<int> maxRow(k, 0);
    vector<int> maxCol(k, 0);

    for(size_t i=0; i<rowIdx.size(); i++){
        int r = rowIdx[i];  // 1-based
        int c = colIdx[i];  // 1-based
        double v = vals[i];

        // 이 col이 어느 파티션?
        int part = assignments[c-1]; 
        // c-1 => columns vector index
        // part => 0..k-1
        if(part<0 || part>=k) {
            // 만약 -1이거나 k 범위 밖이면 무시
            continue;
        }

        partRows[part].push_back(r);
        partCols[part].push_back(c);
        partVals[part].push_back(v);

        if(r>maxRow[part]) maxRow[part]=r;
        if(c>maxCol[part]) maxCol[part]=c;
    }

    // 이제 각 파티션별로 파일 기록
    for(int p=0; p<k; p++){
        if(partRows[p].empty()){
            cerr<<"[INFO] partition "<<p<<" is empty => skip file\n";
            continue;
        }
        // nnz
        long long pnnz = partRows[p].size();

        // 헤더용 row, col 크기
        int nRowsPart = maxRow[p];
        int nColsPart = maxCol[p];

        // 파일명
        ostringstream oss;
        oss << baseName << "_" << p << ".mtx";
        string outF = oss.str();

        ofstream fout(outF);
        if(!fout.is_open()){
            cerr<<"[ERROR] cannot open "<<outF<<"\n";
            continue;
        }

        // MatrixMarket 헤더
        fout<<"%%MatrixMarket matrix coordinate real general\n";
        fout<<"% partition "<<p<<", 1-based row,col\n";
        fout<< nRowsPart <<" "<< nColsPart <<" "<< pnnz <<"\n";

        // (row, col, val) 출력
        for(long long i=0; i<pnnz; i++){
            fout<< partRows[p][i] <<" "
                 << partCols[p][i] <<" "
                 << partVals[p][i] <<"\n";
        }
        fout.close();

        cerr<<"[INFO] Wrote partition "<<p<<" => "<< outF
            <<", #rows="<<nRowsPart
            <<", #cols="<<nColsPart
            <<", nnz="<<pnnz<<"\n";
    }
}

/******************************************************************************
 * main
 ******************************************************************************/
int main(int argc, char** argv){
    ios::sync_with_stdio(false);
    cin.tie(NULL);

    if(argc<2){
        cerr<<"Usage: "<<argv[0]<<" <original.mtx> [k=64] [kmeans_iter=10] [balance_iter=5] [delta=0.2] [outputBase=partition]\n";
        cerr<<" => 1-based indexing, no column splitting\n";
        return 1;
    }

    string inputFile= argv[1];
    int k= (argc>=3)? stoi(argv[2]):64;
    int kmIter= (argc>=4)? stoi(argv[3]):10;
    int balIter= (argc>=5)? stoi(argv[4]):5;
    float delta= (argc>=6)? stof(argv[5]): 0.2f;
    string outBase= (argc>=7)? argv[6] : "partition";
	outBase = "./partitioned_mtx/" + outBase;

    // 1) 원본 .mtx 읽기 (1-based)
    int nRows=0, nCols=0;
    long long nnz=0;
    vector<int> rowIdx, colIdx;   // 모두 1-based로 저장
    vector<double> vals;
    if(!readMtxFile(inputFile, nRows, nCols, nnz, rowIdx, colIdx, vals)){
        cerr<<"[ERROR] fail read original\n";
        return 1;
    }
    cerr<<"[INFO] Original matrix: nRows="<<nRows
        <<", nCols="<<nCols
        <<", nnz="<<nnz<<"\n";

    // 2) 열(Column) 구조 구성 (1-based)
    //    => columns[c-1] 가 "col c"에 대응
    vector<Column> columns(nCols);
    for(int c=1; c<=nCols; c++){
        columns[c-1].colId = c; // 1-based
    }
    // rowIndices
    for(long long i=0; i<nnz; i++){
        int c = colIdx[i]; // 1-based
        int r = rowIdx[i]; // 1-based
        // columns[c-1].rowIndices.push_back(r)
        columns[c-1].rowIndices.push_back(r);
    }

    // 중복 제거, nnz, feature
    for(int c=0; c<nCols; c++){
        auto &ri= columns[c].rowIndices;
        sort(ri.begin(), ri.end());
        ri.erase(unique(ri.begin(), ri.end()), ri.end());
        columns[c].nnz = (int)ri.size();
    }
    // features 계산
    int featureDim=32;
    for(int c=0; c<nCols; c++){
        columns[c].features = computeFeatures(columns[c].rowIndices, nRows, featureDim);
    }

    // totalN
    long long totalN=0;
    for(auto &col : columns){
        totalN += col.nnz;
    }
    long long ideal= (k>0)? (totalN/k) : 0;
    long long minCap= (long long)floor(double(ideal)*(1.0-delta));
    if(minCap<0) minCap=0;
    long long maxCap= (long long)ceil(double(ideal)*(1.0+delta));

    cerr<<"[INFO] K="<<k
        <<", idealNNZ="<<ideal
        <<", minCap="<<minCap
        <<", maxCap="<<maxCap<<"\n";

    // 3) BoundedCapKMeans
    vector<int> assignments(nCols, 0);
    if(k>0){
        BoundedCapKMeans bcm(columns, k, kmIter, minCap, maxCap);
        bcm.run();

        assignments= bcm.assignments;

        // 4) NNZ 균등화 후처리
        balanceNNZRefinement(columns, assignments, k, balIter);

        // 5) 파티션 결과 출력
        //    => (row, col, val) 모두 1-based로
        writePartitionsToMtx(outBase, k, assignments,
                             nRows, nCols,
                             rowIdx, colIdx, vals);

    } else {
        cerr<<"[WARN] k=0 => no partition\n";
    }

    cerr<<"[INFO] Done.\n";
    return 0;
}

