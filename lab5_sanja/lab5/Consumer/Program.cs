using Shared;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO.Pipes;

class Program
{
    static Queue<ImageTask> queue = new Queue<ImageTask>();
    static AutoResetEvent dataReady = new AutoResetEvent(false);
    static bool finished = false;

    static void Main(string[] args)
    {
        Console.WriteLine("Выберите режим работы Consumer:");
        Console.WriteLine("1 — Однопоточный");
        Console.WriteLine("2 — Многопоточный");
        int mode = int.Parse(Console.ReadLine());

        Stopwatch sw = Stopwatch.StartNew();

        using var pipeClient = new AnonymousPipeClientStream(PipeDirection.In, args[0]);
        using var reader = new BinaryReader(pipeClient);

        if (mode == 1)
            RunSingleThread(reader);
        else
            RunMultiThread(reader);

        sw.Stop();
        Console.WriteLine($"Consumer: Время выполнения = {sw.ElapsedMilliseconds} мс");
        Console.ReadLine();
    }

    static void RunSingleThread(BinaryReader reader)
    {
        while (true)
        {
            string fileName = reader.ReadString();
            if (string.IsNullOrEmpty(fileName))
                break;

            int length = reader.ReadInt32();
            byte[] imageBytes = reader.ReadBytes(length);

            Console.WriteLine($"Consumer: обработка {Path.GetFileName(fileName)}");

            using Bitmap image = BytesToBitmap(imageBytes);
            var segments = KMeansSegmentation(image, 3);
            SaveSegments(segments, fileName);
        }
    }

    static void RunMultiThread(BinaryReader reader)
    {
        int workerCount = Environment.ProcessorCount;
        List<Thread> workers = new List<Thread>();

        // Поток чтения из pipe
        Thread producerThread = new Thread(() =>
        {
            while (true)
            {
                string fileName = reader.ReadString();
                if (string.IsNullOrEmpty(fileName))
                {
                    finished = true;
                    for (int i = 0; i < workerCount; i++)
                        dataReady.Set();
                    break;
                }

                int length = reader.ReadInt32();
                byte[] imageBytes = reader.ReadBytes(length);

                lock (queue)
                    queue.Enqueue(new ImageTask { FileName = fileName, ImageBytes = imageBytes });

                dataReady.Set();
            }
        });
        producerThread.Start();

        // Потоки обработки
        for (int i = 0; i < workerCount; i++)
        {
            Thread t = new Thread(() =>
            {
                while (true)
                {
                    ImageTask task = null;
                    lock (queue)
                    {
                        if (queue.Count > 0)
                            task = queue.Dequeue();
                    }

                    if (task != null)
                    {
                        Console.WriteLine($"Consumer: обработка {Path.GetFileName(task.FileName
                            )}");

                        using Bitmap image = BytesToBitmap(task.ImageBytes);
                        var segments = KMeansSegmentation(image, 3);
                        SaveSegments(segments, task.FileName);
                    }
                    else if (finished)
                        break;
                    else
                        dataReady.WaitOne();
                }
            });
            workers.Add(t);
            t.Start();
        }

        producerThread.Join();
        foreach (var t in workers)
            t.Join();
    }

    static Bitmap BytesToBitmap(byte[] data)
    {
        using MemoryStream ms = new MemoryStream(data);
        return new Bitmap(ms);
    }

    static List<Bitmap> KMeansSegmentation(Bitmap image, int k)
    {
        int width = image.Width, height = image.Height;
        Random rnd = new Random();
        Color[] centers = new Color[k];
        for (int i = 0; i < k; i++)
            centers[i] = image.GetPixel(rnd.Next(width), rnd.Next(height));

        int[,] labels = new int[width, height];

        for (int iter = 0; iter < 5; iter++)
        {
            for (int y = 0; y < height; y++)
                for (int x = 0; x < width; x++)
                    labels[x, y] = NearestCenter(image.GetPixel(x, y), centers);

            for (int i = 0; i < k; i++)
            {
                int r = 0, g = 0, b = 0, count = 0;
                for (int y = 0; y < height; y++)
                    for (int x = 0; x < width; x++)
                        if (labels[x, y] == i)
                        {
                            Color c = image.GetPixel(x, y);
                            r += c.R; g += c.G; b += c.B; count++;
                        }
                if (count > 0)
                    centers[i] = Color.FromArgb(r / count, g / count, b / count);
            }
        }

        List<Bitmap> result = new List<Bitmap>();
        for (int i = 0; i < k; i++)
        {
            Bitmap seg = new Bitmap(width, height);
            for (int y = 0; y < height; y++)
                for (int x = 0; x < width; x++)
                    seg.SetPixel(x, y, labels[x, y] == i ? image.GetPixel(x, y) : Color.Black);
            result.Add(seg);
        }
        return result;
    }

    static int NearestCenter(Color c, Color[] centers)
    {
        int best = 0;
        double min = double.MaxValue;
        for (int i = 0; i < centers.Length; i++)
        {
            double d = Math.Pow(c.R - centers[i].R, 2)
                     + Math.Pow(c.G - centers[i].G, 2)
                     + Math.Pow(c.B - centers[i].B, 2);
            if (d < min) { min = d; best = i; }
        }
        return best;
    }

    static void SaveSegments(List<Bitmap> segments, string baseName)
    {
        string dir = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Result");
        Directory.CreateDirectory(dir);

        for (int i = 0; i < segments.Count; i++)
        {
            string name = Path.GetFileNameWithoutExtension(baseName);
            segments[i].Save(
                Path.Combine(dir, $"{name}_segment_{i}.png"),
                ImageFormat.Png);
        }
    }
}
