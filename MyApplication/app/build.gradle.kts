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

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

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

    buildFeatures {
        buildConfig = true
    }

    aaptOptions {
        noCompress("spv", "png", "mp3", "mid", "sf2", "ttf", "ini", "ico", "wav", "ogg", "jpg")
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

val compileShaders by tasks.registering(Exec::class) {
    description = "Compile GLSL shaders to SPIR-V using Vulkan SDK"
    group = "build"

    val sourceRoot = rootProject.projectDir.parentFile
    val shaderDir = file("${sourceRoot}/shaders")
    val compileScript = file("${shaderDir}/compile.bat")
    val outputDir = spvOutputDir.get().asFile

    outputs.dir(outputDir)

    workingDir = shaderDir

    commandLine(
        "cmd", "/c",
        if (compileScript.exists()) compileScript.absolutePath else "",
        outputDir.absolutePath,
        findVulkanCompiler() ?: ""
    )

    onlyIf {
        val compiler = findVulkanCompiler()
        if (compiler == null) {
            logger.warn("Vulkan SDK not found, skipping shader compilation. Install Vulkan SDK to compile shaders.")
            return@onlyIf false
        }
        compileScript.exists()
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
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.constraintlayout)
    implementation(libs.androidx.core.ktx)
    implementation(libs.material)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(libs.androidx.junit)
}
