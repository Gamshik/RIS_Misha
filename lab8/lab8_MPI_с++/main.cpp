#include <mpi.h>
#include <windows.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <algorithm>

using namespace std;

void logToFile(const wstring& message) {
    const string logPath = "log.txt";
    int size_needed = WideCharToMultiByte(
        CP_UTF8, 0, message.c_str(), (int)message.size(),
        nullptr, 0, nullptr, nullptr
    );
    string utf8Str(size_needed, 0);
    WideCharToMultiByte(
        CP_UTF8, 0, message.c_str(), (int)message.size(),
        &utf8Str[0], size_needed, nullptr, nullptr
    );
    ofstream logFile(logPath, ios::app);
    if (!logFile.is_open()) return;
    logFile << utf8Str << endl;
}

// Read full matrix A from file path into rows vector-of-vector
void ReadMatrixRows(const string& path, vector<vector<double>>& rows)
{
    ifstream fin(path);
    if (!fin) throw runtime_error("Cannot open " + path);
    string line;
    while (getline(fin, line))
    {
        istringstream ss(line);
        vector<double> row;
        double v;
        while (ss >> v) row.push_back(v);
        if (!row.empty()) rows.push_back(move(row));
    }
    fin.close();
}

void ReadVector(const string& path, vector<double>& B, int N)
{
    ifstream fin(path);
    if (!fin) throw runtime_error("Cannot open " + path);
    B.assign(N, 0.0);
    for (int i = 0; i < N; ++i)
        if (!(fin >> B[i])) throw runtime_error("Vector file does not match matrix size");
    fin.close();
}

void WriteVector(const string& path, const vector<double>& X)
{
    ofstream fout(path);
    fout << setprecision(17);
    for (double v : X) fout << v << "\n";
    fout.close();
}

int main(int argc, char* argv[])
{
    // ensure console uses UTF-8 for input/output, and we still use printW for Cyrillic output
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    MPI_Init(&argc, &argv);
    int rank = 0, size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2)
    {
        MPI_Finalize();
        return 0;
    }

    string folder = argv[1];
    string fileA = folder + "/A.txt";
    string fileB = folder + "/B.txt";
    string fileX = folder + "/X.txt";

    int N = 0;
    vector<vector<double>> rows;
    vector<double> B;

    if (rank == 0)
    {
        try {
            logToFile(L"[ROOT] Читаю матрицу...");
            ReadMatrixRows(fileA, rows);
            N = (int)rows.size();
            if (N == 0) throw runtime_error("Matrix A is empty");
            for (int i = 0; i < N; ++i)
                if ((int)rows[i].size() != N) throw runtime_error("Matrix is not square");
            ReadVector(fileB, B, N);
            logToFile(L"[ROOT] Матрица и вектор считаны.");
        }
        catch (const exception& ex) {
            wstring msg = L"Ошибка чтения входных файлов: ";
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }
    }

    // Broadcast N, then B
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (N <= 0) {
        MPI_Finalize();
        return 1;
    }

    if (rank != 0) B.assign(N, 0.0);
    MPI_Bcast(B.data(), N, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // Determine local columns for each process: columns with j % size == rank
    vector<int> myColumns;
    for (int j = 0; j < N; ++j) if (j % size == rank) myColumns.push_back(j);
    int localCols = (int)myColumns.size();

    // ALocal: store local columns in column-major within local buffer: index localIdx*N + i -> A[i][col]
    vector<double> ALocal;
    if (localCols > 0) ALocal.assign((size_t)localCols * N, 0.0);

    // Root distributes columns
    if (rank == 0)
    {
        // fill own ALocal
        for (int localIdx = 0; localIdx < localCols; ++localIdx) {
            int col = myColumns[localIdx];
            for (int i = 0; i < N; ++i) ALocal[(size_t)localIdx * N + i] = rows[i][col];
        }

        // send to others
        for (int p = 1; p < size; ++p)
        {
            // collect columns for p
            vector<int> colsP;
            for (int j = 0; j < N; ++j) if (j % size == p) colsP.push_back(j);
            int pCols = (int)colsP.size();
            MPI_Send(&pCols, 1, MPI_INT, p, 0, MPI_COMM_WORLD);
            if (pCols > 0) {
                MPI_Send(colsP.data(), pCols, MPI_INT, p, 1, MPI_COMM_WORLD);
                vector<double> data((size_t)pCols * N);
                for (int localIdx = 0; localIdx < pCols; ++localIdx) {
                    int col = colsP[localIdx];
                    for (int i = 0; i < N; ++i) data[(size_t)localIdx * N + i] = rows[i][col];
                }
                MPI_Send(data.data(), pCols * N, MPI_DOUBLE, p, 2, MPI_COMM_WORLD);
            }
        }

        // free rows to save memory (optional)
        rows.clear();
        rows.shrink_to_fit();

        wstring msg = L"[ROOT] Столбцы распределены. \nМой локальный столбцов: " + to_wstring(localCols) + L"";
        logToFile(msg);
    }
    else
    {
        int pCols = 0;
        MPI_Recv(&pCols, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (pCols > 0) {
            vector<int> cols(pCols);
            MPI_Recv(cols.data(), pCols, MPI_INT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            myColumns = cols;
            localCols = pCols;
            ALocal.assign((size_t)localCols * N, 0.0);
            vector<double> data((size_t)pCols * N);
            MPI_Recv(data.data(), pCols * N, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (int localIdx = 0; localIdx < pCols; ++localIdx)
                for (int i = 0; i < N; ++i)
                    ALocal[(size_t)localIdx * N + i] = data[(size_t)localIdx * N + i];
        }
        wstring msg = L"[WORKER " + to_wstring(rank) + L"] получил " + to_wstring(localCols) + L" столбцов";
        logToFile(msg);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // Ufull: replicated full upper triangular matrix stored full (row-major)
    vector<double> Ufull((size_t)N * N, 0.0);
    vector<double> Ucol(N, 0.0); // buffer to broadcast column

    auto t1 = chrono::high_resolution_clock::now();

    // Main Cholesky loop: compute columns k = 0..N-1
    for (int k = 0; k < N; ++k)
    {
        int owner = k % size;

        if (rank == owner)
        {
            // find local index for column k
            int localIdx = -1;
            for (int idx = 0; idx < localCols; ++idx) if (myColumns[idx] == k) { localIdx = idx; break; }
            if (localIdx == -1) {
                MPI_Abort(MPI_COMM_WORLD, 2);
            }

            // compute U[0..k,k] (column) sequentially using previously broadcasted columns in Ufull
            for (int i = 0; i <= k; ++i)
            {
                // s = A[i,k] - sum_{p=0..i-1} U[p,i] * U[p,k]
                double s = ALocal[(size_t)localIdx * N + i]; // A[i,k]
                for (int p = 0; p < i; ++p)
                {
                    double upi = Ufull[(size_t)p * N + i]; // U[p,i]
                    double upk = Ufull[(size_t)p * N + k]; // U[p,k]
                    s -= upi * upk;
                }
                if (i < k)
                {
                    double uii = Ufull[(size_t)i * N + i];
                    if (fabs(uii) < 1e-18) {
                        wstring msg = L"Нулевой диагональный элемент U[" + to_wstring(i) + L"," + to_wstring(i) + L"]";
                        logToFile(msg);
                        MPI_Abort(MPI_COMM_WORLD, 3);
                    }
                    Ucol[i] = s / uii;
                }
                else // i == k
                {
                    if (s <= 0.0) {
                        wstring msg = L"Матрица не является положительно определенной на k=" + to_wstring(k) + L"";
                        logToFile(msg);
                        MPI_Abort(MPI_COMM_WORLD, 4);
                    }
                    Ucol[k] = sqrt(s);
                }
            }

            // write computed entries to Ufull
            for (int i = 0; i <= k; ++i) Ufull[(size_t)i * N + k] = Ucol[i];
            // ensure tail zeros (not necessary for broadcast of k+1 elements, but keep safe)
            for (int i = k + 1; i < N; ++i) Ucol[i] = 0.0;
        }

        MPI_Bcast(Ucol.data(), k + 1, MPI_DOUBLE, owner, MPI_COMM_WORLD);

        for (int i = 0; i <= k; ++i) Ufull[(size_t)i * N + k] = Ucol[i];

    }

    auto t2 = chrono::high_resolution_clock::now();

    // Solve on rank 0
    if (rank == 0)
    {
        vector<double> Y(N, 0.0);
        for (int i = 0; i < N; ++i)
        {
            double sum = B[i];
            for (int p = 0; p < i; ++p) sum -= Ufull[(size_t)p * N + i] * Y[p];
            double uii = Ufull[(size_t)i * N + i];
            Y[i] = sum / uii;
        }

        vector<double> X(N, 0.0);
        for (int i = N - 1; i >= 0; --i)
        {
            double sum = Y[i];
            for (int j = i + 1; j < N; ++j) sum -= Ufull[(size_t)i * N + j] * X[j];
            X[i] = sum / Ufull[(size_t)i * N + i];
        }

        WriteVector(fileX, X);

        double decomp_ms = chrono::duration<double, milli>(t2 - t1).count();
        wstring out = L"==============================================\n";
        out += L"  Размер матрицы: " + to_wstring(N) + L"x" + to_wstring(N) + L"\n";
        out += L"  Процессов: " + to_wstring(size) + L"\n";
        out += L"  Время: " + to_wstring((long long)decomp_ms) + L" мс\n";
        out += L"==============================================\n";
        logToFile(out);
    }

    // cleanup
    // no dynamic 'rows' kept on root any more; clear local buffers
    ALocal.clear();
    ALocal.shrink_to_fit();
    Ufull.clear();
    Ucol.clear();

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return 0;
}
