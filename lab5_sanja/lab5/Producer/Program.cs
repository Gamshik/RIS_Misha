using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO.Pipes;

class Program
{
    static void Main()
    {
        string imagesPath = @"D:\ProgrammingAndProjects\Studies\7sem\RIS_Misha\lab5_sanja\Images";
        int radius = 2;

        Console.WriteLine("Выберите режим работы Producer:");
        Console.WriteLine("1 — Однопоточный");
        Console.WriteLine("2 — Многопоточный");
        int mode = int.Parse(Console.ReadLine());


        Stopwatch sw = Stopwatch.StartNew();

        using var pipeServer =
            new AnonymousPipeServerStream(PipeDirection.Out, HandleInheritability.Inheritable);

        // Запуск Consumer
        Process consumer = new Process();
        consumer.StartInfo.FileName = @"D:\ProgrammingAndProjects\Studies\7sem\RIS_Misha\lab5_sanja\lab5\Consumer\bin\Debug\net8.0\Consumer.exe";
        consumer.StartInfo.Arguments = pipeServer.GetClientHandleAsString();
        consumer.Start();

        pipeServer.DisposeLocalCopyOfClientHandle();
        using var writer = new BinaryWriter(pipeServer);

        if (mode == 1)
            RunSingleThread(imagesPath, radius, writer);
        else
            RunMultiThread(imagesPath, radius, writer);

        sw.Stop();
        Console.WriteLine($"Producer: Время выполнения = {sw.ElapsedMilliseconds} мс");
        Console.ReadLine();
    }

    static void RunSingleThread(string imagesPath, int radius, BinaryWriter writer)
    {
        foreach (var file in Directory.GetFiles(imagesPath, "*.jpg"))
        {
            Console.WriteLine($"Producer: обработка {Path.GetFileName(file)}");

            using Bitmap original = new Bitmap(file);
            using Bitmap filtered = ApplyMeanFilter(original, radius);
            byte[] imageBytes = BitmapToBytes(filtered);

            writer.Write(Path.GetFileName(file));
            writer.Write(imageBytes.Length);
            writer.Write(imageBytes);
        }
        writer.Write(string.Empty);
    }

    static void RunMultiThread(string imagesPath, int radius, BinaryWriter writer)
    {
        int workerCount = Environment.ProcessorCount;
        Queue<string> filesQueue = new Queue<string>(Directory.GetFiles(imagesPath, "*.jpg"));

        List<Thread> workers = new List<Thread>();

        for (int i = 0; i < workerCount; i++)
        {
            Thread t = new Thread(() =>
            {
                while (true)
                {
                    string file = null;
                    lock (filesQueue)
                    {
                        if (filesQueue.Count > 0)
                            file = filesQueue.Dequeue();
                        else
                            break;
                    }

                    Console.WriteLine($"Producer: обработка {Path.GetFileName(file)}");
                    using Bitmap original = new Bitmap(file);
                    using Bitmap filtered = ApplyMeanFilter(original, radius);
                    byte[] imageBytes = BitmapToBytes(filtered);

                    lock (writer)
                    {
                        writer.Write(Path.GetFileName(file));
                        writer.Write(imageBytes.Length);
                        writer.Write(imageBytes);
                    }
                }
            });
            workers.Add(t);
            t.Start();
        }

        foreach (var t in workers)
            t.Join();

        writer.Write(string.Empty);
    }

    static Bitmap ApplyMeanFilter(Bitmap source, int radius)
    {
        Bitmap result = new Bitmap(source.Width, source.Height);
        for (int y = 0; y < source.Height; y++)
            for (int x = 0; x < source.Width; x++)
            {
                int r = 0, g = 0, b = 0, count = 0;
                for (int dy = -radius; dy <= radius; dy++)
                    for (int dx = -radius; dx <= radius; dx++)
                    {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && ny >= 0 && nx < source.Width && ny < source.Height)
                        {
                            Color c = source.GetPixel(nx, ny);
                            r += c.R; g += c.G; b += c.B;
                            count++;
                        }
                    }
                result.SetPixel(x, y, Color.FromArgb(r / count, g / count, b / count));
            }
        return result;
    }

    static byte[] BitmapToBytes(Bitmap bmp)
    {
        using MemoryStream ms = new MemoryStream();
        bmp.Save(ms, ImageFormat.Png);
        return ms.ToArray();
    }
}
