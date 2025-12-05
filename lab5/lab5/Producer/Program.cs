using Shared;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using System.Diagnostics;

class Program   
{
    static void Main(string[] args)
    {
        if (args.Length < 4)
        {
            Console.WriteLine("Используйте: Producer.exe <ImagesFolder> <R> <G> <B> [multi]");
            return;
        }

        string folderPath = args[0];
        if (!int.TryParse(args[1], out int targetR) ||
            !int.TryParse(args[2], out int targetG) ||
            !int.TryParse(args[3], out int targetB))
        {
            Console.WriteLine("RGB должен быть интом 0..255");
            return;
        }

        bool isMultiThreaded = args.Length > 4 && args[4] == "multi";

        var images = Directory.GetFiles(folderPath, "*.bmp");

        if (images.Length < 10)
        {
            Console.WriteLine("Нужно минимум 10 BMP-изображений в папке.");
            return;
        }

        Console.WriteLine($"[Поставщик] Мод: {(isMultiThreaded ? "многопоточный" : "однопоточный")}. Найдено {images.Length} изображений. Ищем цвет: ({targetR},{targetG},{targetB})");

        using var ipc = new SharedMemoryClient(true);
        var sw = Stopwatch.StartNew();

        int globalComponentId = 0;

        if (isMultiThreaded)
        {
            Parallel.ForEach(images, imgPath =>
            {
                ProcessImageAndSend(imgPath, (byte)targetR, (byte)targetG, (byte)targetB, ipc, ref globalComponentId);
            });
        }
        else
        {
            foreach (var imgPath in images)
            {
                ProcessImageAndSend(imgPath, (byte)targetR, (byte)targetG, (byte)targetB, ipc, ref globalComponentId);
            }
        }

        // Сообщаем конец потока
        var endMsg = new TriangleMessage
        {
            IsEndOfStream = true,
            ImageName = string.Empty,
            TargetR = (byte)targetR,
            TargetG = (byte)targetG,
            TargetB = (byte)targetB,
            ComponentId = -1,
            PointCount = 0,
            Points = new PointStruct[TriangleMessage.MaxPointsPerComponent]
        };

        ipc.Produce(endMsg);

        sw.Stop();
        Console.WriteLine($"[Поставщик] Время: {sw.ElapsedMilliseconds} мс.");
    }

    static void ProcessImageAndSend(string path, byte tR, byte tG, byte tB, SharedMemoryClient ipc, ref int globalComponentId)
    {
        try
        {
            using var image = Image.Load<Rgb24>(path);
            int width = image.Width;
            int height = image.Height;

            // INFO: маска соответствия заданному цвету, типо фотка - двумерный массив и мы пометим тру пиксели, 
            // которые соответствуют цвету
            var mask = new bool[height, width];

            // INFO: метод из либы чтобы пройтись по пикселям
            image.ProcessPixelRows(accessor =>
            {
                for (int y = 0; y < accessor.Height; y++)
                {
                    var row = accessor.GetRowSpan(y);
                    for (int x = 0; x < row.Length; x++)
                    {
                        // INFO: ну тут понятно, если пиксель соответствует, то помечаем
                        ref Rgb24 p = ref row[x];
                        if (p.R == tR && p.G == tG && p.B == tB)
                            mask[y, x] = true;
                    }
                }
            });

            // Поиск связных компонент - треугольников 
            bool[,] visited = new bool[height, width];

            // INFO: опять таки, в целом дальше не важно, он у тебя здесь посмотрит только класс SharedMemoryClient и всё,
            // но на всякий попросил нейро тебе описание сделать, если не поймёшь, скажешь, помогу

            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    // Если эта точка не принадлежит цветовой маске (не целевой цвет)
                    // или если она уже была посещена — пропускаем.
                    if (!mask[y, x] || visited[y, x]) continue;

                    // -------------------------------------------------
                    // Начинаем поиск связной компоненты методом BFS
                    // -------------------------------------------------

                    // Список всех пикселей компоненты (одной цветовой области)
                    var points = new List<PointStruct>();

                    // Очередь для BFS — начинаем с точки (x, y)
                    var q = new Queue<(int, int)>();
                    q.Enqueue((x, y));

                    // Помечаем точку как посещённую
                    visited[y, x] = true;

                    // ------------------ BFS цикл ---------------------
                    while (q.Count > 0)
                    {
                        // Берём следующую точку из очереди
                        var (cx, cy) = q.Dequeue();

                        // Добавляем её в компоненту
                        points.Add(new PointStruct { X = cx, Y = cy });

                        // 4 соседа (вверх, вниз, влево, вправо)
                        var nbrs = new (int dx, int dy)[]
                        {
                (1, 0),   // вправо
                (-1, 0),  // влево
                (0, 1),   // вниз
                (0, -1)   // вверх
                        };

                        // Проверяем каждого соседа
                        foreach (var n in nbrs)
                        {
                            int nx = cx + n.dx; // координата соседа по X
                            int ny = cy + n.dy; // координата соседа по Y

                            // Проверка границ изображения
                            if (nx >= 0 && nx < width && ny >= 0 && ny < height)
                            {
                                // Если сосед ещё не посещён и принадлежит к целевому цвету
                                if (!visited[ny, nx] && mask[ny, nx])
                                {
                                    visited[ny, nx] = true;   // помечаем как посещённого
                                    q.Enqueue((nx, ny));     // добавляем в очередь BFS
                                }
                            }
                        }
                    }
                    // ---------------- конец BFS --------------------

                    // points содержит все пиксели одной связной области (одного цветного треугольника)

                    // Отбрасываем шум — маленькие компоненты
                    if (points.Count >= 3)
                    {
                        // Создаём сообщение для передачи другой программе
                        var msg = new TriangleMessage
                        {
                            IsEndOfStream = false,                       // это НЕ конец потока
                            ImageName = Path.GetFileName(path),          // имя файла картинки
                            TargetR = tR,                                // искомый цвет
                            TargetG = tG,
                            TargetB = tB,
                            ComponentId = Interlocked.Increment(ref globalComponentId),
                            PointCount = Math.Min(points.Count, TriangleMessage.MaxPointsPerComponent),
                            Points = new PointStruct[TriangleMessage.MaxPointsPerComponent]
                        };

                        // Переписываем точки в сообщение
                        for (int i = 0; i < msg.PointCount; i++)
                            msg.Points[i] = points[i];

                        // Отправка в разделяемую память (IPC)
                        ipc.Produce(msg);

                        Console.WriteLine($"[Поставщик] {Path.GetFileName(path)}: отправил компоненту Id={msg.ComponentId}, точек={msg.PointCount}");
                    }
                }
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[Ошибка] {path}: {ex.Message}");
        }
    }
}
