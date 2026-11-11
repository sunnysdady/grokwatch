package io.grokwatch.app

import android.Manifest
import android.graphics.Bitmap
import androidx.compose.ui.graphics.asAndroidBitmap
import androidx.compose.ui.test.*
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.test.rule.GrantPermissionRule
import org.junit.Rule
import org.junit.Test
import java.io.File

class HomeScreenTest {
    @get:Rule(order = 0) val permissions = GrantPermissionRule.grant(
        Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT,
        Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION,
        Manifest.permission.POST_NOTIFICATIONS)
    @get:Rule(order = 1) val rule = createAndroidComposeRule<MainActivity>()

    @Test fun developerOptionsStartCollapsedAndCanBeClosed() {
        rule.onNodeWithText("更换背景图片").assertIsDisplayed()
        rule.onNodeWithText("发送测试导航").assertDoesNotExist()
        rule.onNodeWithText("通知使用权").assertDoesNotExist()
        File(rule.activity.getExternalFilesDir(null), "home-collapsed.png").outputStream().use {
            rule.onRoot().captureToImage().asAndroidBitmap().compress(Bitmap.CompressFormat.PNG, 100, it)
        }
        rule.onNodeWithText("开发者选项").performScrollTo().performClick()
        rule.onNodeWithText("发送测试导航").assertExists()
        rule.onNodeWithText("导航解析日志").assertExists()
        rule.onNodeWithText("开发者选项").performScrollTo().performClick()
        rule.onNodeWithText("发送测试导航").assertDoesNotExist()
        rule.onNodeWithText("导航解析日志").assertDoesNotExist()
        rule.onNodeWithText("骑行设置").performScrollTo().performClick()
        rule.onNodeWithText("车速").assertExists()
    }
}
