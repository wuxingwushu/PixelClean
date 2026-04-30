@echo off
REM Compile shaders to specified output directory
REM Usage: compile_to_dir.bat <output_directory> <glslangValidator_path>

set OUTPUT_DIR=%~1
set GLSLANG=%~2
set SCRIPT_DIR=%~dp0

if "%OUTPUT_DIR%"=="" (
    echo Error: Output directory not specified
    echo Usage: compile_to_dir.bat ^<output_directory^> ^<glslangValidator_path^>
    exit /b 1
)

if "%GLSLANG%"=="" (
    echo Error: glslangValidator path not specified
    echo Usage: compile_to_dir.bat ^<output_directory^> ^<glslangValidator_path^>
    exit /b 1
)

if not exist "%GLSLANG%" (
    echo Error: glslangValidator not found at: %GLSLANG%
    exit /b 1
)

mkdir "%OUTPUT_DIR%" 2>nul

"%GLSLANG%" -V "%SCRIPT_DIR%lessionShader.vert" -o "%OUTPUT_DIR%/vs.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%lessionShader.frag" -o "%OUTPUT_DIR%/fs.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%GifFragShader.vert" -o "%OUTPUT_DIR%/GifFragShaderV.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%GifFragShader.frag" -o "%OUTPUT_DIR%/GifFragShaderF.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%WarfareMist.comp" -o "%OUTPUT_DIR%/WarfareMist.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%DungeonWarfareMist.comp" -o "%OUTPUT_DIR%/DungeonWarfareMist.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%LineShader.vert" -o "%OUTPUT_DIR%/LineShaderV.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%LineShader.frag" -o "%OUTPUT_DIR%/LineShaderF.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%SpotShader.vert" -o "%OUTPUT_DIR%/SpotShaderV.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%SpotShader.frag" -o "%OUTPUT_DIR%/SpotShaderF.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%SpotNormal.geom" -o "%OUTPUT_DIR%/SpotNormal.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%DamagePrompt.vert" -o "%OUTPUT_DIR%/DamagePromptV.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%DamagePrompt.frag" -o "%OUTPUT_DIR%/DamagePromptF.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%UVDynamicDiagram.vert" -o "%OUTPUT_DIR%/UVDynamicDiagramV.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%UVDynamicDiagram.frag" -o "%OUTPUT_DIR%/UVDynamicDiagramF.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%UVDynamicDiagram.geom" -o "%OUTPUT_DIR%/UVDynamicDiagramG.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%Background.comp" -o "%OUTPUT_DIR%/Background.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%GridBackground.comp" -o "%OUTPUT_DIR%/GridBackground.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%CircleShader.vert" -o "%OUTPUT_DIR%/CircleShaderV.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%CircleShader.frag" -o "%OUTPUT_DIR%/CircleShaderF.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%CircleNormal.geom" -o "%OUTPUT_DIR%/CircleNormal.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%2D_GI.comp" -o "%OUTPUT_DIR%/2D_GI.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%RC_SDF.comp" -o "%OUTPUT_DIR%/RC_SDF.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%RC_Cascade.comp" -o "%OUTPUT_DIR%/RC_Cascade.spv"
"%GLSLANG%" -V "%SCRIPT_DIR%RC_Display.comp" -o "%OUTPUT_DIR%/RC_Display.spv"

echo Shader compilation completed. Output directory: %OUTPUT_DIR%
