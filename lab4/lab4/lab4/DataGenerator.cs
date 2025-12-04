namespace lab4
{
    // INFO: не думаю, что тебе тут нужны объяснения, просто генерим рандом дату, плюс башик не смотрит такое, что я тебе говорил во 2 лабе
    public static class DataGenerator
    {
        private static Random random = new Random(42);

        public static double[,] GeneratePositiveDefiniteMatrix(int size)
        {
            double[,] B = new double[size, size];
            for (int i = 0; i < size; i++)
            {
                for (int j = 0; j < size; j++)
                {
                    B[i, j] = random.NextDouble() * 2 - 1; // от -1 до 1
                }
            }

            double[,] A = new double[size, size];
            for (int i = 0; i < size; i++)
            {
                for (int j = 0; j < size; j++)
                {
                    double sum = 0;
                    for (int k = 0; k < size; k++)
                    {
                        sum += B[k, i] * B[k, j];
                    }
                    A[i, j] = sum;
                }
            }

            double lambda = size * 0.1;
            for (int i = 0; i < size; i++)
            {
                A[i, i] += lambda;
            }

            return A;
        }

        public static double[] GenerateVector(int size)
        {
            double[] vector = new double[size];
            for (int i = 0; i < size; i++)
            {
                vector[i] = random.NextDouble() * 10 - 5; // от -5 до 5
            }
            return vector;
        }

        public static void GenerateTestData(int taskCount = 10, int minSize = 100, int maxSize = 300)
        {
            Console.WriteLine();
            Console.WriteLine("--------------- ГЕНЕРАЦИЯ ТЕСТОВЫХ ДАННЫХ ---------------");
            Console.WriteLine();

            List<string> tasksList = new List<string>();

            for (int i = 1; i <= taskCount; i++)
            {
                int size = random.Next(minSize, maxSize + 1);

                Console.Write($"[{i}/{taskCount}] Генерация задачи размерности {size}...\n");

                double[,] A = GeneratePositiveDefiniteMatrix(size);
                double[] b = GenerateVector(size);

                string matrixFile = $"a{i}.csv";
                string vectorFile = $"b{i}.csv";

                MatrixHelper.WriteMatrixToCsv(matrixFile, A);
                MatrixHelper.WriteVectorToCsv(vectorFile, b);

                tasksList.Add($"{matrixFile},{vectorFile}");
            }
     
            File.WriteAllLines("tasks.txt", tasksList);

            Console.WriteLine();
            Console.WriteLine($"[INFO] Сгенерировано задач: {taskCount}");
            Console.WriteLine($"[INFO] Файл tasks.txt создан");
            Console.WriteLine();
        }

        public static bool CheckDataExists()
        {
            if (!File.Exists("tasks.txt"))
                return false;

            var lines = File.ReadAllLines("tasks.txt");
            foreach (var line in lines)
            {
                var files = line.Split(',');
                if (files.Length != 2)
                    return false;

                if (!File.Exists(files[0].Trim()) || !File.Exists(files[1].Trim()))
                    return false;
            }

            return true;
        }
    }
}
