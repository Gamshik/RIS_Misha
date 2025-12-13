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

// Логирование сообщений в UTF-8 файл
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

// Считываем матрицу A построчно
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

// Считываем вектор B
void ReadVector(const string& path, vector<double>& B, int N)
{
    ifstream fin(path);
    if (!fin) throw runtime_error("Cannot open " + path);
    B.assign(N, 0.0);
    for (int i = 0; i < N; ++i)
        if (!(fin >> B[i])) throw runtime_error("Vector file does not match matrix size");
    fin.close();
}

// Записываем вектор X
void WriteVector(const string& path, const vector<double>& X)
{
    ofstream fout(path);
    fout << setprecision(17);
    for (double v : X) fout << v << "\n";
    fout.close();
}

int main(int argc, char* argv[])
{
    // UTF-8 для консоли (Windows)
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

    // Рассылаем N и B всем процессам
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (N <= 0) { MPI_Finalize(); return 1; }

    if (rank != 0) B.assign(N, 0.0);
    MPI_Bcast(B.data(), N, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // Определяем локальные столбцы для процесса
    vector<int> myColumns;
    for (int j = 0; j < N; ++j) if (j % size == rank) myColumns.push_back(j);
    int localCols = (int)myColumns.size();

    vector<double> ALocal;
    if (localCols > 0) ALocal.assign((size_t)localCols * N, 0.0);

    // ROOT рассылает столбцы
    if (rank == 0)
    {
        for (int localIdx = 0; localIdx < localCols; ++localIdx) {
            int col = myColumns[localIdx];
            for (int i = 0; i < N; ++i) ALocal[(size_t)localIdx * N + i] = rows[i][col];
        }

        for (int p = 1; p < size; ++p)
        {
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
        rows.clear(); rows.shrink_to_fit();
        logToFile(L"[ROOT] Столбцы распределены. Мой локальный столбцов: " + to_wstring(localCols));
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
        logToFile(L"[WORKER " + to_wstring(rank) + L"] получил " + to_wstring(localCols) + L" столбцов");
    }

    MPI_Barrier(MPI_COMM_WORLD);

    vector<double> Ufull((size_t)N * N, 0.0);
    vector<double> Ucol(N, 0.0);

    auto t1 = chrono::high_resolution_clock::now();

    // Основной цикл Cholesky
    for (int k = 0; k < N; ++k)
    {
        int owner = k % size;
        if (rank == owner)
        {
            int localIdx = -1;
            for (int idx = 0; idx < localCols; ++idx) if (myColumns[idx] == k) { localIdx = idx; break; }
            if (localIdx == -1) MPI_Abort(MPI_COMM_WORLD, 2);

            for (int i = 0; i <= k; ++i)
            {
                double s = ALocal[(size_t)localIdx * N + i];
                for (int p = 0; p < i; ++p) s -= Ufull[(size_t)p * N + i] * Ufull[(size_t)p * N + k];
                if (i < k) Ucol[i] = s / Ufull[(size_t)i * N + i];
                else
                {
                    if (s <= 0.0) {
                        logToFile(L"Матрица не является положительно определенной на k=" + to_wstring(k));
                        MPI_Abort(MPI_COMM_WORLD, 4);
                    }
                    Ucol[k] = sqrt(s);
                }
            }

            for (int i = 0; i <= k; ++i) Ufull[(size_t)i * N + k] = Ucol[i];
            for (int i = k + 1; i < N; ++i) Ucol[i] = 0.0;
        }

        MPI_Bcast(Ucol.data(), k + 1, MPI_DOUBLE, owner, MPI_COMM_WORLD);
        for (int i = 0; i <= k; ++i) Ufull[(size_t)i * N + k] = Ucol[i];
    }

    auto t2 = chrono::high_resolution_clock::now();

    if (rank == 0)
    {
        vector<double> Y(N, 0.0);
        for (int i = 0; i < N; ++i) {
            double sum = B[i];
            for (int p = 0; p < i; ++p) sum -= Ufull[(size_t)p * N + i] * Y[p];
            Y[i] = sum / Ufull[(size_t)i * N + i];
        }

        vector<double> X(N, 0.0);
        for (int i = N - 1; i >= 0; --i) {
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

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return 0;
}
