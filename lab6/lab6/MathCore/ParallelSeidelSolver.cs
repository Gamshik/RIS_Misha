namespace Common
{
    public static class ThreadedSeidelSolver
    {
        private static readonly object _lock = new object();

        public static double[] Solve(LinearSystem system, int maxIter = 10000, double tol = 1e-10, int threadCount = 12)
        {
            int n = system.N;
            double[] x = new double[n];
            double[] xPrev = new double[n];

            int chunkSize = (n + threadCount - 1) / threadCount; // сколько элементов на поток

            for (int iter = 0; iter < maxIter; iter++)
            {
                // INFO: а вот реализация твоих потоков
                Thread[] threads = new Thread[threadCount];

                for (int t = 0; t < threadCount; t++)
                {
                    int threadIndex = t;
                    threads[t] = new Thread(() =>
                    {
                        int start = threadIndex * chunkSize;
                        int end = Math.Min(start + chunkSize, n);

                        for (int i = start; i < end; i++)
                        {
                            double sum = 0;
                            for (int j = 0; j < n; j++)
                                if (j != i)
                                    sum += system.A[i, j] * x[j];

                            // INFO: в твоём случае нужно реализовать критическую секцию, это самая простая её реализация,
                            // можно было юзнуть монитор, он более низкоуровневый, но башик будет задавать вопросы, я уверен, поэтому не стоит
                            lock (_lock) 
                            {
                                // INFO: вот поэтому так медленно, на каждой итерации нужно обновлять,
                                // так как на следующей мы используем эти данные
                                x[i] = (system.B[i] - sum) / system.A[i, i];
                            }
                        }
                    });

                    threads[t].Start();
                }

                // ждём завершения всех потоков
                foreach (var thread in threads)
                    thread.Join();

                double maxDiff = 0;
                for (int i = 0; i < n; i++)
                    maxDiff = Math.Max(maxDiff, Math.Abs(x[i] - xPrev[i]));

                if (maxDiff < tol)
                    break;

                lock (_lock)
                {
                    Array.Copy(x, xPrev, n);
                }
            }

            return x;
        }
    }
}