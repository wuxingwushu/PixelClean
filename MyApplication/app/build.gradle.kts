plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "com.pixelclean"
    compileSdk {
        version = release(36) {
            minorApiLevel = 1
        }
    }
    ndkVersion = "26.1.10909125"

    defaultConfig {
        applicationId = "com.pixelclean"
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"

        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++17"
                arguments += listOf(
                    "-DANDROID_STL=c++_static",
                    "-DANDROID_PLATFORM=android-26",
                    "-DPIXEL_PLATFORM=Android"
                )
            }
        }

        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
    }

    buildTypes {
        debug {
            isDebuggable = true
            isJniDebuggable = true
            externalNativeBuild {
                cmake {
                    arguments += "-DCMAKE_BUILD_TYPE=Debug"
                }
            }
        }
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            externalNativeBuild {
                cmake {
                    arguments += "-DCMAKE_BUILD_TYPE=Release"
                }
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    sourceSets {
        getByName("main") {
            assets.srcDirs("src/main/assets")
        }
    }

    androidResources {
        noCompress += listOf("spv", "png", "mp3", "mid", "sf2", "ttf", "ini", "ico", "wav", "ogg", "jpg")
    }

    buildFeatures {
        buildConfig = true
    }

    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }
}

val spvOutputDir = layout.buildDirectory.dir("android_shaders")

fun findVulkanCompiler(): String? {
    val envSdk = System.getenv("VULKAN_SDK")
    if (envSdk != null) {
        val glslang = File("${envSdk}/Bin/glslangValidator.exe")
        if (glslang.exists()) return glslang.absolutePath
        val glslc = File("${envSdk}/Bin/glslc.exe")
        if (glslc.exists()) return glslc.absolutePath
    }

    val vulkanDir = File("C:/VulkanSDK")
    if (vulkanDir.isDirectory) {
        vulkanDir.listFiles()?.sortedByDescending { it.name }?.forEach { versionDir ->
            val glslang = File(versionDir, "Bin/glslangValidator.exe")
            if (glslang.exists()) return glslang.absolutePath
            val glslc = File(versionDir, "Bin/glslc.exe")
            if (glslc.exists()) return glslc.absolutePath
        }
    }

    return null
}

val compileShaders by tasks.registering {
    description = "Compile GLSL shaders to SPIR-V using Vulkan SDK"
    group = "build"

    val sourceRoot = rootProject.projectDir.parentFile
    val shaderDir = file("${sourceRoot}/shaders")
    val outputDir = spvOutputDir.get().asFile

    val shaders = mapOf(
        "lessionShader.vert" to "vs.spv",
        "lessionShader.frag" to "fs.spv",
        "GifFragShader.vert" to "GifFragShaderV.spv",
        "GifFragShader.frag" to "GifFragShaderF.spv",
        "WarfareMist.comp" to "WarfareMist.spv",
        "DungeonWarfareMist.comp" to "DungeonWarfareMist.spv",
        "LineShader.vert" to "LineShaderV.spv",
        "LineShader.frag" to "LineShaderF.spv",
        "SpotShader.vert" to "SpotShaderV.spv",
        "SpotShader.frag" to "SpotShaderF.spv",
        "SpotNormal.geom" to "SpotNormal.spv",
        "DamagePrompt.vert" to "DamagePromptV.spv",
        "DamagePrompt.frag" to "DamagePromptF.spv",
        "UVDynamicDiagram.vert" to "UVDynamicDiagramV.spv",
        "UVDynamicDiagram.frag" to "UVDynamicDiagramF.spv",
        "UVDynamicDiagram.geom" to "UVDynamicDiagramG.spv",
        "Background.comp" to "Background.spv",
        "GridBackground.comp" to "GridBackground.spv",
        "CircleShader.vert" to "CircleShaderV.spv",
        "CircleShader.frag" to "CircleShaderF.spv",
        "CircleNormal.geom" to "CircleNormal.spv",
        "2D_GI.comp" to "2D_GI.spv",
        "RC_SDF.comp" to "RC_SDF.spv",
        "RC_Cascade.comp" to "RC_Cascade.spv",
        "RC_Display.comp" to "RC_Display.spv"
    )

    inputs.files(shaders.map { file("${shaderDir}/${it.key}") })
    outputs.dir(outputDir)

    onlyIf {
        val compiler = findVulkanCompiler()
        if (compiler == null) {
            logger.warn("Vulkan SDK not found, skipping shader compilation. Install Vulkan SDK to compile shaders.")
            return@onlyIf false
        }
        true
    }

    doLast {
        val compiler = findVulkanCompiler()!!
        val vulkanBinDir = File(compiler).parentFile!!
        outputDir.mkdirs()

        shaders.forEach { (srcFile, dstFile) ->
            val cmd = listOf(
                compiler, "-V",
                file("${shaderDir}/${srcFile}").absolutePath,
                "-o", file("${outputDir}/${dstFile}").absolutePath
            )
            val pb = ProcessBuilder(cmd)
                .directory(vulkanBinDir)
                .redirectErrorStream(true)

            val existingPath = pb.environment()["PATH"] ?: pb.environment()["Path"] ?: ""
            pb.environment()["PATH"] = "${vulkanBinDir.absolutePath};${existingPath}"

            val process = pb.start()
            val output = process.inputStream.bufferedReader().readText()
            val exitCode = process.waitFor()
            if (exitCode != 0) {
                throw GradleException("Shader compilation failed for $srcFile (exit code: $exitCode)\n$output")
            }
        }
    }
}

val prepareAssets by tasks.registering(Copy::class) {
    description = "Collects runtime resource files into assets directory"
    group = "build"

    val sourceRoot = rootProject.projectDir.parentFile
    val destDir = layout.projectDirectory.dir("src/main/assets")

    doFirst {
        delete(fileTree(destDir.dir("shaders")) {
            exclude("*.spv")
        })
    }

    includeEmptyDirs = true

    from("${sourceRoot}/Info.ini") {
        into("")
    }

    from("${sourceRoot}/PhysicsBlock.ini") {
        into("")
    }

    from("${sourceRoot}/Resource") {
        include("**/*")
        into("Resource")
    }

    from("${sourceRoot}/SoundEffect/Resources") {
        include("**/*")
        into("Resources")
    }

    from("${sourceRoot}/InterfaceUI") {
        include("*.ttf")
        into("")
    }

    from("${sourceRoot}/BlockS/Pixel图片") {
        include("*.png")
        into("BlockS/Pixel图片")
    }

    into(destDir)
}

val copyShaders by tasks.registering(Copy::class) {
    description = "Copy compiled SPIR-V shaders to assets"
    group = "build"

    dependsOn(compileShaders)

    from(spvOutputDir) {
        include("*.spv")
    }

    into(layout.projectDirectory.dir("src/main/assets/shaders"))

    onlyIf {
        findVulkanCompiler() != null
    }
}

tasks.named("preBuild") {
    dependsOn(prepareAssets)
    dependsOn(copyShaders)
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.material)
}
