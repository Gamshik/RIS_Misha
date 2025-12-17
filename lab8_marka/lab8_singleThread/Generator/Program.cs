using System.Globalization;
using System.Text;

class Program
{
    static void Main(string[] args)
    {
        Console.OutputEncoding = Encoding.UTF8;

        if (args.Length < 1)
        {
            Console.WriteLine("Ожидался аргумент - путь к папке с данными.");
            return;
        }

        string folder = args[0];

        if (!Directory.Exists(folder)) Directory.CreateDirectory(folder);

        Console.Write("Введите размер N: ");
        int N = int.Parse(Console.ReadLine() ?? "0");

        double[,] A = new double[N, N];
        double[] B = new double[N];

        Random rnd = new Random();

        for (int i = 0; i < N; i++)
        {
            double sum = 0;
            for (int j = 0; j < N; j++)
            {
                if (i == j) continue;

                double val = rnd.NextDouble() * 10 - 5;
                A[i, j] = val;
                sum += Math.Abs(val);
            }

            A[i, i] = sum + rnd.NextDouble() * 5 + 1;
        }

        for (int i = 0; i < N; i++)
            B[i] = rnd.NextDouble() * 10 - 5;

        string fileA = Path.Combine(folder, "A.txt");
        string fileB = Path.Combine(folder, "B.txt");

        using (var writer = new StreamWriter(fileA))
        {
            for (int i = 0; i < N; i++)
            {
                for (int j = 0; j < N; j++)
                {
                    writer.Write(A[i, j].ToString("G17", CultureInfo.InvariantCulture));
                    if (j != N - 1) writer.Write(" ");
                }
                writer.WriteLine();
            }
        }

        using (var writer = new StreamWriter(fileB))
        {
            for (int i = 0; i < N; i++)
                writer.WriteLine(B[i].ToString("G17", CultureInfo.InvariantCulture));
        }

        Console.WriteLine("Файлы A.txt и B.txt успешно созданы по пути:\n" + folder);
    }
}
