using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;

namespace Shared
{
    public class SharedMemoryClient : IDisposable
    {
        private readonly MemoryMappedFile _mmf;
        private readonly MemoryMappedViewAccessor _view;

        // Именованные семафоры
        private readonly Semaphore _criticalSectionSemaphore;    // бинарный semaphore (0/1) — для mutual exclusion
        private readonly Semaphore _itemsSemaphore;    // количество заполненных ячеек (producer -> release)
        private readonly Semaphore _spacesSemaphore;   // количество свободных ячеек (consumer -> release)

        // INFO: крч, нааш разделяемая память - первые 4 байта это инт, который показывает сколько считали данны,
        // вторрой инт - сколько записали данных, а дальеш идут данные об изображении - наши структуры
        private const int OffsetReadIndex = 0;
        private const int OffsetWriteIndex = 4;
        private const int OffsetDataStart = 8;

        private readonly int _itemSize;

        public SharedMemoryClient(bool isProducer)
        {
            _itemSize = Marshal.SizeOf<TriangleMessage>();
            long totalSize = OffsetDataStart + (_itemSize * Constants.BufferCapacity);

            // INFO - в файле инфо описал что такое разделяемая память и как работает
            // Создаём или открываем общий mapping
            _mmf = MemoryMappedFile.CreateOrOpen(Constants.MemoryMapName, totalSize);
            // INFO: для работы с ней нужно брать именно ссылку на аксессор - через него доступ
            _view = _mmf.CreateViewAccessor();

            // INFO: все семафоры - именные, это обеспечивает межпроцессовый достум и использование
            // (метод синхронизации между потоками, а остальные два симафора для синхронизации между приложениями)
            _criticalSectionSemaphore = CreateOrOpenSemaphore(Constants.SemaphoreCriticalSectionName, 1, 1);

            // INFO: тут initialCount = 0, что указывает на то, что ни кто не получит доступ к нему, пока не релизнем руками,
            // так мы просто убеждаемся в том что его ни кто не захватит, до первого вызова Produce, тоесть поставщик должен первым что-то сделать
            _itemsSemaphore = CreateOrOpenSemaphore(Constants.SemaphoreItemsName, 0, Constants.BufferCapacity);

            _spacesSemaphore = CreateOrOpenSemaphore(Constants.SemaphoreSpacesName, Constants.BufferCapacity, Constants.BufferCapacity);
        }

        private Semaphore CreateOrOpenSemaphore(string name, int initialCount, int maximumCount)
        {
            try
            {
                var sem = Semaphore.OpenExisting(name);
                return sem;
            }
            catch (WaitHandleCannotBeOpenedException)
            {
                return new Semaphore(initialCount, maximumCount, name);
            }
        }

        // Производитель кладёт сообщение в буфер
        public void Produce(TriangleMessage message)
        {
            // INFO: условный счётчик кол-ва данных в буфере, он у нас ограниченный,если забит, то будем ждать
            _spacesSemaphore.WaitOne();

            // INFO: это твоя критическая секция, доступ может быть только у одного участника
            _criticalSectionSemaphore.WaitOne();
            try
            {
               // INFO: берём данные из разделяемой памяти
                int writeIndex = _view.ReadInt32(OffsetWriteIndex);
                
                long offset = OffsetDataStart + (writeIndex * _itemSize);
                WriteStructure(offset, message);
                
                // INFO: считаем относительно объёма буфера, так как мы его не переполним ни когда,
                // в твоём случае за этим следят семафоры
                int nextWrite = (writeIndex + 1) % Constants.BufferCapacity;
                _view.Write(OffsetWriteIndex, nextWrite);
            }
            finally
            {
                _criticalSectionSemaphore.Release();
            }

            // INFO: сигнализируем потребителю, что может брать данные
            _itemsSemaphore.Release();
        }

        // Потребитель читает сообщение из буфера
        public TriangleMessage Consume()
        {
            // Ждём, пока появится заполненная ячейка
            _itemsSemaphore.WaitOne();

            TriangleMessage msg;

            // Критическая секция
            _criticalSectionSemaphore.WaitOne();
            try
            {
                int readIndex = _view.ReadInt32(OffsetReadIndex);

                long offset = OffsetDataStart + (readIndex * _itemSize);
                msg = ReadStructure(offset);

                // INFO: считаем относительно объёма буфера, так как мы его не переполним ни когда,
                // в твоём случае за этим следят семафоры
                int nextRead = (readIndex + 1) % Constants.BufferCapacity;
                _view.Write(OffsetReadIndex, nextRead);
            }
            finally
            {
                _criticalSectionSemaphore.Release();
            }

            // Увеличиваем пространство (освобождаем слот)
            _spacesSemaphore.Release();

            return msg;
        }

        private void WriteStructure(long offset, TriangleMessage data)
        {
            int size = Marshal.SizeOf(data);
            byte[] buffer = new byte[size];
            IntPtr ptr = Marshal.AllocHGlobal(size);
            try
            {
                Marshal.StructureToPtr(data, ptr, false);
                Marshal.Copy(ptr, buffer, 0, size);
            }
            finally
            {
                Marshal.FreeHGlobal(ptr);
            }
            _view.WriteArray(offset, buffer, 0, size);
        }

        private TriangleMessage ReadStructure(long offset)
        {
            int size = Marshal.SizeOf<TriangleMessage>();
            byte[] buffer = new byte[size];
            _view.ReadArray(offset, buffer, 0, size);
            IntPtr ptr = Marshal.AllocHGlobal(size);
            try
            {
                Marshal.Copy(buffer, 0, ptr, size);
                return Marshal.PtrToStructure<TriangleMessage>(ptr);
            }
            finally
            {
                Marshal.FreeHGlobal(ptr);
            }
        }

        public void Dispose()
        {
            try { _criticalSectionSemaphore?.Dispose(); } catch { }
            try { _itemsSemaphore?.Dispose(); } catch { }
            try { _spacesSemaphore?.Dispose(); } catch { }

            _view?.Dispose();
            _mmf?.Dispose();
        }
    }
}
