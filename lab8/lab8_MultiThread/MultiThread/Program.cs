using System.Globalization;

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

        double[,] A;
        double[] B;
        int N;

        ReadMatrix(folder, out A, out B, out N);

        var sw = System.Diagnostics.Stopwatch.StartNew();

        double[,] U = new double[N, N];
        ParallelOptions popt = new ParallelOptions { MaxDegreeOfParallelism = numThreads };

        // Cholesky-разложение (U^T * U)
        for (int k = 0; k < N; k++)
        {
            double sum = A[k, k];
            for (int p = 0; p < k; p++)
                sum -= U[p, k] * U[p, k];

            if (sum <= 0)
                throw new Exception("Матрица не является положительно определенной");

            U[k, k] = Math.Sqrt(sum);

            // Обновляем строки j > k параллельно
            int trailing = N - (k + 1);
            if (trailing > 0)
            {
                Parallel.For(k + 1, N, popt, i =>
                {
                    double s = A[k, i];
                    for (int p = 0; p < k; p++)
                        s -= U[p, k] * U[p, i];
                    U[k, i] = s / U[k, k];
                });
            }
        }

        // Решение U^T * Y = B (прямой ход)
        double[] Y = new double[N];
        for (int i = 0; i < N; i++)
        {
            double sum = B[i];
            for (int k = 0; k < i; k++)
                sum -= U[k, i] * Y[k];
            Y[i] = sum / U[i, i];
        }

        // Решение U * X = Y (обратный ход)
        double[] X = new double[N];
        for (int i = N - 1; i >= 0; i--)
        {
            double sum = Y[i];
            for (int j = i + 1; j < N; j++)
                sum -= U[i, j] * X[j];
            X[i] = sum / U[i, i];
        }

        sw.Stop();

        WriteVector(Path.Combine(folder, "X.txt"), X);

        Console.WriteLine("==============================================");
        Console.WriteLine($"  Размер матрицы: {N}x{N}");
        Console.WriteLine($"  Потоков: {numThreads}");
        Console.WriteLine($"  Время: {sw.Elapsed.TotalMilliseconds:F3} мс");
        Console.WriteLine("==============================================");
    }

    static void ReadMatrix(string folder, out double[,] A, out double[] B, out int N)
    {
        string pathA = Path.Combine(folder, "A.txt");
        string pathB = Path.Combine(folder, "B.txt");

        string[] linesA = File.ReadAllLines(pathA);
        N = linesA.Length;
        A = new double[N, N];
        for (int i = 0; i < N; i++)
        {
            string[] parts = linesA[i].Split(' ', StringSplitOptions.RemoveEmptyEntries);
            for (int j = 0; j < N; j++)
                A[i, j] = double.Parse(parts[j], CultureInfo.InvariantCulture);
        }

        string[] linesB = File.ReadAllLines(pathB);
        B = new double[N];
        for (int i = 0; i < N; i++)
            B[i] = double.Parse(linesB[i], CultureInfo.InvariantCulture);
    }

    static void WriteVector(string path, double[] X)
    {
        using var sw = new StreamWriter(path);
        foreach (double v in X)
            sw.WriteLine(v.ToString("G17", CultureInfo.InvariantCulture));
    }
}
