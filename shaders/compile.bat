mkdir Spv

.\..\glslangValidator.exe  -V lessionShader.vert -o Spv/vs.spv

.\..\glslangValidator.exe  -V lessionShader.frag -o Spv/fs.spv

.\..\glslangValidator.exe  -V GifFragShader.vert -o Spv/GifFragShaderV.spv

.\..\glslangValidator.exe  -V GifFragShader.frag -o Spv/GifFragShaderF.spv

.\..\glslangValidator.exe  -V WarfareMist.comp -o Spv/WarfareMist.spv

.\..\glslangValidator.exe  -V DungeonWarfareMist.comp -o Spv/DungeonWarfareMist.spv

.\..\glslangValidator.exe  -V LineShader.vert -o Spv/LineShaderV.spv

.\..\glslangValidator.exe  -V LineShader.frag -o Spv/LineShaderF.spv

.\..\glslangValidator.exe  -V SpotShader.vert -o Spv/SpotShaderV.spv

.\..\glslangValidator.exe  -V SpotShader.frag -o Spv/SpotShaderF.spv

.\..\glslangValidator.exe  -V SpotNormal.geom -o Spv/SpotNormal.spv

.\..\glslangValidator.exe  -V DamagePrompt.vert -o Spv/DamagePromptV.spv

.\..\glslangValidator.exe  -V DamagePrompt.frag -o Spv/DamagePromptF.spv

.\..\glslangValidator.exe  -V UVDynamicDiagram.vert -o Spv/UVDynamicDiagramV.spv

.\..\glslangValidator.exe  -V UVDynamicDiagram.frag -o Spv/UVDynamicDiagramF.spv

.\..\glslangValidator.exe  -V UVDynamicDiagram.geom -o Spv/UVDynamicDiagramG.spv

.\..\glslangValidator.exe  -V Background.comp -o Spv/Background.spv

.\..\glslangValidator.exe  -V GridBackground.comp -o Spv/GridBackground.spv

.\..\glslangValidator.exe  -V CircleShader.vert -o Spv/CircleShaderV.spv

.\..\glslangValidator.exe  -V CircleShader.frag -o Spv/CircleShaderF.spv

.\..\glslangValidator.exe  -V CircleNormal.geom -o Spv/CircleNormal.spv

.\..\glslangValidator.exe  -V 2D_GI.comp -o Spv/2D_GI.spv

pause