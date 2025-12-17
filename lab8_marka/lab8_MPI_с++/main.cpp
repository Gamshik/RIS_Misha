#include <mpi.h>
#include <windows.h> // Обязательно для твоей функции логирования
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <map>

using namespace std;

// ==========================================================
// ТВОЯ ФУНКЦИЯ ЛОГИРОВАНИЯ
// ==========================================================
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
    
    // Используем ios::app для дозаписи
    ofstream logFile(logPath, ios::app);
    if (!logFile.is_open()) return;
    
    // Добавляем перенос строки, если его нет
    logFile << utf8Str << endl;
}

// Вспомогательная функция для конвертации string -> wstring (для логов)
wstring to_wstr(const string& str) {
    if (str.empty()) return wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// ==========================================================
// Чтение данных
// ==========================================================
void ReadMatrixRows(const string& path, vector<vector<double>>& rows, int& N) {
    ifstream fin(path);
    if (!fin.is_open()) return; 
    
    string line;
    while (getline(fin, line)) {
        if (line.empty()) continue;
        istringstream ss(line);
        vector<double> row;
        double v;
        while (ss >> v) row.push_back(v);
        if (!row.empty()) rows.push_back(move(row));
    }
    fin.close();
    N = (int)rows.size();
}

void ReadVector(const string& path, vector<double>& B, int N) {
    ifstream fin(path);
    if (!fin.is_open()) return;
    B.resize(N);
    for (int i = 0; i < N; ++i) fin >> B[i];
    fin.close();
}

void WriteVector(const string& path, const vector<double>& X) {
    ofstream fout(path);
    fout << setprecision(17);
    for (double v : X) fout << v << "\n";
    fout.close();
}

// ==========================================================
// Работа с диагоналями
// ==========================================================
// Индекс диагонали: k = j - i.
int GetDiagIndex(int i, int j) {
    return j - i;
}

// Определяет владельца диагонали (Cyclic distribution)
int GetDiagOwner(int diagIndex, int N, int size) {
    long long shifted = (long long)diagIndex + N;
    return (int)(shifted % size);
}

// ==========================================================
// Main
// ==========================================================
int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2) {
        if (rank == 0) logToFile(L"Ошибка: Ожидался аргумент — путь к папке с данными.");
        MPI_Finalize();
        return 0;
    }

    string folder = argv[1];
    string fileA = folder + "\\A.txt";
    string fileB = folder + "\\B.txt";
    string fileX = folder + "\\X.txt";

    int N = 0;
    vector<vector<double>> tempRows; 
    vector<double> B;

    // 1. Чтение данных (Только Rank 0)
    if (rank == 0) {
        logToFile(L"[ROOT] Начинаю чтение матрицы из: " + to_wstr(fileA));
        ReadMatrixRows(fileA, tempRows, N);
        
        if (N == 0) {
            logToFile(L"[ROOT] Ошибка: Матрица пуста или файл не найден.");
        } else {
            logToFile(L"[ROOT] Размер матрицы: " + to_wstring(N) + L"x" + to_wstring(N));
            ReadVector(fileB, B, N);
        }
    }

    // Рассылка размера N всем процессам
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (N == 0) { MPI_Finalize(); return 0; }

    // 2. Распределение матрицы по диагоналям
    // Локальное хранилище: map[diagIndex] -> vector<values>
    map<int, vector<double>> myDiags;

    if (rank == 0) {
        logToFile(L"[ROOT] Распределяю диагонали между процессами...");
        
        vector<vector<double>> sendBuffers(size);
        
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < N; ++j) {
                int d = GetDiagIndex(i, j);
                int owner = GetDiagOwner(d, N, size);
                sendBuffers[owner].push_back(tempRows[i][j]);
            }
        }
        
        tempRows.clear(); // Освобождаем память

        // Рассылаем
        for (int p = 0; p < size; ++p) {
            if (p == 0) {
                // Копируем себе
                int idx = 0;
                for (int i = 0; i < N; ++i) {
                    for (int j = 0; j < N; ++j) {
                        int d = GetDiagIndex(i, j);
                        if (GetDiagOwner(d, N, size) == 0) {
                            myDiags[d].push_back(sendBuffers[0][idx++]);
                        }
                    }
                }
            } else {
                int count = (int)sendBuffers[p].size();
                MPI_Send(&count, 1, MPI_INT, p, 100, MPI_COMM_WORLD);
                if (count > 0) {
                    MPI_Send(sendBuffers[p].data(), count, MPI_DOUBLE, p, 101, MPI_COMM_WORLD);
                }
            }
        }
    } else {
        // Остальные принимают
        int count = 0;
        MPI_Recv(&count, 1, MPI_INT, 0, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (count > 0) {
            vector<double> buf(count);
            MPI_Recv(buf.data(), count, MPI_DOUBLE, 0, 101, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            int idx = 0;
            for (int i = 0; i < N; ++i) {
                for (int j = 0; j < N; ++j) {
                    int d = GetDiagIndex(i, j);
                    if (GetDiagOwner(d, N, size) == rank) {
                        myDiags[d].push_back(buf[idx++]);
                    }
                }
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) logToFile(L"[ROOT] Распределение завершено. Начинаю LU-разложение...");

    // 3. Алгоритм LU
    // Храним полные матрицы L и U для простоты доступа (репликация данных)
    vector<double> L_full((size_t)N * N, 0.0);
    vector<double> U_full((size_t)N * N, 0.0);

    for (int i = 0; i < N; ++i) L_full[(size_t)i * N + i] = 1.0;

    map<int, int> myDiagPos; // Курсор чтения для каждой диагонали

    double t_start = MPI_Wtime();

    vector<double> rowU(N);
    vector<double> colL(N);
    vector<double> rowU_global(N);
    vector<double> colL_global(N);

    for (int i = 0; i < N; ++i) {
        // --- Строка U ---
        fill(rowU.begin(), rowU.end(), 0.0);
        for (int j = i; j < N; ++j) {
            int d = GetDiagIndex(i, j); 
            if (GetDiagOwner(d, N, size) == rank) {
                double a_val = myDiags[d][myDiagPos[d]];
                myDiagPos[d]++; 

                double sum = 0.0;
                for (int k = 0; k < i; ++k) {
                    sum += L_full[(size_t)i * N + k] * U_full[(size_t)k * N + j];
                }
                rowU[j] = a_val - sum;
            }
        }
        MPI_Allreduce(rowU.data(), rowU_global.data(), N, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        
        for (int j = i; j < N; ++j) U_full[(size_t)i * N + j] = rowU_global[j];

        double u_ii = U_full[(size_t)i * N + i];
        
        // Проверка деления на ноль (сингулярная матрица)
        if (abs(u_ii) < 1e-15) {
             if (rank == 0) logToFile(L"[ERROR] Нулевой элемент на диагонали! i=" + to_wstring(i));
             // В реальной задаче тут нужен break или abort
        }

        // --- Столбец L ---
        fill(colL.begin(), colL.end(), 0.0);
        for (int j = i + 1; j < N; ++j) {
            int d = GetDiagIndex(j, i);
            if (GetDiagOwner(d, N, size) == rank) {
                double a_val = myDiags[d][myDiagPos[d]];
                myDiagPos[d]++; 

                double sum = 0.0;
                for (int k = 0; k < i; ++k) {
                    sum += L_full[(size_t)j * N + k] * U_full[(size_t)k * N + i];
                }
                colL[j] = (a_val - sum) / u_ii;
            }
        }
        MPI_Allreduce(colL.data(), colL_global.data(), N, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        
        for (int j = i + 1; j < N; ++j) L_full[(size_t)j * N + i] = colL_global[j];
    }

    double t_end = MPI_Wtime();

    if (rank == 0) {
        double elapsedMs = (t_end - t_start) * 1000.0;
        logToFile(L"[ROOT] LU-разложение завершено.");

        // 4. Решение СЛАУ (Прямой/Обратный ход)
        vector<double> Z(N);
        for (int i = 0; i < N; ++i) {
            double sum = B[i];
            for (int j = 0; j < i; ++j) sum -= L_full[(size_t)i * N + j] * Z[j];
            Z[i] = sum;
        }

        vector<double> X(N);
        for (int i = N - 1; i >= 0; --i) {
            double sum = Z[i];
            for (int j = i + 1; j < N; ++j) sum -= U_full[(size_t)i * N + j] * X[j];
            X[i] = sum / U_full[(size_t)i * N + i];
        }

        WriteVector(fileX, X);

        wstring report = L"==============================================\n";
        report += L"Размер матрицы: " + to_wstring(N) + L"x" + to_wstring(N) + L"\n";
        report += L"Количество процессов: " + to_wstring(size) + L"\n";
        report += L"Время: " + to_wstring((long long)elapsedMs) + L" мс\n";
        report += L"==============================================\n";
        logToFile(report);
        logToFile(L"[ROOT] Вектор решения X записан в файл.");
    }

    MPI_Finalize();
    return 0;
}