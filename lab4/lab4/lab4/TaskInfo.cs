namespace lab4
{
    public class TaskInfo
    {
        public int Id { get; set; }
        public string MatrixAFile { get; set; }
        public string VectorBFile { get; set; }
        public string ResultFile { get; set; }

        public TaskInfo(int id, string matrixFile, string vectorFile, string resultFile, int dimension = 0)
        {
            Id = id;
            MatrixAFile = matrixFile;
            VectorBFile = vectorFile;
            ResultFile = resultFile;
        }
    }
}
