#include <bits/stdc++.h>
#include <pthread.h>
#include <atomic>
using namespace std;

#define THREAD_COUNT 4

struct Triple {
    int row;
    int col;
    double val;
};

struct ThreadData {
    int thread_id;
    int part_start;
    int part_end;
    const vector<Triple>* origTriples;
    vector<bool>* used;
    long long origNnz;
    atomic<bool>* failed;
    atomic<long long>* sumPartitionNnz;
};

bool readMtxFile(const string& filename, int& outRows, int& outCols, long long& outNnz, vector<Triple>& triples) {
    ifstream fin(filename);
    if (!fin.is_open()) {
        cerr << "[ERROR] cannot open " << filename << "\n";
        return false;
    }

    string firstLine;
    if (!getline(fin, firstLine) || firstLine.rfind("%%MatrixMarket", 0) != 0) {
        cerr << "[ERROR] not a MatrixMarket file: " << filename << "\n";
        return false;
    }

    while (fin.peek() == '%' || fin.peek() == '\n') {
        string dummy;
        getline(fin, dummy);
    }

    if (!(fin >> outRows >> outCols >> outNnz)) {
        cerr << "[ERROR] read (rows,cols,nnz) fail: " << filename << "\n";
        return false;
    }

    triples.resize(outNnz);
    for (long long i = 0; i < outNnz; i++) {
        int r, c;
        double v;
        if (!(fin >> r >> c >> v)) {
            cerr << "[ERROR] read (r,c,v) fail i=" << i << " in " << filename << "\n";
            return false;
        }
        triples[i] = {r, c, v};
    }

    fin.close();
    return true;
}

void* processPartitions(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    for (int p = data->part_start; p < data->part_end; p++) {
        ostringstream oss;
        oss << "partition_" << p << ".mtx";
        string partFile = oss.str();

        int pRows = 0, pCols = 0;
        long long pNnz = 0;
        vector<Triple> partTriples;
        if (!readMtxFile(partFile, pRows, pCols, pNnz, partTriples)) {
            cerr << "[ERROR] fail read partition file " << partFile << "\n";
            *(data->failed) = true;
            pthread_exit(nullptr);
        }

        data->sumPartitionNnz->fetch_add(pNnz);

        for (const auto& pt : partTriples) {
            bool foundMatch = false;
            for (long long j = 0; j < data->origNnz; j++) {
                if ((*data->used)[j]) continue;
                if ((*data->origTriples)[j].row == pt.row &&
                    (*data->origTriples)[j].col == pt.col /*&&
                    (*data->origTriples)[j].val == pt.val*/) {
                    (*data->used)[j] = true;
                    foundMatch = true;
                    break;
                }
            }

            if (!foundMatch) {
                cerr << "[FAIL] partition_" << p << " has (row=" << pt.row
                     << ", col=" << pt.col << ", val=" << pt.val << ") not found in original.\n";
                *(data->failed) = true;
                pthread_exit(nullptr);
            }
        }
    }

    pthread_exit(nullptr);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <original.mtx>\n"
             << " (will read partition_0.mtx ~ partition_63.mtx in current directory)\n";
        return 1;
    }

    string originalFile = argv[1];
    int origRows = 0, origCols = 0;
    long long origNnz = 0;
    vector<Triple> origTriples;
    if (!readMtxFile(originalFile, origRows, origCols, origNnz, origTriples)) {
        cerr << "[ERROR] fail read original " << originalFile << "\n";
        return 1;
    }
    cerr << "[INFO] original: rows=" << origRows << ", cols=" << origCols << ", nnz=" << origNnz << "\n";

    vector<bool> used(origNnz, false);
    atomic<bool> failed(false);
    atomic<long long> sumPartitionNnz(0);

    pthread_t threads[THREAD_COUNT];
    ThreadData threadData[THREAD_COUNT];

    const int PART_COUNT = 64;
    int partsPerThread = PART_COUNT / THREAD_COUNT;

    for (int t = 0; t < THREAD_COUNT; t++) {
        threadData[t] = {
            t,
            t * partsPerThread,
            (t == THREAD_COUNT - 1) ? PART_COUNT : (t + 1) * partsPerThread,
            &origTriples,
            &used,
            origNnz,
            &failed,
            &sumPartitionNnz
        };
        pthread_create(&threads[t], nullptr, processPartitions, &threadData[t]);
    }

    for (int t = 0; t < THREAD_COUNT; t++) {
        pthread_join(threads[t], nullptr);
    }

    if (failed.load()) {
        return 1;
    }

    if (sumPartitionNnz.load() != origNnz) {
        cerr << "[FAIL] sumPartitionNnz=" << sumPartitionNnz
             << ", but original nnz=" << origNnz << "\n";
        return 1;
    }

    for (long long j = 0; j < origNnz; j++) {
        if (!used[j]) {
            auto& ot = origTriples[j];
            cerr << "[FAIL] original has un-partitioned triple ("
                 << ot.row << "," << ot.col << "," << ot.val << ")\n";
            return 1;
        }
    }

    cerr << "[RESULT] All partition_0..63.mtx matches original EXACTLY.\n";
    return 0;
}

