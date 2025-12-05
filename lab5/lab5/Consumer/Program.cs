using Shared;
using System.Diagnostics;

class Program
{
    static void Main(string[] args)
    {
        bool isMultiThreaded = args.Length > 0 && args[0] == "multi";
        Console.WriteLine($"[Потребитель] Мод: {(isMultiThreaded ? "многопоточный" : "однопоточный")}. Ожидание данных...");

        using var ipc = new SharedMemoryClient(false);
        var components = new List<(string ImageName, int ComponentId, PointStruct[] Points, int PointCount)>();

        // INFO: собираем все данные пока не соберём
        while (true)
        {
            var msg = ipc.Consume();
            if (msg.IsEndOfStream) break;

            var pts = new PointStruct[msg.PointCount];
            Array.Copy(msg.Points, pts, msg.PointCount);

            components.Add((msg.ImageName, msg.ComponentId, pts, msg.PointCount));
        }

        Console.WriteLine($"[Потребитель] Получено компонент: {components.Count}. Начинаем обработку...");

        var sw = Stopwatch.StartNew();

        if (isMultiThreaded)
        {
            var results = new List<string>();
            // INFO: SemaphoreSlim какая-то оптимизированная под капотом версия семафора
            var semaphore = new SemaphoreSlim(1, 1);

            Parallel.ForEach(components, comp =>
            {
                var s = ProcessComponent(comp.ImageName, comp.ComponentId, comp.Points, comp.PointCount);

                semaphore.Wait();
                
                try
                {
                    results.Add(s);
                }
                finally
                {
                    semaphore.Release(); // всегда освобождаем семафор
                }
            });

            sw.Stop();
            foreach (var r in results) // вывод в упорядоченном виде
                Console.WriteLine(r);
        }
        else
        {
            foreach (var comp in components)
            {
                var s = ProcessComponent(comp.ImageName, comp.ComponentId, comp.Points, comp.PointCount);
                Console.WriteLine(s);
            }
            sw.Stop();
        }

        Console.WriteLine($"[Потребитель] Время обработки: {sw.ElapsedMilliseconds} мс.");
    }


    // INFO: ну тут тоже нет смысла объяснять, математика на которую он не смотрит
    static string ProcessComponent(string imageName, int compId, PointStruct[] ptsArr, int count)
    {
        // Собираем список точек
        var pts = new List<(int X, int Y)>(count);
        for (int i = 0; i < count; i++)
            pts.Add((ptsArr[i].X, ptsArr[i].Y));

        // Вычисляем выпуклую оболочку (Monotone chain)
        var hull = ConvexHull(pts);

        (int X, int Y)[] vertices = null;

        if (hull.Count == 3)
        {
            vertices = hull.ToArray();
        }
        else if (hull.Count > 3)
        {
            // Эвристика: ищем пару максимального расстояния, затем третью точку, которая максимально отличается
            var (p1, p2) = FindFarthestPair(hull);
            var p3 = hull.OrderByDescending(p => Math.Min(DistSquared(p, p1), DistSquared(p, p2))).First();
            vertices = new[] { p1, p2, p3 };
        }
        else
        {
            return $"[{imageName} | comp {compId}] НЕ треугольник (hull count = {hull.Count})";
        }

        // Вычисляем уравнения для сторон
        var lines = new List<(double A, double B, double C)>();
        for (int i = 0; i < 3; i++)
        {
            var a = vertices[i];
            var b = vertices[(i + 1) % 3];

            double Acoef = a.Y - b.Y;
            double Bcoef = b.X - a.X;
            double Ccoef = a.X * b.Y - b.X * a.Y;

            lines.Add((Acoef, Bcoef, Ccoef));
        }

        // Формируем вывод
        var sb = new System.Text.StringBuilder();
        sb.AppendLine($"[{imageName} | comp {compId}] Треугольник - вершины: ({vertices[0].X},{vertices[0].Y}), ({vertices[1].X},{vertices[1].Y}), ({vertices[2].X},{vertices[2].Y})");
        sb.AppendLine($"  Стороны (Ax + By + C = 0):");
        for (int i = 0; i < 3; i++)
        {
            var l = lines[i];
            sb.AppendLine($"    {l.A:F0}x + {l.B:F0}y + {l.C:F0} = 0");
        }

        return sb.ToString();
    }

    static long DistSquared((int X, int Y) a, (int X, int Y) b)
    {
        long dx = a.X - b.X;
        long dy = a.Y - b.Y;
        return dx * dx + dy * dy;
    }

    static ((int X, int Y), (int X, int Y)) FindFarthestPair(List<(int X, int Y)> pts)
    {
        long maxd = -1;
        (int X, int Y) p1 = (0, 0), p2 = (0, 0);
        for (int i = 0; i < pts.Count; i++)
        {
            for (int j = i + 1; j < pts.Count; j++)
            {
                var d = DistSquared(pts[i], pts[j]);
                if (d > maxd)
                {
                    maxd = d;
                    p1 = pts[i];
                    p2 = pts[j];
                }
            }
        }
        return (p1, p2);
    }

    // Monotone chain convex hull - возвращает точки hull по часовой (no duplicate)
    static List<(int X, int Y)> ConvexHull(List<(int X, int Y)> points)
    {
        var pts = points.Distinct().ToList();
        if (pts.Count <= 1) return new List<(int X, int Y)>(pts);

        pts.Sort((a, b) => a.X != b.X ? a.X - b.X : a.Y - b.Y);

        var lower = new List<(int X, int Y)>();
        foreach (var p in pts)
        {
            while (lower.Count >= 2 && Cross(lower[lower.Count - 2], lower[lower.Count - 1], p) <= 0)
                lower.RemoveAt(lower.Count - 1);
            lower.Add(p);
        }

        var upper = new List<(int X, int Y)>();
        for (int i = pts.Count - 1; i >= 0; i--)
        {
            var p = pts[i];
            while (upper.Count >= 2 && Cross(upper[upper.Count - 2], upper[upper.Count - 1], p) <= 0)
                upper.RemoveAt(upper.Count - 1);
            upper.Add(p);
        }

        lower.RemoveAt(lower.Count - 1);
        upper.RemoveAt(upper.Count - 1);
        lower.AddRange(upper);
        return lower;
    }

    static long Cross((int X, int Y) o, (int X, int Y) a, (int X, int Y) b)
    {
        return (long)(a.X - o.X) * (b.Y - o.Y) - (long)(a.Y - o.Y) * (b.X - o.X);
    }
}
