using System;

namespace lab4
{
    // INFO: тут в целом тож тебе ни чего не нужно,но ес хочешь, я попросил ии нагенерить описание, ибо за матан я и сам не шарю, ни в своих ни в какхи лабах
    // ну и по базе, просто не показывай башику, ему пофиг
    public static class SeidelSolver
    {
        /// <summary>
        /// Метод решения СЛАУ методом Зейделя (итерационный метод)
        /// </summary>
        /// <param name="A">Матрица коэффициентов</param>
        /// <param name="b">Вектор правой части</param>
        /// <param name="maxIterations">Максимальное число итераций</param>
        /// <param name="tolerance">Допустимая точность сходимости</param>
        /// <returns>Вектор решения x</returns>
        public static double[] Solve(double[,] A, double[] b, int maxIterations = 10000, double tolerance = 1e-8)
        {
            int n = A.GetLength(0);          // Размерность системы
            double[] x = new double[n];      // Текущие значения решения
            double[] xOld = new double[n];   // Значения решения на предыдущей итерации

            for (int iter = 0; iter < maxIterations; iter++)
            {
                // Сохраняем предыдущие значения для проверки сходимости
                Array.Copy(x, xOld, n);

                // Проходим по каждой переменной системы
                for (int i = 0; i < n; i++)
                {
                    double sum = 0;

                    // Суммируем произведения коэффициентов на текущие значения переменных,
                    // кроме диагонального элемента
                    for (int j = 0; j < n; j++)
                    {
                        if (j != i)
                            sum += A[i, j] * x[j];
                    }

                    // Вычисляем новое значение переменной по формуле Зейделя
                    x[i] = (b[i] - sum) / A[i, i];
                }

                // Проверка сходимости: считаем "норму изменения" между итерациями
                double norm = 0;
                for (int i = 0; i < n; i++)
                {
                    norm += Math.Abs(x[i] - xOld[i]);
                }

                // Если изменение стало меньше заданного порога, завершаем итерации
                if (norm < tolerance)
                    break;
            }

            return x; // Возвращаем решение
        }

        /// <summary>
        /// Вычисление нормы невязки ||Ax - b|| для проверки точности решения
        /// </summary>
        /// <param name="A">Матрица коэффициентов</param>
        /// <param name="x">Вектор решения</param>
        /// <param name="b">Вектор правой части</param>
        /// <returns>Значение нормы невязки</returns>
        public static double VerifySolution(double[,] A, double[] x, double[] b)
        {
            // Вычисляем Ax
            double[] Ax = MatrixHelper.MultiplyMatrixVector(A, x);

            // Вычисляем невязку: Ax - b
            double[] residual = MatrixHelper.SubtractVectors(Ax, b);

            // Возвращаем норму невязки
            return MatrixHelper.VectorNorm(residual);
        }
    }
}
