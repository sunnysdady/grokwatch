import java.util.Properties
plugins { id("com.android.application"); id("org.jetbrains.kotlin.android"); id("org.jetbrains.kotlin.plugin.compose") }
android {
  namespace = "io.grokwatch.app"; compileSdk = 36
  defaultConfig {
    testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    applicationId = "io.grokwatch.app"; minSdk = 31; targetSdk = 36; versionCode = 63; versionName = "0.6.3"
    // Optional default update manifest URL; empty = user sets it in the app. Override: -PupdateUrl=...
    buildConfigField("String", "UPDATE_URL", "\"${(project.findProperty("updateUrl") as String?) ?: ""}\"")
  }
  // Release signing is read from a properties file OUTSIDE the repo (never commit keys):
  // env GW_KEYSTORE_PROPS or ~/.grokwatch-release/keystore.properties (storeFile, storePassword, keyAlias, keyPassword)
  val ksFile = file(System.getenv("GW_KEYSTORE_PROPS") ?: "${System.getProperty("user.home")}/.grokwatch-release/keystore.properties")
  signingConfigs { if (ksFile.exists()) create("release") { val p = Properties().apply { ksFile.inputStream().use { load(it) } }
    storeFile = file(p.getProperty("storeFile")); storePassword = p.getProperty("storePassword"); keyAlias = p.getProperty("keyAlias"); keyPassword = p.getProperty("keyPassword") } }
  buildTypes { getByName("debug") { applicationIdSuffix = ".qa" }; getByName("release") { isMinifyEnabled = false; if (ksFile.exists()) signingConfig = signingConfigs.getByName("release") } }
  buildFeatures { compose = true; buildConfig = true }
  compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
  kotlin { jvmToolchain(21) }
}
kotlin { compilerOptions { jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17) } }
dependencies {
  val bom = platform("androidx.compose:compose-bom:2025.09.01")
  implementation(bom)
  androidTestImplementation(bom)
  androidTestImplementation("androidx.compose.ui:ui-test-junit4")
  androidTestImplementation("androidx.test:runner:1.7.0")
  androidTestImplementation("androidx.test:rules:1.7.0")
  androidTestImplementation("androidx.test.espresso:espresso-core:3.7.0")
  debugImplementation("androidx.compose.ui:ui-test-manifest")
  implementation("androidx.activity:activity-compose:1.11.0")
  implementation("androidx.compose.ui:ui"); implementation("androidx.compose.foundation:foundation")
  implementation("androidx.compose.material3:material3")
  implementation("androidx.core:core-ktx:1.17.0")
  implementation("androidx.lifecycle:lifecycle-runtime-compose:2.9.4")
}
