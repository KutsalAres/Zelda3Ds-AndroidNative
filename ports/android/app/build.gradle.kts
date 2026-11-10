plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

dependencies {
    implementation(project(":controls"))
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.documentfile:documentfile:1.0.1")
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.json:json:20240303")
}

// Native compilation is a separate incremental build, never a Gradle side effect.
val defaultStage = File(projectDir.parentFile, "staged_native_libs")
val nativeStage: Provider<File> = providers.gradleProperty("triaevumNativeStage")
    .map { path ->
        val f = File(path)
        if (f.isAbsolute) f else File(projectDir.parentFile, path)
    }
    .orElse(defaultStage)

android {
    namespace = "org.triaevum.android"
    compileSdk = 35
    defaultConfig {
        applicationId = "org.triaevum.android"
        minSdk = 29
        targetSdk = 33
        versionCode = 1
        versionName = "0.1-intro-dev"
        ndk { abiFilters += "arm64-v8a" }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
    sourceSets.getByName("main") {
        val stageDir = nativeStage.get()
        if (stageDir.isDirectory) {
            jniLibs.srcDir(stageDir)
        }
    }
    packaging { jniLibs.useLegacyPackaging = true }
    androidResources {
        noCompress += listOf("bin", "o3tu", "mp4", "mp3", "zip")
    }
}
tasks.register("checkNativeStage") {
    onlyIf {
        gradle.startParameter.taskNames.any { it.contains("assemble", ignoreCase = true) || it.contains("package", ignoreCase = true) }
    }
    doLast {
        val stageDir = nativeStage.get()
        check(stageDir.isDirectory) {
            "Native stage directory does not exist: ${stageDir.absolutePath}\n" +
            "Supply -PtriaevumNativeStage=<path> from the Android native build, or place staged native libraries in ${stageDir.absolutePath}."
        }
        for (name in listOf("TriAevum", "triaevum_title_bootstrap", "triaevum_title_aot", "c++_shared")) {
            val libFile = File(stageDir, "arm64-v8a/lib$name.so")
            check(libFile.isFile) {
                "Missing native library: lib$name.so at ${libFile.absolutePath}\n" +
                "Supply -PtriaevumNativeStage=<path> pointing to the staged native build output, or place native libraries in ${stageDir.absolutePath}/arm64-v8a/."
            }
        }
    }
}
tasks.named("preBuild") { dependsOn("checkNativeStage") }
