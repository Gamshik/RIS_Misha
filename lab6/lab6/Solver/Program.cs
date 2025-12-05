using Common;
using System.Diagnostics;
using System.IO.Pipes;

const string TasksDir = "D:\\ProgrammingAndProjects\\Studies\\7sem\\RIS\\lab6\\Tasks";
const string ResultsDir = "D:\\ProgrammingAndProjects\\Studies\\7sem\\RIS\\lab6\\Results";
const string PipeName = "LinearSystemsPipe";

Directory.CreateDirectory(ResultsDir);

var results = new List<(int Id, int N, double SingleMs, double MultiMs)>();

Console.WriteLine("Решатель запущен. Подключаемся к Генератору через Named Pipe...");

// INFO: для взаимодействия использовали именованный канал, через который тут получаем таски
using var pipe = new NamedPipeClientStream(".", PipeName, PipeDirection.In, PipeOptions.Asynchronous);
// INFO: ждём 5 сек, если не подключится, то офаемся, сделал просто по приколу
using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(5));

try
{
    await pipe.ConnectAsync(cts.Token);
    Console.WriteLine("Подключено! Ожидаем задачи от Генератора...\n");
}
catch (Exception ex)
{
    Console.WriteLine($"Ошибка подключения: {ex.Message}");
    return;
}

using var reader = new StreamReader(pipe);

string? line;
// INFO: при помощи ридера читаем
while ((line = await reader.ReadLineAsync()) != null)
{
    if (!line.StartsWith("DONE|")) continue;

    var parts = line.Split('|');
    int id = int.Parse(parts[1]);
    int n = int.Parse(parts[2]);

    string aPath = Path.Combine(TasksDir, $"a{id}.csv");
    string bPath = Path.Combine(TasksDir, $"b{id}.csv");

    var system = await MatrixFileIO.ReadAsync(aPath, bPath);

    var sw = Stopwatch.StartNew();
    double[] xSingle = SeidelSolver.Solve(system);
    double singleMs = sw.Elapsed.TotalMilliseconds;

    sw.Restart();
    double[] xMulti = ThreadedSeidelSolver.Solve(system);
    double multiMs = sw.Elapsed.TotalMilliseconds;

    await MatrixFileIO.WriteSolutionAsync(Path.Combine(ResultsDir, $"x{id}.csv"), xMulti, n);

    double speedup = singleMs / multiMs;
    results.Add((id, n, singleMs, multiMs));

    Console.WriteLine($"Задача {id,2} | N={n,4} | Однопот: {singleMs,8:F2} мс | Многопот: {multiMs,8:F2} мс | Ускорение: {speedup,5:F2}x");
}

Console.WriteLine("\n" + new string('=', 90));
Console.WriteLine("ЗАДАЧА   N    ОДНОПОТОЧНЫЙ    МНОГОПОТОЧНЫЙ    УСКОРЕНИЕ");
Console.WriteLine(new string('=', 90));
foreach (var r in results.OrderBy(x => x.Id))
{
    Console.WriteLine($"{r.Id,4}   {r.N,5}   {r.SingleMs,10:F2} мс    {r.MultiMs,10:F2} мс     {r.SingleMs / r.MultiMs,8:F2}x");
}
