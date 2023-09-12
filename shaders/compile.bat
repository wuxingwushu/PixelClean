mkdir Spv

.\..\glslangValidator.exe  -V lessionShader.vert -o Spv/vs.spv

.\..\glslangValidator.exe  -V lessionShader.frag -o Spv/fs.spv

.\..\glslangValidator.exe  -V GifFragShader.vert -o Spv/GifFragShaderV.spv

.\..\glslangValidator.exe  -V GifFragShader.frag -o Spv/GifFragShaderF.spv

.\..\glslangValidator.exe  -V WarfareMist.comp -o Spv/WarfareMist.spv

.\..\glslangValidator.exe  -V DungeonWarfareMist.comp -o Spv/DungeonWarfareMist.spv

pause