using System.Diagnostics;
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

        string fileA = Path.Combine(folder, "A.txt");
        string fileB = Path.Combine(folder, "B.txt");
        string fileX = Path.Combine(folder, "X.txt");

        if (!File.Exists(fileA) || !File.Exists(fileB))
        {
            Console.WriteLine("Файлы A.txt или B.txt не найдены в указанной папке.");
            return;
        }

        double[,] A;
        double[] B;

        ReadMatrix(fileA, fileB, out A, out B);

        Stopwatch time = Stopwatch.StartNew();
        int N = B.Length;

        // Разложение L* D U*
        double[,] L;  // L* с 1 на диагонали
        double[,] U;  // U* с 1 на диагонали
        double[] D;   // Диагональная матрица D в виде массива
        LDUDecomposition(A, N, out L, out D, out U);

        // Решение L* Z = B
        double[] Z = ForwardSubstitutionL(L, B, N);

        // Решение D * Y = Z
        double[] Y = new double[N];
        for (int i = 0; i < N; i++)
            Y[i] = Z[i] / D[i];

        // Решение U* X = Y
        double[] X = BackwardSubstitutionU(U, Y, N);

        using (var w = new StreamWriter(fileX))
        {
            for (int i = 0; i < N; i++)
                w.WriteLine(X[i].ToString("G17", CultureInfo.InvariantCulture));
        }

        time.Stop();

        Console.WriteLine("==============================================");
        Console.WriteLine($"  Размер матрицы: {N}x{N}");
        Console.WriteLine($"  Время: {time.ElapsedMilliseconds:F3} мс");
        Console.WriteLine("==============================================");
    }

    static void ReadMatrix(string fileA, string fileB, out double[,] A, out double[] B)
    {
        string[] aLines = File.ReadAllLines(fileA);
        string[] bLines = File.ReadAllLines(fileB);

        int N = aLines.Length;

        A = new double[N, N];
        B = new double[N];

        for (int i = 0; i < N; i++)
        {
            string[] parts = aLines[i].Split(' ', StringSplitOptions.RemoveEmptyEntries);
            for (int j = 0; j < N; j++)
                A[i, j] = double.Parse(parts[j], CultureInfo.InvariantCulture);
        }

        for (int i = 0; i < N; i++)
            B[i] = double.Parse(bLines[i], CultureInfo.InvariantCulture);
    }

    static void LDUDecomposition(double[,] A, int N, out double[,] L, out double[] D, out double[,] U)
    {
        L = new double[N, N];
        U = new double[N, N];
        D = new double[N];

        for (int i = 0; i < N; i++)
        {
            // L* с 1 на диагонали
            L[i, i] = 1.0;

            for (int j = i; j < N; j++)
            {
                double sumU = 0;
                for (int k = 0; k < i; k++)
                    sumU += L[i, k] * D[k] * U[k, j];
                U[i, j] = (A[i, j] - sumU);
            }

            D[i] = U[i, i]; // Диагональ D
            U[i, i] = 1.0;  // 1 на диагонали U*

            for (int j = i + 1; j < N; j++)
            {
                double sumL = 0;
                for (int k = 0; k < i; k++)
                    sumL += L[j, k] * D[k] * U[k, i];
                L[j, i] = (A[j, i] - sumL) / D[i];
            }
        }
    }

    static double[] ForwardSubstitutionL(double[,] L, double[] B, int N)
    {
        double[] Z = new double[N];

        for (int i = 0; i < N; i++)
        {
            double sum = B[i];
            for (int j = 0; j < i; j++)
                sum -= L[i, j] * Z[j];
            Z[i] = sum; // делить не нужно, диагональ L* = 1
        }

        return Z;
    }

    static double[] BackwardSubstitutionU(double[,] U, double[] Y, int N)
    {
        double[] X = new double[N];

        for (int i = N - 1; i >= 0; i--)
        {
            double sum = Y[i];
            for (int j = i + 1; j < N; j++)
                sum -= U[i, j] * X[j];
            X[i] = sum; // делить не нужно, диагональ U* = 1
        }

        return X;
    }
}
