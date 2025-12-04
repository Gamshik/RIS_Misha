using System.Globalization;

namespace lab4
{
    // INFO: просто помогаторы для матриц, ио операции, плюс математические перемножение и тд
    public static class MatrixHelper
    {
        public static double[,] ReadMatrixFromCsv(string fileName)
        {
            var lines = File.ReadAllLines(fileName);
            int rows = lines.Length;
            int cols = lines[0].Split(',').Length;

            double[,] matrix = new double[rows, cols];

            for (int i = 0; i < rows; i++)
            {
                var values = lines[i].Split(',');
                for (int j = 0; j < cols; j++)
                {
                    matrix[i, j] = double.Parse(values[j].Trim(), CultureInfo.InvariantCulture);
                }
            }

            return matrix;
        }

        public static double[] ReadVectorFromCsv(string fileName)
        {
            var lines = File.ReadAllLines(fileName);
            double[] vector = new double[lines.Length];

            for (int i = 0; i < lines.Length; i++)
            {
                vector[i] = double.Parse(lines[i].Trim(), CultureInfo.InvariantCulture);
            }

            return vector;
        }

        public static void WriteVectorToCsv(string fileName, double[] vector)
        {
            using (StreamWriter writer = new StreamWriter(fileName))
            {
                foreach (double value in vector)
                {
                    writer.WriteLine(value.ToString("G17", CultureInfo.InvariantCulture));
                }
            }
        }

        public static void WriteMatrixToCsv(string fileName, double[,] matrix)
        {
            int rows = matrix.GetLength(0);
            int cols = matrix.GetLength(1);

            using (StreamWriter writer = new StreamWriter(fileName))
            {
                for (int i = 0; i < rows; i++)
                {
                    var line = string.Join(",",
                        Enumerable.Range(0, cols).Select(j => matrix[i, j].ToString("G17", CultureInfo.InvariantCulture)));
                    writer.WriteLine(line);
                }
            }
        }

        public static double VectorNorm(double[] vector)
        {
            double sum = 0;
            foreach (double v in vector)
            {
                sum += v * v;
            }
            return Math.Sqrt(sum);
        }

        public static double[] MultiplyMatrixVector(double[,] matrix, double[] vector)
        {
            int rows = matrix.GetLength(0);
            int cols = matrix.GetLength(1);
            double[] result = new double[rows];

            for (int i = 0; i < rows; i++)
            {
                double sum = 0;
                for (int j = 0; j < cols; j++)
                {
                    sum += matrix[i, j] * vector[j];
                }
                result[i] = sum;
            }

            return result;
        }

        public static double[] SubtractVectors(double[] a, double[] b)
        {
            double[] result = new double[a.Length];
            for (int i = 0; i < a.Length; i++)
            {
                result[i] = a[i] - b[i];
            }
            return result;
        }
    }

}
