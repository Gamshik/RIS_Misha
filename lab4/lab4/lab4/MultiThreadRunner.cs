using System.Diagnostics;

namespace lab4
{
    public class MultiThreadRunner
    {
        private List<TaskInfo> _tasks;
        private int _maxConcurrentThreads;
        private Semaphore _semaphore;
        private int _completedTasks = 0;
        private object lockObject = new object();

        public MultiThreadRunner()
        {
            _tasks = new List<TaskInfo>();
            // INFO: максимальное число одновременно выполняющихся задач = ядра + 4 - по заданию
            _maxConcurrentThreads = Environment.ProcessorCount + 4;
            // INFO: вот тут ты создаёшт семафор и указываем параметры
            // 1 - начальное количество "разрешений" - количество потоков, которые могут сразу получить доступ к ресурсу без ожидания
            // 2 - максимальное количество разрешений
            // допустим, если первым параметром укажешь 1, то у тебя ток один поток зайдёт сразу, и только когда он релизнет семафор остальные получат доступ
            // и тогда уже постоянно будет доступ кол-ву потоков, которое ты указал во 2 параметре
            _semaphore = new Semaphore(_maxConcurrentThreads, _maxConcurrentThreads);
        }

        // INFO: также просто собираем таски
        public void LoadTasks()
        {
            _tasks.Clear();
            var lines = File.ReadAllLines("tasks.txt");

            for (int i = 0; i < lines.Length; i++)
            {
                var parts = lines[i].Split(',');
                if (parts.Length == 2)
                {
                    string matrixFile = parts[0].Trim();
                    string vectorFile = parts[1].Trim();
                    string resultFile = $"x{i + 1}_multi.csv";

                    _tasks.Add(new TaskInfo(i + 1, matrixFile, vectorFile, resultFile));
                }
            }
        }

        private void ProcessTask(TaskInfo task, int threadId)
        {
            // INFO: если достигнуто макс кол-во потоков, то ждём, но у тебя такого не будет, так как тасок 10, а у тебя 12 лог процессоров
            // это так, к сведению
            _semaphore.WaitOne();

            Console.WriteLine($"[Поток #{threadId}] Взял {task.MatrixAFile}");

            try
            {
                Stopwatch taskTimer = Stopwatch.StartNew();

                double[,] A = MatrixHelper.ReadMatrixFromCsv(task.MatrixAFile);
                double[] b = MatrixHelper.ReadVectorFromCsv(task.VectorBFile);

                double[] x = SeidelSolver.Solve(A, b);

                MatrixHelper.WriteVectorToCsv(task.ResultFile, x);

                taskTimer.Stop();

                // INFO: это кстати условный мьютекс, на минималках, гарантирует, что толлько 1 поток получит доступ к общим данным
                // это твоя критическая секция
                // тоесть семафором ты ограничиваешь кол-во потоков, а критическую секцию решаешь так
                lock (lockObject)
                {
                    _completedTasks++;
                }

                Console.WriteLine($"[Поток #{threadId}] Завершил {task.MatrixAFile} ({taskTimer.ElapsedMilliseconds} мс)");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[Поток #{threadId}] Ошибка в {task.MatrixAFile}: {ex.Message}");
            }
            finally
            {
                // INFO: освобождаем занятый семафор
                _semaphore.Release(); 
            }
        }

        public long Run()
        {
            Console.WriteLine();
            Console.WriteLine($"--------------- МНОГОПОТОЧНАЯ ВЕРСИЯ (ПУЛ ПОТОКОВ, макс {_maxConcurrentThreads} одновременно) ---------------");
            Console.WriteLine();

            _completedTasks = 0;

            Stopwatch stopwatch = Stopwatch.StartNew();

            int taskId = 0;
            foreach (var task in _tasks)
            {
                int threadId = taskId + 1;
                // INFO: отправляем таску в пул потоков .NET, там он под копотом сам им управляет, определяет скок чего нужно и тд
                ThreadPool.QueueUserWorkItem(_ => ProcessTask(task, threadId));
                taskId++;
            }

            // INFO: ждём завершения всех задач
            while (true)
            {
                lock (lockObject)
                {
                    if (_completedTasks >= _tasks.Count)
                        break;
                }
                // INFO: небольшая пауза, чтобы не грузить цпюшку
                Thread.Sleep(50); 
            }

            stopwatch.Stop();

            Console.WriteLine();
            Console.WriteLine($"Выполнено задач: {_completedTasks}");
            Console.WriteLine($"Результаты сохранены в: x1_multi.csv ... x{_tasks.Count}_multi.csv");
            Console.WriteLine($"Общее время: {stopwatch.ElapsedMilliseconds} мс");
            Console.WriteLine();

            _semaphore.Dispose();

            return stopwatch.ElapsedMilliseconds;
        }

        public int GetTaskCount() => _tasks.Count;
        public int GetMaxConcurrentThreads() => _maxConcurrentThreads;
    }
}
