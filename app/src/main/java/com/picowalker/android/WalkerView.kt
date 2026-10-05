package com.picowalker.android

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.util.AttributeSet
import android.view.View
import kotlin.math.floor
import kotlin.math.min
import kotlin.math.roundToInt

/**
 * Renders the walker's 96x64 4-shade framebuffer, scaled up to fit.
 * Nearest-neighbour scaling keeps the pixel-art crisp.
 */
class WalkerView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private val bitmap: Bitmap =
        Bitmap.createBitmap(WalkerCore.WIDTH, WalkerCore.HEIGHT, Bitmap.Config.ARGB_8888)

    private val bitmapPaint = Paint().apply {
        isFilterBitmap = false
        isDither = false
        isAntiAlias = false
    }

    private val bezelPaint = Paint().apply {
        color = Color.parseColor("#2a2c26")
        style = Paint.Style.STROKE
        isAntiAlias = true
    }

    private val dstRect = Rect()

    fun updateFrame(pixels: IntArray) {
        bitmap.setPixels(pixels, 0, WalkerCore.WIDTH, 0, 0, WalkerCore.WIDTH, WalkerCore.HEIGHT)
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (width == 0 || height == 0) return

        val scaleX = width.toFloat() / WalkerCore.WIDTH
        val scaleY = height.toFloat() / WalkerCore.HEIGHT
        var scale = min(scaleX, scaleY)
        if (scale >= 1f) scale = floor(scale) // integer scale for crisp pixels

        val dw = (WalkerCore.WIDTH * scale).roundToInt().coerceAtLeast(1)
        val dh = (WalkerCore.HEIGHT * scale).roundToInt().coerceAtLeast(1)
        val left = (width - dw) / 2
        val top = (height - dh) / 2
        dstRect.set(left, top, left + dw, top + dh)

        bezelPaint.strokeWidth = (min(dw, dh) / 24f).coerceAtLeast(2f)
        canvas.drawRoundRect(
            dstRect.left - bezelPaint.strokeWidth,
            dstRect.top - bezelPaint.strokeWidth,
            dstRect.right + bezelPaint.strokeWidth,
            dstRect.bottom + bezelPaint.strokeWidth,
            bezelPaint.strokeWidth * 2f,
            bezelPaint.strokeWidth * 2f,
            bezelPaint
        )

        canvas.drawBitmap(bitmap, null, dstRect, bitmapPaint)
    }
}
