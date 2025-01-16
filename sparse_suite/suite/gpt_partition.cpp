#include <bits/stdc++.h>
using namespace std;

/**
 * @brief Triplet: (row, col, val)을 저장
 */
struct Triplet {
    int row;
    int col;
    double val;
};

/**
 * @brief ColumnData: 각 열의 정보
 */
struct ColumnData {
    vector<int> rowIndices;  // 정렬된 row 인덱스
    int nnz;                 // rowIndices.size()
    int clusterID;           // 최종 할당 클러스터
};

/**
 * @brief 자카드 유사도(Jaccard)
 *  A, B: 정렬된 row 인덱스 벡터
 */
double jaccardSimilarity(const vector<int>& A, const vector<int>& B) {
    int i=0, j=0;
    int interCount=0;
    int sizeA=(int)A.size(), sizeB=(int)B.size();
    while(i<sizeA && j<sizeB) {
        if(A[i] == B[j]) {
            interCount++; i++; j++;
        } else if(A[i]<B[j]) {
            i++;
        } else {
            j++;
        }
    }
    int unionCount = sizeA + sizeB - interCount;
    if(unionCount==0) return 0.0;
    return (double)interCount / (double)unionCount;
}

/**
 * @brief 클러스터의 NNZ 합, 평균 NNZ를 계산
 */
void computeClusterStats(const vector<ColumnData>& columns, 
                         int K,
                         vector<long long>& clusterNNZ,
                         vector<int>& clusterCount,
                         long long& totalNNZ,
                         double& avgNNZ)
{
    // 초기화
    clusterNNZ.assign(K, 0LL);
    clusterCount.assign(K, 0);
    totalNNZ = 0LL;

    // 누적
    for(auto &col : columns){
        int cid = col.clusterID;
        clusterNNZ[cid] += col.nnz;
        clusterCount[cid]++;
    }
    // 합
    for(auto &x : clusterNNZ) {
        totalNNZ += x;
    }
    avgNNZ = (double)totalNNZ / (double)K;
}


int main(int argc, char* argv[]){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    if(argc<2) {
        cerr << "Usage: " << argv[0] << " input.mtx" << endl;
        return 1;
    }

    //----------------------------------------------------------------------------
    // 1) .mtx 파일 읽기
    //----------------------------------------------------------------------------
    ifstream fin(argv[1]);
    if(!fin.is_open()){
        cerr << "[ERROR] Cannot open " << argv[1] << endl;
        return 1;
    }

    int M, N, NNZ;
    bool headerFound=false;
    while(!headerFound && !fin.eof()){
        string line;
        getline(fin, line);
        if(line.size()==0) continue;
        if(line[0]=='%'){
            // 주석
            continue;
        } else {
            // 예: "5 5 9"
            stringstream ss(line);
            ss >> M >> N >> NNZ;
            headerFound=true;
        }
    }

    vector<Triplet> entries;
    entries.reserve(NNZ);
    for(int i=0; i<NNZ; i++){
        Triplet t;
        fin >> t.row >> t.col >> t.val;
        // 1-based -> 0-based
        t.row -= 1;
        t.col -= 1;
        entries.push_back(t);
    }
    fin.close();

    cerr << "[INFO] M="<<M<<", N="<<N<<", NNZ="<<NNZ<<"\n";

    //----------------------------------------------------------------------------
    // 2) 열별 rowIndices 구성
    //----------------------------------------------------------------------------
    vector<ColumnData> columns(N);
    for(int c=0; c<N; c++){
        columns[c].nnz=0;
        columns[c].clusterID=-1;
    }
    for(auto &e : entries){
        columns[e.col].rowIndices.push_back(e.row);
    }
    for(int c=0; c<N; c++){
        auto &v=columns[c].rowIndices;
        sort(v.begin(), v.end());
        v.erase(unique(v.begin(), v.end()), v.end());
        columns[c].nnz = (int)v.size();
    }
    entries.clear();
    entries.shrink_to_fit();

    //----------------------------------------------------------------------------
    // 파라미터
    //----------------------------------------------------------------------------
    const int K=64;            // 클러스터 개수
    const int MAX_ITER=5;      // 반복 횟수
    const double lambda=0.3;   // NNZ 불균형 페널티 계수 (조정 가능)
    const double eps=1e-9;     // 분모 0 방지

    //----------------------------------------------------------------------------
    // 3) 초기 대표열(medoid): NNZ 큰 순으로 상위 K개 선택
    //   (원한다면 다른 방식(무작위, k-means++)로 초기화 가능)
    //----------------------------------------------------------------------------
    vector<int> colIdx(N);
    iota(colIdx.begin(), colIdx.end(), 0); // 0..N-1
    sort(colIdx.begin(), colIdx.end(), [&](int a, int b){
        return columns[a].nnz > columns[b].nnz; // 내림차순
    });
    vector<int> medoids;
    for(int i=0; i<min(K,N); i++){
        medoids.push_back(colIdx[i]);
    }
    cerr << "[INFO] Picked top "<<medoids.size()<<" columns by NNZ as medoids.\n";

    //----------------------------------------------------------------------------
    // 4) 반복: 자카드 유사도 - lambda * loadPenalty
    //----------------------------------------------------------------------------
    // (메도이드 갱신은 표준 k-medoids와 동일)
    bool converged=false;
    for(int iter=0; iter<MAX_ITER && !converged; iter++){
        cerr << "[INFO] Iteration "<<iter<<"/"<<MAX_ITER<<"\n";

        // (a) 클러스터 통계 -> clusterNNZ, avgNNZ
        vector<long long> clusterNNZ(K,0LL);
        vector<int> clusterCount(K,0);
        long long totalNnzAll=0LL;
        double avgNnz=0.0;
        
        // 먼저 "현재" 할당 상태(처음엔 clusterID=-1이지만)에 대해 통계 계산
        // (사실 첫 iter에는 전부 -1이므로 0이긴 하지만,
        //  두 번째 iter부터는 바뀐 상태가 될 것)
        computeClusterStats(columns, K, clusterNNZ, clusterCount, totalNnzAll, avgNnz);

        // (b) 할당 단계 (batch):
        //     각 열 c -> argmax_k [ jaccardSim(c, medoid_k) - lambda*(clusterNNZ[k]/(avgNnz+eps)) ]
        //     단, clusterNNZ[k]는 "이전 iteration" 상태(혹은 직전 상태)
        //     완벽하게 즉시 갱신하려면 복잡해지므로 여기서는 배치 업데이트
        int changes=0;
        for(int c=0; c<N; c++){
            double bestScore = -1e15; 
            int bestK = columns[c].clusterID; // 기존 할당
            // (기존엔 -1일 수도 있음)

            for(int k=0; k<K; k++){
                int mcol = medoids[k];
                double sim = jaccardSimilarity(columns[c].rowIndices, columns[mcol].rowIndices);
                // load penalty
                double penalty = (double)clusterNNZ[k] / (avgNnz + eps);
                double score = sim - lambda * penalty;

                if(score>bestScore){
                    bestScore=score;
                    bestK=k;
                }
            }
            if(bestK!=columns[c].clusterID){
                changes++;
                columns[c].clusterID=bestK;
            }
        }
        cerr << "[DEBUG] assignment changes="<<changes<<"\n";

        if(changes==0 && iter>0){
            // 더 이상 바뀔 열이 없으면 수렴
            cerr << "[INFO] No assignment changes - converged.\n";
            break;
        }

        // (c) 대표열 갱신
        //     각 클러스터에 속한 열들 중, "내부 유사도 합"이 최대인 열을 새 medoid로
        vector<vector<int>> clusterMembers(K);
        for(int c=0; c<N; c++){
            int cid=columns[c].clusterID;
            clusterMembers[cid].push_back(c);
        }
        int medoidChanges=0;
        for(int k=0; k<K; k++){
            auto &memb=clusterMembers[k];
            if(memb.empty()) continue;

            int currMedoid=medoids[k];
            double bestSum=-1.0;
            int newMedoid=currMedoid;

            // O(|memb|^2) 자카드 계산
            for(auto &cand : memb){
                double sumSim=0.0;
                for(auto &c2 : memb){
                    if(c2==cand) continue;
                    sumSim += jaccardSimilarity(columns[cand].rowIndices, columns[c2].rowIndices);
                }
                if(sumSim>bestSum){
                    bestSum=sumSim;
                    newMedoid=cand;
                }
            }
            if(newMedoid!=currMedoid){
                medoids[k]=newMedoid;
                medoidChanges++;
            }
        }
        cerr << "[DEBUG] medoid changes="<<medoidChanges<<"\n";
        if(medoidChanges==0 && changes==0){
            cerr << "[INFO] Full convergence at iteration "<<iter<<"\n";
            converged=true;
        }
    }

    //----------------------------------------------------------------------------
    // 5) 최종 결과
    //----------------------------------------------------------------------------
    // 최종 통계
    vector<long long> finalClusterNNZ(K,0LL);
    vector<int> finalClusterCount(K,0);
    long long finalTotal=0LL;
    double finalAvg=0.0;

    computeClusterStats(columns, K, finalClusterNNZ, finalClusterCount, finalTotal, finalAvg);

    cout << "---------------------------------------------\n";
    cout << "[RESULT] #clusters="<<K<<"\n";
    cout << "[RESULT] totalNNZ="<<finalTotal<<", avgNNZ="<<finalAvg<<"\n";
    for(int k=0; k<K; k++){
        cout << "  Cluster "<<k<<": #cols="<<finalClusterCount[k]
             << ", totalNNZ="<<finalClusterNNZ[k]
             << ", medoid col="<<medoids[k]
             << "\n";
    }

    // (원하면 columns c->clusterID 출력)
    cout << "[INFO] Column->cluster (first 50)\n";
    for(int c=0; c<min(N,50); c++){
        cout << "  col="<<c<<", clusterID="<<columns[c].clusterID
             <<", nnz="<<columns[c].nnz<<"\n";
    }
    cout << "[INFO] Done.\n";
    return 0;
}

