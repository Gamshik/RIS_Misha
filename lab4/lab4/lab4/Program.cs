namespace lab4
{
    class Program
    {
        // INFO: ну тут тоже всё понятно, сразу запускаем генерацию, если файлов нет, если есть спрашиваем или нужно перегенерить
        // затем однопоточка, многопоточка и верификация, просто юз готовых методов
        static void Main(string[] args)
        {
            if (!DataGenerator.CheckDataExists())
            {
                Console.WriteLine("[INFO] Файлы данных не найдены. Запуск генератора...");
                Console.WriteLine();
                DataGenerator.GenerateTestData(taskCount: 10, minSize: 400, maxSize: 600);
            }
            else
            {
                Console.WriteLine("[INFO] Файлы данных найдены.");
                Console.Write("[?] Сгенерировать новые данные? (y/n): ");
                string answer = Console.ReadLine()?.ToLower();

                if (answer == "y" || answer == "yes" || answer == "д" || answer == "да")
                {
                    Console.WriteLine();
                    DataGenerator.GenerateTestData(taskCount: 10, minSize: 400, maxSize: 600);
                }
                else
                {
                    Console.WriteLine();
                }
            }

            Console.WriteLine("[INFO] Загрузка задач из tasks.txt...");
            var lines = File.ReadAllLines("tasks.txt");
            Console.WriteLine($"[INFO] Найдено задач: {lines.Length}");
            Console.WriteLine($"[INFO] Количество ядер процессора: {Environment.ProcessorCount}");
            Console.WriteLine();

            Console.WriteLine("Нажмите любую клавишу для запуска решения...");
            Console.ReadKey();
            Console.WriteLine();

            SingleThreadRunner singleRunner = new SingleThreadRunner();
            singleRunner.LoadTasks();
            long singleThreadTime = singleRunner.Run();

            Console.WriteLine("Нажмите любую клавишу для запуска многопоточной версии...");
            Console.ReadKey();
            Console.WriteLine();

            MultiThreadRunner multiRunner = new MultiThreadRunner();
            multiRunner.LoadTasks();
            long multiThreadTime = multiRunner.Run();

            PrintComparison(singleThreadTime, multiThreadTime, multiRunner.GetMaxConcurrentThreads());

            Console.WriteLine();
            Console.WriteLine(" --------------- ПРОВЕРКА КОРРЕКТНОСТИ РЕШЕНИЙ ---------------");
            Console.WriteLine();

            bool allCorrect = VerifyResults(singleRunner.GetTaskCount());

            if (allCorrect)
            {
                Console.WriteLine("[INFO] Все результаты совпадают! Решения корректны.");
            }
            else
            {
                Console.WriteLine("[ERR] Обнаружены расхождения в результатах!");
            }

            Console.WriteLine();
            Console.WriteLine("Нажмите любую клавишу для завершения...");
            Console.ReadKey();
        }

        static void PrintComparison(long singleTime, long multiTime, int threadCount)
        {
            Console.WriteLine();
            Console.WriteLine("--------------- СРАВНЕНИЕ РЕЗУЛЬТАТОВ ---------------");
            Console.WriteLine();

            double speedup = (double)singleTime / multiTime;

            Console.WriteLine($"Однопоточная версия:   {singleTime,6} мс");
            Console.WriteLine($"Многопоточная версия:  {multiTime,6} мс (макс. потоков: {threadCount})");
            Console.WriteLine();
            Console.WriteLine($"Ускорение:             {speedup,6:F2}x");
            Console.WriteLine();
        }

        static bool VerifyResults(int taskCount)
        {
            bool allCorrect = true;

            for (int i = 1; i <= taskCount; i++)
            {
                try
                {
                    string singleFile = $"x{i}_single.csv";
                    string multiFile = $"x{i}_multi.csv";

                    if (!File.Exists(singleFile) || !File.Exists(multiFile))
                    {
                        Console.WriteLine($"[ERR] Задача {i}: Файлы результатов не найдены");
                        allCorrect = false;
                        continue;
                    }

                    double[] xSingle = MatrixHelper.ReadVectorFromCsv(singleFile);
                    double[] xMulti = MatrixHelper.ReadVectorFromCsv(multiFile);

                    if (xSingle.Length != xMulti.Length)
                    {
                        Console.WriteLine($"[ERR] Задача {i}: Разная размерность векторов");
                        allCorrect = false;
                        continue;
                    }

                    double difference = 0;
                    for (int j = 0; j < xSingle.Length; j++)
                    {
                        double diff = Math.Abs(xSingle[j] - xMulti[j]);
                        if (diff > difference)
                            difference = diff;
                    }

                    const double tolerance = 1e-6;

                    if (difference < tolerance)
                    {
                        Console.WriteLine($"[INFO] Задача {i}: Результаты совпадают");
                    }
                    else
                    {
                        Console.WriteLine($"[ERR] Задача {i}: Расхождение {difference:E2} > {tolerance:E2}");
                        allCorrect = false;
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[ERR] Задача {i}: Ошибка проверки - {ex.Message}");
                    allCorrect = false;
                }
            }

            return allCorrect;
        }
    }
}