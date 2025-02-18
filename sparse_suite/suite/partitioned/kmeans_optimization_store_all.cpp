#include <bits/stdc++.h>
using namespace std;

/******************************************************************************
 * 1) .mtx (Matrix Market) 파일을 1-based 인덱스로 읽기
 ******************************************************************************/
bool readMtxFile(const string& filename,
                 int& numRows,
                 int& numCols,
                 long long& nnz,
                 vector<int>& rows,
                 vector<int>& cols,
                 vector<double>& vals)
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

    // (numRows, numCols, nnz) 읽기
    fin >> numRows >> numCols >> nnz;
    rows.reserve(nnz);
    cols.reserve(nnz);
    vals.reserve(nnz);

    // (row, col, val) (1-based)로 저장
    for (long long i=0; i<nnz; i++){
        int r,c;
        double v;
        if (!(fin >> r >> c >> v)) {
            cerr<<"[ERROR] reading (row,col,val) failed at index "<< i <<"\n";
            return false;
        }
        rows.push_back(r);
        cols.push_back(c);
        vals.push_back(v);
    }
    fin.close();
    return true;
}

/******************************************************************************
 * 2) 열(Column) 구조체
 ******************************************************************************/
struct Column {
    int colId;
    vector<int> rowIndices;
    int nnz;
    vector<float> features;
};

/******************************************************************************
 * 3) row 분포 -> feature 벡터
 ******************************************************************************/
vector<float> computeFeatures(const vector<int>& rIdx, int maxRow, int dim){
    vector<float> feats(dim, 0.0f);
    if(rIdx.empty()) return feats;

    int segSize = (maxRow + dim - 1) / dim;
    for(auto r : rIdx){
        int idx = (r - 1) / segSize;
        if(idx >= dim) idx = dim - 1;
        feats[idx] += 1.0f;
    }
    // L2 정규화
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
 * 5) Min/Max Capacity K-means
 ******************************************************************************/
struct BoundedCapKMeans {
    vector<Column>& cols;
    int k;
    int maxIter;
    long long minCap;
    long long maxCap;
    vector<int> assignments;
    vector<long long> clusterNnz;
    vector<vector<float>> centroids;

    BoundedCapKMeans(vector<Column>& c, int K, int iters,
                     long long minC, long long maxC)
      : cols(c), k(K), maxIter(iters), minCap(minC), maxCap(maxC)
    {
        assignments.resize(cols.size(), -1);
        clusterNnz.resize(k, 0LL);
        centroids.resize(k);
    }

    void initCentroids() {
        srand((unsigned)time(NULL));
        for(int i=0; i<k; i++){
            int r = rand() % cols.size();
            centroids[i] = cols[r].features;
        }
        fill(clusterNnz.begin(), clusterNnz.end(), 0LL);
    }
	/*void initCentroids() {
    // 가장 NNZ가 많은 열을 기준으로 초기 중심점 설정
    	vector<pair<int, int>> nnzWithIndex; // {nnz, column index}
    	for (size_t i = 0; i < cols.size(); i++) {
        	nnzWithIndex.push_back({cols[i].nnz, static_cast<int>(i)});
    	}

    	// NNZ 기준 내림차순 정렬
    	sort(nnzWithIndex.rbegin(), nnzWithIndex.rend());

		// 상위 k개의 열을 초기 중심점으로 선택
		for (int i = 0; i < k; i++) {
			int colIndex = nnzWithIndex[i].second;
			centroids[i] = cols[colIndex].features;
		}

		// 클러스터 NNZ 초기화
		fill(clusterNnz.begin(), clusterNnz.end(), 0LL);
	}*/


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

            // minCap, maxCap 고려
            for(int cl=0; cl<k; cl++){
                long long future = newN[cl] + colNnz;
                if(future <= maxCap){
                    float dist = distanceEuclid(feat, centroids[cl]);
                    if(newN[cl] < minCap){
                        dist *= 0.5f;
                    }
                    if(dist < bestCost){
                        bestCost = dist;
                        bestCluster = cl;
                    }
                }
            }
            // 모든 클러스터가 maxCap 초과 => 가장 NNZ 적은 클러스터
            if(bestCluster<0){
                int minIdx = 0;
                long long mn = newN[0];
                for(int cl=1; cl<k; cl++){
                    if(newN[cl] < mn){
                        mn = newN[cl];
                        minIdx = cl;
                    }
                }
                bestCluster = minIdx;
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
        vector<vector<float>> newC(k, vector<float>(centroids[0].size(), 0.0f));
        vector<int> count(k, 0);

        for(size_t c=0; c<cols.size(); c++){
            int cl = assignments[c];
            if(cl>=0 && cl<k){
                auto &f = cols[c].features;
                for(size_t d=0; d<f.size(); d++){
                    newC[cl][d]+= f[d];
                }
                count[cl]++;
            }
        }
        for(int cl=0; cl<k; cl++){
            if(count[cl]>0){
                for(size_t d=0; d<newC[cl].size(); d++){
                    newC[cl][d]/= float(count[cl]);
                }
            } else {
                // 비어있는 클러스터는 임의 할당
                int r = rand() % cols.size();
                newC[cl] = cols[r].features;
            }
        }
        centroids = newC;
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
    auto getClusterNnz=[&](int cl){
        long long s=0;
        for(size_t c=0; c<cols.size(); c++){
            if(assignments[c]==cl) s+= cols[c].nnz;
        }
        return s;
    };
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
                v /= float(count);
            }
        }
        return cent;
    };

    long long totalNnz=0;
    for(auto &co: cols) totalNnz += co.nnz;
    long long ideal= (k>0)? (totalNnz/k) : 0;

    for(int iter=0; iter<maxIter; iter++){
        vector<long long> cN(k);
        vector<vector<float>> cCent(k);
        for(int cl=0; cl<k; cl++){
            cN[cl]   = getClusterNnz(cl);
            cCent[cl]= getClusterCentroid(cl);
        }

        bool improved=false;
        for(int big=0; big<k; big++){
            if(cN[big] <= ideal) continue;
            for(int small=0; small<k; small++){
                if(cN[small] >= ideal) continue;
                if(big==small) continue;

                for(size_t c=0; c<cols.size(); c++){
                    if(assignments[c]!=big) continue;
                    float dCurr = distanceEuclid(cols[c].features, cCent[big]);
                    float dNew  = distanceEuclid(cols[c].features, cCent[small]);
                    if(dNew - dCurr < distThreshold){
                        long long nb = cN[big]   - cols[c].nnz;
                        long long ns = cN[small] + cols[c].nnz;
                        auto sq=[](long long x){return x*x;};
                        long long oldErr = sq(cN[big]-ideal) + sq(cN[small]-ideal);
                        long long newErr = sq(nb-ideal)      + sq(ns-ideal);
                        if(newErr < oldErr){
                            assignments[c] = small;
                            cN[big]   = nb;
                            cN[small] = ns;
                            improved  = true;
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
 ******************************************************************************/
void writePartitionsToMtx(const string& baseName,
                          int k,
                          const vector<int>& assignments,
                          int numRows,
                          int numCols,
                          const vector<int>& rowIdx,
                          const vector<int>& colIdx,
                          const vector<double>& vals)
{
    vector<vector<int>> partRows(k), partCols(k);
    vector<vector<double>> partVals(k);

    vector<int> maxRow(k, 0);
    vector<int> maxCol(k, 0);

    // (row, col, val) 파티션별 분배
    for(size_t i=0; i<rowIdx.size(); i++){
        int r = rowIdx[i];
        int c = colIdx[i];
        double v = vals[i];

        int part = assignments[c-1];
        if(part<0 || part>=k) continue;

        partRows[part].push_back(r);
        partCols[part].push_back(c);
        partVals[part].push_back(v);

        maxRow[part] = max(maxRow[part], r);
        maxCol[part] = max(maxCol[part], c);
    }

    // 각 파티션에 대해 .mtx 파일로 저장
    for(int p=0; p<k; p++){
        if(partRows[p].empty()){
            cerr<<"[INFO] partition "<<p<<" is empty => skip file\n";
            continue;
        }
        long long pnnz = (long long)partRows[p].size();
        int nRowsPart = maxRow[p];
        int nColsPart = maxCol[p];

        ostringstream oss;
        oss << baseName << "_" << p << ".mtx";
        string outF = oss.str();

        ofstream fout(outF);
        if(!fout.is_open()){
            cerr<<"[ERROR] cannot open "<<outF<<"\n";
            continue;
        }

        fout<<"%%MatrixMarket matrix coordinate real general\n";
        fout<<"% partition "<<p<<", 1-based row,col\n";
        fout<<nRowsPart<<" "<<nColsPart<<" "<<pnnz<<"\n";

        for(long long i=0; i<pnnz; i++){
            fout << partRows[p][i] << " "
                 << partCols[p][i] << " "
                 << partVals[p][i] << "\n";
        }
        fout.close();

        /*cerr<<"[INFO] Wrote partition "<<p<<" => "<< outF
            <<", #rows="<<nRowsPart
            <<", #cols="<<nColsPart
            <<", nnz="<<pnnz<<"\n";*/
    }
}

/******************************************************************************
 * 8) 단일 .mtx 파일 처리 (경로 + 파일명)
 ******************************************************************************/
void processSingleMtx(const string &fullPath,
                      int k, int kmIter, int balIter, float delta,
                      const string &outRoot)
{
    // 1) read
    int nRows=0, nCols=0;
    long long nnz=0;
    vector<int> rowIdx, colIdx;
    vector<double> vals;

    if(!readMtxFile(fullPath, nRows, nCols, nnz, rowIdx, colIdx, vals)){
        cerr<<"[ERROR] fail read "<< fullPath <<"\n";
        return;
    }
    cerr<<"[INFO] Original matrix: nRows="<<nRows
        <<", nCols="<<nCols
        <<", nnz="<<nnz<<"\n";

    // 2) 열(Column) 구성
    vector<Column> columns(nCols);
    for(int c=1; c<=nCols; c++){
        columns[c-1].colId = c;
    }
    for(long long i=0; i<nnz; i++){
        int c = colIdx[i];
        columns[c-1].rowIndices.push_back(rowIdx[i]);
    }

    // 중복 제거 & nnz & feature
    for(int c=0; c<nCols; c++){
        auto &ri= columns[c].rowIndices;
        sort(ri.begin(), ri.end());
        ri.erase(unique(ri.begin(), ri.end()), ri.end());
        columns[c].nnz = (int)ri.size();
    }
    int featureDim = 32;
    for(int c=0; c<nCols; c++){
        columns[c].features = computeFeatures(columns[c].rowIndices, nRows, featureDim);
    }

    // cap 계산
    long long totalN=0;
    for(auto &col : columns) totalN += col.nnz;
    long long ideal= (k>0)? (totalN/k) : 0;
    long long minCap= (long long)floor((double)ideal*(1.0 - delta));
    if(minCap<0) minCap=0;
    long long maxCap= (long long)ceil((double)ideal*(1.0 + delta));

    cerr<<"[INFO] K="<<k
        <<", idealNNZ="<<ideal
        <<", minCap="<<minCap
        <<", maxCap="<<maxCap<<"\n";

    // 3) 파티션 (KMeans + 후처리)
    vector<int> assignments(nCols, 0);
    if(k>0){
        BoundedCapKMeans bcm(columns, k, kmIter, minCap, maxCap);
        bcm.run();
        assignments = bcm.assignments;

        balanceNNZRefinement(columns, assignments, k, balIter);
    } else {
        cerr<<"[WARN] k=0 => no partition\n";
    }

    // 4) 출력 경로 조합
    //    outRoot = "/home/taewoon/second_drive/scratch_PIMsim/sparse_suite/suite/partitioned/"
    //    => 최종: "/home/taewoon/second_drive/.../partitioned/<파일이름폴더>/partition"
    //    여기서 <파일이름폴더> = "ASIC_100k_new" (확장자 .mtx 제거)
    // fullPath: "/home/.../sorted_suite/ASIC_100k_new.mtx"
    //  (1) 파일이름만 추출
    size_t slashPos = fullPath.find_last_of("/\\");
    string fileName = (slashPos == string::npos) ? fullPath : fullPath.substr(slashPos+1);

    // (2) 확장자 .mtx 제거
    size_t dotPos = fileName.find_last_of(".");
    if(dotPos != string::npos){
        fileName = fileName.substr(0, dotPos);
    }

    // (3) 최종 outBase
    // 예: outRoot + "ASIC_100k_new" + "/partition"
    //  => "/home/.../partitioned/ASIC_100k_new/partition"
    ostringstream oss;
    oss << outRoot << fileName << "/partition";
    string outBase = oss.str();

    // 5) 결과 저장
    writePartitionsToMtx(outBase, k, assignments,
                         nRows, nCols, rowIdx, colIdx, vals);

    cerr<<"[INFO] Done processing "<< fullPath <<"\n\n";
}

/******************************************************************************
 * 9) main - 하드코딩된 mtx 파일 목록을 처리
 ******************************************************************************/
int main(){
    ios::sync_with_stdio(false);
    cin.tie(NULL);

    // -------------------------------
    // 1) 파라미터
    // -------------------------------
    const int K = 64;
    const int KM_ITER = 20;
    const int BAL_ITER = 5;
    const float DELTA = 0.2f;

    // -------------------------------
    // 2) 원본 .mtx 파일들이 있는 경로 + 파일명
    // -------------------------------
    const string basePath = "/home/taewoon/second_drive/scratch_PIMsim/sparse_suite/suite/sorted_suite/";

    // 처리할 mtx 파일 목록
    vector<string> mtxFiles = {
        "ASIC_100k_new.mtx",
        "Stanford_new.mtx",
        "bcsstk32_new.mtx",
        "consph_new.mtx",
        "ct20stif_new.mtx",
        "ohne2_new.mtx",
        "pwtk_new.mtx",
        "shipsec1_new.mtx",
        "sorted_consph_new.mtx",
        "xenon2_new.mtx",
        "G2_circuit_new.mtx",
        "cant_new.mtx",
        "crankseg_2_new.mtx",
        "lhr71_new.mtx",
        "pdb1HYS_new.mtx",
        "rma10_new.mtx",
        "soc-sign-epinions_new.mtx",
        "webbase-1M_new.mtx"
    };

    // -------------------------------
    // 3) 결과를 저장할 폴더 (이미 존재한다고 가정)
    // -------------------------------
    const string outRoot = "/home/taewoon/second_drive/scratch_PIMsim/sparse_suite/suite/partitioned/";

    // -------------------------------
    // 4) 각 파일에 대해 처리
    // -------------------------------
    for(const auto &file : mtxFiles){
        // 전체 경로
        string fullPath = basePath + file;
        cerr << "[INFO] Start processing: " << fullPath << "\n";

        // 파티션 작업
        processSingleMtx(fullPath, K, KM_ITER, BAL_ITER, DELTA, outRoot);
    }

    cerr<<"[INFO] All done.\n";
    return 0;
}

