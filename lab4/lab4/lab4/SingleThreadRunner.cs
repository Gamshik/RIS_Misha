using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;

namespace lab4
{
    public class SingleThreadRunner
    {
        private List<TaskInfo> tasks;

        public SingleThreadRunner()
        {
            tasks = new List<TaskInfo>();
        }

        // INFO: тут парсим наши таски
        public void LoadTasks()
        {
            tasks.Clear();
            var lines = File.ReadAllLines("tasks.txt");

            for (int i = 0; i < lines.Length; i++)
            {
                var parts = lines[i].Split(',');
                if (parts.Length == 2)
                {
                    string matrixFile = parts[0].Trim();
                    string vectorFile = parts[1].Trim();
                    string resultFile = $"x{i + 1}_single.csv";

                    tasks.Add(new TaskInfo(i + 1, matrixFile, vectorFile, resultFile));
                }
            }
        }

        public long Run()
        {
            Console.WriteLine();
            Console.WriteLine("--------------- ОДНОПОТОЧНАЯ ВЕРСИЯ ---------------");
            Console.WriteLine();

            Stopwatch stopwatch = Stopwatch.StartNew();

            // INFO: просто в лоб решаем каждую таску
            for (int i = 0; i < tasks.Count; i++)
            {
                var task = tasks[i];
                Console.Write($"[{i + 1}/{tasks.Count}] Решение задачи {task.MatrixAFile}... ");

                try
                {
                    Stopwatch taskTimer = Stopwatch.StartNew();

                    double[,] A = MatrixHelper.ReadMatrixFromCsv(task.MatrixAFile);
                    double[] b = MatrixHelper.ReadVectorFromCsv(task.VectorBFile);

                    double[] x = SeidelSolver.Solve(A, b);

                    MatrixHelper.WriteVectorToCsv(task.ResultFile, x);

                    taskTimer.Stop();

                    Console.WriteLine($"({taskTimer.ElapsedMilliseconds} мс)");
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Ошибка: {ex.Message}");
                }
            }

            stopwatch.Stop();

            Console.WriteLine();
            Console.WriteLine($"Результаты сохранены в: x1_single.csv ... x{tasks.Count}_single.csv");
            Console.WriteLine($"Общее время: {stopwatch.ElapsedMilliseconds} мс");
            Console.WriteLine();

            return stopwatch.ElapsedMilliseconds;
        }

        public int GetTaskCount() => tasks.Count;
    }
}
