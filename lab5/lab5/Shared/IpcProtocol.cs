using System.Runtime.InteropServices;

namespace Shared
{
    // IPC - Inter-Process Communication
    // INFO: так как мы эти данные будем кидать чере память, нужно указывать эти аттрибуты,
    // так мы гарантируем корректность работы программы
    // LayoutKind.Sequential - поля структуры будут лежать в памяти в том порядке, в котором объявлены
    // CharSet.Unicode - как строки и символы кодируются в памяти, Unicode означает UTF-16 (2 байта на символ)
    // Pack = 1 - крч, чтобы между данными в памяти не было паддингов (отступов)
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode, Pack = 1)]
    public struct PointStruct
    {
        public int X;
        public int Y;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode, Pack = 1)]
    public struct TriangleMessage
    {
        // INFO: это короче указатель на конец сообщения,
        // когда мы все изображения отправим, то мы создадим структуру с путсыми полями,
        // а этот будет тру, чтобы консюмер понял, что больше данных не ожидается
        public bool IsEndOfStream;

        // UnmanagedType.ByValTStr - Указывает, что строка будет храниться как фиксированный массив символов внутри структуры
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string ImageName;

        public byte TargetR;
        public byte TargetG;
        public byte TargetB;

        public int ComponentId;

        public int PointCount;

        // UnmanagedType.ByValArray - Указывает, что массив хранится встроенно в структуре, а не как ссылка
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = MaxPointsPerComponent)]
        public PointStruct[] Points;

        public const int MaxPointsPerComponent = 30000;
    }

    public class Constants
    {
        public const string MemoryMapName = "Lab5_SharedMemory";

        public const string SemaphoreCriticalSectionName = "Lab5_Semaphore_CriticalSection";  
        public const string SemaphoreItemsName = "Lab5_Semaphore_Items";  
        public const string SemaphoreSpacesName = "Lab5_Semaphore_Spaces"; 

        public const int BufferCapacity = 5;
    }
}
