# ObjToBin
Short single file c program to convert Wavefront objs to interleaved vertex and index data, particularly with game engines in mind. Input objs must have triangulated faces, results are undefined if they are not.

Wavefront obj is a great open format for creating, sharing, and visualising models. However, programs such as game engines suffer from long loading times when trying to read Wavefront obj mesh files in to a useful format. Game engines using graphics APIs such as OpenGL, Vulkan, or DirectX want to have interleaved vertex data packed tightly in to a buffer, with separate index data packed in their own buffer.

To reduce loading times for the engine or application, ObjToBin should be used to generate binary files prior to running the application. It is recommended to use a post build command for this purpose if available.

## Compiling

To compile, use through any modern c compiler such as MSVC or gcc. See releases for compiled executables.

## Running

Run in the command line, see help.
```
ObjToBinary help:
        Converts Wavefront obj meshes containing vertex positions, uvs, and normals with triangulated faces to an interleaved binary format.
        Output mode (-c):
                Read a wavefront obj and output it in binary format.
        Usage: ObjToBinary.exe -c [input obj] [output bin] [flags]
                Flags:
                         -t (Generate tangents)
                         -v (Verbose)
                         -f (Flip texcoords vertically)
        Inspect mode (-i):
                Read a binary obj file and display its data.
                Usage: ObjToBinary.exe -i [input bin]
        Batch mode (-b):
                Batch the input binary files together to one file.
                Usage: ObjToBinary.exe -b [output bin] [input bin 1, input bin 2, ...]
```

## Format

The binary data begins with a single Header to give the number of meshes N to follow.
```
typedef struct Header {
    unsigned int MeshCount; // How many meshes there are following the header
    unsigned int VertexSize; // Num floats making up a vertex
    unsigned int IndexSize; // Num bytes making up an index
    unsigned int Components; // Components making up a vertex
    unsigned int TotalVertices;
    unsigned int TotalIndices;
} Header;

typedef struct Mesh {
    unsigned int VertexCount; // Number of vertices
    unsigned int IndexCount; // Number of indices
    unsigned int VertexOffset; // sizeof(float) * VertexSize offset in to vertex data
    unsigned int IndexOffset; // IndexSize offset in to vertex data
} Mesh;
```
Using the header and mesh information, the data can be extracted. The vertex data begins immediately after the headers, and its length in floats can be calculated by adding the `VertexCount * VertexSize` of all the headers together. The index data immediately follows the vertex block and its length in floats can be calculated by adding the `IndexCount * IndexSize` of all the headers together. Individual meshes are packed tightly, therefore to get the second mesh, its vertex and index offset is the `VertexCount` and `IndexCount` of the first mesh.

See an example Vertex struct that may be used to represent a full vertex. In the binary data, the `Components` value can be used to determine which attributes are included:
```
struct Vertex {
  float x, y, z; 
  float u, v;
  float nx, ny, nz;
  float tx, ty, tz;
};

enum VertexComponents {
    VERTEX_POSITION = 0x0001,
    VERTEX_TEXCOORDS = 0x0002,
    VERTEX_NORMALS = 0x0004,
    VERTEX_TANGENTS = 0x0008
};
```

## Using in an Application

See `bool ReadBinaryFile(FILE* binFile)` for an example of how to read the data in. The data can also be viewed with this tool by using the `-i` mode in the command line.

### Future
 - Allow loader to read multiple meshes from the same .obj
 - Generate tangents
 - Batch multiple binary files in to a single file
