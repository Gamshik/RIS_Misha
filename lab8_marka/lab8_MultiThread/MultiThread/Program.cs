using System.Globalization;
using System.Threading.Tasks;
using System.Diagnostics;

class Program
{
    static void Main(string[] args)
    {
        if (args.Length < 1)
        {
            Console.WriteLine("Ожидался аргумент - путь к папке с данными.");
            return;
        }

        string folder = args[0];

        int numThreads = Environment.ProcessorCount;
        if (args.Length >= 2 && int.TryParse(args[1], out int t))
            numThreads = Math.Max(1, t);

        ReadMatrix(folder, out double[,] A, out double[] B, out int N);

        ParallelOptions popt = new ParallelOptions
        {
            MaxDegreeOfParallelism = numThreads
        };

        Stopwatch sw = Stopwatch.StartNew();

        // ============================================================
        // Диагональное хранение:
        // U[d][i] = U[i][i + d], d >= 0
        // L[d][j] = L[j + d][j], d > 0, L[i,i] = 1
        // ============================================================

        double[][] U = new double[N][];
        for (int d = 0; d < N; d++)
            U[d] = new double[N - d];

        double[][] L = new double[N][];
        for (int d = 1; d < N; d++)
            L[d] = new double[N - d];

        // ============================================================
        // LU-разложение (распараллеливание по j)
        // ============================================================
        for (int i = 0; i < N; i++)
        {
            // ---------- вычисление U[i, j], j >= i ----------
            Parallel.For(i, N, popt, j =>
            {
                double sum = 0.0;
                for (int k = 0; k < i; k++)
                    sum += GetL(L, i, k) * GetU(U, k, j);

                SetU(U, i, j, A[i, j] - sum);
            });

            // ---------- вычисление L[j, i], j > i ----------
            if (i < N - 1)
            {
                Parallel.For(i + 1, N, popt, j =>
                {
                    double sum = 0.0;
                    for (int k = 0; k < i; k++)
                        sum += GetL(L, j, k) * GetU(U, k, i);

                    SetL(L, j, i, (A[j, i] - sum) / GetU(U, i, i));
                });
            }
        }

        // ============================================================
        // Прямой ход: L * Z = B
        // ============================================================
        double[] Z = new double[N];
        for (int i = 0; i < N; i++)
        {
            double sum = B[i];
            for (int j = 0; j < i; j++)
                sum -= GetL(L, i, j) * Z[j];

            Z[i] = sum; // L[i,i] = 1
        }

        // ============================================================
        // Обратный ход: U * X = Z
        // ============================================================
        double[] X = new double[N];
        for (int i = N - 1; i >= 0; i--)
        {
            double sum = Z[i];
            for (int j = i + 1; j < N; j++)
                sum -= GetU(U, i, j) * X[j];

            X[i] = sum / GetU(U, i, i);
        }

        sw.Stop();

        WriteVector(Path.Combine(folder, "X.txt"), X);

        Console.WriteLine("==============================================");
        Console.WriteLine($"  Размер матрицы: {N}");
        Console.WriteLine($"  Потоков: {numThreads}");
        Console.WriteLine($"  Время: {sw.Elapsed.TotalMilliseconds:F3} мс");
        Console.WriteLine("==============================================");
    }

    // ============================================================
    // Ввод
    // ============================================================
    static void ReadMatrix(string folder, out double[,] A, out double[] B, out int N)
    {
        string[] linesA = File.ReadAllLines(Path.Combine(folder, "A.txt"));
        string[] linesB = File.ReadAllLines(Path.Combine(folder, "B.txt"));

        N = linesA.Length;
        A = new double[N, N];
        B = new double[N];

        for (int i = 0; i < N; i++)
        {
            var parts = linesA[i].Split(' ', StringSplitOptions.RemoveEmptyEntries);
            for (int j = 0; j < N; j++)
                A[i, j] = double.Parse(parts[j], CultureInfo.InvariantCulture);
        }

        for (int i = 0; i < N; i++)
            B[i] = double.Parse(linesB[i], CultureInfo.InvariantCulture);
    }

    static void WriteVector(string path, double[] X)
    {
        using var sw = new StreamWriter(path);
        foreach (double v in X)
            sw.WriteLine(v.ToString("G17", CultureInfo.InvariantCulture));
    }

    // ============================================================
    // Доступ к диагонально размещённым матрицам
    // ============================================================

    static double GetU(double[][] U, int i, int j)
        => U[j - i][i];

    static void SetU(double[][] U, int i, int j, double value)
        => U[j - i][i] = value;

    static double GetL(double[][] L, int i, int j)
        => (i == j) ? 1.0 : L[i - j][j];

    static void SetL(double[][] L, int i, int j, double value)
        => L[i - j][j] = value;
}
