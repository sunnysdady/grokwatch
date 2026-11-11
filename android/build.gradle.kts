plugins {
  id("com.android.application") version "8.13.0" apply false
  id("org.jetbrains.kotlin.android") version "2.2.20" apply false
  id("org.jetbrains.kotlin.plugin.compose") version "2.2.20" apply false
}

// Optional local build root avoids cloud-evicted intermediates on synced workspaces.
System.getenv("GW_ANDROID_BUILD_ROOT")?.let { localRoot ->
    allprojects { layout.buildDirectory.set(file("$localRoot/${project.name}")) }
}
