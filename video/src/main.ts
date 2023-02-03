import FfmpegKit from '../libffwasm/libffmpeg.js'
import { DrawYuv } from './helper.js'

interface IYUVObject {
  width: number
  height: number
  yData: Uint8Array
  uData: Uint8Array
  vData: Uint8Array
}

interface IVideoInfo {
  width: number
  height: number
  duration: number
  fps: number
}

type decodeYUVCallback = (
  yAddr: number, // y 数据在共享内存的起始地址
  uAddr: number, // u 数据在共享内存的起始地址
  vAddr: number, // v 数据在共享内存的起始地址
  yLen: number, // y 数据的长度
  uLen: number, // u 数据的长度
  vLen: number, // v 数据的长度
  width: number, // 这一帧的宽度
  height: number, // 这一帧的高度
  time: number // 视频中这一帧的时间 ms
) => void

// draw yuv data
const canvas = document.getElementById('video-canvas')! as HTMLCanvasElement
const yuvCanvas = new DrawYuv(canvas)
const controller = document.getElementById('video-controller')! as HTMLInputElement

function setStyle(width: number, height: number) {
  canvas.width = width
  canvas.height = height
  canvas.style.width = `${width}px`
  canvas.style.height = `${height}px`
  controller.style.width = `${width + 4}px`
}

(async () => {
  const module = await FfmpegKit({ locateFile: () => '../libffwasm/libffmpeg.wasm' })

  const drawYUVFrame: decodeYUVCallback = (yAddr, uAddr, vAddr, yLen, uLen, vLen, width, height, time) => {
    const yData = module.HEAPU8.subarray(yAddr, yAddr + yLen * height)
    const uData = module.HEAPU8.subarray(uAddr, uAddr + uLen * height / 2)
    const vData = module.HEAPU8.subarray(vAddr, vAddr + vLen * height / 2)
    const yuvObject: IYUVObject = {
      yData: new Uint8Array(yData),
      uData: new Uint8Array(uData),
      vData: new Uint8Array(vData),
      width,
      height,
    }
    yuvCanvas.play(yuvObject)
  }
  const drawYUVFrameP = module.addFunction(drawYUVFrame, 'viiiiiiiii')

  let videoInfo: IVideoInfo
  const videoInfoCallback = (width: number, height: number, duration: number, fps: number) => {
    videoInfo = {
      width,
      height,
      duration,
      fps,
    }
    setStyle(width, height)
  }
  const videoInfoCallbackP = module.addFunction(videoInfoCallback, 'viiif')
  const res = await fetch('/src/assets/640.mp4')
  const videoData = await res.arrayBuffer()
  const videoDataP = module._malloc(videoData.byteLength)
  const heap = new Uint8Array(module.HEAPU8.buffer, videoDataP, videoData.byteLength)
  heap.set(new Uint8Array(videoData))
  const ret = module._ffwasm_decode_open(videoDataP, videoData.byteLength, videoInfoCallbackP)
  if (ret < 0) {
    console.error('_ffwasm_decode_open error')
    return
  }

  const draw = (time: number, onError?: () => void) => {
    if (time < videoInfo.duration) {
      console.log("\n")
      console.log("----- start pts", time)
      const r = module._ffwasm_decode_frame(ret, time, drawYUVFrameP)
      if (r < 0)
        onError && onError()
    }
  }

  const seekdraw = (time: number, onError?: () => void) => {
    if (time < videoInfo.duration) {
      console.log("\n")
      console.log("----- start pts", time)
      const r = module._ffwasm_seek_frame(ret, time, drawYUVFrameP)
      if (r < 0)
        onError && onError()
    }
  }

  let start = Date.now()
  let rafId = requestAnimationFrame(play)
  let delay = 0
  let startStopTime = 0
  function play() {
    const d = Date.now() - start
    if (d >= videoInfo.duration) {
      // TODO: 播放完清空数据
      start = Date.now()
      delay = 0
      startStopTime = 0
      cancelAnimationFrame(rafId)
      return
    }
    delay = d
    controller.value = String(d / videoInfo.duration * 100)
    draw(d, () => {
      cancelAnimationFrame(rafId)
    })
    rafId = requestAnimationFrame(play)
  }

  document.getElementById('video-stop')?.addEventListener('click', () => {
    startStopTime = Date.now()
    if (rafId)
      cancelAnimationFrame(rafId)
  })
  document.getElementById('video-play')?.addEventListener('click', () => {
    const v = Number(controller.value || '0') / 100 * videoInfo.duration
    start += Date.now() - startStopTime - (v - delay)
    rafId = requestAnimationFrame(play)
  })
  controller.addEventListener('mousedown', (e: any) => {
    const v = Number(e?.target?.value || '0') / 100 * videoInfo.duration
    console.log("click mousedown " + v);
    module._ffwasm_hold_seek(ret, true)
  })

  controller.addEventListener('change', (e: any) => {
    const v = Number(e?.target?.value || '0') / 100 * videoInfo.duration
    console.log("click mouseup " + v);
    seekdraw(v)
    module._ffwasm_hold_seek(ret, false)
  })
})()
