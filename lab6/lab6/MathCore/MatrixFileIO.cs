using System.Globalization;

namespace Common
{
    public static class MatrixFileIO
    {
        private static readonly CultureInfo Culture = CultureInfo.InvariantCulture;

        // INFO: вот на самом деле и суть этой лабы, что у тебя ИО операции выполняются асинхронно
        public static async Task WriteAsync(string aPath, string bPath, LinearSystem system)
        {
            await using var aw = new StreamWriter(aPath);
            await using var bw = new StreamWriter(bPath);

            await aw.WriteLineAsync(system.N.ToString());
            await bw.WriteLineAsync(system.N.ToString());

            for (int i = 0; i < system.N; i++)
            {
                var line = string.Join(";", Enumerable.Range(0, system.N)
                    .Select(j => system.A[i, j].ToString("R", Culture)));
                await aw.WriteLineAsync(line);
                await bw.WriteLineAsync(system.B[i].ToString("R", Culture));
            }
        }

        public static async Task<LinearSystem> ReadAsync(string aPath, string bPath)
        {
            string[] aLines = await File.ReadAllLinesAsync(aPath);
            string[] bLines = await File.ReadAllLinesAsync(bPath);

            int n = int.Parse(aLines[0]);
            var A = new double[n, n];
            var b = new double[n];

            for (int i = 0; i < n; i++)
            {
                var parts = aLines[i + 1].Split(';', StringSplitOptions.RemoveEmptyEntries);
                for (int j = 0; j < n; j++)
                    A[i, j] = double.Parse(parts[j], Culture);

                b[i] = double.Parse(bLines[i + 1], Culture);
            }

            return new LinearSystem(A, b, n);
        }

        public static async Task WriteSolutionAsync(string path, double[] x, int n)
        {
            await using var w = new StreamWriter(path);
            await w.WriteLineAsync(n.ToString());
            foreach (var val in x)
                await w.WriteLineAsync(val.ToString("R", Culture));
        }
    }
}
