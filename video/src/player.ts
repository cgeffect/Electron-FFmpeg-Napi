import { DrawYuv } from './helper.js'
import type { DecodeYUVCallback, IFFModule, IVideoInfo, IYUVObject } from './types'

export class FFWasmPlayer {
  private readonly canvas: HTMLCanvasElement
  private readonly controller: HTMLInputElement
  private readonly playBtn: HTMLButtonElement
  private readonly stopBtn: HTMLButtonElement
  private readonly yuvCanvas: any
  private module!: IFFModule
  private handle = 0
  private videoInfo: IVideoInfo | null = null
  private videoDataPtr = 0
  private decodeCallbackPtr = 0
  private infoCallbackPtr = 0
  private rafId = 0
  private startTs = 0
  private pauseAt = 0
  private lastDrawPts = 0

  constructor() {
    const canvas = document.getElementById('video-canvas') as HTMLCanvasElement | null
    const controller = document.getElementById('video-controller') as HTMLInputElement | null
    const playBtn = document.getElementById('video-play') as HTMLButtonElement | null
    const stopBtn = document.getElementById('video-stop') as HTMLButtonElement | null
    if (!canvas || !controller || !playBtn || !stopBtn)
      throw new Error('Missing required DOM elements')

    this.canvas = canvas
    this.controller = controller
    this.playBtn = playBtn
    this.stopBtn = stopBtn
    this.yuvCanvas = new DrawYuv(this.canvas)
  }

  async init(defaultSource = './assets/11.mp4') {
    const ffmpegModule = await import('/wasm/libffmpeg.js')
    const ffmpegInit = ffmpegModule.default as (options: { locateFile: () => string }) => Promise<IFFModule>
    this.module = await ffmpegInit({ locateFile: () => '/wasm/libffmpeg.wasm' })
    this.bindCallbacks()
    this.bindEvents()
    await this.loadFromUrl(defaultSource)
  }

  async loadFromUrl(url: string) {
    const response = await fetch(url)
    if (!response.ok)
      throw new Error(`fetch video failed: ${response.status} ${response.statusText}`)
    const videoData = await response.arrayBuffer()
    this.openVideo(videoData)
  }

  getVideoInfo() {
    return this.videoInfo
  }

  loadFromFile(file: File) {
    return file.arrayBuffer().then((videoData) => {
      this.openVideo(videoData)
    })
  }

  private bindCallbacks() {
    const drawYUVFrame: DecodeYUVCallback = (yAddr, uAddr, vAddr, yLen, uLen, vLen, width, height) => {
      const yData = this.module.HEAPU8.subarray(yAddr, yAddr + yLen * height)
      const uData = this.module.HEAPU8.subarray(uAddr, uAddr + (uLen * height) / 2)
      const vData = this.module.HEAPU8.subarray(vAddr, vAddr + (vLen * height) / 2)
      const frame: IYUVObject = {
        yData: new Uint8Array(yData),
        uData: new Uint8Array(uData),
        vData: new Uint8Array(vData),
        width,
        height,
      }
      this.yuvCanvas.play(frame)
    }
    this.decodeCallbackPtr = this.module.addFunction(drawYUVFrame, 'viiiiiiiii')

    const videoInfoCallback = (width: number, height: number, duration: number, fps: number) => {
      this.videoInfo = { width, height, duration, fps }
      this.setStyle(width, height)
      this.controller.value = '0'
    }
    this.infoCallbackPtr = this.module.addFunction(videoInfoCallback as (...args: number[]) => void, 'viiif')
  }

  private bindEvents() {
    this.playBtn.addEventListener('click', () => this.play())
    this.stopBtn.addEventListener('click', () => this.pause())
    this.controller.addEventListener('mousedown', () => {
      if (!this.handle)
        return
      this.module._ffwasm_hold_seek(this.handle, 1)
      this.pause()
    })
    this.controller.addEventListener('change', () => {
      const info = this.videoInfo
      if (!info || !this.handle)
        return
      const pts = (Number(this.controller.value || '0') / 100) * info.duration
      const ret = this.module._ffwasm_seek_frame(this.handle, pts, this.decodeCallbackPtr)
      if (ret < 0)
        console.error('_ffwasm_seek_frame failed', ret, pts)
      this.module._ffwasm_hold_seek(this.handle, 0)
      this.lastDrawPts = pts
      this.startTs = Date.now() - pts
    })
  }

  private resetPlaybackState() {
    if (this.rafId) {
      cancelAnimationFrame(this.rafId)
      this.rafId = 0
    }
    this.startTs = 0
    this.pauseAt = 0
    this.lastDrawPts = 0
    this.controller.value = '0'
  }

  private closeCurrentVideo() {
    this.resetPlaybackState()
    if (this.handle) {
      this.module._ffwasm_decode_free(this.handle)
      this.handle = 0
    }
    if (this.videoDataPtr) {
      this.module._free(this.videoDataPtr)
      this.videoDataPtr = 0
    }
    this.videoInfo = null
  }

  private openVideo(videoData: ArrayBuffer) {
    this.closeCurrentVideo()
    this.videoDataPtr = this.module._malloc(videoData.byteLength)
    const heap = new Uint8Array(this.module.HEAPU8.buffer, this.videoDataPtr, videoData.byteLength)
    heap.set(new Uint8Array(videoData))

    this.handle = this.module._ffwasm_decode_open(this.videoDataPtr, videoData.byteLength, this.infoCallbackPtr)
    if (this.handle < 0)
      throw new Error('_ffwasm_decode_open failed')
    if (!this.videoInfo)
      throw new Error('video info callback was not called')
    this.play()
  }

  private setStyle(width: number, height: number) {
    this.canvas.width = width
    this.canvas.height = height
    this.canvas.style.width = `${width}px`
    this.canvas.style.height = `${height}px`
    this.controller.style.width = `${width + 4}px`
  }

  private drawAt(pts: number) {
    const ret = this.module._ffwasm_decode_frame(this.handle, pts, this.decodeCallbackPtr)
    if (ret < 0)
      console.error('_ffwasm_decode_frame failed', ret, pts)
    return ret
  }

  private tick = () => {
    const info = this.videoInfo
    if (!info || !this.handle)
      return
    const pts = Date.now() - this.startTs
    if (pts >= info.duration) {
      this.pause()
      this.controller.value = '100'
      return
    }

    this.lastDrawPts = pts
    this.controller.value = String((pts / info.duration) * 100)
    const ret = this.drawAt(pts)
    if (ret >= 0)
      this.rafId = requestAnimationFrame(this.tick)
  }

  private play() {
    if (!this.videoInfo || !this.handle)
      return
    if (this.rafId)
      cancelAnimationFrame(this.rafId)
    const resumePts = this.pauseAt || this.lastDrawPts
    this.startTs = Date.now() - resumePts
    this.pauseAt = 0
    this.rafId = requestAnimationFrame(this.tick)
  }

  private pause() {
    if (this.rafId) {
      cancelAnimationFrame(this.rafId)
      this.rafId = 0
    }
    this.pauseAt = this.lastDrawPts
  }
}
