using Common;
using System.IO.Pipes;

const int TaskCount = 15;
const int MinN = 300;
const int MaxN = 900;
const string TasksDir = "D:\\ProgrammingAndProjects\\Studies\\7sem\\RIS\\lab6\\Tasks";
const string PipeName = "LinearSystemsPipe";

Directory.CreateDirectory(TasksDir);
var random = Random.Shared;

// INFO: для взаимодействия использовали именованный канал, через который генератор даёт таски
await using var pipe = new NamedPipeServerStream(
    PipeName,
    PipeDirection.Out,
    1,
    PipeTransmissionMode.Message,
    PipeOptions.Asynchronous);
// INFO: ждём 5 сек, если не подключится, то офаемся, сделал просто по приколу
using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(5));

try
{
    Console.WriteLine("Генератор запущен. Ожидаем подключения Решателя...");
    await pipe.WaitForConnectionAsync(cts.Token);
    Console.WriteLine("Решатель подключился! Начинаем генерацию задач.\n");
}
catch (Exception ex)
{
    Console.WriteLine($"Ошибка подключения: {ex.Message}");
    return;
}


using var writer = new StreamWriter(pipe) { AutoFlush = true };

for (int id = 0; id < TaskCount; id++)
{
    int n = random.Next(MinN, MaxN + 1);
    var system = MatrixGenerator.GeneratePositiveDefinite(n, random);

    string aPath = Path.Combine(TasksDir, $"a{id}.csv");
    string bPath = Path.Combine(TasksDir, $"b{id}.csv");

    await MatrixFileIO.WriteAsync(aPath, bPath, system);

    if (!pipe.IsConnected) break;

    // INFO: таким образом передаёт данные решателю
    string message = $"DONE|{id}|{n}";
    await writer.WriteLineAsync(message);

    Console.WriteLine($"Задача {id,2} отправлена | N = {n,4}");
}

Console.WriteLine("Генератор завершил работу.");
