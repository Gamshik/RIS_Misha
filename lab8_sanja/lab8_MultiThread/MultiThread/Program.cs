using System.Globalization;
using System.Threading.Tasks;

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

        double[,] L = new double[N, N];
        double[,] U = new double[N, N];
        double[] D = new double[N];

        ParallelOptions popt = new ParallelOptions { MaxDegreeOfParallelism = numThreads };

        // L* D U* разложение
        for (int i = 0; i < N; i++)
        {
            L[i, i] = 1.0; // диагональ L* = 1

            // Вычисление U[i, j] параллельно для j >= i
            Parallel.For(i, N, popt, j =>
            {
                double sumU = 0;
                for (int k = 0; k < i; k++)
                    sumU += L[i, k] * D[k] * U[k, j];
                U[i, j] = A[i, j] - sumU;
            });

            D[i] = U[i, i];  // диагональ D
            U[i, i] = 1.0;   // 1 на диагонали U*

            // Вычисление L[j, i] параллельно для j > i
            if (i < N - 1)
            {
                Parallel.For(i + 1, N, popt, j =>
                {
                    double sumL = 0;
                    for (int k = 0; k < i; k++)
                        sumL += L[j, k] * D[k] * U[k, i];
                    L[j, i] = (A[j, i] - sumL) / D[i];
                });
            }
        }

        // Прямой ход: L* Z = B
        double[] Z = new double[N];
        for (int i = 0; i < N; i++)
        {
            double sum = B[i];
            for (int j = 0; j < i; j++)
                sum -= L[i, j] * Z[j];
            Z[i] = sum; // делить не нужно, диагональ L* = 1
        }

        // D * Y = Z
        double[] Y = new double[N];
        for (int i = 0; i < N; i++)
            Y[i] = Z[i] / D[i];

        // Обратный ход: U* X = Y
        double[] X = new double[N];
        for (int i = N - 1; i >= 0; i--)
        {
            double sum = Y[i];
            for (int j = i + 1; j < N; j++)
                sum -= U[i, j] * X[j];
            X[i] = sum; // делить не нужно, диагональ U* = 1
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
