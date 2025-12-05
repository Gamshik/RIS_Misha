namespace Common
{
    public static class MatrixGenerator
    {
        public static LinearSystem GeneratePositiveDefinite(int n, Random? rnd = null)
        {
            rnd ??= Random.Shared;
            var M = new double[n, n];
            for (int i = 0; i < n; i++)
                for (int j = 0; j < n; j++)
                    M[i, j] = rnd.NextDouble() * 2 - 1;

            var A = new double[n, n];
            for (int i = 0; i < n; i++)
                for (int j = 0; j < n; j++)
                    for (int k = 0; k < n; k++)
                        A[i, j] += M[i, k] * M[j, k];

            for (int i = 0; i < n; i++)
                A[i, i] += n * 10; 

            var b = new double[n];
            for (int i = 0; i < n; i++)
                b[i] = rnd.NextDouble() * 100;

            return new LinearSystem(A, b, n);
        }
    }
}
