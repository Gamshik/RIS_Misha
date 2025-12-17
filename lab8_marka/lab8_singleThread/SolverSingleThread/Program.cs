using System.Diagnostics;
using System.Globalization;

class Program
{
    static void Main(string[] args)
    {
        // --------------------------------------------
        // Проверка аргументов командной строки
        // --------------------------------------------
        if (args.Length < 1)
        {
            Console.WriteLine("Ожидался аргумент - путь к папке с данными.");
            return;
        }

        string folder = args[0];
        string fileA = Path.Combine(folder, "A.txt");
        string fileB = Path.Combine(folder, "B.txt");
        string fileX = Path.Combine(folder, "X.txt");

        // --------------------------------------------
        // Чтение входных данных
        // --------------------------------------------
        ReadMatrix(fileA, fileB, out double[,] A, out double[] B);
        int N = B.Length;

        Stopwatch sw = Stopwatch.StartNew();

        // ============================================================
        // LU-разложение
        // L – нижнетреугольная матрица (L[i,i] = 1)
        // U – верхнетреугольная матрица
        //
        // ХРАНЕНИЕ:
        //  - Матрицы L и U хранятся в диагональном виде
        //  - Каждая диагональ – отдельный массив
        // ============================================================
        LUDecompositionDiagonal(A, N, out double[][] L, out double[][] U);

        // --------------------------------------------
        // Решение L * Z = B  (прямой ход)
        // --------------------------------------------
        double[] Z = ForwardSubstitution(L, B, N);

        // --------------------------------------------
        // Решение U * X = Z  (обратный ход)
        // --------------------------------------------
        double[] X = BackwardSubstitution(U, Z, N);

        sw.Stop();

        // --------------------------------------------
        // Запись результата
        // --------------------------------------------
        using (var w = new StreamWriter(fileX))
        {
            for (int i = 0; i < N; i++)
                w.WriteLine(X[i].ToString("G17", CultureInfo.InvariantCulture));
        }

        Console.WriteLine("==============================================");
        Console.WriteLine($"  Размер матрицы: {N}");
        Console.WriteLine($"  Время выполнения: {sw.ElapsedMilliseconds} мс");
        Console.WriteLine("==============================================");
    }

    // ============================================================
    // Чтение матрицы A и вектора B из файлов
    // ============================================================
    static void ReadMatrix(string fileA, string fileB,
                           out double[,] A, out double[] B)
    {
        var aLines = File.ReadAllLines(fileA);
        var bLines = File.ReadAllLines(fileB);
        int N = aLines.Length;

        A = new double[N, N];
        B = new double[N];

        for (int i = 0; i < N; i++)
        {
            var parts = aLines[i].Split(' ', StringSplitOptions.RemoveEmptyEntries);
            for (int j = 0; j < N; j++)
                A[i, j] = double.Parse(parts[j], CultureInfo.InvariantCulture);
        }

        for (int i = 0; i < N; i++)
            B[i] = double.Parse(bLines[i], CultureInfo.InvariantCulture);
    }

    // ============================================================
    // LU-разложение с диагональным размещением
    //
    // U[d][i] = U[i][i + d]   – главная и верхние диагонали
    // L[d][j] = L[j + d][j]   – диагонали ниже главной
    //
    // Главная диагональ L не хранится (L[i,i] = 1)
    // ============================================================
    static void LUDecompositionDiagonal(
        double[,] A, int N,
        out double[][] L,
        out double[][] U)
    {
        // --------------------------------------------
        // Инициализация диагоналей L
        // d = i - j > 0
        // --------------------------------------------
        L = new double[N][];
        for (int d = 1; d < N; d++)
            L[d] = new double[N - d];

        // --------------------------------------------
        // Инициализация диагоналей U
        // d = j - i >= 0
        // --------------------------------------------
        U = new double[N][];
        for (int d = 0; d < N; d++)
            U[d] = new double[N - d];

        // --------------------------------------------
        // Основной алгоритм LU
        // --------------------------------------------
        for (int i = 0; i < N; i++)
        {
            // ---------- Вычисление U ----------
            for (int j = i; j < N; j++)
            {
                double sum = 0.0;
                for (int k = 0; k < i; k++)
                    sum += GetL(L, i, k) * GetU(U, k, j);

                SetU(U, i, j, A[i, j] - sum);
            }

            // ---------- Вычисление L ----------
            for (int j = i + 1; j < N; j++)
            {
                double sum = 0.0;
                for (int k = 0; k < i; k++)
                    sum += GetL(L, j, k) * GetU(U, k, i);

                SetL(L, j, i, (A[j, i] - sum) / GetU(U, i, i));
            }
        }
    }

    // ============================================================
    // Прямой ход: L * Z = B
    // ============================================================
    static double[] ForwardSubstitution(double[][] L, double[] B, int N)
    {
        double[] Z = new double[N];

        for (int i = 0; i < N; i++)
        {
            double sum = B[i];
            for (int j = 0; j < i; j++)
                sum -= GetL(L, i, j) * Z[j];

            // Деление не требуется, т.к. L[i,i] = 1
            Z[i] = sum;
        }

        return Z;
    }

    // ============================================================
    // Обратный ход: U * X = Z
    // ============================================================
    static double[] BackwardSubstitution(double[][] U, double[] Z, int N)
    {
        double[] X = new double[N];

        for (int i = N - 1; i >= 0; i--)
        {
            double sum = Z[i];
            for (int j = i + 1; j < N; j++)
                sum -= GetU(U, i, j) * X[j];

            X[i] = sum / GetU(U, i, i);
        }

        return X;
    }

    // ============================================================
    // Доступ к элементам диагонально размещённых матриц
    // ============================================================

    // Получение элемента U[i,j], j >= i
    static double GetU(double[][] U, int i, int j)
        => U[j - i][i];

    // Запись элемента U[i,j]
    static void SetU(double[][] U, int i, int j, double value)
        => U[j - i][i] = value;

    // Получение элемента L[i,j], i >= j
    static double GetL(double[][] L, int i, int j)
        => (i == j) ? 1.0 : L[i - j][j];

    // Запись элемента L[i,j]
    static void SetL(double[][] L, int i, int j, double value)
        => L[i - j][j] = value;
}
