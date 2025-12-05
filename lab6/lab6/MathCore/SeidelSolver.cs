namespace Common
{
    public static class SeidelSolver
    {
        public static double[] Solve(LinearSystem system, int maxIter = 10000, double tol = 1e-10)
        {
            int n = system.N;
            double[] x = new double[n];
            double[] xPrev = new double[n];

            for (int iter = 0; iter < maxIter; iter++)
            {
                for (int i = 0; i < n; i++)
                {
                    double sum = 0;
                    for (int j = 0; j < n; j++)
                        if (j != i)
                            sum += system.A[i, j] * x[j];

                    x[i] = (system.B[i] - sum) / system.A[i, i];
                }

                double maxDiff = 0;
                for (int i = 0; i < n; i++)
                    maxDiff = Math.Max(maxDiff, Math.Abs(x[i] - xPrev[i]));

                if (maxDiff < tol) break;
                Array.Copy(x, xPrev, n);
            }

            return x;
        }
    }
}
