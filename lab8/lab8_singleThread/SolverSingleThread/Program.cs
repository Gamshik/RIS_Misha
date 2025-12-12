using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;

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

        // Решение СЛАУ через U^T U-разложение
        double[,] U = CholeskyDecomposition(A, N);
        double[] Y = ForwardSubstitution(U, B, N);  // Решаем U^T * Y = B
        double[] X = BackwardSubstitution(U, Y, N); // Решаем U * X = Y

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

    // Cholesky-разложение A = U^T * U
    static double[,] CholeskyDecomposition(double[,] A, int N)
    {
        double[,] U = new double[N, N];

        for (int i = 0; i < N; i++)
        {
            for (int j = i; j < N; j++)
            {
                double sum = A[i, j];
                for (int k = 0; k < i; k++)
                    sum -= U[k, i] * U[k, j];

                if (i == j)
                {
                    if (sum <= 0)
                        throw new Exception("Матрица не является положительно определенной");
                    U[i, i] = Math.Sqrt(sum);
                }
                else
                {
                    U[i, j] = sum / U[i, i];
                }
            }
        }

        return U;
    }

    // Решение U^T * Y = B
    static double[] ForwardSubstitution(double[,] U, double[] B, int N)
    {
        double[] Y = new double[N];

        for (int i = 0; i < N; i++)
        {
            double sum = B[i];
            for (int k = 0; k < i; k++)
                sum -= U[k, i] * Y[k];
            Y[i] = sum / U[i, i];
        }

        return Y;
    }

    // Решение U * X = Y
    static double[] BackwardSubstitution(double[,] U, double[] Y, int N)
    {
        double[] X = new double[N];

        for (int i = N - 1; i >= 0; i--)
        {
            double sum = Y[i];
            for (int j = i + 1; j < N; j++)
                sum -= U[i, j] * X[j];
            X[i] = sum / U[i, i];
        }

        return X;
    }
}
