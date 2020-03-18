/*
Copyright 2020 Ralph Ridley

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "getline.c"

#define MAX_BUFFER_SIZE 1000 * 100 // 100,000 data members per buffer
#define FLT_EQUALS(a, b) abs(a - b) < 0.000001

const char kHelpArg[6] = "-help";
const char kTangentArg[3] = "-t";
const char kVerboseArg[3] = "-v";
const char kInputExt[5] = ".obj";
const char kVertexDelim[1] = {' '};
const char kPositionIndicator[2] = "v";
const char kTexcoordIndicator[3] = "vt";
const char kNormalIndicator[3] = "vn";
const char kIndexIndicator[2] = "f";

typedef int bool;
enum { 
    false = 0, 
    true  = 1
};

typedef enum Flags {
    FLAG_VERBOSE = 0x0002,
    FLAG_GENERATE_TANGENTS = 0x0004,
    FLAG_FLIP_TEXCOORD_V = 0x0008
} Flags;

enum VertexComponents {
    VERTEX_POSITION = 0x0001,
    VERTEX_TEXCOORDS = 0x0002,
    VERTEX_NORMALS = 0x0004,
    VERTEX_TANGENTS = 0x0008
};

typedef struct Vec2 {
    float x;
    float y;
} Vec2;

typedef struct Vec3 {
    float x;
    float y;
    float z;
} Vec3;

typedef struct Header {
    unsigned int MeshCount;
    unsigned int VertexSize; // Num floats making up a vertex
    unsigned int IndexSize; // Num bytes making up an index
    unsigned int Components; // Components making up a vertex
} Header;

typedef struct Mesh {
    unsigned int VertexCount; // Number of vertices
    unsigned int IndexCount; // Number of indices
    unsigned int VertexOffset; // sizeof(float) * VertexSize offset in to vertex data
    unsigned int IndexOffset; // IndexSize offset in to vertex data
} Mesh;

typedef struct Buffers {
    size_t PositionCount;
    size_t TexcoordCount;
    size_t NormalCount;

    Vec3* Positions;
    Vec2* Texcoords;
    Vec3* Normals;

    unsigned int* PosIndices;
    unsigned int* TexIndices;
    unsigned int* NormIndices;

    Header Header;
    Mesh Mesh;
    float* Vertices;
    unsigned int* Indices;
} Buffers;

Flags g_Flags = 0;

bool VertexEqual(const float* v0, const float* v1, const size_t vertexSize) {
    for (size_t i = 0; i < vertexSize; ++i) {
        if (!FLT_EQUALS(v0[i], v1[i])) return false;
    }
    return true;
}

bool AllocateBuffers(Buffers* buffers) {
    memset(buffers, 0, sizeof(Buffers));
    buffers->Positions = malloc(MAX_BUFFER_SIZE * sizeof(Vec3));
    buffers->Texcoords = malloc(MAX_BUFFER_SIZE * sizeof(Vec2));
    buffers->Normals = malloc(MAX_BUFFER_SIZE * sizeof(Vec3));
    buffers->PosIndices = malloc(MAX_BUFFER_SIZE * sizeof(int));
    buffers->TexIndices = malloc(MAX_BUFFER_SIZE * sizeof(int));
    buffers->NormIndices = malloc(MAX_BUFFER_SIZE * sizeof(int));
    buffers->Indices = malloc(MAX_BUFFER_SIZE * sizeof(unsigned int));
    buffers->Vertices = malloc(MAX_BUFFER_SIZE * sizeof(float) * 12);
    
    return buffers->Positions && buffers->Texcoords && buffers->Normals && buffers->PosIndices && 
           buffers->TexIndices && buffers->NormIndices && buffers->Indices && buffers->Vertices;
}

void FreeBuffers(Buffers* buffers) {
    free(buffers->Positions);
    free(buffers->Texcoords);
    free(buffers->Normals);
    free(buffers->PosIndices);
    free(buffers->TexIndices);
    free(buffers->NormIndices);
    free(buffers->Indices);
    free(buffers->Vertices);
}

void ExtractVec2(Vec2* vec, char* line, const char* delim) {
    char* splitToken = strtok(NULL, delim);
    vec->x = atof(splitToken);
    splitToken = strtok(NULL, delim);
    vec->y = atof(splitToken);
}

void ExtractVec3(Vec3* vec, char* line, const char* delim) {
    char* splitToken = strtok(NULL, delim);
    vec->x = atof(splitToken);
    splitToken = strtok(NULL, delim);
    vec->y = atof(splitToken);
    splitToken = strtok(NULL, delim);
    vec->z = atof(splitToken);
}

// Assumes obj has triangulated faces
void ExtractFace(int* pos, int* uv, int* norm, char* line, const char* delim, unsigned int* components) {
    for (size_t i = 0; i < 3; ++i) {
        char* splitToken = strtok(NULL, delim);
        pos[i] = strtoul(splitToken, NULL, 0) - 1;
        splitToken = strtok(NULL, delim);
        if (*components & VERTEX_TEXCOORDS) {
            uv[i] = strtoul(splitToken, NULL, 0) - 1;
            if (*components & VERTEX_NORMALS) splitToken = strtok(NULL, delim);
        }
        if (*components & VERTEX_NORMALS) {
            norm[i] = strtoul(splitToken, NULL, 0) - 1;
        }
    }
}

bool CompareIndicator(const char* indicator, const char* token) {
    size_t indLen = strlen(indicator);
    if (strlen(token) != indLen) return false;
    for (size_t i = 0; i < indLen; ++i) {
        if (token[i] != indicator[i]) return false;
    }
    return true;
}

char* ReadVec2s(FILE** objFile, Vec2* buffer, size_t* count, char* line, const char* indicator) {
    size_t len;
    int read;
    do {
        ExtractVec2(&buffer[(*count)++], line, kVertexDelim);
    } while ((read = getline(&line, &len, *objFile)) != -1 && CompareIndicator(indicator, strtok(line, kVertexDelim)));
    return line;
}

char* ReadVec3s(FILE** objFile, Vec3* buffer, size_t* count, char* line, const char* indicator) {
    size_t len;
    int read;
    do {
        ExtractVec3(&buffer[(*count)++], line, kVertexDelim);
    } while ((read = getline(&line, &len, *objFile)) != -1 && CompareIndicator(indicator, strtok(line, kVertexDelim)));
    return line;
}

void ReadVertexData(FILE* objFile, Buffers* buffers) {
    char* line = NULL;
    size_t len = 0;
    int read;
    buffers->Header.Components |= VERTEX_POSITION;
    while ((read = getline(&line, &len, objFile)) != -1 && !CompareIndicator(kPositionIndicator, strtok(line, kVertexDelim))) { }
    line = ReadVec3s(&objFile, buffers->Positions, &buffers->PositionCount, line, kPositionIndicator);
    
    if (CompareIndicator(kTexcoordIndicator, line)) {
        buffers->Header.Components |= VERTEX_TEXCOORDS;
        line = ReadVec2s(&objFile, buffers->Texcoords, &buffers->TexcoordCount, line, kTexcoordIndicator);
    }
    else if (CompareIndicator(kNormalIndicator, line)) {
        buffers->Header.Components |= VERTEX_NORMALS;
        line = ReadVec3s(&objFile, buffers->Normals, &buffers->NormalCount, line, kNormalIndicator);
    }
    else {
        return;
    }
    
    if (CompareIndicator(kTexcoordIndicator, line)) {
        buffers->Header.Components |= VERTEX_TEXCOORDS;
        line = ReadVec2s(&objFile, buffers->Texcoords, &buffers->TexcoordCount, line, kTexcoordIndicator);
    }
    else if (CompareIndicator(kNormalIndicator, line)) {
        buffers->Header.Components |= VERTEX_NORMALS;
        line = ReadVec3s(&objFile, buffers->Normals, &buffers->NormalCount, line, kNormalIndicator);
    }
}

void ReadIndexData(FILE** objFile, Buffers* buffers) {
    char* line = NULL;
    size_t len = 0;
    size_t i = 0;
    int read;
    while ((read = getline(&line, &len, *objFile)) != -1 && !CompareIndicator(kIndexIndicator, strtok(line, kVertexDelim))) { }
    
    char delim[3] = "/ ";
    do {
        ExtractFace(&buffers->PosIndices[i], &buffers->TexIndices[i], &buffers->NormIndices[i], line, delim, &buffers->Header.Components);
        buffers->Mesh.IndexCount += 3;
        i += 3;
    } while ((read = getline(&line, &len, *objFile)) != -1 && CompareIndicator(kIndexIndicator, strtok(line, kVertexDelim)));
}

bool ConvertData(FILE* binFile, Buffers* buffers) {
    buffers->Header.VertexSize = 3; // Assume position
    buffers->Header.VertexSize += buffers->Header.Components & VERTEX_TEXCOORDS ? 2 : 0;
    buffers->Header.VertexSize += buffers->Header.Components & VERTEX_NORMALS   ? 3 : 0;
    buffers->Header.VertexSize += buffers->Header.Components & VERTEX_TANGENTS  ? 3 : 0;
    buffers->Header.IndexSize = sizeof(unsigned int);
    
    for (unsigned int i = 0; i < buffers->Mesh.IndexCount; ++i) {
        unsigned int j = buffers->Mesh.VertexCount * buffers->Header.VertexSize;
        float* vertex = &buffers->Vertices[j];
        float* vptr = vertex;

        memcpy(vptr, &buffers->Positions[buffers->PosIndices[i]], sizeof(Vec3));
        vptr += 3;
        if (buffers->Header.Components & VERTEX_TEXCOORDS) {
            memcpy(vptr, &buffers->Texcoords[buffers->TexIndices[i]], sizeof(Vec2));
            if (g_Flags & FLAG_FLIP_TEXCOORD_V) vptr[1] = 1.0f - vptr[1];
            vptr += 2;
        }
        if (buffers->Header.Components & VERTEX_NORMALS) {
            memcpy(vptr, &buffers->Normals[buffers->NormIndices[i]], sizeof(Vec3));
            vptr += 3;
        }

        float* duplicate = NULL;
        unsigned int dupIdx;
        for (dupIdx = 0; dupIdx < buffers->Mesh.VertexCount; ++dupIdx) {
            if (VertexEqual(vertex, &buffers->Vertices[dupIdx * buffers->Header.VertexSize], buffers->Header.VertexSize)) {
                duplicate = &buffers->Vertices[dupIdx * buffers->Header.VertexSize];
                break;
            }
        }
        if (duplicate) buffers->Indices[i] = dupIdx;
        else buffers->Indices[i] = buffers->Mesh.VertexCount++;
    }
    
    buffers->Header.MeshCount = 1;
    buffers->Mesh.IndexOffset = 0;
    buffers->Mesh.VertexOffset = 0;
    if (fwrite(&buffers->Header, sizeof(Header), 1, binFile) < 1) {
        printf("Error: Failed to write binary header! Aborting.");
        return false;
    }
    if (fwrite(&buffers->Mesh, sizeof(Mesh), 1, binFile) < 1) {
        printf("Error: Failed to write binary mesh! Aborting.");
        return false;
    }
    if (fwrite(buffers->Vertices, sizeof(float), buffers->Mesh.VertexCount * buffers->Header.VertexSize, binFile) < buffers->Mesh.VertexCount * buffers->Header.VertexSize) {
        printf("Error: Failed to write binary vertex data! Aborting.");
        return false;
    }
    if (fwrite(buffers->Indices, sizeof(unsigned int), buffers->Mesh.IndexCount, binFile) < buffers->Mesh.IndexCount) {
        printf("Error: Failed to write binary index data! Aborting.");
        return false;
    }
    
    return true;
}

bool Convert(const char* inName, const char* outName) {
    FILE* objFile = fopen(inName, "r");
    FILE* binFile = fopen(outName, "wb");
    if (!objFile || !binFile) {
        printf("Error: Failed to open the files.");
        return false;
    }

    Buffers buffers;
    if (!AllocateBuffers(&buffers)) {
        printf("Error: Failed to allocate required internal memory.");
        return false;
    }

    ReadVertexData(objFile, &buffers);
    ReadIndexData(&objFile, &buffers);
    ConvertData(binFile, &buffers);
    
    FreeBuffers(&buffers);
    if (fclose(objFile) || fclose(binFile)) {
        printf("Error: Failed to close the files!");
    }

    if (g_Flags & FLAG_VERBOSE) {
        ReadBinary(outName);
    }

    return true;
}

bool ReadBinaryFile(FILE* binFile) {
    Header header;
    if (fread(&header, sizeof(Header), 1, binFile) < 1) {
        printf("Error: Failed to read binary header! Aborting.");
        return false;
    }
    Mesh mesh;
    if (fread(&mesh, sizeof(Mesh), 1, binFile) < 1) {
        printf("Error: Failed to read binary mesh! Aborting.");
        return false;
    }

    float* vertices = malloc(sizeof(float) * header.VertexSize * mesh.VertexCount);
    unsigned int* indices = malloc(sizeof(unsigned int) * mesh.IndexCount);

    if (fread(vertices, sizeof(float) * header.VertexSize, mesh.VertexCount, binFile) < mesh.VertexCount) {
        printf("Error: Failed to read binary vertices! Aborting.");
        free(vertices);
        free(indices);
        return false;
    }
    if (fread(indices, sizeof(unsigned int), mesh.IndexCount, binFile) < mesh.IndexCount) {
        printf("Error: Failed to read binary indices! Aborting.");
        free(vertices);
        free(indices);
        return false;
    }

    printf("Object:    Vertex Count %i    Vertex Size %i    Index Count %i    Index Size %i    Components %i\n", 
        mesh.VertexCount, header.VertexSize, mesh.IndexCount, header.IndexSize, header.Components);
    for (unsigned int i = 0; i < mesh.VertexCount; ++i) {
        unsigned int j = i * header.VertexSize;
        printf("Vertex %i v(%f, %f, %f) ", i, vertices[j], vertices[j + 1], vertices[j + 2]);
        j += 3;
        if (header.Components & VERTEX_TEXCOORDS) {
            printf("vt(%f, %f) ", vertices[j], vertices[j + 1]);
            j += 2;
        }
        if (header.Components & VERTEX_NORMALS) {
            printf("vn(%f, %f, %f)", vertices[j], vertices[j + 1], vertices[j + 2]);
            j += 3;
        }
        if (header.Components & VERTEX_TANGENTS) {
            printf("tn(%f, %f, %f)", vertices[j], vertices[j + 1], vertices[j + 2]);
        }
        printf("\n");
    }
    printf("Indices\n");
    for (unsigned int i = 0; i < mesh.IndexCount; ++i) {
        printf("%i ", indices[i]);
    }
    printf("\n");

    free(vertices);
    free(indices);
    return true;
}

bool ReadBinary(const char* binName) {
    FILE* binFile = fopen(binName, "rb");
    if (!binFile) {
        printf("Error: Failed to open the binary file for reading.");
        return false;
    }

    ReadBinaryFile(binFile);

    if (fclose(binFile)) {
        printf("Error: Failed to close the files!");
        return false;
    }
    return true;
}

bool BatchBinaries(const char* inBinName, const char* const* srcNames, int srcCount) {
    // Batch the binary vertex data and index data packed tightly in one file, with multiple "Headers" for each separate dataset.
    return true;
}

void OutputHelp() {
    printf("objtobin help: \n\t"
            "Converts Wavefront obj meshes containing vertex positions, uvs, and normals with triangulated faces to an interleaved binary format.\n\t"
            "Output mode (-c):\n\t\tRead a wavefront obj and output it in binary format.\n\t"
                "Usage: objtobin.exe -c [input obj] [output bin] [flags]\n\t\t"
                "Flags:\n\t\t\t -t (Generate tangents)\n\t\t\t -v (Verbose)\n\t\t\t -f (Flip texcoords vertically)\n\t"
            "Inspect mode (-i):\n\t\tRead a binary obj file and display its data.\n\t\t"
                "Usage: objtobin.exe -i [input bin]\n\t"
            "Batch mode (-b):\n\t\tBatch the input binary files together to one file.\n\t\t"
                "Usage: objtobin.exe -b [output bin] [input bin 1, input bin 2, ...]\n\t");
}

bool ParseConvertArgs(int argc, char** argv, char** inObjName, char** outBinName) {
    if (argc < 4) {
        OutputHelp();
        printf("Error: Not enough arguments provided.\n");
        return false;
    }

    size_t inNameLen = strlen(argv[2]);
    size_t outNameLen = strlen(argv[3]);
    if (inNameLen < 4 || strcmp(&argv[2][inNameLen - 4], kInputExt) != 0) {
        printf("Error: Input obj name is not valid, must be in format *.obj and both names (%s, %s)\n", argv[2], argv[3]);
        return false;
    }
    *inObjName = argv[2];
    *outBinName = argv[3];

    for (size_t i = 4; i < argc; ++i) {
        if (strcmp(argv[i], "-t") == 0) g_Flags |= FLAG_GENERATE_TANGENTS;
        else if (strcmp(argv[i], "-v") == 0) g_Flags |= FLAG_VERBOSE;
        else if (strcmp(argv[i], "-f") == 0) g_Flags |= FLAG_FLIP_TEXCOORD_V;
        else OutputHelp();
    }
    return true;
}

bool ParseReadArgs(int argc, char** argv, char** inName) {
    if (argc < 3) {
        OutputHelp();
        printf("Error: Not enough arguments provided.\n");
        return false;
    }

    *inName = argv[2];

    return true;
}

bool ParseBatchArgs(int argc, char** argv, char** outName, char*** srcNames, int* srcCount) {
    if (argc < 4) {
        OutputHelp();
        printf("Error: Not enough arguments provided.\n");
        return false;
    }

    *outName = argv[2];
    *srcNames = &argv[3];
    *srcCount = argc - 3;

    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        OutputHelp();
        return 0;
    }
    if (strcmp(argv[1], "-c") == 0) {
        char* inObjName;
        char* outBinName;
        if (!ParseConvertArgs(argc, argv, &inObjName, &outBinName)) return false;
        printf("Converting %s -> %s...\n", inObjName, outBinName);
        bool success = Convert(inObjName, outBinName);
        if (success) printf("Successfully converted.\n");
        else printf("Failed to convert.\n");
        return success;
    }
    else if (strcmp(argv[1], "-i") == 0) {
        char* inBinName;
        if (!ParseReadArgs(argc, argv, &inBinName)) return false;
        return ReadBinary(inBinName);
    }
    else if (strcmp(argv[1], "-b") == 0) {
        char* inBinName;
        char** srcNames;
        int srcCount;
        if (!ParseBatchArgs(argc, argv, &inBinName, &srcNames, &srcCount)) return false;
        return BatchBinaries(inBinName, srcNames, srcCount);
    }
    else {
        OutputHelp();
        return 0;
    }
}
