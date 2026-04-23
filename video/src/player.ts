import { DrawYuv } from './helper.js'
import type { DecodeYUVCallback, IFFModule, IVideoInfo, IYUVObject } from './types'

export class FFWasmPlayer {
  private readonly pageHorizontalPadding = 24
  private readonly reservedUiHeight = 210
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

  private heapU8() {
    if (this.module.HEAPU8)
      return this.module.HEAPU8
    const buffer = this.module.wasmMemory?.buffer
    if (!buffer)
      throw new Error('wasm heap is unavailable (missing HEAPU8 and wasmMemory)')
    return new Uint8Array(buffer)
  }

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

  async init(defaultSource?: string) {
    const runtimeUrl = new URL('/wasm/libffmpeg.js', window.location.origin).href
    let ffmpegModule: { default?: (options: { locateFile: () => string }) => Promise<IFFModule> }
    try {
      ffmpegModule = await import(/* @vite-ignore */ runtimeUrl)
    }
    catch (error) {
      throw new Error(`load runtime failed: ${String(error)}`)
    }
    if (!ffmpegModule.default)
      throw new Error('runtime module has no default initializer')
    const ffmpegInit = ffmpegModule.default
    try {
      this.module = await ffmpegInit({ locateFile: () => '/wasm/libffmpeg.wasm' })
    }
    catch (error) {
      throw new Error(`initialize wasm failed: ${String(error)}`)
    }
    this.bindCallbacks()
    this.bindEvents()
    if (defaultSource)
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
      const heapU8 = this.heapU8()
      const yData = heapU8.subarray(yAddr, yAddr + yLen * height)
      const uData = heapU8.subarray(uAddr, uAddr + (uLen * height) / 2)
      const vData = heapU8.subarray(vAddr, vAddr + (vLen * height) / 2)
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
    this.playBtn.addEventListener('click', () => this.restartFromBeginning())
    this.stopBtn.addEventListener('click', () => this.pause())
    window.addEventListener('resize', () => {
      const info = this.videoInfo
      if (info)
        this.setStyle(info.width, info.height)
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
    const heapU8 = this.heapU8()
    const heap = new Uint8Array(heapU8.buffer, this.videoDataPtr, videoData.byteLength)
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
    const maxDisplayWidth = Math.max(1, window.innerWidth - this.pageHorizontalPadding)
    const maxDisplayHeight = Math.max(1, window.innerHeight - this.reservedUiHeight)
    const ratio = Math.min(1, maxDisplayWidth / width, maxDisplayHeight / height)
    const displayWidth = Math.max(1, Math.floor(width * ratio))
    const displayHeight = Math.max(1, Math.floor(height * ratio))
    this.canvas.style.width = `${displayWidth}px`
    this.canvas.style.height = `${displayHeight}px`
    this.controller.style.width = `${displayWidth + 4}px`
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

  private restartFromBeginning() {
    if (!this.handle)
      return
    this.pause()
    this.lastDrawPts = 0
    this.pauseAt = 0
    this.controller.value = '0'
    const ret = this.module._ffwasm_seek_frame(this.handle, 0, this.decodeCallbackPtr)
    if (ret < 0)
      console.error('_ffwasm_seek_frame failed', ret, 0)
    this.play()
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
